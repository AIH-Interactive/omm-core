<#
.SYNOPSIS
    M0 sample inventory: classifies local model samples by content, not by extension.

.DESCRIPTION
    Walks the top level of the sample root (default refs/avatars) and writes a
    deterministic JSON manifest with container kind, engine, SHA-256 and size.
    Directory hashes are SHA-256 over the sorted "relative-path<TAB>file-sha256" lines,
    so the manifest is reproducible on any machine holding the same files.
    The script only reads samples; it never decrypts or extracts them.
#>
param(
    [string] $SampleRoot = (Join-Path $PSScriptRoot '..\..\refs\avatars'),
    [string] $OutputPath = (Join-Path $PSScriptRoot '..\..\docs\m0\sample-manifest.json')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-Sha256Hex([byte[]] $bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-FileSha256([string] $path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
}

function Get-DirectoryDigest([string] $root) {
    $rootFull = (Resolve-Path -LiteralPath $root).Path.TrimEnd('\') + '\'
    $files = Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object { $_.FullName.Substring($rootFull.Length).Replace('\', '/') } -Culture ''
    $lines = New-Object System.Text.StringBuilder
    $total = 0L
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($rootFull.Length).Replace('\', '/')
        [void] $lines.Append($relative).Append("`t").Append((Get-FileSha256 $file.FullName)).Append("`n")
        $total += $file.Length
    }
    return [pscustomobject] @{
        Sha256    = Get-Sha256Hex ([System.Text.Encoding]::UTF8.GetBytes($lines.ToString()))
        Bytes     = $total
        FileCount = $files.Count
        Names     = @($files | ForEach-Object { $_.FullName.Substring($rootFull.Length).Replace('\', '/') })
    }
}

# Engine/layout from a list of relative entry names (directory listing or ZIP entries).
function Get-LayoutFromNames([string[]] $names) {
    $leaf = @($names | Where-Object { $_ } | ForEach-Object { ($_.ToLowerInvariant() -split '/')[-1] })

    if ($leaf -contains 'avatar.json') {
        return @('figura', 'figura-avatar')
    }
    if ($leaf -contains 'ysm.json') {
        return @('ysm', 'ysm-raw-current')
    }
    if (@($leaf | Where-Object { $_ -like '*.animation.json' }).Count -gt 0 -and $leaf -contains 'main.json') {
        return @('ysm', 'ysm-raw-legacy')
    }
    return @('unknown', 'unrecognized-layout')
}

function Get-FileClassification([string] $path) {
    $stream = [System.IO.File]::OpenRead($path)
    try {
        $header = New-Object byte[] 8
        $read = $stream.Read($header, 0, 8)
    }
    finally {
        $stream.Dispose()
    }

    $offset = 0
    if ($read -ge 3 -and $header[0] -eq 0xEF -and $header[1] -eq 0xBB -and $header[2] -eq 0xBF) {
        $offset = 3
    }
    $isYsgp = $read -ge ($offset + 4) -and
        $header[$offset] -eq 0x59 -and $header[$offset + 1] -eq 0x53 -and
        $header[$offset + 2] -eq 0x47 -and $header[$offset + 3] -eq 0x50

    if ($isYsgp -and $offset -eq 0 -and $read -ge 8) {
        $version = ([int] $header[4] -shl 24) -bor ([int] $header[5] -shl 16) -bor ([int] $header[6] -shl 8) -bor [int] $header[7]
        if ($version -eq 1 -or $version -eq 2) {
            return @('ysm', "ysgp-binary-legacy-v$version")
        }
    }
    if ($isYsgp) {
        # Text-header YSGP: the crypto version follows the metadata block and is resolved in M2.
        return @('ysm', 'ysgp-text-header')
    }
    if ($read -ge 4 -and $header[0] -eq 0x50 -and $header[1] -eq 0x4B -and $header[2] -eq 0x03 -and $header[3] -eq 0x04) {
        $zip = [System.IO.Compression.ZipFile]::OpenRead($path)
        try {
            $layout = Get-LayoutFromNames @($zip.Entries | ForEach-Object { $_.FullName })
            return @($layout[0], "zip/$($layout[1])")
        }
        finally {
            $zip.Dispose()
        }
    }
    return @('unknown', 'unrecognized-file')
}

$root = (Resolve-Path -LiteralPath $SampleRoot).Path
$entries = New-Object System.Collections.Generic.List[object]

foreach ($item in (Get-ChildItem -LiteralPath $root | Sort-Object Name -Culture '')) {
    if ($item.PSIsContainer) {
        $digest = Get-DirectoryDigest $item.FullName
        $layout = Get-LayoutFromNames $digest.Names
        $entries.Add([ordered] @{
            name      = $item.Name
            type      = 'directory'
            engine    = $layout[0]
            container = "directory/$($layout[1])"
            sha256    = $digest.Sha256
            bytes     = $digest.Bytes
            fileCount = $digest.FileCount
        })
    }
    else {
        $classification = Get-FileClassification $item.FullName
        $entries.Add([ordered] @{
            name      = $item.Name
            type      = 'file'
            engine    = $classification[0]
            container = $classification[1]
            sha256    = Get-FileSha256 $item.FullName
            bytes     = $item.Length
            fileCount = 1
        })
    }
}

$manifest = [ordered] @{
    schema      = 'omm-m0-sample-manifest/1'
    sampleRoot  = 'refs/avatars'
    hashRule    = 'file: SHA-256 of bytes; directory: SHA-256 of sorted UTF-8 lines "relpath<TAB>sha256<LF>"'
    entries     = $entries
}

$outputDirectory = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}
$json = $manifest | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText($OutputPath, $json + "`n", (New-Object System.Text.UTF8Encoding $false))

$entries | ForEach-Object { [pscustomobject] $_ } | Group-Object container | Sort-Object Name |
    ForEach-Object { '{0,-36} {1}' -f $_.Name, $_.Count }
Write-Output "entries: $($entries.Count) -> $OutputPath"
