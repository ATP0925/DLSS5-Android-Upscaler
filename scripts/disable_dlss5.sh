#!/system/bin/sh
# DLSS 5 Android Deactivation Script

echo "[*] Disabling DLSS 5 GPU layers..."

settings put global enable_gpu_debug_layers 0
settings delete global gpu_debug_app
settings delete global gpu_debug_layers

echo "[+] DLSS 5 Disabled. Android graphics returned to default state."

