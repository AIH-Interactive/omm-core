<#
.SYNOPSIS
    Builds, tests and packages the omm-core native SDK for one target.

.DESCRIPTION
    Entry point contract: docs/OmniMorph-native-plan.md section 7.
    Each step runs in the foreground, tees its output to build/<target>-<config>/logs/<step>.log,
    and stops on the first non-zero exit code. The output directory receives the library,
    public headers, bridge JAR, license and manifest.json.

    Works on Windows PowerShell 5.1 and PowerShell 7.

.PARAMETER Target
    windows-x64 | linux-x64 | android-arm64 | macos-arm64 | macos-x64

.PARAMETER RequireAllJdks
    Fail unless JNI load tests ran on Java 17, 21 and 25 (local acceptance).
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('windows-x64', 'linux-x64', 'android-arm64', 'macos-arm64', 'macos-x64')]
    [string] $Target,

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory,

    [switch] $RequireAllJdks
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = Join-Path (Join-Path $repoRoot 'build') "$Target-$Configuration"
$logDirectory = Join-Path $buildDirectory 'logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
if (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot $OutputDirectory
}

$hostIsWindows = [System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT
$hostIsMac = (-not $hostIsWindows) -and ((& uname -s) -eq 'Darwin')
$hostIsLinux = (-not $hostIsWindows) -and (-not $hostIsMac)

$validation = [ordered] @{}

# Runs one external command in the foreground with a dedicated log; throws on failure.
function Invoke-Step {
    param(
        [string] $Name,
        [string] $Command,
        [string[]] $Arguments
    )
    $log = Join-Path $logDirectory "$Name.log"
    Write-Host "==> [$Name] $Command $($Arguments -join ' ')"
    # Native stderr must not become a terminating PowerShell error; the exit code decides.
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # Out-Host keeps the live console output without leaking it into callers' return values.
        & $Command @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log | Out-Host
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        Write-Host "[$Name] failed with exit code $exitCode; last log lines:"
        Get-Content -LiteralPath $log -Tail 40 | ForEach-Object { Write-Host $_ }
        throw "Step '$Name' failed with exit code $exitCode (log: $log)"
    }
}

function Assert-HostMatchesTarget {
    $ok = $false
    switch ($Target) {
        'windows-x64' { $ok = $hostIsWindows }
        'linux-x64' { $ok = $hostIsLinux }
        'android-arm64' { $ok = $hostIsLinux -or $hostIsMac }
        'macos-arm64' { $ok = $hostIsMac }
        'macos-x64' { $ok = $hostIsMac }
    }
    if (-not $ok) {
        throw "Target $Target cannot be built on this host."
    }
}

function Get-AndroidNdk {
    foreach ($name in @('ANDROID_NDK_ROOT', 'ANDROID_NDK_HOME', 'ANDROID_NDK_LATEST_HOME')) {
        $value = [System.Environment]::GetEnvironmentVariable($name)
        if ($value -and (Test-Path -LiteralPath (Join-Path $value 'build/cmake/android.toolchain.cmake'))) {
            return $value
        }
    }
    throw 'Android NDK not found (set ANDROID_NDK_ROOT to NDK r28c or newer).'
}

# JDK homes keyed by feature version. Explicit OMM_JDK_<n> wins, then CI JAVA_HOME_<n>_<arch>, then local defaults.
function Get-JdkHomes {
    $homes = [ordered] @{}
    foreach ($feature in @(17, 21, 25)) {
        $candidates = @(
            [System.Environment]::GetEnvironmentVariable("OMM_JDK_$feature"),
            [System.Environment]::GetEnvironmentVariable("JAVA_HOME_${feature}_X64"),
            [System.Environment]::GetEnvironmentVariable("JAVA_HOME_${feature}_ARM64"),
            [System.Environment]::GetEnvironmentVariable("JAVA_HOME_${feature}_arm64")
        )
        if ($hostIsWindows) {
            $candidates += "D:\Program Files\Java\$feature"
        }
        foreach ($candidate in $candidates) {
            if (-not $candidate) {
                continue
            }
            $javaName = 'java'
            if ($hostIsWindows) {
                $javaName = 'java.exe'
            }
            if (Test-Path -LiteralPath (Join-Path (Join-Path $candidate 'bin') $javaName)) {
                $homes["$feature"] = $candidate
                break
            }
        }
    }
    return $homes
}

function Get-LibraryFileName {
    if ($Target -eq 'windows-x64') {
        return 'omm_core.dll'
    }
    if ($Target -like 'macos-*') {
        return 'libomm_core.dylib'
    }
    return 'libomm_core.so'
}

# Every exported symbol must be omm_* or the JNI-mandated JNI_OnLoad.
function Test-Exports([string] $libraryPath, [string] $ndk) {
    $symbols = @()
    if ($Target -eq 'windows-x64') {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $dumpbin = Get-ChildItem -Path (Join-Path $vsPath 'VC\Tools\MSVC') -Recurse -Filter dumpbin.exe |
            Where-Object { $_.FullName -like '*Hostx64\x64*' } | Select-Object -First 1
        if (-not $dumpbin) {
            throw 'dumpbin.exe not found'
        }
        # The library must not import the shared MSVC STL/CRT: hosts (JVMs) may preload older copies.
        Invoke-Step -Name 'dependents' -Command $dumpbin.FullName -Arguments @('/nologo', '/dependents', $libraryPath)
        $sharedCrt = @(Get-Content -LiteralPath (Join-Path $logDirectory 'dependents.log') |
            Where-Object { $_ -match '(?i)^\s+(msvcp|vcruntime|ucrtbase)\S*\.dll' })
        if ($sharedCrt.Count -gt 0) {
            throw "omm_core.dll imports the shared CRT: $($sharedCrt -join ', ')"
        }

        Invoke-Step -Name 'exports' -Command $dumpbin.FullName -Arguments @('/nologo', '/exports', $libraryPath)
        $inTable = $false
        foreach ($line in (Get-Content -LiteralPath (Join-Path $logDirectory 'exports.log'))) {
            if ($line -match '^\s+ordinal\s+hint\s+RVA\s+name') {
                $inTable = $true
                continue
            }
            if ($inTable -and $line -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') {
                $symbols += $Matches[1]
            }
            elseif ($inTable -and $line -match '^\s+Summary') {
                break
            }
        }
    }
    else {
        $nm = 'nm'
        $arguments = @('-D', '--defined-only', $libraryPath)
        if ($Target -like 'macos-*') {
            $arguments = @('-gU', $libraryPath)
        }
        elseif ($Target -eq 'android-arm64') {
            $nm = (Get-ChildItem -Path (Join-Path $ndk 'toolchains/llvm/prebuilt') -Recurse -Filter 'llvm-nm' | Select-Object -First 1).FullName
        }
        Invoke-Step -Name 'exports' -Command $nm -Arguments $arguments
        foreach ($line in (Get-Content -LiteralPath (Join-Path $logDirectory 'exports.log'))) {
            if ($line -match '^\S*\s+[TtDdBbRrVvWwiI]\s+(\S+)$') {
                $symbols += $Matches[1]
            }
        }
    }

    $unexpected = @()
    foreach ($symbol in $symbols) {
        $name = $symbol
        if ($Target -like 'macos-*' -and $name.StartsWith('_')) {
            $name = $name.Substring(1)
        }
        if (-not ($name.StartsWith('omm_') -or $name -eq 'JNI_OnLoad')) {
            $unexpected += $symbol
        }
    }
    if ($symbols.Count -eq 0) {
        throw 'No exported symbols found; export check cannot pass.'
    }
    if ($unexpected.Count -gt 0) {
        throw "Unexpected exported symbols: $($unexpected -join ', ')"
    }
    Write-Host "exports: $($symbols.Count) symbols, all omm_* or JNI_OnLoad"
    return $symbols
}

function Get-CompilerDescription {
    $cache = Join-Path $buildDirectory 'CMakeCache.txt'
    $compilerFile = Get-ChildItem -Path (Join-Path $buildDirectory 'CMakeFiles') -Recurse -Filter 'CMakeCXXCompiler.cmake' |
        Select-Object -First 1
    $id = 'unknown'
    $version = 'unknown'
    if ($compilerFile) {
        foreach ($line in (Get-Content -LiteralPath $compilerFile.FullName)) {
            if ($line -match '^set\(CMAKE_CXX_COMPILER_ID "([^"]*)"\)') {
                $id = $Matches[1]
            }
            if ($line -match '^set\(CMAKE_CXX_COMPILER_VERSION "([^"]*)"\)') {
                $version = $Matches[1]
            }
        }
    }
    if (-not (Test-Path -LiteralPath $cache)) {
        throw 'CMakeCache.txt missing after configure'
    }
    return "$id $version"
}

function Get-AbiVersion {
    $header = Get-Content -LiteralPath (Join-Path $repoRoot 'include/omm/omm.h') -Raw
    $major = [regex]::Match($header, '#define OMM_ABI_MAJOR (\d+)u').Groups[1].Value
    $minor = [regex]::Match($header, '#define OMM_ABI_MINOR (\d+)u').Groups[1].Value
    $asset = [regex]::Match($header, '#define OMM_ASSET_SCHEMA_VERSION (\d+)u').Groups[1].Value
    $mesh = [regex]::Match($header, '#define OMM_MESH_SCHEMA_VERSION (\d+)u').Groups[1].Value
    $cacheVersion = [regex]::Match($header, '#define OMM_CACHE_VERSION (\d+)u').Groups[1].Value
    return [ordered] @{
        abi         = "$major.$minor"
        assetSchema = [int] $asset
        meshSchema  = [int] $mesh
        cache       = [int] $cacheVersion
    }
}

Assert-HostMatchesTarget
$sourceCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
$dirty = [bool] (& git -C $repoRoot status --porcelain --untracked-files=no)
$buildId = "0.1.0+$((& git -C $repoRoot rev-parse --short=12 HEAD).Trim())"
if ($dirty) {
    $buildId = "$buildId-dirty"
}

# ---- Configure ----
$configureArguments = @('-S', $repoRoot, '-B', $buildDirectory, "-DOMM_BUILD_ID=$buildId")
$ndk = $null
$runsTests = $true
switch ($Target) {
    'windows-x64' {
        $configureArguments += @('-A', 'x64')
    }
    'linux-x64' {
        $configureArguments += @("-DCMAKE_BUILD_TYPE=$Configuration")
    }
    'android-arm64' {
        $ndk = Get-AndroidNdk
        $configureArguments += @(
            "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $ndk 'build/cmake/android.toolchain.cmake')",
            '-DANDROID_ABI=arm64-v8a',
            '-DANDROID_PLATFORM=android-26',
            "-DCMAKE_BUILD_TYPE=$Configuration",
            '-DOMM_BUILD_TESTS=OFF')
        $runsTests = $false
    }
    'macos-arm64' {
        $configureArguments += @("-DCMAKE_BUILD_TYPE=$Configuration", '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0')
    }
    'macos-x64' {
        $configureArguments += @("-DCMAKE_BUILD_TYPE=$Configuration", '-DCMAKE_OSX_ARCHITECTURES=x86_64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0')
    }
}

$jdkHomes = Get-JdkHomes
if ($jdkHomes.Count -eq 0) {
    throw 'No JDK 17/21/25 found (set OMM_JDK_17 / OMM_JDK_21 / OMM_JDK_25).'
}
# The JNI headers come from the oldest available JDK; the bridge targets Java 17.
$env:JAVA_HOME = @($jdkHomes.Values)[0]

Invoke-Step -Name 'configure' -Command 'cmake' -Arguments $configureArguments

# ---- Build ----
Invoke-Step -Name 'build' -Command 'cmake' -Arguments @('--build', $buildDirectory, '--config', $Configuration, '--parallel')
$validation['build'] = 'PASS'

# ---- Native tests ----
if ($runsTests) {
    Invoke-Step -Name 'ctest' -Command 'ctest' -Arguments @('--test-dir', $buildDirectory, '-C', $Configuration, '--output-on-failure')
    $validation['nativeTests'] = 'PASS'
}
else {
    $validation['nativeTests'] = 'NOT_RUN (cross-compiled; target execution is MANUAL/PENDING)'
}

# ---- Install ----
if (Test-Path -LiteralPath $OutputDirectory) {
    $expectedOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
    $resolvedOutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path.TrimEnd('\', '/')
    $volumeRoot = [System.IO.Path]::GetPathRoot($expectedOutputDirectory).TrimEnd('\', '/')
    if (-not [string]::Equals($resolvedOutputDirectory, $expectedOutputDirectory, [System.StringComparison]::OrdinalIgnoreCase) -or
        [string]::Equals($resolvedOutputDirectory, $repoRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        [string]::Equals($resolvedOutputDirectory, $volumeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unsafe install directory: $resolvedOutputDirectory"
    }
    Remove-Item -LiteralPath $resolvedOutputDirectory -Recurse -Force
}
Invoke-Step -Name 'install' -Command 'cmake' -Arguments @('--install', $buildDirectory, '--config', $Configuration, '--prefix', $OutputDirectory)

$libraryName = Get-LibraryFileName
$library = Get-ChildItem -Path $OutputDirectory -Recurse -Filter $libraryName | Select-Object -First 1
if (-not $library) {
    throw "Installed library $libraryName not found under $OutputDirectory"
}

# ---- Export check ----
$exports = Test-Exports $library.FullName $ndk
$validation['exports'] = 'PASS'

# ---- Java bridge JAR and JNI load tests ----
$bridgeHome = $jdkHomes[0]
$javaClasses = Join-Path $buildDirectory 'java/classes'
$testClasses = Join-Path $buildDirectory 'java/test-classes'
New-Item -ItemType Directory -Force -Path $javaClasses, $testClasses | Out-Null
$bridgeSources = @(Get-ChildItem -Path (Join-Path $repoRoot 'bindings/java/src/main/java') -Recurse -Filter '*.java' | ForEach-Object { $_.FullName })
$javac = Join-Path (Join-Path $bridgeHome 'bin') 'javac'
$jar = Join-Path (Join-Path $bridgeHome 'bin') 'jar'
Invoke-Step -Name 'javac-bridge' -Command $javac -Arguments (@('--release', '17', '-encoding', 'UTF-8', '-d', $javaClasses) + $bridgeSources)
$bridgeJar = Join-Path $OutputDirectory 'java/omm-core-bridge.jar'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $bridgeJar) | Out-Null
Invoke-Step -Name 'jar-bridge' -Command $jar -Arguments @('--create', '--file', $bridgeJar, '-C', $javaClasses, '.')

$jniResults = [ordered] @{}
if ($runsTests) {
    $currentFixture = Join-Path $buildDirectory 'java/current-fixture.ysm'
    $fixtureExecutable = if ($hostIsWindows) {
        Join-Path $buildDirectory "bin/$Configuration/omm_current_parser_test.exe"
    } else {
        Join-Path $buildDirectory 'omm_current_parser_test'
    }
    Invoke-Step -Name 'current-fixture' -Command $fixtureExecutable -Arguments @('--write-fixture', $currentFixture)
    $javaTests = @(Get-ChildItem -Path (Join-Path $repoRoot 'tests/java') -Filter '*.java' | ForEach-Object { $_.FullName })
    Invoke-Step -Name 'javac-tests' -Command $javac -Arguments (@('--release', '17', '-encoding', 'UTF-8', '-cp', $javaClasses, '-d', $testClasses) + $javaTests)
    $classPath = "$javaClasses$([System.IO.Path]::PathSeparator)$testClasses"
    foreach ($feature in $jdkHomes.Keys) {
        $java = Join-Path (Join-Path $jdkHomes[$feature] 'bin') 'java'
        Invoke-Step -Name "jni-load-java$feature" -Command $java -Arguments @('-cp', $classPath, 'JniLoadTest', $library.FullName)
        Invoke-Step -Name "jni-parser-java$feature" -Command $java -Arguments @('-cp', $classPath, 'JniParserTest', $library.FullName, $currentFixture)
        $jniResults["java$feature"] = 'PASS'
    }
    foreach ($feature in @('17', '21', '25')) {
        if (-not $jniResults.Contains("java$feature")) {
            if ($RequireAllJdks) {
                throw "JNI load test on Java $feature did not run (JDK not found)."
            }
            $jniResults["java$feature"] = 'NOT_RUN (JDK not available on this host)'
        }
    }
}
else {
    foreach ($feature in @('17', '21', '25')) {
        $jniResults["java$feature"] = 'NOT_RUN (cross-compiled)'
    }
}
$validation['jniLoad'] = $jniResults
$validation['gpu'] = 'NOT_APPLICABLE (no GPU code before M4)'
$validation['ompDifferential'] = 'LOCAL_ONLY (tools/m2/Test-OmpDifferential.ps1; third-party samples are not in CI)'
$validation['realGame'] = 'MANUAL/PENDING'

# ---- Manifest ----
$osName = ($Target -split '-')[0]
$arch = ($Target -split '-')[1]
$minimumRuntime = @{
    'windows-x64'   = 'Windows 10 x64'
    'linux-x64'     = 'glibc of the build image (Ubuntu 22.04)'
    'android-arm64' = 'Android API 26'
    'macos-arm64'   = 'macOS 13.0'
    'macos-x64'     = 'macOS 13.0'
}[$Target]

$manifest = [ordered] @{
    schema          = 'omm-native-manifest/1'
    target          = $Target
    os              = $osName
    arch            = $arch
    configuration   = $Configuration
    sourceCommit    = $sourceCommit
    sourceDirty     = $dirty
    buildId         = $buildId
    versions        = Get-AbiVersion
    minimumRuntime  = $minimumRuntime
    compiler        = Get-CompilerDescription
    dependencies    = (Get-Content -LiteralPath (Join-Path $repoRoot 'deps/dependencies.lock.json') -Raw | ConvertFrom-Json)
    library         = [ordered] @{
        file   = $library.FullName.Substring($OutputDirectory.Length).TrimStart('\', '/').Replace('\', '/')
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $library.FullName).Hash.ToLowerInvariant()
        bytes  = $library.Length
    }
    bridgeJar       = [ordered] @{
        file   = 'java/omm-core-bridge.jar'
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $bridgeJar).Hash.ToLowerInvariant()
    }
    exportedSymbols = @($exports | Sort-Object)
    validation      = $validation
}
$manifestPath = Join-Path $OutputDirectory 'manifest.json'
[System.IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6) + "`n", (New-Object System.Text.UTF8Encoding $false))
Write-Host "manifest: $manifestPath"
Write-Host "library sha256: $($manifest.library.sha256)"
exit 0
