#pragma once
#include <string>

enum class DLSS5QualityMode {
    UltraQuality,   // 1.3x Scale
    Quality,        // 1.5x Scale
    Balanced,       // 1.7x Scale
    Performance,    // 2.0x Scale
    UltraPerformance// 3.0x Scale
};

struct DLSS5Config {
    bool enabled = true;
    DLSS5QualityMode mode = DLSS5QualityMode::Quality;
    float sharpness = 0.85f;    // 0.0 to 1.0 (RCAS filter)
    bool frameGenEmulation = false; // Post-process optical flow frame smoothing
    bool logFps = true;
    int targetFps = 60;         // 60, 90, 120 FPS
    
    // Auto resolution scaling calculation
    float getScaleFactor() const {
        switch (mode) {
            case DLSS5QualityMode::UltraQuality:    return 1.3f;
            case DLSS5QualityMode::Quality:         return 1.5f;
            case DLSS5QualityMode::Balanced:        return 1.7f;
            case DLSS5QualityMode::Performance:     return 2.0f;
            case DLSS5QualityMode::UltraPerformance:return 3.0f;
            default:                                return 1.5f;
        }
    }
};

class ConfigManager {
public:
    static DLSS5Config LoadFromAndroidProps();
    static void SaveConfig(const DLSS5Config& config);
};
