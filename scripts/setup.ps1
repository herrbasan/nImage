<#
.SYNOPSIS
    nImage dependency setup and build script

.DESCRIPTION
    This script sets up nImage's native dependencies and builds the native module.

    It handles:
    - MSYS2 installation (if not present)
    - Package installation via pacman
    - Direct binary download fallback (if pacman fails)
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

function Test-MSYS2PackagesInstalled {
    param([string]$PacmanPath)

    $env:MSYSTEM = "MINGW64"
    $env:PATH = "$MINGW64_BIN;$($MSYS2_ROOT)\usr\bin;$env:PATH"

    $installed = & $PacmanPath -Q 2>$null | ForEach-Object { ($_ -split ' ')[0] }
    $missing = $RequiredPackages | Where-Object { $_ -notin $installed }

    return $missing.Count -eq 0
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
# Package Installation via pacman
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

    # Try to update package database
    Write-Host "Updating package database..."
    $syncResult = & $pacman -Sy 2>&1

    if ($LASTEXITCODE -ne 0) {
        Write-Warn "Pacman sync failed - attempting to fix CA certificates..."

        # Try to fix corrupted CA bundle
        $caBundleDest = "$MSYS2_ROOT\usr\ssl\certs\ca-bundle.crt"

        if (Test-Path $caBundleDest) {
            # Check if CA bundle is corrupted (should be > 10KB, not "404 Not Found")
            $caContent = Get-Content $caBundleDest -Raw -ErrorAction SilentlyContinue
            if ($caContent -and $caContent.Length -lt 1000) {
                Write-Host "CA bundle appears corrupted, attempting to fix..."
                try {
                    Invoke-WebRequest -Uri "https://curl.se/ca/cacert.pem" -OutFile $caBundleDest -UseBasicParsing
                    Write-Success "CA bundle fixed"
                } catch {
                    Write-Warn "Could not download CA bundle: $_"
                }
            }
        }

        # Retry pacman sync
        Write-Host "Retrying pacman sync..."
        $syncResult = & $pacman -Sy 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Pacman sync still failing after CA fix"
            Write-Host "Falling back to direct binary download..."
            Download-DirectBinaries
            return
        }
    }

    # Install missing packages
    Write-Host "Installing packages: $($RequiredPackages -join ', ')..."
    $installResult = & $pacman -S --noconfirm $RequiredPackages 2>&1

    if ($LASTEXITCODE -ne 0) {
        Write-Warn "Pacman install failed"
        Write-Host "Falling back to direct binary download..."
        Download-DirectBinaries
        return
    }

    Write-Success "Packages installed"
}

# ============================================================================
# Direct Binary Download (fallback when pacman fails)
# ============================================================================

function Download-DirectBinaries {
    Write-Step "Downloading binaries directly"

    # Create deps directories
    foreach ($dir in @($DepInclude, $DepLib, $DepBin)) {
        if (Test-Path $dir) {
            Remove-Item -Path $dir -Recurse -Force
        }
        New-Item -Path $dir -ItemType Directory -Force | Out-Null
    }

    # Download libraw from GitHub releases
    Write-Host "Downloading libraw..."
    $librawUrl = "https://github.com/LibRaw/LibRaw/releases/download/0.21.2/LibRaw-0.21.2-msys2-x64.zip"
    $librawZip = "$env:TEMP\libraw.zip"

    try {
        Invoke-WebRequest -Uri $librawUrl -OutFile $librawZip -UseBasicParsing
        Expand-Archive -Path $librawZip -DestinationPath $DepDir -Force
        Write-Success "libraw downloaded and extracted"
    } catch {
        Write-Warn "Failed to download libraw: $_"
        Write-Host "Trying alternative approach..."
        Create-ManualFallback
        return
    }

    # Download libheif (this is trickier - it depends on libde265, aom, etc.)
    Write-Host "Downloading libheif..."
    $libheifUrl = "https://github.com/strukturag/libheif/releases/download/v1.18.2/libheif-1.18.2-msys2-x64.zip"
    $libheifZip = "$env:TEMP\libheif.zip"

    try {
        Invoke-WebRequest -Uri $libheifUrl -OutFile $libheifZip -UseBasicParsing
        Expand-Archive -Path $libheifZip -DestinationPath $DepDir -Force
        Write-Success "libheif downloaded and extracted"
    } catch {
        Write-Warn "Failed to download libheif: $_"
    }

    # If direct downloads failed, try manual approach
    if (-not (Test-Path (Join-Path $DepLib "libraw.a"))) {
        Create-ManualFallback
    }
}

function Create-ManualFallback {
    Write-Step "Manual fallback - installing via MSYS2 shell"

    # The MSYS2 CA bundle is corrupted. Try to fix it first by downloading a fresh one
    $caBundleUrl = "https://curl.se/ca/cacert.pem"
    $caBundleDest = "$MSYS2_ROOT\usr\ssl\certs\ca-bundle.crt"
    $caBundleBackup = "$MSYS2_ROOT\usr\ssl\certs\ca-bundle.crt.bak"

    Write-Host "Attempting to fix MSYS2 CA certificates..."

    try {
        # Backup corrupted file
        if (Test-Path $caBundleDest) {
            Copy-Item -Path $caBundleDest -Destination $caBundleBackup -Force
        }

        # Download proper CA bundle
        Invoke-WebRequest -Uri $caBundleUrl -OutFile $caBundleDest -UseBasicParsing
        Write-Success "CA bundle fixed"

        # Now retry pacman
        $pacman = Join-Path $MSYS2_ROOT "usr\bin\pacman.exe"
        $env:MSYSTEM = "MINGW64"
        $env:PATH = "$MINGW64_BIN;$($MSYS2_ROOT)\usr\bin;$env:PATH"

        Write-Host "Retrying pacman sync..."
        & $pacman -Sy

        if ($LASTEXITCODE -eq 0) {
            Write-Host "Installing packages..."
            & $pacman -S --noconfirm $RequiredPackages
            if ($LASTEXITCODE -eq 0) {
                Write-Success "Packages installed via pacman"
                Copy-DependenciesFromMSYS2
                return
            }
        }
    } catch {
        Write-Warn "Could not fix CA bundle: $_"
    }

    # Last resort: use prebuilt binaries from nImage releases or document manual steps
    Write-Warn "Automatic installation failed."
    Write-Host ""
    Write-Host "Manual installation required:"
    Write-Host ""
    Write-Host "Option 1: Fix MSYS2 CA certificates"
    Write-Host "  1. Open MSYS2 MINGW64 terminal as Administrator"
    Write-Host "  2. Run: pacman -Sy ca-certificates"
    Write-Host "  3. Run: pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif"
    Write-Host "  4. Re-run this script"
    Write-Host ""
    Write-Host "Option 2: Download and extract manually"
    Write-Host "  1. Download LibRaw from https://www.libraw.org"
    Write-Host "  2. Download libheif from https://github.com/strukturag/libheif"
    Write-Host "  3. Extract headers to deps/include/"
    Write-Host "  4. Extract libs to deps/lib/"
    Write-Host ""
    throw "Automatic installation failed. Please follow manual steps above."
}

function Copy-DependenciesFromMSYS2 {
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

    # Copy library files (.a, .dll.a, .lib)
    if (Test-Path $srcMingwLib) {
        $libFiles = Get-ChildItem -Path $srcMingwLib -Filter "*.a" -File
        $dllALibs = Get-ChildItem -Path $srcMingwLib -Filter "*.dll.a" -File
        $libLibs = Get-ChildItem -Path $srcMingwLib -Filter "*.lib" -File
        $allLibs = @($libFiles) + @($dllALibs) + @($libLibs)
        Write-Host "Copying $($allLibs.Count) library files..."
        foreach ($lib in $allLibs) {
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
# Copy Dependencies (from MSYS2)
# ============================================================================

function Copy-Dependencies {
    Copy-DependenciesFromMSYS2
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

        # Check if deps already exist in deps/
        $depsLibExists = Test-Path (Join-Path $DepLib "libraw.a")
        $depsBinExists = Test-Path (Join-Path $DepBin "libraw-24.dll")

        if ($depsLibExists -and $depsBinExists) {
            Write-Success "Dependencies already present in deps/"
        } else {
            # Package installation
            Install-Packages

            # Copy dependencies
            Copy-Dependencies
        }
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
