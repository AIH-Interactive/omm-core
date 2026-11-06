<#
.SYNOPSIS
    Compiles the OMP differential harness (reference side of the M2 parser differential).

.DESCRIPTION
    Copies the unmodified OMP 26.3 decoder/parser sources it needs from refs/OMP, adds the
    harness stubs (tools/m2/omp-harness/stub: Molang text stub, YsmPackage directory loader,
    IOUtils, PNG pass-through) and compiles everything with JDK 25 into build/omp-harness/classes.
    The Gson, aircompressor and JOML jars come from the local Gradle cache used to build OMP.
#>
param(
    [string] $Jdk = 'D:\Program Files\Java\25',
    [string] $GradleCache = (Join-Path $env:USERPROFILE '.gradle\caches\modules-2\files-2.1')
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$omp = Join-Path $repoRoot 'refs\OMP\OMP_26.3\common\src\main\java\top\aihmc\omnimorph'
$harness = Join-Path $repoRoot 'build\omp-harness'
$sources = Join-Path $harness 'src'
$classes = Join-Path $harness 'classes'

if (Test-Path -LiteralPath $harness) {
    $resolvedHarness = (Resolve-Path -LiteralPath $harness).Path
    $expectedHarness = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'build\omp-harness'))
    if (-not [string]::Equals($resolvedHarness, $expectedHarness, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected harness directory: $resolvedHarness"
    }
    Remove-Item -LiteralPath $resolvedHarness -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sources, $classes | Out-Null

# Unmodified OMP sources (relative to top/aihmc/omnimorph).
$ompFiles = @(
    'avatar\ysm\crypto\*.java',
    'avatar\ysm\binary\YsmBinaryReader.java', 'avatar\ysm\binary\YsmBinaryModel.java', 'avatar\ysm\binary\YsmBinaryInput.java',
    'avatar\ysm\binary\YsmBinaryResourceMapper.java', 'avatar\ysm\binary\YsmBinaryMetadataMapper.java',
    'avatar\ysm\binary\YsmBinaryAnimationMapper.java', 'avatar\ysm\binary\YsmBinaryGeometryMapper.java',
    'avatar\ysm\YsmResourceLimits.java', 'avatar\ysm\YsmMemoryResourceSource.java', 'avatar\ysm\YsmResourceSource.java',
    'avatar\ysm\YsmManifestReader.java', 'avatar\ysm\YsmJson.java', 'avatar\ysm\YsmManifest.java',
    'avatar\ysm\YsmResourceIndex.java', 'avatar\ysm\YsmTextureOption.java', 'avatar\ysm\YsmAvatarKind.java',
    'avatar\ysm\YsmAvatarMetadata.java',
    'model\ysm\YsmGeometryParser.java', 'model\ysm\YsmGeometry.java',
    'model\ysm\animation\YsmAnimationParser.java', 'model\ysm\animation\YsmAnimationClip.java',
    'model\ysm\animation\YsmAnimationChannel.java', 'model\ysm\animation\YsmKeyframe.java',
    'model\ysm\animation\YsmAnimationEvent.java', 'model\ysm\animation\YsmBoneAnimation.java',
    'model\ysm\animation\YsmAnimationEventList.java',
    'model\ysm\controller\YsmAnimationControllerParser.java', 'model\ysm\controller\YsmAnimationController.java',
    'model\ysm\controller\YsmControllerState.java', 'model\ysm\controller\YsmBlendTransition.java',
    'model\ysm\controller\YsmControllerAnimationRef.java', 'model\ysm\controller\YsmControllerTransition.java',
    'model\ysm\controller\YsmControllerSlot.java'
)
foreach ($pattern in $ompFiles) {
    foreach ($file in (Get-ChildItem -Path (Join-Path $omp $pattern))) {
        $relative = $file.FullName.Substring($omp.Length).TrimStart('\')
        $target = Join-Path (Join-Path $sources 'top\aihmc\omnimorph') $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }
}
# Harness stubs overlay (never copied over an OMP parser or data class).
Copy-Item -Path (Join-Path $PSScriptRoot 'omp-harness\stub\*') -Destination $sources -Recurse -Force

$gson = Get-ChildItem -Path (Join-Path $GradleCache 'com.google.code.gson\gson\2.11.0') -Recurse -Filter 'gson-2.11.0.jar' | Select-Object -First 1
$joml = Get-ChildItem -Path (Join-Path $GradleCache 'org.joml\joml\1.10.8') -Recurse -Filter 'joml-1.10.8.jar' | Select-Object -First 1
$aircompressor = Get-ChildItem -Path (Join-Path $GradleCache 'io.airlift\aircompressor-v3\3.10') -Recurse -Filter 'aircompressor-v3-3.10.jar' | Select-Object -First 1
if (-not $gson -or -not $aircompressor -or -not $joml) {
    throw 'gson-2.11.0.jar, aircompressor-v3-3.10.jar or joml-1.10.8.jar not found in the Gradle cache.'
}
$classPath = "$($gson.FullName);$($aircompressor.FullName);$($joml.FullName)"
Set-Content -LiteralPath (Join-Path $harness 'classpath.txt') -Value "$classes;$classPath" -Encoding ASCII

$javaSources = @(Get-ChildItem -Path $sources -Recurse -Filter '*.java' | ForEach-Object { $_.FullName })
$javaSources += @(Get-ChildItem -Path (Join-Path $PSScriptRoot 'omp-harness') -Filter '*.java' | ForEach-Object { $_.FullName })
# omm-core Java bridge, for the JNI side of ParseBenchmark.
$javaSources += @(Get-ChildItem -Path (Join-Path $repoRoot 'bindings\java\src\main\java') -Recurse -Filter '*.java' | ForEach-Object { $_.FullName })
& (Join-Path $Jdk 'bin\javac.exe') '-J-Duser.language=en' -nowarn -encoding UTF-8 -d $classes -cp $classPath @javaSources
exit $LASTEXITCODE
