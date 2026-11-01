package com.vsu.upscaler.services

import android.app.AppOpsManager
import android.app.usage.UsageEvents
import android.app.usage.UsageStatsManager
import android.content.Context
import android.os.Process
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class ForegroundDetector(private val context: Context) {
    private val usageStatsManager = context.getSystemService(Context.USAGE_STATS_SERVICE) as? UsageStatsManager

    private val _foregroundApp = MutableStateFlow<String?>(null)
    val foregroundApp: StateFlow<String?> = _foregroundApp.asStateFlow()

    fun hasUsageStatsPermission(): Boolean {
        val appOps = context.getSystemService(Context.APP_OPS_SERVICE) as? AppOpsManager ?: return false
        val mode = appOps.unsafeCheckOpNoThrow(
            AppOpsManager.OPSTR_GET_USAGE_STATS,
            Process.myUid(),
            context.packageName
        )
        return mode == AppOpsManager.MODE_ALLOWED
    }

    /**
     * Polls the foreground activity using official UsageEvents API.
     * Called periodically by the foreground service or background coroutine.
     */
    fun pollForegroundApp(): String? {
        if (!hasUsageStatsPermission() || usageStatsManager == null) {
            return null
        }

        val endTime = System.currentTimeMillis()
        val startTime = endTime - 1000 * 5 // Query last 5 seconds

        val usageEvents = usageStatsManager.queryEvents(startTime, endTime)
        val event = UsageEvents.Event()
        var latestForegroundPackage: String? = null

        while (usageEvents.hasNextEvent()) {
            usageEvents.getNextEvent(event)
            if (event.eventType == UsageEvents.Event.ACTIVITY_RESUMED) {
                latestForegroundPackage = event.packageName
            }
        }

        if (latestForegroundPackage != null && latestForegroundPackage != context.packageName) {
            _foregroundApp.value = latestForegroundPackage
        }

        return latestForegroundPackage
    }
}

