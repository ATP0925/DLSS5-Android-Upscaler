package com.vsu.upscaler.services

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Build
import android.os.IBinder
import android.view.Gravity
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.app.NotificationCompat

class OverlayService : Service() {
    private var windowManager: WindowManager? = null
    private var overlayLayout: LinearLayout? = null
    private var hudTextView: TextView? = null

    override fun onCreate() {
        super.onCreate()
        startForeground(NOTIFICATION_ID, createNotification())
        createTouchThroughOverlay()
    }

    private fun createNotification(): Notification {
        val channelId = "vsu_overlay_channel"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "VSU Spatial Engine Active",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Monitors game graphics pipeline and manages spatial upscaling"
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }

        return NotificationCompat.Builder(this, channelId)
            .setContentTitle("Vulkan Spatial Upscaler")
            .setContentText("Hardware-Safe Spatial Engine Active")
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setOngoing(true)
            .build()
    }

    private fun createTouchThroughOverlay() {
        windowManager = getSystemService(Context.WINDOW_SERVICE) as WindowManager

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            else
                @Suppress("DEPRECATION")
                WindowManager.LayoutParams.TYPE_PHONE,
            // CRITICAL: Touch-Through flags ensure 100% touch transparency during gaming
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or
                    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.TOP or Gravity.END
            x = 32
            y = 80
        }

        overlayLayout = LinearLayout(this).apply {
            setBackgroundColor(Color.argb(160, 16, 24, 32))
            setPadding(24, 12, 24, 12)
        }

        hudTextView = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 11.0f
            text = "VSU: Standby | Cool"
        }

        overlayLayout?.addView(hudTextView)
        windowManager?.addView(overlayLayout, params)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val statusText = intent?.getStringExtra("EXTRA_STATUS") ?: "VSU Active"
        hudTextView?.text = statusText
        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        if (overlayLayout != null) {
            windowManager?.removeView(overlayLayout)
            overlayLayout = null
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        const val NOTIFICATION_ID = 2001
    }
}

