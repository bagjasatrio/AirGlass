# Fetches a prebuilt FFmpeg shared build for Windows x64 into vendor/ffmpeg.
#
# Why this script exists: the old workflow hardcoded
#   .../releases/latest/download/ffmpeg-master-latest-win64-gpl-shared.zip
# BtbN renamed the asset to include the build hash (ffmpeg-N-128100-g...-win64-gpl-shared.zip)
# and the old alias now returns HTTP 404. curl wrote that 404 page to disk and
# Expand-Archive failed with "Central Directory corrupt", which is how CI went red
# without anyone noticing the real cause. Resolve the asset through the API and
# verify it is a real archive before extracting.

param(
    [string]$Dest = "vendor/ffmpeg",
    [string]$Repo = "BtbN/FFmpeg-Builds",
    [string]$Pattern = "*win64-gpl-shared.zip",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

if ((Test-Path $Dest) -and -not $Force) {
    Write-Host "FFmpeg already present at $Dest (use -Force to refetch)"
    exit 0
}

$headers = @{ "User-Agent" = "airglass-build"; "Accept" = "application/vnd.github+json" }
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -Headers $headers

$asset = $release.assets | Where-Object { $_.name -like $Pattern } | Select-Object -First 1
if (-not $asset) {
    Write-Error "No asset matching '$Pattern' in release $($release.tag_name). Found: $($release.assets.name -join ', ')"
}

Write-Host "Downloading $($asset.name) ($([math]::Round($asset.size / 1MB, 1)) MB)"

$tmp = Join-Path $env:TEMP ("airglass-ffmpeg-" + [guid]::NewGuid().ToString("N") + ".zip")
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmp -Headers $headers -MaximumRedirection 10

# A 404 or an HTML error page must fail here, not inside Expand-Archive.
$bytes = [System.IO.File]::ReadAllBytes($tmp)[0..3]
if (-not ($bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B)) {
    Remove-Item $tmp -ErrorAction SilentlyContinue
    Write-Error "Downloaded file is not a zip archive (likely an error page)."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$extract = Join-Path ([System.IO.Path]::GetTempPath()) ("airglass-ffmpeg-" + [guid]::NewGuid().ToString("N"))
[System.IO.Compression.ZipFile]::ExtractToDirectory($tmp, $extract)

$inner = Get-ChildItem -Path $extract -Directory | Select-Object -First 1
if (-not $inner) { Write-Error "Archive did not contain a top-level folder." }

if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
New-Item -ItemType Directory -Force -Path (Split-Path $Dest) | Out-Null
Move-Item -Path $inner.FullName -Destination $Dest

Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
Remove-Item $tmp -ErrorAction SilentlyContinue

$dll = Join-Path $Dest "bin\avcodec-*.dll"
if (-not (Get-ChildItem $dll -ErrorAction SilentlyContinue)) {
    Write-Error "Extracted tree has no bin\avcodec-*.dll; the archive layout changed."
}

Write-Host "FFmpeg ready at $Dest"
