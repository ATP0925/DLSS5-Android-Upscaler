#!/usr/bin/env python3
"""
Vulkan Spatial Upscaler (VSU) - Real Hardware Measurement & Benchmark Suite
Records real frame-times, jank rates, and SoC thermals via ADB without synthetic estimation.
"""

import subprocess
import sys
import time
import re
from datetime import datetime

def run_adb(cmd):
    full_cmd = ["adb"] + cmd
    try:
        res = subprocess.run(full_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        return res.stdout.strip()
    except subprocess.CalledProcessError as e:
        return ""

def get_device_info():
    model = run_adb(["shell", "getprop", "ro.product.model"]) or "Unknown Device"
    android_ver = run_adb(["shell", "getprop", "ro.build.version.release"]) or "Unknown Android"
    soc = run_adb(["shell", "getprop", "ro.board.platform"]) or "Unknown SoC"
    return {
        "model": model,
        "android": android_ver,
        "soc": soc,
        "date": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }

def get_battery_temp_celsius():
    dumpsys_battery = run_adb(["shell", "dumpsys", "battery"])
    match = re.search(r"temperature:\s*(\d+)", dumpsys_battery)
    if match:
        raw_temp = int(match.group(1))
        return raw_temp / 10.0  # Android reports battery temp in tenths of a degree Celsius
    return None

def benchmark_game(package_name, duration_seconds=15):
    print(f"[*] Starting benchmark for target: {package_name} ({duration_seconds}s sample)...")

    info = get_device_info()
    temp_start = get_battery_temp_celsius()

    # Reset frame statistics
    run_adb(["shell", "dumpsys", "gfxinfo", package_name, "reset"])
    print("[*] Frame buffers reset. Benchmark window active. Please play/navigate scene...")

    time.sleep(duration_seconds)

    temp_end = get_battery_temp_celsius()
    raw_stats = run_adb(["shell", "dumpsys", "gfxinfo", package_name, "framestats"])

    # Parse framestats
    frame_times_ms = []
    in_stats = False

    for line in raw_stats.splitlines():
        line = line.strip()
        if "---PROFILEDATA---" in line:
            in_stats = True
            continue
        if in_stats and line.startswith("Flags"):
            continue
        if in_stats and line and not line.startswith("---"):
            parts = line.split(",")
            if len(parts) >= 13:
                try:
                    # IntendedVsync to FrameCompleted in nanoseconds
                    intended_vsync = int(parts[1])
                    frame_completed = int(parts[13])
                    total_ns = frame_completed - intended_vsync
                    if 0 < total_ns < 200_000_000: # Discard outliers (>200ms)
                        frame_times_ms.append(total_ns / 1_000_000.0)
                except ValueError:
                    continue

    if not frame_times_ms:
        print("[!] No framestats captured. Ensure the app is running in the foreground and rendered via HWUI/Vulkan.")
        return

    frame_times_ms.sort()
    count = len(frame_times_ms)
    avg_ms = sum(frame_times_ms) / count
    p95_ms = frame_times_ms[int(count * 0.95)]
    p99_ms = frame_times_ms[int(count * 0.99)]
    jank_frames = sum(1 for t in frame_times_ms if t > 16.67) # >60 FPS budget
    jank_pct = (jank_frames / count) * 100.0

    print("\n" + "=" * 60)
    print(" VULKAN SPATIAL UPSCALER - HARDWARE BENCHMARK REPORT")
    print("=" * 60)
    print(f" Device Model      : {info['model']} ({info['soc']})")
    print(f" OS Version        : Android {info['android']}")
    print(f" Target Package    : {package_name}")
    print(f" Timestamp         : {info['date']}")
    print(f" Sample Window     : {duration_seconds} seconds ({count} frames analyzed)")
    print("-" * 60)
    print(f" Average Frametime : {avg_ms:.2f} ms (~{1000.0 / avg_ms:.1f} FPS)")
    print(f" 95th Percentile   : {p95_ms:.2f} ms")
    print(f" 99th Percentile   : {p99_ms:.2f} ms")
    print(f" Jank Rate (>16.6ms): {jank_pct:.1f}% ({jank_frames}/{count} frames)")
    if temp_start and temp_end:
        print(f" Device Thermals   : {temp_start:.1f}°C -> {temp_end:.1f}°C (Delta: {temp_end - temp_start:+.1f}°C)")
    print("=" * 60)
    print("[*] Verification methodology: FrameMetrics / dumpsys gfxinfo raw vsync delta.")

if __name__ == "__main__":
    pkg = sys.argv[1] if len(sys.argv) > 1 else "com.vsu.upscaler"
    sec = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    benchmark_game(pkg, sec)
