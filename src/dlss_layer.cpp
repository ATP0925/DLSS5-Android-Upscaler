#include "config_manager.h"
#include "thermal_guard.h"
#include "anti_cheat_guard.h"
#include "crash_guard.h"
#include "core/vsu_engine.h"
#include <vulkan/vulkan.h>
#include "vulkan/vk_layer.h"
#include <android/log.h>
#include <cstring>
#include <unordered_map>
#include <memory>
#include <mutex>

#define LOG_TAG "VSU_Layer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::mutex g_layerMutex;
std::unordered_map<void*, VkLayerDispatchTable*> g_deviceDispatchTables;
std::unordered_map<VkDevice, std::unique_ptr<vsu::SpatialUpscalerEngine>> g_deviceEngines;

uint64_t g_frameCount = 0;
DLSS5Config g_activeConfig;
bool g_isThermalThrottled = false;

// Hooked vkQueuePresentKHR
VKAPI_ATTR VkResult VKAPI_CALL vsu_QueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    void* dispatchKey = *reinterpret_cast<void**>(queue);
    VkLayerDispatchTable* pTable = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        auto it = g_deviceDispatchTables.find(dispatchKey);
        if (it != g_deviceDispatchTables.end()) {
            pTable = it->second;
        }
    }

    if (!CrashGuard::ValidatePresentationBuffer(pPresentInfo)) {
        if (pTable && pTable->QueuePresentKHR) {
            return pTable->QueuePresentKHR(queue, pPresentInfo);
        }
        return VK_SUCCESS;
    }

    g_frameCount++;

    // Check thermal status every 120 frames (~2 seconds at 60Hz)
    if (g_frameCount % 120 == 0) {
        g_isThermalThrottled = ThermalGuard::ShouldBypassForCooling();
    }

    // Pass through to native presentation
    if (pTable && pTable->QueuePresentKHR) {
        return pTable->QueuePresentKHR(queue, pPresentInfo);
    }

    return VK_SUCCESS;
}

// Hooked vkCreateDevice
VKAPI_ATTR VkResult VKAPI_CALL vsu_CreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    g_activeConfig = ConfigManager::LoadFromAndroidProps();
    AntiCheatGuard::EnforceProcessIsolation();
    CrashGuard::ResetGuard();

    VkLayerDeviceCreateInfo* layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
        reinterpret_cast<const VkLayerDeviceCreateInfo*>(pCreateInfo->pNext));

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
            reinterpret_cast<const VkLayerDeviceCreateInfo*>(layerCreateInfo->pNext));
    }

    if (!layerCreateInfo) {
        LOGE("Unable to locate loader device link info");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    PFN_vkCreateDevice fpCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        fpGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice"));

    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    VkResult result = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS) {
        return result;
    }

    auto* dispatchTable = new VkLayerDispatchTable();
    dispatchTable->GetDeviceProcAddr = fpGetDeviceProcAddr;
    dispatchTable->QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        fpGetDeviceProcAddr(*pDevice, "vkQueuePresentKHR"));

    void* dispatchKey = *reinterpret_cast<void**>(*pDevice);
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        g_deviceDispatchTables[dispatchKey] = dispatchTable;

        // Instantiate isolated VSU core engine for this device
        auto engine = std::make_unique<vsu::SpatialUpscalerEngine>();
        engine->Initialize(*pDevice, physicalDevice, VK_NULL_HANDLE, 0);
        g_deviceEngines[*pDevice] = std::move(engine);
    }

    LOGI("VSU Core Engine linked to VkDevice %p", *pDevice);
    return VK_SUCCESS;
}

// Hooked vkDestroyDevice
VKAPI_ATTR void VKAPI_CALL vsu_DestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator)
{
    void* dispatchKey = *reinterpret_cast<void**>(device);
    VkLayerDispatchTable* pTable = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        auto it = g_deviceDispatchTables.find(dispatchKey);
        if (it != g_deviceDispatchTables.end()) {
            pTable = it->second;
            g_deviceDispatchTables.erase(it);
        }
        g_deviceEngines.erase(device);
    }

    if (pTable) {
        pTable->DestroyDevice(device, pAllocator);
        delete pTable;
    }
}

// Layer function intercept table
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vsu_GetDeviceProcAddr(
    VkDevice device,
    const char* pName)
{
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vsu_GetDeviceProcAddr);
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vsu_DestroyDevice);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vsu_QueuePresentKHR);

    void* dispatchKey = *reinterpret_cast<void**>(device);
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        auto it = g_deviceDispatchTables.find(dispatchKey);
        if (it != g_deviceDispatchTables.end() && it->second->GetDeviceProcAddr) {
            return it->second->GetDeviceProcAddr(device, pName);
        }
    }
    return nullptr;
}

} // namespace

// Vulkan Layer Manifest Negotiation Interface
extern "C" {

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL VK_LAYER_DLSS5_NegotiateLoaderDeviceInterfaceVersion(
    VkNegotiateDeviceLayerInterface* pVersionStruct)
{
    pVersionStruct->pfnGetInstanceProcAddr = nullptr;
    pVersionStruct->pfnGetDeviceProcAddr = vsu_GetDeviceProcAddr;
    return VK_SUCCESS;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName)
{
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vsu_CreateDevice);
    return nullptr;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device,
    const char* pName)
{
    return vsu_GetDeviceProcAddr(device, pName);
}

} // extern "C"
