# Windows dependency setup script for Yakety
# This script installs all required dependencies for building Yakety on Windows
# Run as Administrator!
# Requires: Chocolatey (https://chocolatey.org/install)

param(
    [switch]$SkipChocolateyCheck = $false
)

Write-Host "=== Installing Windows Dependencies for Yakety ===" -ForegroundColor Green
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Please right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Check if Chocolatey is installed
if (-not (Test-Path -Path "$env:ProgramData\chocolatey\bin\choco.exe")) {
    if (-not $SkipChocolateyCheck) {
        Write-Host "WARNING: Chocolatey is not installed!" -ForegroundColor Yellow
        Write-Host "Chocolatey should have been installed by the workflow." -ForegroundColor Yellow
        Write-Host "If running locally, please install Chocolatey first:" -ForegroundColor Yellow
        Write-Host "  https://chocolatey.org/install" -ForegroundColor Cyan
        exit 1
    }
}

Write-Host "Installing build tools and CMake..." -ForegroundColor Cyan
choco install cmake ninja --yes

Write-Host "Installing Visual Studio Build Tools..." -ForegroundColor Cyan
choco install visualstudio2022buildtools --yes
choco install visualstudio2022-workload-nativedesktop --yes

Write-Host "Installing git and development tools..." -ForegroundColor Cyan
choco install git curl wget --yes

Write-Host "Installing optional development tools..." -ForegroundColor Cyan
choco install pkgconfiglite --yes

# Add to PATH if needed
$chocoPath = "$env:ProgramData\chocolatey\bin"
if ($env:PATH -notlike "*$chocoPath*") {
    $env:PATH = "$chocoPath;$env:PATH"
    [Environment]::SetEnvironmentVariable("PATH", $env:PATH, [System.EnvironmentVariableTarget]::Machine)
}

Write-Host ""
Write-Host "✅ Windows dependencies installed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "You can now build Yakety with:" -ForegroundColor Cyan
Write-Host "  .\run.sh                    # Build release" -ForegroundColor White
Write-Host "  .\run.sh debug              # Build debug" -ForegroundColor White
Write-Host ""
Write-Host "Note: You may need to restart your terminal for PATH changes to take effect." -ForegroundColor Yellow
