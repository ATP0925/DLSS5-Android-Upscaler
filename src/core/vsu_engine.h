#pragma once

#include "vsu_types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace vsu {

/**
 * Self-contained Vulkan Spatial Upscaler Engine.
 * Implements a 2-pass compute pipeline (FSR 1.0 EASU + CAS).
 * Handles intermediate storage emulation, format feature checks, per-frame barriers,
 * resolution scaling, dynamic resizing, and fail-safe bypass.
 */
class SpatialUpscalerEngine {
public:
    SpatialUpscalerEngine();
    ~SpatialUpscalerEngine();

    SpatialUpscalerEngine(const SpatialUpscalerEngine&) = delete;
    SpatialUpscalerEngine& operator=(const SpatialUpscalerEngine&) = delete;

    // Phase 1: Initialize Vulkan device context and pipelines
    bool Initialize(VkDevice device,
                    VkPhysicalDevice physicalDevice,
                    VkQueue computeQueue,
                    uint32_t queueFamilyIndex);

    // Phase 2: Allocate or resize resolution-dependent buffers
    bool PrepareResources(VkExtent2D inputExtent,
                          VkExtent2D outputExtent,
                          VkFormat swapchainFormat,
                          VkImageUsageFlags swapchainUsageFlags);

    // Phase 3: Record compute passes into existing command buffer
    bool RecordUpscaleCommands(VkCommandBuffer cmd,
                               VkImage srcImage,
                               VkImageLayout srcLayout,
                               VkImage dstImage,
                               VkImageLayout dstLayout,
                               const VSUConfig& config);

    // Diagnostics & Safety Query
    FormatSupportMode CheckFormatSupport(VkFormat format, VkImageUsageFlags usage);
    void InvalidateDynamicResources();
    void Destroy();

    bool IsInitialized() const { return m_initialized; }
    bool AreResourcesReady() const { return m_resourcesReady; }
    FormatSupportMode GetCurrentFormatMode() const { return m_formatMode; }

private:
    bool CreateSamplers();
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool CreateComputePipelines();
    bool AllocateIntermediateImage(IntermediateImage& img, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);
    void FreeIntermediateImage(IntermediateImage& img);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void InsertImageBarrier(VkCommandBuffer cmd,
                            VkImage image,
                            VkAccessFlags srcAccess,
                            VkAccessFlags dstAccess,
                            VkImageLayout oldLayout,
                            VkImageLayout newLayout,
                            VkPipelineStageFlags srcStage,
                            VkPipelineStageFlags dstStage);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;

    VkSampler m_linearClampSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkShaderModule m_easuShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_casShaderModule = VK_NULL_HANDLE;
    VkPipeline m_easuPipeline = VK_NULL_HANDLE;
    VkPipeline m_casPipeline = VK_NULL_HANDLE;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_easuDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet m_casDescriptorSet = VK_NULL_HANDLE;

    IntermediateImage m_pingPongImage;      // Inter-pass buffer (EASU output -> CAS input)
    IntermediateImage m_storageOutputImage;  // Used if swapchain lacks VK_IMAGE_USAGE_STORAGE_BIT

    VkExtent2D m_currentInputExtent = {0, 0};
    VkExtent2D m_currentOutputExtent = {0, 0};
    VkFormat m_currentSwapchainFormat = VK_FORMAT_UNDEFINED;
    FormatSupportMode m_formatMode = FormatSupportMode::Unsupported;

    bool m_initialized = false;
    bool m_resourcesReady = false;
    std::mutex m_engineMutex;
};

} // namespace vsu
