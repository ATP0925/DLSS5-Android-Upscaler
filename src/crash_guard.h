#pragma once
#include <vulkan/vulkan.h>

class CrashGuard {
public:
    // Validates swapchain images and dimensions before compute pass
    static bool ValidatePresentationBuffer(const VkPresentInfoKHR* pPresentInfo);

    // Records any runtime graphics anomaly and engages silent fail-safe pass-through
    static void TriggerSafePassThrough(const char* reason);

    static bool IsSafeToExecute();
    static void ResetGuard();
};
