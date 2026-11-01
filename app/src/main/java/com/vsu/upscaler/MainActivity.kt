package com.vsu.upscaler

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.List
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.vsu.upscaler.services.OverlayService
import com.vsu.upscaler.ui.DiagnosticsScreen
import com.vsu.upscaler.ui.GameProfilesScreen
import com.vsu.upscaler.ui.HomeScreen
import com.vsu.upscaler.viewmodel.MainViewModel

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme {
                MainAppScaffold()
            }
        }
    }

    @Composable
    fun MainAppScaffold() {
        val uiState by viewModel.uiState.collectAsState()
        var selectedTab by remember { mutableStateOf(0) }

        Scaffold(
            bottomBar = {
                NavigationBar {
                    NavigationBarItem(
                        selected = selectedTab == 0,
                        onClick = { selectedTab = 0 },
                        icon = { Icon(Icons.Default.Home, contentDescription = "Home") },
                        label = { Text("Home") }
                    )
                    NavigationBarItem(
                        selected = selectedTab == 1,
                        onClick = { selectedTab = 1 },
                        icon = { Icon(Icons.Default.List, contentDescription = "Profiles") },
                        label = { Text("Profiles") }
                    )
                    NavigationBarItem(
                        selected = selectedTab == 2,
                        onClick = { selectedTab = 2 },
                        icon = { Icon(Icons.Default.Info, contentDescription = "Diagnostics") },
                        label = { Text("Diagnostics") }
                    )
                }
            }
        ) { paddingValues ->
            Surface(modifier = Modifier.padding(paddingValues)) {
                when (selectedTab) {
                    0 -> HomeScreen(
                        uiState = uiState,
                        onToggleOverlay = { active ->
                            viewModel.toggleOverlay(active)
                            handleOverlayToggle(active)
                        },
                        onRequestUsagePermission = {
                            startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS))
                        }
                    )
                    1 -> GameProfilesScreen(
                        profiles = uiState.profiles,
                        onUpdateSharpness = { pkg, sharpness -> viewModel.updateSharpness(pkg, sharpness) },
                        onUpdateScale = { pkg, scale -> viewModel.updateScale(pkg, scale) },
                        onToggleGame = { pkg, enabled -> viewModel.toggleGame(pkg, enabled) }
                    )
                    2 -> DiagnosticsScreen(
                        telemetry = uiState.telemetry,
                        onRequestShizuku = { viewModel.requestShizukuPermission() }
                    )
                }
            }
        }
    }

    private fun handleOverlayToggle(active: Boolean) {
        if (active) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this)) {
                val intent = Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:$packageName")
                )
                startActivity(intent)
                return
            }
            startService(Intent(this, OverlayService::class.java))
        } else {
            stopService(Intent(this, OverlayService::class.java))
        }
    }
}
