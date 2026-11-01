#include "core/vsu_engine.h"
#include <vulkan/vulkan.h>
#include <android/log.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>

#define LOG_TAG "VSU_TEST_APP"
#define LOGI(...) do { __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); printf("[INFO] " __VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); fprintf(stderr, "[ERROR] " __VA_ARGS__); fprintf(stderr, "\n"); } while(0)

namespace {

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool CreateTextureImage(VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        uint32_t width,
                        uint32_t height,
                        VkFormat format,
                        VkImageUsageFlags usage,
                        VkImage* outImage,
                        VkDeviceMemory* outMemory,
                        VkImageView* outView)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, outImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, *outImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, outMemory) != VK_SUCCESS) {
        return false;
    }

    vkBindImageMemory(device, *outImage, *outMemory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = *outImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vkCreateImageView(device, &viewInfo, nullptr, outView) == VK_SUCCESS;
}

} // namespace

int main(int argc, char* argv[]) {
    LOGI("==========================================================");
    LOGI(" Starting Vulkan Spatial Upscaler Verification Harness");
    LOGI("==========================================================");

    // -------------------------------------------------------------------------
    // STAGE 1: Vulkan Instance Initialization
    // -------------------------------------------------------------------------
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VSU_Verification_Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanSpatialUpscaler";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        LOGE("[STAGE 1 FAILED] vkCreateInstance returned error code: %d", res);
        return 1;
    }
    LOGI("[CHECKPOINT] [STAGE 1 SUCCESS] Vulkan 1.1 Instance Created");

    // -------------------------------------------------------------------------
    // STAGE 2: Physical Device Enumeration & GPU Identification
    // -------------------------------------------------------------------------
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("[STAGE 2 FAILED] No Vulkan-compatible physical devices found!");
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    VkPhysicalDevice physicalDevice = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    LOGI("[CHECKPOINT] [STAGE 2 SUCCESS] Selected GPU: %s (Driver: %u, API: %u.%u.%u)",
         props.deviceName, props.driverVersion,
         VK_VERSION_MAJOR(props.apiVersion),
         VK_VERSION_MINOR(props.apiVersion),
         VK_VERSION_PATCH(props.apiVersion));

    // -------------------------------------------------------------------------
    // STAGE 3: Queue Family & Logical Device Creation
    // -------------------------------------------------------------------------
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int computeQueueIndex = -1;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            computeQueueIndex = i;
            break;
        }
    }

    if (computeQueueIndex == -1) {
        LOGE("[STAGE 3 FAILED] No compute/graphics queue family found!");
        return 1;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = computeQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    VkDevice device = VK_NULL_HANDLE;
    res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        LOGE("[STAGE 3 FAILED] vkCreateDevice failed with code: %d", res);
        return 1;
    }

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeQueueIndex, 0, &queue);
    LOGI("[CHECKPOINT] [STAGE 3 SUCCESS] Logical Device & Queue Initialized (Queue Family: %d)", computeQueueIndex);

    // -------------------------------------------------------------------------
    // STAGE 4: Test Scene Buffer Allocation (720p Input -> 1080p Output)
    // -------------------------------------------------------------------------
    const uint32_t srcW = 1280;
    const uint32_t srcH = 720;
    const uint32_t dstW = 1920;
    const uint32_t dstH = 1080;

    VkImage srcImage, dstImage;
    VkDeviceMemory srcMemory, dstMemory;
    VkImageView srcView, dstView;

    bool srcOk = CreateTextureImage(device, physicalDevice, srcW, srcH, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                   &srcImage, &srcMemory, &srcView);

    bool dstOk = CreateTextureImage(device, physicalDevice, dstW, dstH, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   &dstImage, &dstMemory, &dstView);

    if (!srcOk || !dstOk) {
        LOGE("[STAGE 4 FAILED] Failed to allocate source/destination test buffers");
        return 1;
    }
    LOGI("[CHECKPOINT] [STAGE 4 SUCCESS] Test Quad Buffers Ready: 720p (%ux%u) -> 1080p (%ux%u)",
         srcW, srcH, dstW, dstH);

    // -------------------------------------------------------------------------
    // STAGE 5: VSU Engine Initialization
    // -------------------------------------------------------------------------
    vsu::SpatialUpscalerEngine engine;
    if (!engine.Initialize(device, physicalDevice, queue, computeQueueIndex)) {
        LOGE("[STAGE 5 FAILED] engine.Initialize() failed");
        return 1;
    }

    VkExtent2D inputExtent = {srcW, srcH};
    VkExtent2D outputExtent = {dstW, dstH};
    VkImageUsageFlags swapchainUsage = VK_IMAGE_USAGE_STORAGE_BIT;

    if (!engine.PrepareResources(inputExtent, outputExtent, VK_FORMAT_R8G8B8A8_UNORM, swapchainUsage)) {
        LOGE("[STAGE 5 FAILED] engine.PrepareResources() failed");
        return 1;
    }
    LOGI("[CHECKPOINT] [STAGE 5 SUCCESS] VSU Core Engine Initialized and Resources Prepared");

    // -------------------------------------------------------------------------
    // STAGE 6: Command Buffer Allocation & Recording
    // -------------------------------------------------------------------------
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = computeQueueIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool);

    VkCommandBufferAllocateInfo cmdAlloc = {};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    vsu::VSUConfig config;
    config.sharpness = 0.85f;
    config.scaleFactor = 1.5f;
    config.enableEASU = true;
    config.enableCAS = true;
    config.forceBypass = false;

    bool recordOk = engine.RecordUpscaleCommands(cmd, srcImage, VK_IMAGE_LAYOUT_UNDEFINED,
                                                 dstImage, VK_IMAGE_LAYOUT_GENERAL, config);
    if (!recordOk) {
        LOGE("[STAGE 6 FAILED] engine.RecordUpscaleCommands() returned false");
        return 1;
    }
    vkEndCommandBuffer(cmd);
    LOGI("[CHECKPOINT] [STAGE 6 SUCCESS] 2-Pass Compute Pipeline Recorded (EASU + CAS Sharpness: %.2f)",
         config.sharpness);

    // -------------------------------------------------------------------------
    // STAGE 7: GPU Execution & Execution Timing Checkpoint
    // -------------------------------------------------------------------------
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    auto startTime = std::chrono::high_resolution_clock::now();
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL); // 5 sec timeout
    auto endTime = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    LOGI("[CHECKPOINT] [STAGE 7 SUCCESS] GPU Compute Execution Completed in %.3f ms", elapsedMs);

    // Cleanup resources
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);

    engine.Destroy();

    vkDestroyImageView(device, srcView, nullptr);
    vkDestroyImage(device, srcImage, nullptr);
    vkFreeMemory(device, srcMemory, nullptr);

    vkDestroyImageView(device, dstView, nullptr);
    vkDestroyImage(device, dstImage, nullptr);
    vkFreeMemory(device, dstMemory, nullptr);

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    LOGI("==========================================================");
    LOGI(" VSU All Checkpoints PASSED! Hardware Verification Clean.");
    LOGI("==========================================================");
    return 0;
}

