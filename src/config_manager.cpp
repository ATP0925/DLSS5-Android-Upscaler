#include "config_manager.h"
#include <sys/system_properties.h>
#include <cstdlib>
#include <fstream>
#include <android/log.h>

#define LOG_TAG "DLSS5_Config"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

DLSS5Config ConfigManager::LoadFromAndroidProps() {
    DLSS5Config config;

    char propBuffer[PROP_VALUE_MAX] = {0};

    // 1. Check enabled state: getprop debug.dlss5.enabled
    if (__system_property_get("debug.dlss5.enabled", propBuffer) > 0) {
        config.enabled = (atoi(propBuffer) != 0);
    }

    // 2. Check quality preset: getprop debug.dlss5.mode (0: UQ, 1: Q, 2: Bal, 3: Perf, 4: U-Perf)
    if (__system_property_get("debug.dlss5.mode", propBuffer) > 0) {
        int m = atoi(propBuffer);
        if (m >= 0 && m <= 4) {
            config.mode = static_cast<DLSS5QualityMode>(m);
        }
    }

    // 3. Check sharpness: getprop debug.dlss5.sharpness (e.g. 0.85)
    if (__system_property_get("debug.dlss5.sharpness", propBuffer) > 0) {
        float s = static_cast<float>(atof(propBuffer));
        if (s >= 0.0f && s <= 1.0f) {
            config.sharpness = s;
        }
    }

    // 4. Frame Generation Emulation: getprop debug.dlss5.framegen
    if (__system_property_get("debug.dlss5.framegen", propBuffer) > 0) {
        config.frameGenEmulation = (atoi(propBuffer) != 0);
    }

    LOGI("[DLSS 5 Config] Active: %d | Mode Scale: %.2fx | Sharpness: %.2f | FrameGen: %d",
         config.enabled, config.getScaleFactor(), config.sharpness, config.frameGenEmulation);

    return config;
}

void ConfigManager::SaveConfig(const DLSS5Config& config) {
    std::ofstream file("/data/local/tmp/dlss5_active.conf");
    if (file.is_open()) {
        file << "enabled=" << config.enabled << "\n";
        file << "mode=" << static_cast<int>(config.mode) << "\n";
        file << "scale=" << config.getScaleFactor() << "\n";
        file << "sharpness=" << config.sharpness << "\n";
        file << "framegen=" << config.frameGenEmulation << "\n";
        file.close();
    }
}
