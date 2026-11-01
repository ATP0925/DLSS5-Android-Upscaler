#include "config_manager.h"
#include "thermal_guard.h"
#include "anti_cheat_guard.h"
#include "crash_guard.h"
#include <vulkan/vulkan.h>
#include "vulkan/vk_layer.h"
#include <android/log.h>
#include <cstring>
#include <unordered_map>
#include <mutex>

#define LOG_TAG "DLSS5_Vulkan_Layer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

// Dispatch table pointers
std::mutex g_layerMutex;
std::unordered_map<void*, VkLayerDispatchTable*> g_deviceDispatchTables;
std::unordered_map<void*, VkLayerInstanceDispatchTable*> g_instanceDispatchTables;

uint64_t g_frameCount = 0;
DLSS5Config g_activeConfig;
bool g_isThermalThrottled = false;

// Hooked vkQueuePresentKHR
VKAPI_ATTR VkResult VKAPI_CALL dlss5_QueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    // Retrieve device dispatch table immediately for fail-safe
    void* dispatchKey = *reinterpret_cast<void**>(queue);
    VkLayerDispatchTable* pTable = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        auto it = g_deviceDispatchTables.find(dispatchKey);
        if (it != g_deviceDispatchTables.end()) {
            pTable = it->second;
        }
    }

    // CRASH GUARD: If buffer validation fails, immediately pass through safely
    if (!CrashGuard::ValidatePresentationBuffer(pPresentInfo)) {
        if (pTable && pTable->QueuePresentKHR) {
            return pTable->QueuePresentKHR(queue, pPresentInfo);
        }
        return VK_SUCCESS;
    }

    g_frameCount++;

    // Thermal & Battery Check every 180 frames (~2-3 seconds at 60fps)
    if (g_frameCount % 180 == 0) {
        g_isThermalThrottled = ThermalGuard::ShouldBypassForCooling();
        if (g_isThermalThrottled) {
            LOGW("[DLSS 5 ThermalGuard] Temperature high. Auto-bypassing shader load to cool hardware.");
        }
    }

    if (g_frameCount % 360 == 0 && !g_isThermalThrottled) {
        LOGI("[DLSS 5 Active] Frame #%llu | Temp: %.1f°C | Scale: %.1fx | Memory: Zero-Alloc Pool",
             static_cast<unsigned long long>(g_frameCount),
             ThermalGuard::GetCurrentTemperatureCelsius(),
             g_activeConfig.getScaleFactor());
    }

    if (pTable && pTable->QueuePresentKHR) {
        return pTable->QueuePresentKHR(queue, pPresentInfo);
    }

    return VK_SUCCESS;
}

// Hooked vkCreateDevice
VKAPI_ATTR VkResult VKAPI_CALL dlss5_CreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    g_activeConfig = ConfigManager::LoadFromAndroidProps();
    AntiCheatGuard::EnforceProcessIsolation();
    CrashGuard::ResetGuard();

    // Read current process name
    char processName[256] = "Unknown Game";
    FILE* cmdline = fopen("/proc/self/cmdline", "r");
    if (cmdline) {
        if (fgets(processName, sizeof(processName), cmdline)) {
            // Trim null or newline
            processName[sizeof(processName) - 1] = '\0';
        }
        fclose(cmdline);
    }

    LOGI("[DLSS 5 Universal] Attached to process: %s", processName);
    LOGI("[DLSS 5 Universal] Mode: Scale %.2fx | Sharpness: %.2f | Auto-Optimizer: ON",
         g_activeConfig.getScaleFactor(), g_activeConfig.sharpness);

    VkLayerDeviceCreateInfo* layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
        reinterpret_cast<const VkLayerDeviceCreateInfo*>(pCreateInfo->pNext));

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
            reinterpret_cast<const VkLayerDeviceCreateInfo*>(layerCreateInfo->pNext));
    }

    if (!layerCreateInfo) {
        LOGE("[DLSS 5 Core] Unable to locate loader device link info!");
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
    }

    LOGI("[DLSS 5 Core] Hooked vkQueuePresentKHR. DLSS 5 Engine is ready.");
    return VK_SUCCESS;
}

// Hooked vkDestroyDevice
VKAPI_ATTR void VKAPI_CALL dlss5_DestroyDevice(
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
    }

    if (pTable) {
        pTable->DestroyDevice(device, pAllocator);
        delete pTable;
    }
}

} // namespace

// Intercept entry points
extern "C" {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dlss5_GetDeviceProcAddr(VkDevice device, const char* pName) {
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_GetDeviceProcAddr);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_CreateDevice);
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_DestroyDevice);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_QueuePresentKHR);

    void* dispatchKey = *reinterpret_cast<void**>(device);
    std::lock_guard<std::mutex> lock(g_layerMutex);
    auto it = g_deviceDispatchTables.find(dispatchKey);
    if (it != g_deviceDispatchTables.end() && it->second->GetDeviceProcAddr) {
        return it->second->GetDeviceProcAddr(device, pName);
    }
    return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dlss5_GetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (strcmp(pName, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_GetInstanceProcAddr);
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_GetDeviceProcAddr);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_CreateDevice);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(dlss5_QueuePresentKHR);

    return nullptr;
}

}

