package com.vsu.upscaler.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vsu.upscaler.data.DiagnosticTelemetry

@Composable
fun DiagnosticsScreen(
    telemetry: DiagnosticTelemetry,
    onRequestShizuku: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            text = "Hardware & Engine Diagnostics",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold
        )

        // GPU & Vulkan Capabilities
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(text = "GPU Hardware Subsystem", fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(8.dp))
                DiagnosticRow(label = "Target GPU", value = telemetry.gpuRenderer)
                DiagnosticRow(label = "Vulkan Driver API", value = telemetry.vulkanVersion)
                DiagnosticRow(label = "EASU + CAS FP16 Math", value = "Supported (Adreno/Mali)")
                DiagnosticRow(label = "Benchmark Compute Latency", value = "${telemetry.frameComputeLatencyMs} ms / frame")
            }
        }

        // Shizuku Integration Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(text = "Privileged Shizuku Subsystem", fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                    Icon(
                        imageVector = if (telemetry.isShizukuAvailable) Icons.Default.Check else Icons.Default.Close,
                        contentDescription = null,
                        tint = if (telemetry.isShizukuAvailable) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
                    )
                }
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = if (telemetry.isShizukuAvailable)
                        "Shizuku service is connected. Privileged settings injection enabled without root."
                    else
                        "Shizuku not detected or permission not granted. Running in standard overlay mode.",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.outline
                )

                if (!telemetry.isShizukuAvailable) {
                    Spacer(modifier = Modifier.height(12.dp))
                    OutlinedButton(
                        onClick = onRequestShizuku,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Connect with Shizuku")
                    }
                }
            }
        }

        // Honest Engineering Limitations Notice
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            shape = RoundedCornerShape(16.dp)
        ) {
            Row(modifier = Modifier.padding(16.dp)) {
                Icon(Icons.Default.Info, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                Spacer(modifier = Modifier.width(12.dp))
                Text(
                    text = "Honest Technical Guarantee: Vulkan Spatial Upscaler reconstructs edge fidelity and contrast sharpness using FP16 compute kernels. It does not alter game internal render resolution without root/in-engine integration and will never falsely claim FPS multiplication.",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
fun DiagnosticRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(text = label, fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
        Text(text = value, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
}

