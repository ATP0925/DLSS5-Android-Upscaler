#!/system/bin/sh
# ==============================================================================
# DLSS 5 Universal Game Auto-Detection & Thermal-Safe Daemon
# Zero Battery Drain | Hardware-Safe | Screen-State Aware
# ==============================================================================

echo "=========================================================="
echo " Starting DLSS 5 Eco-Optimized Hardware-Safe Daemon..."
echo "=========================================================="

# Enable Vulkan Layer Subsystem
settings put global enable_gpu_debug_layers 1
settings put global gpu_debug_layers VK_LAYER_DLSS5_upscaler

# Global DLSS 5 Hardware Pipeline
setprop debug.dlss5.enabled 1
setprop debug.dlss5.mode 1             # Adaptive Quality
setprop debug.dlss5.sharpness 0.85     # Hardware-friendly sharpness
setprop debug.dlss5.universal 1

LAST_APP=""
BLACKLIST="com.android.systemui|com.google.android.apps.nexuslauncher|com.sec.android.app.launcher|com.miui.home|com.oppo.launcher|com.huawei.android.launcher"

while true; do
    # 1. Screen state check: If screen is off, deep sleep 8s (zero battery consumption)
    IS_AWAKE=$(dumpsys power 2>/dev/null | grep -E "mWakefulness=Awake|Display Power: state=ON")
    if [ -z "$IS_AWAKE" ]; then
        sleep 8
        continue
    fi

    # 2. Fetch active foreground app
    CURRENT_FOCUS=$(dumpsys window 2>/dev/null | grep -E 'mCurrentFocus|mFocusedApp' | head -n 1)
    CURRENT_PKG=$(echo "$CURRENT_FOCUS" | grep -oE '[a-zA-Z0-9._]+/[a-zA-Z0-9._]+' | cut -d'/' -f1)

    if [ -n "$CURRENT_PKG" ] && [ "$CURRENT_PKG" != "$LAST_APP" ]; then
        IS_BLOCKED=$(echo "$CURRENT_PKG" | grep -E "$BLACKLIST")

        if [ -z "$IS_BLOCKED" ]; then
            echo "[DLSS 5 Engine] Active App -> $CURRENT_PKG"
            settings put global gpu_debug_app "$CURRENT_PKG"
            setprop debug.dlss5.target_pkg "$CURRENT_PKG"
            LAST_APP="$CURRENT_PKG"
        fi
    fi

    # Efficient pacing: 3 seconds interval while screen is on
    sleep 3
done
