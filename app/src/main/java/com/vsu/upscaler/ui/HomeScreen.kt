package com.vsu.upscaler.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vsu.upscaler.data.HardwareThermalLevel
import com.vsu.upscaler.viewmodel.MainUiState

@Composable
fun HomeScreen(
    uiState: MainUiState,
    onToggleOverlay: (Boolean) -> Unit,
    onRequestUsagePermission: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // App Title Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp)) {
                Text(
                    text = "Vulkan Spatial Upscaler",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Hardware-safe edge-adaptive reconstruction (EASU + CAS)",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.8f)
                )
            }
        }

        // Active Foreground Game Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = "Active Game",
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(36.dp)
                )
                Spacer(modifier = Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = "Active Game Target",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.outline
                    )
                    Text(
                        text = uiState.activeGame?.appName ?: "No Game Running",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.SemiBold
                    )
                    Text(
                        text = if (uiState.activeGame != null) "Mode: ${uiState.activeGame.scaleFactor}x Spatial | Sharpness: ${(uiState.activeGame.sharpness * 100).toInt()}%" else "Listening via UsageStats...",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }

        // Real-Time Thermal State Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                val (color, text) = when (uiState.thermalLevel) {
                    HardwareThermalLevel.COOL -> Color(0xFF4CAF50) to "Optimal (<38°C)"
                    HardwareThermalLevel.MODERATE -> Color(0xFF8BC34A) to "Warm (~40°C)"
                    HardwareThermalLevel.WARM -> Color(0xFFFF9800) to "Hot (~43°C)"
                    HardwareThermalLevel.CRITICAL -> Color(0xFFF44336) to "CRITICAL (>45°C) Auto-Bypass"
                }

                Box(
                    modifier = Modifier
                        .size(16.dp)
                        .background(color, RoundedCornerShape(8.dp))
                )
                Spacer(modifier = Modifier.width(16.dp))
                Column {
                    Text(text = "SoC Thermal State (PowerManager API)", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                    Text(text = text, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                }
            }
        }

        // Overlay Viewport Toggle
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = "Touch-Through Overlay HUD", fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                    Text(
                        text = "Displays live compute latency and upscaler status over games without blocking touch.",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.outline
                    )
                }
                Switch(
                    checked = uiState.isOverlayActive,
                    onCheckedChange = onToggleOverlay
                )
            }
        }

        if (!uiState.hasUsageStatsPermission) {
            Button(
                onClick = onRequestUsagePermission,
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
            ) {
                Icon(Icons.Default.Warning, contentDescription = null)
                Spacer(modifier = Modifier.width(8.dp))
                Text("Grant Usage Access for Game Auto-Detection")
            }
        }
    }
}

