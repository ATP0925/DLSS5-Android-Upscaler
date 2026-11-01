#pragma once
#include <cstdint>

enum class ThermalStatus {
    Cool,       // < 38°C -> Full DLSS 5 Quality Mode
    Warm,       // 38°C - 42°C -> Balanced Mode (Power optimized)
    Hot,        // 42°C - 45°C -> Ultra-Lightweight mode (Prevents Throttling)
    Critical    // > 45°C -> Safety Bypass (DLSS turns off temporarily to cool device)
};

class ThermalGuard {
public:
    static ThermalStatus GetDeviceThermalState();
    static float GetCurrentTemperatureCelsius();
    static bool ShouldBypassForCooling();
};

