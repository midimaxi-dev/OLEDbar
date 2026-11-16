<#
.SYNOPSIS
    Packages a release build of OLEDbar as a portable zip and, if Inno Setup 6 is installed, a per-user installer.
.DESCRIPTION
    The version is read from the EXE's version resource (CMake project(VERSION)).
    Output in out\dist: OLEDbar-<version>-x64.zip, OLEDbar-<version>-x64-setup.exe, SHA256SUMS.txt
    -Sign signs the installer and its uninstaller with azure-sign.ps1 while compiling (CI release job only);
    OLEDbar.exe in BuildDir must already be signed.
#>
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\out\build\x64-release'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\out\dist'),
    [switch]$Sign
)
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = Join-Path $BuildDir 'OLEDbar.exe'
if (-not (Test-Path $exe)) {
    throw "Release build not found at $exe. Build first: cmake --preset x64-release; cmake --build --preset x64-release"
}
$exe = (Resolve-Path $exe).Path
$version = (Get-Item $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Unexpected version '$version' in $exe" }

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

# Portable zip.
$name = "OLEDbar-$version-x64"
$stage = Join-Path $OutDir $name
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item $exe $stage
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'README.md') $stage
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $OutDir "$name.zip") -CompressionLevel Optimal
Remove-Item $stage -Recurse -Force
Write-Host "Created $name.zip"

# Installer.
$iscc = Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $iscc) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if ($iscc) {
    $isccArgs = @('/Qp', "/DAppVersion=$version", "/DSourceExe=$exe", "/DOutputDir=$OutDir")
    if ($Sign) {
        # Inno Setup replaces $q with a quote and $f with the quoted path of the file to sign.
        $pwsh = (Get-Process -Id $PID).Path
        $signScript = Join-Path $PSScriptRoot 'azure-sign.ps1'
        $isccArgs += '/DSignInstaller'
        $isccArgs += '/Sazure=$q' + $pwsh + '$q -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $q' + $signScript + '$q -Path $f'
    }
    & $iscc @isccArgs (Join-Path $root 'packaging\installer\oledbar.iss')
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
    Write-Host "Created $name-setup.exe"
} elseif ($Sign) {
    throw 'Inno Setup 6 not found; cannot build the signed installer.'
} else {
    Write-Warning 'Inno Setup 6 not found; skipping the installer (winget install JRSoftware.InnoSetup).'
}

# Checksums for the release page.
Get-ChildItem $OutDir -File | Where-Object { $_.Name -like '*.zip' -or $_.Name -like '*-setup.exe' } |
    Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash.ToLowerInvariant())  $(Split-Path $_.Path -Leaf)" } |
    Set-Content (Join-Path $OutDir 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Packages in $OutDir"
