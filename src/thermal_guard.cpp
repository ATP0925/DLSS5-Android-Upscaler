#include "thermal_guard.h"
#include <cstdio>
#include <cstdlib>
#include <android/log.h>

#define LOG_TAG "DLSS5_ThermalGuard"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {
    const char* THERMAL_PATHS[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        "/sys/class/power_supply/battery/temp"
    };
}

float ThermalGuard::GetCurrentTemperatureCelsius() {
    for (const char* path : THERMAL_PATHS) {
        FILE* fp = fopen(path, "r");
        if (fp) {
            int rawTemp = 0;
            if (fscanf(fp, "%d", &rawTemp) == 1) {
                fclose(fp);
                if (rawTemp > 1000) {
                    // Millidegrees Celsius (e.g. 41000 -> 41.0°C)
                    return static_cast<float>(rawTemp) / 1000.0f;
                } else if (rawTemp > 100) {
                    // Tenths of a degree (e.g. 410 -> 41.0°C)
                    return static_cast<float>(rawTemp) / 10.0f;
                } else if (rawTemp > 0) {
                    return static_cast<float>(rawTemp);
                }
            } else {
                fclose(fp);
            }
        }
    }
    // Safe default if sysfs is restricted on non-root
    return 36.0f;
}

ThermalStatus ThermalGuard::GetDeviceThermalState() {
    float temp = GetCurrentTemperatureCelsius();

    if (temp >= 45.0f) {
        LOGW("[DLSS 5 ThermalGuard] CRITICAL HEAT (%.1f°C)! Auto-Bypassing to prevent hardware strain.", temp);
        return ThermalStatus::Critical;
    } else if (temp >= 42.0f) {
        LOGI("[DLSS 5 ThermalGuard] Device Hot (%.1f°C). Throttling down shader passes.", temp);
        return ThermalStatus::Hot;
    } else if (temp >= 38.0f) {
        return ThermalStatus::Warm;
    }
    return ThermalStatus::Cool;
}

bool ThermalGuard::ShouldBypassForCooling() {
    return GetDeviceThermalState() == ThermalStatus::Critical;
}

