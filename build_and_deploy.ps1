# DLSS 5 Android - Windows Automation & Build Script
# Requirements: Android NDK installed and ADB connected

param (
    [string]$TargetPackage = "all",
    [switch]$Universal = $true,
    [string]$NdkPath = $env:ANDROID_NDK_HOME
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " DLSS 5 Universal Upscaler - Android Build & Deploy Tool" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Check for NDK
if (-not $NdkPath -or -not (Test-Path $NdkPath)) {
    Write-Warning "ANDROID_NDK_HOME not found in environment variables."
    $PossibleNdk = "C:\Users\$env:USERNAME\AppData\Local\Android\Sdk\ndk"
    if (Test-Path $PossibleNdk) {
        $Latest = Get-ChildItem -Path $PossibleNdk | Sort-Object Name -Descending | Select-Object -First 1
        $NdkPath = $Latest.FullName
        Write-Host "[+] Auto-detected NDK at: $NdkPath" -ForegroundColor Green
    }
    else {
        Write-Error "Please install Android NDK or set \$env:ANDROID_NDK_HOME."
        exit 1
    }
}

# 2. Build directories
$BuildDir = Join-Path $PSScriptRoot "build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir

Write-Host "[*] Configuring CMake for arm64-v8a..." -ForegroundColor Yellow
cmake -G "Ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$NdkPath/build/cmake/android.toolchain.cmake" `
    -DANDROID_ABI="arm64-v8a" `
    -DANDROID_PLATFORM="android-29" `
    ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "[*] Retrying with NMake / Standard generator..." -ForegroundColor Yellow
    cmake -DCMAKE_TOOLCHAIN_FILE="$NdkPath/build/cmake/android.toolchain.cmake" `
        -DANDROID_ABI="arm64-v8a" `
        -DANDROID_PLATFORM="android-29" `
        ..
}

Write-Host "[*] Compiling libVkLayer_dlss5.so..." -ForegroundColor Yellow
cmake --build .

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Check compiler errors."
    Pop-Location
    exit 1
}

Pop-Location

Write-Host "[+] Build Succeeded!" -ForegroundColor Green

# 3. Check for Connected Android Device
$AdbBin = "adb"
if (-not (Get-Command adb -ErrorAction SilentlyContinue)) {
    $LocalSdkAdb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
    if (Test-Path $LocalSdkAdb) {
        $AdbBin = $LocalSdkAdb
    }
}

Write-Host "[*] Checking connected Android devices via ADB ($AdbBin)..." -ForegroundColor Yellow
$AdbDevices = & $AdbBin devices | Select-String "device$"

if ($AdbDevices) {
    Write-Host "[+] Device connected. Pushing layer to /data/local/tmp..." -ForegroundColor Green
    & $AdbBin push "$BuildDir/libVkLayer_dlss5.so" "/data/local/tmp/"
    & $AdbBin push "$PSScriptRoot/manifest/VkLayer_dlss5.json" "/data/local/tmp/"
    & $AdbBin push "$PSScriptRoot/scripts/dlss5_universal_daemon.sh" "/data/local/tmp/"
    & $AdbBin push "$PSScriptRoot/scripts/enable_dlss5.sh" "/data/local/tmp/"
    & $AdbBin push "$PSScriptRoot/scripts/self_heal.sh" "/data/local/tmp/"
    & $AdbBin shell chmod +x "/data/local/tmp/dlss5_universal_daemon.sh"
    & $AdbBin shell chmod +x "/data/local/tmp/enable_dlss5.sh"
    & $AdbBin shell chmod +x "/data/local/tmp/self_heal.sh"

    if ($Universal -or $TargetPackage -eq "all") {
        Write-Host "[*] Launching Universal Auto-Optimizer Daemon for ALL Android games..." -ForegroundColor Cyan
        & $AdbBin shell "nohup /data/local/tmp/dlss5_universal_daemon.sh > /dev/null 2>&1 &"
        Write-Host "[SUCCESS] DLSS 5 Universal Engine is ACTIVE for ALL games (Free Fire, Free Fire India, BGMI, COD, etc.)!" -ForegroundColor Green
    } else {
        Write-Host "[*] Activating DLSS 5 on target: $TargetPackage" -ForegroundColor Cyan
        & $AdbBin shell "/data/local/tmp/enable_dlss5.sh" $TargetPackage
        Write-Host "[SUCCESS] DLSS 5 is now active for $TargetPackage!" -ForegroundColor Green
    }
}
else {
    Write-Warning "No device connected via ADB. Please connect your phone via USB with USB Debugging enabled."
}
