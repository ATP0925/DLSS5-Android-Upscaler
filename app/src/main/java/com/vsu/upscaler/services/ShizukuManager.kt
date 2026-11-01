package com.vsu.upscaler.services

import android.content.Context
import android.content.pm.PackageManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import rikka.shizuku.Shizuku

class ShizukuManager(private val context: Context) {
    private val _isAvailable = MutableStateFlow(false)
    val isAvailable: StateFlow<Boolean> = _isAvailable.asStateFlow()

    private val _hasPermission = MutableStateFlow(false)
    val hasPermission: StateFlow<Boolean> = _hasPermission.asStateFlow()

    private val binderReceivedListener = Shizuku.OnBinderReceivedListener {
        checkShizukuState()
    }

    private val binderDeadListener = Shizuku.OnBinderDeadListener {
        _isAvailable.value = false
        _hasPermission.value = false
    }

    private val requestPermissionResultListener = Shizuku.OnRequestPermissionResultListener { requestCode, grantResult ->
        if (requestCode == REQUEST_CODE_SHIZUKU) {
            _hasPermission.value = (grantResult == PackageManager.PERMISSION_GRANTED)
        }
    }

    fun initialize() {
        try {
            Shizuku.addBinderReceivedListenerSticky(binderReceivedListener)
            Shizuku.addBinderDeadListener(binderDeadListener)
            Shizuku.addRequestPermissionResultListener(requestPermissionResultListener)
            checkShizukuState()
        } catch (e: Throwable) {
            _isAvailable.value = false
        }
    }

    fun requestShizukuPermission() {
        if (Shizuku.isPre_V11() || !Shizuku.pingBinder()) return
        if (!Shizuku.checkSelfPermission().equals(PackageManager.PERMISSION_GRANTED)) {
            Shizuku.requestPermission(REQUEST_CODE_SHIZUKU)
        }
    }

    private fun checkShizukuState() {
        val ping = try {
            Shizuku.pingBinder()
        } catch (e: Throwable) {
            false
        }
        _isAvailable.value = ping

        if (ping) {
            _hasPermission.value = (Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED)
        }
    }

    fun cleanup() {
        try {
            Shizuku.removeBinderReceivedListener(binderReceivedListener)
            Shizuku.removeBinderDeadListener(binderDeadListener)
            Shizuku.removeRequestPermissionResultListener(requestPermissionResultListener)
        } catch (e: Throwable) {
            // Ignore teardown errors
        }
    }

    companion object {
        const val REQUEST_CODE_SHIZUKU = 1001
    }
}

