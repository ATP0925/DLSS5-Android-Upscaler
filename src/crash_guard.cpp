#include "crash_guard.h"
#include <android/log.h>

#define LOG_TAG "DLSS5_CrashGuard"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {
    bool g_safeToExecute = true;
    uint32_t g_consecutiveErrors = 0;
}

bool CrashGuard::ValidatePresentationBuffer(const VkPresentInfoKHR* pPresentInfo) {
    if (!pPresentInfo || pPresentInfo->swapchainCount == 0 || !pPresentInfo->pSwapchains || !pPresentInfo->pImageIndices) {
        TriggerSafePassThrough("Invalid Swapchain Presentation Pointer");
        return false;
    }
    return g_safeToExecute;
}

void CrashGuard::TriggerSafePassThrough(const char* reason) {
    g_consecutiveErrors++;
    LOGW("[DLSS 5 CrashGuard] Anomaly detected: %s | Auto-Bypassing to keep game 100%% stable.", reason);
    if (g_consecutiveErrors > 5) {
        g_safeToExecute = false; // Temporarily lock in safe pass-through to avoid game stutter
    }
}

bool CrashGuard::IsSafeToExecute() {
    return g_safeToExecute;
}

void CrashGuard::ResetGuard() {
    g_safeToExecute = true;
    g_consecutiveErrors = 0;
}

