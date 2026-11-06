param(
    [string] $Jdk = 'D:\Program Files\Java\25',
    [string] $Library = 'dist/windows-x64/bin/omm_core.dll',
    [ValidateRange(0, 1000)] [int] $Warmup = 5,
    [ValidateRange(1, 10000)] [int] $Iterations = 20,
    [int[]] $ThreadCounts = @(1, 8, 16),
    [string[]] $Samples = @('refs/avatars/01_taisho_maid', 'refs/avatars/old.ysm',
        'refs/avatars/new.ysm', 'refs/avatars/芙宁娜v3.14中文配音.ysm')
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$classPath = (Get-Content build/omp-harness/classpath.txt -Raw).Trim()
$libraryPath = (Resolve-Path -LiteralPath $Library).Path
$previousThreads = $env:OMM_BENCH_THREADS
try {
    foreach ($threads in $ThreadCounts) {
        if ($threads -lt 0) {
            throw 'Thread counts must be nonnegative (0 = automatic).'
        }
    }
    $env:OMM_BENCH_THREADS = $ThreadCounts -join ','
    $arguments = @('--enable-native-access=ALL-UNNAMED', '--sun-misc-unsafe-memory-access=allow',
        '-Dstdout.encoding=UTF-8', '-Xmx4g', '-cp', $classPath,
        'top.aihmc.omnimorph.avatar.ysm.ParseBenchmark', $libraryPath, "$Warmup", "$Iterations") + $Samples
    $log = 'build-m2-benchmark.log'
    Write-Host "Benchmark DLL SHA-256: $((Get-FileHash -LiteralPath $libraryPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    & (Join-Path $Jdk 'bin/java.exe') @arguments 2>&1 | Tee-Object -FilePath $log | Out-Host
    $benchmarkExitCode = $LASTEXITCODE
    if ($benchmarkExitCode -ne 0) {
        Get-Content -LiteralPath $log -Tail 30 | Out-Host
        throw "Parser benchmark failed with exit code $benchmarkExitCode"
    }
}
finally {
    $env:OMM_BENCH_THREADS = $previousThreads
}
