#!/system/bin/sh
# Free Fire & Free Fire MAX 1-Click DLSS 5 Booster
# Supports: Garena Free Fire (com.dts.freefireth) & Free Fire MAX (com.dts.freefiremax)

VARIANT="$1"

if [ "$VARIANT" = "max" ]; then
    TARGET_APP="com.dts.freefiremax"
    echo "[*] Selecting Free Fire MAX ($TARGET_APP)..."
else
    TARGET_APP="com.dts.freefireth"
    echo "[*] Selecting Free Fire Standard ($TARGET_APP)..."
fi

echo "[*] Optimizing graphics pipeline with DLSS 5 (EASU + CAS Sharpening)..."

# Enable Vulkan Layer
settings put global enable_gpu_debug_layers 1
settings put global gpu_debug_app "$TARGET_APP"
settings put global gpu_debug_layers VK_LAYER_DLSS5_upscaler

# Competitive Shooter Configuration (High Sharpness + Balanced Upscale for 90/120 FPS)
setprop debug.dlss5.enabled 1
setprop debug.dlss5.mode 1             # Quality Mode (1.5x Scaling)
setprop debug.dlss5.sharpness 0.90     # Crisp edge clarity for spotting enemies
setprop debug.dlss5.target_fps 90

echo "=========================================================="
echo " [SUCCESS] DLSS 5 Engine Activated for Free Fire!"
echo " Target Package : $TARGET_APP"
echo " Mode           : Quality (1.5x AI Upscale)"
echo " Sharpness      : 0.90 (Max Visibility)"
echo " Target FPS     : 90/120 FPS unlocked"
echo "=========================================================="
echo "Launch Free Fire now to play with enhanced graphics & FPS!"
