# DLSS 5 Universal Vulkan Upscaler for Native Android Games

A high-performance **Vulkan Graphics Layer & AI Spatial Upscaler** designed to inject Super Resolution and Contrast-Adaptive Sharpening (CAS) into **native Android games** (e.g., BGMI, Genshin Impact, COD Mobile, CarX Street) without modifying game APKs or source code.

---

## 📁 Project Architecture

* **`src/dlss_layer.cpp`**: Core C++ Vulkan Layer that intercepts game swapchain frames via `vkQueuePresentKHR`.
* **`shaders/upscaler.comp`**: GLSL Compute Shader executing edge-preserving spatial upscaling and contrast-adaptive sharpening.
* **`manifest/VkLayer_dlss5.json`**: Android Vulkan Layer registration manifest.
* **`scripts/enable_dlss5.sh`**: One-click activation script for any Android game package.
* **`scripts/disable_dlss5.sh`**: Deactivation and restoration script.

---

## 🛠️ How to Compile (Using Android NDK)

1. Ensure **Android NDK (r25c or higher)** and **CMake** are installed.
2. Run the build command targeting your device's architecture (ARM64-v8a):

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-29 \
      ..
cmake --build .
```

This will generate `libVkLayer_dlss5.so`.

---

## 📲 How to Deploy on Android Phone

### Step 1: Push Layer & Manifest to Device
```bash
adb push build/libVkLayer_dlss5.so /data/local/tmp/
adb push manifest/VkLayer_dlss5.json /data/local/tmp/
adb push scripts/enable_dlss5.sh /data/local/tmp/
```

### Step 2: Grant Execution Permissions
```bash
adb shell chmod +x /data/local/tmp/enable_dlss5.sh
```

### Step 3: Activate for Any Game
Run the activation script with your target game's package name:
```bash
# Example for BGMI:
adb shell /data/local/tmp/enable_dlss5.sh com.pubg.imobile

# Example for Genshin Impact:
adb shell /data/local/tmp/enable_dlss5.sh com.miHoYo.GenshinImpact
```

### Step 4: Verify in Logcat
Launch the game on your device and check logs:
```bash
adb logcat -s DLSS5_Vulkan_Layer
```
You will see:
`[DLSS 5 Core] Frame intercepted successfully. Upscaling & Sharpening Active.`

