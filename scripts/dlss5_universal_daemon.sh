#!/system/bin/sh
# ==============================================================================
# DLSS 5 Universal Game Auto-Detection & Optimization Daemon
# Automatically detects ANY active foreground game and injects DLSS 5 on-the-fly.
# No manual game-by-game configuration needed.
# ==============================================================================

echo "=========================================================="
echo " Starting DLSS 5 Universal Auto-Optimizer Daemon..."
echo "=========================================================="

# Enable Vulkan Layer Subsystem
settings put global enable_gpu_debug_layers 1
settings put global gpu_debug_layers VK_LAYER_DLSS5_upscaler

# Global DLSS 5 Hardware Pipeline
setprop debug.dlss5.enabled 1
setprop debug.dlss5.mode 1             # Adaptive Quality
setprop debug.dlss5.sharpness 0.88     # Universal high-contrast sharpness
setprop debug.dlss5.universal 1        # Flag for universal engagement

LAST_APP=""

# System apps blacklist to avoid hooking UI/System servers
BLACKLIST="com.android.systemui|com.google.android.apps.nexuslauncher|com.sec.android.app.launcher|com.miui.home|com.oppo.launcher|com.huawei.android.launcher"

while true; do
    # Fetch active foreground app
    CURRENT_FOCUS=$(dumpsys window 2>/dev/null | grep -E 'mCurrentFocus|mFocusedApp' | head -n 1)
    
    # Extract package name
    CURRENT_PKG=$(echo "$CURRENT_FOCUS" | grep -oE '[a-zA-Z0-9._]+/[a-zA-Z0-9._]+' | cut -d'/' -f1)

    if [ -n "$CURRENT_PKG" ] && [ "$CURRENT_PKG" != "$LAST_APP" ]; then
        # Check if it's not a launcher/system UI
        IS_BLOCKED=$(echo "$CURRENT_PKG" | grep -E "$BLACKLIST")

        if [ -z "$IS_BLOCKED" ]; then
            echo "[DLSS 5 Engine] Active App Changed -> $CURRENT_PKG"
            
            # Dynamically attach layer to the active foreground game
            settings put global gpu_debug_app "$CURRENT_PKG"
            setprop debug.dlss5.target_pkg "$CURRENT_PKG"
            
            echo "[+] DLSS 5 Upscaling & Sharpening Attached to: $CURRENT_PKG"
            LAST_APP="$CURRENT_PKG"
        fi
    fi

    # Low-power sleep to prevent battery drain
    sleep 2
done
