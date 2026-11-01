#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace vsu {

struct VSUConfig {
    float sharpness = 0.75f;    // 0.0f (soft) to 1.0f (maximum sharpness)
    float scaleFactor = 1.0f;   // 1.0f (native pass-through) to 2.0f (2x spatial scale)
    bool enableEASU = true;     // Pass 1: Edge-Adaptive Spatial Reconstruction
    bool enableCAS = true;      // Pass 2: Contrast-Adaptive Sharpening
    bool forceBypass = false;   // Explicit bypass toggle
};

// Push constants aligned to 16 bytes for Vulkan compute pipeline compatibility
struct alignas(16) VSUPushConstants {
    float srcWidth;
    float srcHeight;
    float dstWidth;
    float dstHeight;
    float sharpness;
    float scaleFactor;
    uint32_t flags; // Bit 0: EASU enabled, Bit 1: sRGB format, Bit 2: Pass index (0 = EASU, 1 = CAS)
    uint32_t reserved;
};

enum class FormatSupportMode {
    DirectStorage,              // Swapchain supports VK_IMAGE_USAGE_STORAGE_BIT directly
    IntermediateBlitRequired,  // Swapchain lacks storage bit; offscreen buffer + blit/copy required
    Unsupported                 // Hardware lacks compute format capabilities; pass-through fallback
};

struct IntermediateImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

} // namespace vsu

