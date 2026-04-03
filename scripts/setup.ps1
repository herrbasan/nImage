<#
.SYNOPSIS
    nImage dependency setup and build script

.DESCRIPTION
    This script sets up nImage's native dependencies and builds the native module.

    It handles:
    - MSYS2 installation (if not present)
    - Package installation via pacman
    - Copying dependencies to deps/
    - Building the native module

    Run as Administrator in PowerShell:
        .\scripts\setup.ps1

.PARAMETER SkipInstall
    Skip MSYS2/package installation (assumes deps already present)

.PARAMETER SkipBuild
    Only install dependencies, don't build

.EXAMPLE
    .\scripts\setup.ps1          # Full setup and build
    .\scripts\setup.ps1 -SkipBuild  # Only install deps
#>

param(
    [switch]$SkipInstall,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# ============================================================================
# Configuration
# ============================================================================

$MSYS2_URL = "https://github.com/msys2/msys2-installer/releases/download/2024-01-18/msys2-x86_64-20240118.exe"
$MSYS2_INSTALLER = "$env:TEMP\msys2-installer.exe"
$MSYS2_ROOT = "C:\msys64"
$MINGW64_BIN = "$MSYS2_ROOT\mingw64\bin"

$RequiredPackages = @(
    "mingw-w64-x86_64-libraw",
    "mingw-w64-x86_64-libheif"
)

$ModuleRoot = Split-Path -Parent $PSScriptRoot
$DepDir = Join-Path $ModuleRoot "deps"
$DepInclude = Join-Path $DepDir "include"
$DepLib = Join-Path $DepDir "lib"
$DepBin = Join-Path $DepDir "bin"

# ============================================================================
# Helper Functions
# ============================================================================

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "=== $Message ===" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Test-Command {
    param([string]$Command)
    $null = Get-Command $Command -ErrorAction SilentlyContinue
    return $?
}

# ============================================================================
# MSYS2 Installation
# ============================================================================

function Install-MSYS2 {
    Write-Step "Installing MSYS2"

    if (Test-Path $MSYS2_ROOT) {
        Write-Success "MSYS2 already installed at $MSYS2_ROOT"
        return
    }

    Write-Host "Downloading MSYS2 installer..."
    Invoke-WebRequest -Uri $MSYS2_URL -OutFile $MSYS2_INSTALLER -UseBasicParsing

    Write-Host "Running installer..."
    Write-Host "NOTE: The installer will open a window. Please complete installation."
    Write-Host "The installer will add MSYS2 to your system PATH."
    Start-Process -FilePath $MSYS2_INSTALLER -ArgumentList "install" -Wait

    # Wait for MSYS2 to finish installing
    Start-Sleep -Seconds 5

    if (Test-Path $MSYS2_ROOT) {
        Write-Success "MSYS2 installed successfully"
    } else {
        throw "MSYS2 installation failed or was cancelled"
    }
}

# ============================================================================
# Package Installation
# ============================================================================

function Install-Packages {
    Write-Step "Installing MSYS2 packages"

    # Find pacman
    $pacman = Join-Path $MSYS2_ROOT "usr\bin\pacman.exe"
    if (-not (Test-Path $pacman)) {
        throw "pacman not found at $pacman. Is MSYS2 installed correctly?"
    }

    # Create environment for MSYS2
    $env:MSYSTEM = "MINGW64"
    $env:PATH = "$MINGW64_BIN;$($MSYS2_ROOT)\usr\bin;$env:PATH"
    $env:MSYS2_ARG_CONV_EXCL_N = "*"

    # Check if packages are already installed
    Write-Host "Checking installed packages..."
    $installed = & $pacman -Q 2>$null | ForEach-Object { ($_ -split ' ')[0] }

    $missing = $RequiredPackages | Where-Object { $_ -notin $installed }

    if ($missing.Count -eq 0) {
        Write-Success "All required packages already installed"
        return
    }

    Write-Host "Missing packages: $($missing -join ', ')"
    Write-Host ""

    # Update package database first
    Write-Host "Updating package database (this may take a minute)..."
    $syncResult = & $pacman -Sy 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "Package sync had issues: $syncResult"
    }

    # Install missing packages
    Write-Host "Installing packages: $($RequiredPackages -join ', ')..."
    $installResult = & $pacman -S --noconfirm $RequiredPackages 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install packages: $installResult"
    }

    Write-Success "Packages installed"
}

# ============================================================================
# Copy Dependencies
# ============================================================================

function Copy-Dependencies {
    Write-Step "Copying dependencies to deps/"

    # Create directories
    foreach ($dir in @($DepInclude, $DepLib, $DepBin)) {
        if (Test-Path $dir) {
            Remove-Item -Path $dir -Recurse -Force
        }
        New-Item -Path $dir -ItemType Directory | Out-Null
    }

    $srcMingwInclude = Join-Path $MSYS2_ROOT "mingw64\include"
    $srcMingwLib = Join-Path $MSYS2_ROOT "mingw64\lib"
    $srcMingwBin = Join-Path $MSYS2_ROOT "mingw64\bin"

    # Copy headers
    if (Test-Path $srcMingwInclude) {
        $headerFiles = Get-ChildItem -Path $srcMingwInclude -Include "*.h","*.hpp" -Recurse
        Write-Host "Copying $($headerFiles.Count) header files..."
        foreach ($header in $headerFiles) {
            $destDir = $DepInclude
            if ($header.DirectoryName -ne $srcMingwInclude) {
                $relativePath = $header.DirectoryName.Replace($srcMingwInclude, "")
                $destDir = Join-Path $DepInclude $relativePath.TrimStart("\")
            }
            if (-not (Test-Path $destDir)) {
                New-Item -Path $destDir -ItemType Directory -Force | Out-Null
            }
            Copy-Item -Path $header.FullName -Destination $destDir -Force
        }
    }

    # Copy library files
    if (Test-Path $srcMingwLib) {
        $libFiles = Get-ChildItem -Path $srcMingwLib -Include "*.a","*.dll.a","*.lib" -File
        Write-Host "Copying $($libFiles.Count) library files..."
        foreach ($lib in $libFiles) {
            Copy-Item -Path $lib.FullName -Destination $DepLib -Force
        }
    }

    # Copy ALL DLLs from bin (to capture transitive dependencies)
    if (Test-Path $srcMingwBin) {
        $dllFiles = Get-ChildItem -Path $srcMingwBin -Filter "*.dll" -File
        Write-Host "Copying $($dllFiles.Count) DLL files..."
        foreach ($dll in $dllFiles) {
            Copy-Item -Path $dll.FullName -Destination $DepBin -Force
        }
    }

    Write-Success "Dependencies copied to deps/"
}

# ============================================================================
# Build Native Module
# ============================================================================

function Build-NativeModule {
    Write-Step "Building native module"

    # Set up MSYS2 environment for build
    $env:MSYSTEM = "MINGW64"
    $env:PATH = "$MINGW64_BIN;$($MSYS2_ROOT)\usr\bin;$env:PATH"
    $env:MSYS2_ARG_CONV_EXCL_N = "*"

    Push-Location $ModuleRoot

    try {
        Write-Host "Running: npm run build"
        npm run build

        if ($LASTEXITCODE -ne 0) {
            throw "npm run build failed"
        }

        Write-Success "Native module built"
    }
    finally {
        Pop-Location
    }
}

# ============================================================================
# Run Tests
# ============================================================================

function Run-Tests {
    Write-Step "Running tests"

    Push-Location $ModuleRoot

    try {
        npm test

        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Some tests failed"
        } else {
            Write-Success "All tests passed"
        }
    }
    finally {
        Pop-Location
    }
}

# ============================================================================
# Main
# ============================================================================

function Main {
    Write-Host ""
    Write-Host "nImage Setup Script" -ForegroundColor Magenta
    Write-Host "===================" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "Module: $ModuleRoot"
    Write-Host "MSYS2:  $MSYS2_ROOT"
    Write-Host ""

    # Check prerequisites
    if (-not (Test-Command "npm")) {
        throw "npm is not installed. Please install Node.js 18+ first."
    }

    $nodeVersion = (node --version)
    Write-Host "Node.js: $nodeVersion"

    if (-not $SkipInstall) {
        # MSYS2 installation
        if (-not (Test-Path $MSYS2_ROOT)) {
            Install-MSYS2
        } else {
            Write-Success "MSYS2 found at $MSYS2_ROOT"
        }

        # Package installation
        Install-Packages

        # Copy dependencies
        Copy-Dependencies
    } else {
        Write-Warn "Skipping MSYS2/package installation"
    }

    # Build
    if (-not $SkipBuild) {
        Build-NativeModule

        # Run tests
        Run-Tests
    } else {
        Write-Warn "Skipping build"
    }

    Write-Host ""
    Write-Host "Setup complete!" -ForegroundColor Green
    Write-Host ""

    # Verify deps exist
    $depsCheck = @(
        @{Path=(Join-Path $DepLib "libraw.a"); Name="libraw"},
        @{Path=(Join-Path $DepLib "libheif.dll.a"); Name="libheif"},
        @{Path=(Join-Path $DepBin "libraw-24.dll"); Name="libraw DLL"}
    )

    Write-Host "Dependency verification:"
    foreach ($check in $depsCheck) {
        if (Test-Path $check.Path) {
            Write-Success "$($check.Name) present"
        } else {
            Write-Fail "$($check.Name) missing at $($check.Path)"
        }
    }
}

Main
