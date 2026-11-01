#!/system/bin/sh
# DLSS 5 Android Game Hook Activation Script
# Usage: ./enable_dlss5.sh <package_name>

TARGET_APP="$1"

# Friendly game aliases
case "$TARGET_APP" in
    freefire|ff)
        TARGET_APP="com.dts.freefireth"
        ;;
    freefiremax|ffmax)
        TARGET_APP="com.dts.freefiremax"
        ;;
    bgmi|pubg)
        TARGET_APP="com.pubg.imobile"
        ;;
    genshin)
        TARGET_APP="com.miHoYo.GenshinImpact"
        ;;
    codm|cod)
        TARGET_APP="com.activision.callofduty.shooter"
        ;;
esac

if [ -z "$TARGET_APP" ]; then
    echo "=========================================================="
    echo " DLSS 5 Android Game Hook Activation"
    echo "=========================================================="
    echo "Error: Target game package name required!"
    echo "Example: ./enable_dlss5.sh com.pubg.imobile"
    echo "Example: ./enable_dlss5.sh com.miHoYo.GenshinImpact"
    echo "=========================================================="
    exit 1
fi

echo "[*] Setting up DLSS 5 Vulkan Layer for $TARGET_APP..."

# 1. Enable Global GPU Debug Layers on Android
settings put global enable_gpu_debug_layers 1

# 2. Assign Target Game Package
settings put global gpu_debug_app "$TARGET_APP"

# 3. Inject DLSS 5 Layer
settings put global gpu_debug_layers VK_LAYER_DLSS5_upscaler

# 4. Set Environment Configuration
setprop debug.dlss5.upscale 1
setprop debug.dlss5.sharpness 0.85
setprop debug.dlss5.factor 1.5

echo "[+] DLSS 5 Successfully Activated for: $TARGET_APP"
echo "[*] Launch the game on your device now to see upscaled performance."

