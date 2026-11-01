package com.vsu.upscaler.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.vsu.upscaler.data.DiagnosticTelemetry
import com.vsu.upscaler.data.GameProfile
import com.vsu.upscaler.data.HardwareThermalLevel
import com.vsu.upscaler.data.ProfileRepository
import com.vsu.upscaler.services.ForegroundDetector
import com.vsu.upscaler.services.ShizukuManager
import com.vsu.upscaler.services.ThermalMonitor
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class MainUiState(
    val profiles: List<GameProfile> = emptyList(),
    val activeGame: GameProfile? = null,
    val thermalLevel: HardwareThermalLevel = HardwareThermalLevel.COOL,
    val isShizukuAvailable: Boolean = false,
    val hasUsageStatsPermission: Boolean = false,
    val isOverlayActive: Boolean = false,
    val telemetry: DiagnosticTelemetry = DiagnosticTelemetry()
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = ProfileRepository(application)
    private val thermalMonitor = ThermalMonitor(application)
    private val foregroundDetector = ForegroundDetector(application)
    private val shizukuManager = ShizukuManager(application)

    private val _uiState = MutableStateFlow(MainUiState())
    val uiState: StateFlow<MainUiState> = _uiState.asStateFlow()

    init {
        shizukuManager.initialize()
        startObservationLoops()
    }

    private fun startObservationLoops() {
        // Observe profiles
        viewModelScope.launch {
            repository.profilesFlow.collect { list ->
                _uiState.value = _uiState.value.copy(profiles = list)
            }
        }

        // Observe hardware thermal state
        viewModelScope.launch {
            thermalMonitor.thermalState.collect { thermal ->
                _uiState.value = _uiState.value.copy(
                    thermalLevel = thermal,
                    telemetry = _uiState.value.telemetry.copy(thermalLevel = thermal)
                )
            }
        }

        // Observe Shizuku
        viewModelScope.launch {
            shizukuManager.isAvailable.collect { available ->
                _uiState.value = _uiState.value.copy(
                    isShizukuAvailable = available,
                    telemetry = _uiState.value.telemetry.copy(isShizukuAvailable = available)
                )
            }
        }

        // Foreground game polling loop (every 2.5s)
        viewModelScope.launch {
            while (true) {
                val hasPerm = foregroundDetector.hasUsageStatsPermission()
                _uiState.value = _uiState.value.copy(hasUsageStatsPermission = hasPerm)

                if (hasPerm) {
                    val pkg = foregroundDetector.pollForegroundApp()
                    val activeProfile = pkg?.let { repository.getProfile(it) }
                    _uiState.value = _uiState.value.copy(
                        activeGame = activeProfile,
                        telemetry = _uiState.value.telemetry.copy(activeGamePackage = pkg)
                    )
                }
                delay(2500)
            }
        }
    }

    fun updateSharpness(packageName: String, sharpness: Float) {
        val profile = repository.getProfile(packageName) ?: return
        repository.saveProfile(profile.copy(sharpness = sharpness))
    }

    fun updateScale(packageName: String, scaleFactor: Float) {
        val profile = repository.getProfile(packageName) ?: return
        repository.saveProfile(profile.copy(scaleFactor = scaleFactor))
    }

    fun toggleGame(packageName: String, enabled: Boolean) {
        val profile = repository.getProfile(packageName) ?: return
        repository.saveProfile(profile.copy(isEnabled = enabled))
    }

    fun toggleOverlay(active: Boolean) {
        _uiState.value = _uiState.value.copy(isOverlayActive = active)
    }

    fun requestShizukuPermission() {
        shizukuManager.requestShizukuPermission()
    }

    override fun onCleared() {
        super.onCleared()
        thermalMonitor.unregister()
        shizukuManager.cleanup()
    }
}

