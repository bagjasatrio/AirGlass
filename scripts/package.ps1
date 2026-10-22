# AirGlass portable packager
# Collects the built exe plus every DLL it needs at runtime into a single
# folder and zips it, so a user can download one archive and run the app
# without Visual Studio, vcpkg or FFmpeg.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts/package.ps1 [-Version 0.1]
param(
  [string]$Version = "0.1",
  [string]$BuildDir = "build/Release"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$exe = Join-Path $BuildDir "airglass.exe"
if (-not (Test-Path $exe)) {
  throw "airglass.exe not found in $BuildDir - build first: cmake --build build --config Release"
}

# Runtime dependencies. FFmpeg, SDL, libplist, OpenSSL and their transitive
# codec libraries ship next to the exe; the MSVC runtime is normally provided
# by the VC++ redistributable, but app-local copies are permitted so the zip
# stays self-contained.
$dlls = @(
  "avcodec-63.dll", "avformat-63.dll", "avutil-61.dll",
  "swresample-7.dll", "swscale-10.dll",
  "SDL2.dll", "SDL2_ttf.dll",
  "plist-2.0.dll",
  "libcrypto-3-x64.dll",
  "freetype.dll", "libpng16.dll", "z.dll", "bz2.dll",
  "brotlicommon.dll", "brotlidec.dll"
)

$outName = "AirGlass-$Version-win64"
$dist    = Join-Path $repo "dist/$outName"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist -Force | Out-Null

Copy-Item $exe $dist
$missing = @()
foreach ($d in $dlls) {
  $src = Join-Path $BuildDir $d
  if (Test-Path $src) { Copy-Item $src $dist } else { $missing += $d }
}

# App-local MSVC runtime: look in the VS redist folder first, then System32.
$redistNames = @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$redistDirs = @()
if (Test-Path $vswhere) {
  $vsRoot = & $vswhere -latest -property installationPath
  if ($vsRoot) {
    $redistDirs += Get-ChildItem -Path (Join-Path $vsRoot "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue |
      Sort-Object Name -Descending |
      ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" }
    $redistDirs += Get-ChildItem -Path (Join-Path $vsRoot "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue |
      Sort-Object Name -Descending |
      ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC142.CRT" }
  }
}
$redistDirs += "C:\Windows\System32"

foreach ($name in $redistNames) {
  $copied = $false
  foreach ($dir in $redistDirs) {
    $candidate = Join-Path $dir $name
    if (Test-Path $candidate) { Copy-Item $candidate $dist; $copied = $true; break }
  }
  if (-not $copied) { $missing += $name }
}

# Never ship per-user state.
foreach ($junk in @("identity.key", "imgui.ini", "settings.ini", "presets.ini", "soak.csv")) {
  Remove-Item (Join-Path $dist $junk) -ErrorAction SilentlyContinue
}

Copy-Item "LICENSE" $dist
Copy-Item "THIRD_PARTY_NOTICES.md" $dist
Copy-Item "README.md" $dist

$zip = Join-Path $repo "dist/$outName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip

$sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
"packaged  $zip  ($sizeMb MB)"
"files     $((Get-ChildItem $dist -File).Count)"
if ($missing.Count -gt 0) { "MISSING   $($missing -join ', ')" }
