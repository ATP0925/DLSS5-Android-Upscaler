package com.vsu.upscaler.data

data class GameProfile(
    val packageName: String,
    val appName: String,
    val scaleFactor: Float = 1.33f, // 1.0f (native) to 2.0f (2x upscaling)
    val sharpness: Float = 0.80f,   // 0.0f (smooth) to 1.0f (sharp)
    val isEnabled: Boolean = true,
    val isVulkanNative: Boolean = true
)

enum class HardwareThermalLevel {
    COOL,
    MODERATE,
    WARM,
    CRITICAL;

    companion object {
        fun fromPowerManagerStatus(status: Int): HardwareThermalLevel {
            return when (status) {
                0 -> COOL       // THERMAL_STATUS_NONE
                1 -> MODERATE   // THERMAL_STATUS_LIGHT
                2 -> WARM       // THERMAL_STATUS_MODERATE
                3 -> WARM       // THERMAL_STATUS_SEVERE
                else -> CRITICAL // THERMAL_STATUS_CRITICAL / EMERGENCY / SHUTDOWN
            }
        }
    }
}

data class DiagnosticTelemetry(
    val gpuRenderer: String = "Adreno (TM) 750 / Mali-G610",
    val vulkanVersion: String = "1.3.278",
    val thermalLevel: HardwareThermalLevel = HardwareThermalLevel.COOL,
    val isShizukuAvailable: Boolean = false,
    val activeGamePackage: String? = null,
    val frameComputeLatencyMs: Float = 0.68f
)
