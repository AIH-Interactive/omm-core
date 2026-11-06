<# Local M2 differential. Third-party sample data and normalized dumps never enter artifacts. #>
param(
    [string] $Jdk = 'D:\Program Files\Java\25',
    [string] $Python = 'D:\ProgramData\miniconda3\python.exe',
    [string] $BuildDirectory = 'build/windows-x64-Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location -LiteralPath $repoRoot
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$manifest = Get-Content docs/m0/sample-manifest.json -Raw -Encoding UTF8 | ConvertFrom-Json
$samples = @($manifest.entries | Where-Object engine -eq 'ysm')
$paths = @($samples | ForEach-Object { Join-Path "$repoRoot/refs/avatars" $_.name })
$encrypted = @($samples | Where-Object type -eq 'file' | ForEach-Object { Join-Path "$repoRoot/refs/avatars" $_.name })
$allPaths = @($manifest.entries | ForEach-Object { Join-Path "$repoRoot/refs/avatars" $_.name })
$classPath = (Get-Content build/omp-harness/classpath.txt -Raw).Trim()
$java = Join-Path $Jdk 'bin/java.exe'
$javaOptions = @('--enable-native-access=ALL-UNNAMED', '--sun-misc-unsafe-memory-access=allow',
    '-Dstdout.encoding=UTF-8', '-Xmx4g', '-cp', $classPath)
$bin = Join-Path $BuildDirectory 'bin/Release'
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ('omm-m2-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
Write-Host "Local differential data: $temporary"

function Invoke-Check {
    param([string] $Name, [string] $Command, [string[]] $Arguments)
    $log = Join-Path $BuildDirectory "logs/m2-$Name.log"
    & $Command @Arguments 2>&1 | Tee-Object -FilePath $log | Out-Host
    $checkExitCode = $LASTEXITCODE
    if ($checkExitCode -ne 0) {
        Get-Content -LiteralPath $log -Tail 30 | Out-Host
        throw "$Name failed with exit code $checkExitCode"
    }
}

Invoke-Check 'decode-java' $java ($javaOptions + @('OmpDecodeProbe') + $encrypted)
Invoke-Check 'decode-native' (Join-Path $bin 'omm_ysm_decode_probe.exe') $encrypted
$javaDecode = @(Get-Content "$BuildDirectory/logs/m2-decode-java.log" -Encoding UTF8 |
    Where-Object { $_ -match '^(file|entry|payload|binary)\t' } | Sort-Object)
$nativeDecode = @(Get-Content "$BuildDirectory/logs/m2-decode-native.log" -Encoding UTF8 |
    Where-Object { $_ -match '^(file|entry|payload|binary)\t' } | Sort-Object)
if ($javaDecode.Count -eq 0 -or (Compare-Object $javaDecode $nativeDecode) -or
    ($javaDecode -match '\tERROR') -or ($nativeDecode -match '\tERROR') -or
    @($javaDecode | Where-Object { $_ -match '^file\t' }).Count -ne $encrypted.Count) {
    throw 'Decoded payload/resource or binary-model digest mismatch'
}
Write-Host "Decode differential: $($encrypted.Count) samples PASS"

$reference = Join-Path $temporary 'java.txt'
Invoke-Check 'normalized-java' $java ($javaOptions + @('top.aihmc.omnimorph.avatar.ysm.NormalizedDump',
    '--output', $reference) + $paths)
$previousThreads = $env:OMM_TEST_THREADS
try {
    foreach ($threads in @(1, 0)) {
        $env:OMM_TEST_THREADS = "$threads"
        $candidate = Join-Path $temporary "native-$threads.txt"
        $candidateArguments = @('--output', $candidate) + $paths
        Invoke-Check "normalized-native-$threads" (Join-Path $bin 'omm_ysm_normalized_dump.exe') $candidateArguments
        Invoke-Check "compare-$threads" $Python @('-I', '-X', 'utf8', 'tools/m2/compare_normalized.py', $reference, $candidate)
        $count = @(Select-String -LiteralPath $candidate -Pattern '^SAMPLE\t').Count
        if ($count -ne $samples.Count) {
            throw "Expected $($samples.Count) normalized samples, received $count"
        }
    }
}
finally {
    $env:OMM_TEST_THREADS = $previousThreads
}

Invoke-Check 'corpus' (Join-Path $bin 'omm_ysm_asset_probe.exe') $allPaths
$corpus = @(Get-Content "$BuildDirectory/logs/m2-corpus.log" -Encoding UTF8)
if (@($corpus | Where-Object { $_ -match '^OK\t' }).Count -ne $samples.Count -or
    @($corpus | Where-Object { $_ -match '^FAIL\t' }).Count -ne ($allPaths.Count - $samples.Count)) {
    throw 'Unexpected corpus acceptance/rejection count'
}
Write-Host "M2 differential PASS: $($samples.Count) YSM, $($allPaths.Count - $samples.Count) negative samples"
