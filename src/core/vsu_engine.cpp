#include "vsu_engine.h"
#include "../../shaders/spv_fsr_easu.h"
#include "../../shaders/spv_cas.h"
#include <android/log.h>
#include <cstring>
#include <algorithm>

#define LOG_TAG "VSU_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vsu {

SpatialUpscalerEngine::SpatialUpscalerEngine() = default;

SpatialUpscalerEngine::~SpatialUpscalerEngine() {
    Destroy();
}

bool SpatialUpscalerEngine::Initialize(VkDevice device,
                                      VkPhysicalDevice physicalDevice,
                                      VkQueue computeQueue,
                                      uint32_t queueFamilyIndex)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);

    if (m_initialized) {
        return true;
    }

    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
        LOGE("Invalid Vulkan device handles passed to Initialize()");
        return false;
    }

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_queue = computeQueue;
    m_queueFamilyIndex = queueFamilyIndex;

    if (!CreateSamplers()) {
        LOGE("Failed to create linear texture sampler");
        Destroy();
        return false;
    }

    if (!CreateDescriptorSetLayout()) {
        LOGE("Failed to create descriptor set layout");
        Destroy();
        return false;
    }

    if (!CreatePipelineLayout()) {
        LOGE("Failed to create compute pipeline layout");
        Destroy();
        return false;
    }

    if (!CreateComputePipelines()) {
        LOGE("Failed to compile EASU and CAS compute pipelines");
        Destroy();
        return false;
    }

    // Allocate Descriptor Pool (2 sets for EASU and CAS)
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        Destroy();
        return false;
    }

    // Allocate Descriptor Sets
    VkDescriptorSetLayout layouts[2] = {m_descriptorSetLayout, m_descriptorSetLayout};
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;

    VkDescriptorSet sets[2];
    if (vkAllocateDescriptorSets(m_device, &allocInfo, sets) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets");
        Destroy();
        return false;
    }

    m_easuDescriptorSet = sets[0];
    m_casDescriptorSet = sets[1];

    m_initialized = true;
    LOGI("VSU Core Engine initialized successfully on physical device");
    return true;
}

bool SpatialUpscalerEngine::CreateSamplers() {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    return vkCreateSampler(m_device, &samplerInfo, nullptr, &m_linearClampSampler) == VK_SUCCESS;
}

bool SpatialUpscalerEngine::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    
    // Binding 0: Input Texture Sampler
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Output Storage Image
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    return vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

bool SpatialUpscalerEngine::CreatePipelineLayout() {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(VSUPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    return vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) == VK_SUCCESS;
}

bool SpatialUpscalerEngine::CreateComputePipelines() {
    // 1. Create EASU Shader Module
    VkShaderModuleCreateInfo easuInfo = {};
    easuInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    easuInfo.codeSize = shaders::EASU_COMP_SPV_SIZE;
    easuInfo.pCode = shaders::EASU_COMP_SPV;

    if (vkCreateShaderModule(m_device, &easuInfo, nullptr, &m_easuShaderModule) != VK_SUCCESS) {
        LOGE("Failed to create EASU shader module from embedded SPIR-V");
        return false;
    }

    // 2. Create CAS Shader Module
    VkShaderModuleCreateInfo casInfo = {};
    casInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    casInfo.codeSize = shaders::CAS_COMP_SPV_SIZE;
    casInfo.pCode = shaders::CAS_COMP_SPV;

    if (vkCreateShaderModule(m_device, &casInfo, nullptr, &m_casShaderModule) != VK_SUCCESS) {
        LOGE("Failed to create CAS shader module from embedded SPIR-V");
        return false;
    }

    // 3. Create Compute Pipelines
    VkComputePipelineCreateInfo computeInfos[2] = {};

    computeInfos[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computeInfos[0].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeInfos[0].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeInfos[0].stage.module = m_easuShaderModule;
    computeInfos[0].stage.pName = "main";
    computeInfos[0].layout = m_pipelineLayout;

    computeInfos[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computeInfos[1].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeInfos[1].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeInfos[1].stage.module = m_casShaderModule;
    computeInfos[1].stage.pName = "main";
    computeInfos[1].layout = m_pipelineLayout;

    VkPipeline pipelines[2];
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 2, computeInfos, nullptr, pipelines) != VK_SUCCESS) {
        LOGE("Failed to build compute pipelines");
        return false;
    }

    m_easuPipeline = pipelines[0];
    m_casPipeline = pipelines[1];

    return true;
}

FormatSupportMode SpatialUpscalerEngine::CheckFormatSupport(VkFormat format, VkImageUsageFlags usage) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

    bool hasStorage = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
    bool hasUsageStorage = (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;

    if (hasStorage && hasUsageStorage) {
        return FormatSupportMode::DirectStorage;
    }

    bool hasBlitDst = (props.optimalTilingFeatures & (VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) != 0;
    if (hasBlitDst) {
        return FormatSupportMode::IntermediateBlitRequired;
    }

    return FormatSupportMode::Unsupported;
}

uint32_t SpatialUpscalerEngine::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool SpatialUpscalerEngine::AllocateIntermediateImage(IntermediateImage& img,
                                                     VkExtent2D extent,
                                                     VkFormat format,
                                                     VkImageUsageFlags usage)
{
    FreeIntermediateImage(img);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &img.image) != VK_SUCCESS) {
        LOGE("Failed to create intermediate image (%ux%u)", extent.width, extent.height);
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, img.image, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &img.memory) != VK_SUCCESS) {
        LOGE("Failed to allocate device memory for intermediate image");
        vkDestroyImage(m_device, img.image, nullptr);
        img.image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(m_device, img.image, img.memory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = img.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &img.view) != VK_SUCCESS) {
        LOGE("Failed to create intermediate image view");
        FreeIntermediateImage(img);
        return false;
    }

    img.extent = extent;
    img.format = format;
    img.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return true;
}

void SpatialUpscalerEngine::FreeIntermediateImage(IntermediateImage& img) {
    if (img.view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, img.view, nullptr);
        img.view = VK_NULL_HANDLE;
    }
    if (img.image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, img.image, nullptr);
        img.image = VK_NULL_HANDLE;
    }
    if (img.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, img.memory, nullptr);
        img.memory = VK_NULL_HANDLE;
    }
    img.extent = {0, 0};
    img.format = VK_FORMAT_UNDEFINED;
}

bool SpatialUpscalerEngine::PrepareResources(VkExtent2D inputExtent,
                                            VkExtent2D outputExtent,
                                            VkFormat swapchainFormat,
                                            VkImageUsageFlags swapchainUsageFlags)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);

    if (!m_initialized) {
        LOGE("Cannot PrepareResources: Engine not initialized");
        return false;
    }

    if (m_resourcesReady &&
        m_currentInputExtent.width == inputExtent.width &&
        m_currentInputExtent.height == inputExtent.height &&
        m_currentOutputExtent.width == outputExtent.width &&
        m_currentOutputExtent.height == outputExtent.height &&
        m_currentSwapchainFormat == swapchainFormat)
    {
        return true; // Already configured
    }

    m_currentInputExtent = inputExtent;
    m_currentOutputExtent = outputExtent;
    m_currentSwapchainFormat = swapchainFormat;
    m_formatMode = CheckFormatSupport(swapchainFormat, swapchainUsageFlags);

    if (m_formatMode == FormatSupportMode::Unsupported) {
        LOGW("Swapchain format lacks compute/blit capabilities. Bypassing.");
        m_resourcesReady = false;
        return false;
    }

    // Allocate Ping-Pong Image between EASU and CAS (UNORM format for storage image compatibility)
    VkImageUsageFlags pingPongUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!AllocateIntermediateImage(m_pingPongImage, outputExtent, VK_FORMAT_R8G8B8A8_UNORM, pingPongUsage)) {
        LOGE("Failed to allocate Ping-Pong inter-pass buffer");
        m_resourcesReady = false;
        return false;
    }

    // If swapchain cannot directly act as storage image, allocate offscreen presentation target
    if (m_formatMode == FormatSupportMode::IntermediateBlitRequired) {
        VkImageUsageFlags storageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (!AllocateIntermediateImage(m_storageOutputImage, outputExtent, VK_FORMAT_R8G8B8A8_UNORM, storageUsage)) {
            LOGE("Failed to allocate intermediate storage output buffer");
            m_resourcesReady = false;
            return false;
        }
    }

    m_resourcesReady = true;
    LOGI("VSU Engine Prepared Resources: %ux%u -> %ux%u (Mode: %s)",
         inputExtent.width, inputExtent.height, outputExtent.width, outputExtent.height,
         m_formatMode == FormatSupportMode::DirectStorage ? "DirectStorage" : "IntermediateBlit");
    return true;
}

void SpatialUpscalerEngine::InsertImageBarrier(VkCommandBuffer cmd,
                                              VkImage image,
                                              VkAccessFlags srcAccess,
                                              VkAccessFlags dstAccess,
                                              VkImageLayout oldLayout,
                                              VkImageLayout newLayout,
                                              VkPipelineStageFlags srcStage,
                                              VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool SpatialUpscalerEngine::RecordUpscaleCommands(VkCommandBuffer cmd,
                                                 VkImage srcImage,
                                                 VkImageLayout srcLayout,
                                                 VkImage dstImage,
                                                 VkImageLayout dstLayout,
                                                 const VSUConfig& config)
{
    if (config.forceBypass || !m_resourcesReady || m_formatMode == FormatSupportMode::Unsupported) {
        return false; // Graceful pass-through bypass
    }

    // 1. Transition intermediate ping-pong buffer to GENERAL layout for compute store
    InsertImageBarrier(cmd, m_pingPongImage.image,
                       0, VK_ACCESS_SHADER_WRITE_BIT,
                       m_pingPongImage.currentLayout, VK_IMAGE_LAYOUT_GENERAL,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    m_pingPongImage.currentLayout = VK_IMAGE_LAYOUT_GENERAL;

    // 2. Pass 1: EASU Dispatch
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_easuPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_easuDescriptorSet, 0, nullptr);

    VSUPushConstants pc = {};
    pc.srcWidth = static_cast<float>(m_currentInputExtent.width);
    pc.srcHeight = static_cast<float>(m_currentInputExtent.height);
    pc.dstWidth = static_cast<float>(m_currentOutputExtent.width);
    pc.dstHeight = static_cast<float>(m_currentOutputExtent.height);
    pc.sharpness = std::clamp(config.sharpness, 0.0f, 1.0f);
    pc.scaleFactor = config.scaleFactor;
    pc.flags = 0; // EASU pass

    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VSUPushConstants), &pc);

    uint32_t groupsX = (m_currentOutputExtent.width + 15) / 16;
    uint32_t groupsY = (m_currentOutputExtent.height + 15) / 16;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // 3. Inter-Pass Barrier: Transition ping-pong buffer from GENERAL (write) to SHADER_READ_ONLY (sample)
    InsertImageBarrier(cmd, m_pingPongImage.image,
                       VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    m_pingPongImage.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 4. Pass 2: CAS Dispatch
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_casPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_casDescriptorSet, 0, nullptr);

    pc.flags = 1; // CAS pass
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VSUPushConstants), &pc);
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // 5. Final Coherency Barrier before presentation or blit
    VkImage targetFinalImage = (m_formatMode == FormatSupportMode::DirectStorage) ? dstImage : m_storageOutputImage.image;
    InsertImageBarrier(cmd, targetFinalImage,
                       VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                       VK_IMAGE_LAYOUT_GENERAL, dstLayout,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    return true;
}

void SpatialUpscalerEngine::InvalidateDynamicResources() {
    std::lock_guard<std::mutex> lock(m_engineMutex);
    FreeIntermediateImage(m_pingPongImage);
    FreeIntermediateImage(m_storageOutputImage);
    m_resourcesReady = false;
}

void SpatialUpscalerEngine::Destroy() {
    std::lock_guard<std::mutex> lock(m_engineMutex);

    if (!m_initialized) {
        return;
    }

    FreeIntermediateImage(m_pingPongImage);
    FreeIntermediateImage(m_storageOutputImage);

    if (m_easuPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_easuPipeline, nullptr);
        m_easuPipeline = VK_NULL_HANDLE;
    }
    if (m_casPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_casPipeline, nullptr);
        m_casPipeline = VK_NULL_HANDLE;
    }
    if (m_easuShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_easuShaderModule, nullptr);
        m_easuShaderModule = VK_NULL_HANDLE;
    }
    if (m_casShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_casShaderModule, nullptr);
        m_casShaderModule = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_linearClampSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_linearClampSampler, nullptr);
        m_linearClampSampler = VK_NULL_HANDLE;
    }

    m_initialized = false;
    m_resourcesReady = false;
    LOGI("VSU Core Engine destroyed cleanly");
}

} // namespace vsu
