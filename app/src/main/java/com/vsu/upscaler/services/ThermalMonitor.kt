package com.vsu.upscaler.services

import android.content.Context
import android.os.Build
import android.os.PowerManager
import com.vsu.upscaler.data.HardwareThermalLevel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.concurrent.Executor

class ThermalMonitor(context: Context) {
    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as? PowerManager

    private val _thermalState = MutableStateFlow(HardwareThermalLevel.COOL)
    val thermalState: StateFlow<HardwareThermalLevel> = _thermalState.asStateFlow()

    private var listener: PowerManager.OnThermalStatusChangedListener? = null

    init {
        registerThermalCallback(context)
    }

    private fun registerThermalCallback(context: Context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && powerManager != null) {
            val currentStatus = powerManager.currentThermalStatus
            _thermalState.value = HardwareThermalLevel.fromPowerManagerStatus(currentStatus)

            listener = PowerManager.OnThermalStatusChangedListener { status ->
                _thermalState.value = HardwareThermalLevel.fromPowerManagerStatus(status)
            }

            powerManager.addThermalStatusListener(context.mainExecutor, listener!!)
        }
    }

    fun unregister() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && powerManager != null && listener != null) {
            powerManager.removeThermalStatusListener(listener!!)
            listener = null
        }
    }
}
