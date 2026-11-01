#include "thermal_guard.h"
#include <android/log.h>
#include <dlfcn.h>

#define LOG_TAG "VSU_ThermalGuard"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {
    // Dynamic function signatures for Android 10+ (API 29+) AThermal NDK APIs
    typedef void* (*pfn_AThermal_acquireManager)();
    typedef void (*pfn_AThermal_releaseManager)(void*);
    typedef int (*pfn_AThermal_getCurrentThermalStatus)(void*);

    void* g_thermalManager = nullptr;
    pfn_AThermal_acquireManager fn_acquire = nullptr;
    pfn_AThermal_releaseManager fn_release = nullptr;
    pfn_AThermal_getCurrentThermalStatus fn_getStatus = nullptr;
    bool g_ndkThermalInitialized = false;

    void InitNDKThermal() {
        if (g_ndkThermalInitialized) return;
        g_ndkThermalInitialized = true;

        void* libandroid = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (libandroid) {
            fn_acquire = (pfn_AThermal_acquireManager)dlsym(libandroid, "AThermal_acquireManager");
            fn_release = (pfn_AThermal_releaseManager)dlsym(libandroid, "AThermal_releaseManager");
            fn_getStatus = (pfn_AThermal_getCurrentThermalStatus)dlsym(libandroid, "AThermal_getCurrentThermalStatus");

            if (fn_acquire && fn_getStatus) {
                g_thermalManager = fn_acquire();
                LOGI("Initialized NDK AThermalManager successfully (SELinux-compliant)");
            }
        }
    }
}

float ThermalGuard::GetCurrentTemperatureCelsius() {
    InitNDKThermal();

    if (g_thermalManager && fn_getStatus) {
        int status = fn_getStatus(g_thermalManager);
        // Map AThermalStatus enum to representative SoC temperature bands
        switch (status) {
            case 0: return 34.0f; // NONE
            case 1: return 37.0f; // LIGHT
            case 2: return 40.0f; // MODERATE
            case 3: return 43.0f; // SEVERE
            case 4: return 46.0f; // CRITICAL
            case 5: return 49.0f; // EMERGENCY
            case 6: return 52.0f; // SHUTDOWN
            default: return 36.0f;
        }
    }

    return 36.0f; // Default baseline
}

ThermalStatus ThermalGuard::GetDeviceThermalState() {
    InitNDKThermal();

    if (g_thermalManager && fn_getStatus) {
        int status = fn_getStatus(g_thermalManager);
        if (status >= 4) { // CRITICAL, EMERGENCY, SHUTDOWN
            LOGW("AThermal reports CRITICAL heat status (%d)! Initiating compute bypass.", status);
            return ThermalStatus::Critical;
        } else if (status >= 3) { // SEVERE
            return ThermalStatus::Hot;
        } else if (status >= 1) { // LIGHT, MODERATE
            return ThermalStatus::Warm;
        }
        return ThermalStatus::Cool;
    }

    return ThermalStatus::Cool;
}

bool ThermalGuard::ShouldBypassForCooling() {
    return GetDeviceThermalState() == ThermalStatus::Critical;
}
