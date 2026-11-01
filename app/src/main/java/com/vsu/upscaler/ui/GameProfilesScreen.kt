package com.vsu.upscaler.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vsu.upscaler.data.GameProfile

@Composable
fun GameProfilesScreen(
    profiles: List<GameProfile>,
    onUpdateSharpness: (String, Float) -> Unit,
    onUpdateScale: (String, Float) -> Unit,
    onToggleGame: (String, Boolean) -> Unit
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        item {
            Text(
                text = "Per-Game Tuning Profiles",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = "Configure custom spatial reconstruction scale and CAS sharpness per package.",
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.outline
            )
            Spacer(modifier = Modifier.height(8.dp))
        }

        items(profiles, key = { it.packageName }) { profile ->
            GameProfileCard(
                profile = profile,
                onUpdateSharpness = { onUpdateSharpness(profile.packageName, it) },
                onUpdateScale = { onUpdateScale(profile.packageName, it) },
                onToggleGame = { onToggleGame(profile.packageName, it) }
            )
        }
    }
}

@Composable
fun GameProfileCard(
    profile: GameProfile,
    onUpdateSharpness: (Float) -> Unit,
    onUpdateScale: (Float) -> Unit,
    onToggleGame: (Boolean) -> Unit
) {
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
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = profile.appName, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                    Text(text = profile.packageName, fontSize = 11.sp, color = MaterialTheme.colorScheme.outline)
                }
                Switch(checked = profile.isEnabled, onCheckedChange = onToggleGame)
            }

            if (profile.isEnabled) {
                Spacer(modifier = Modifier.height(12.dp))

                // Sharpness Slider
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(text = "CAS Edge Sharpness", fontSize = 12.sp)
                    Text(text = "${(profile.sharpness * 100).toInt()}%", fontSize = 12.sp, fontWeight = FontWeight.Bold)
                }
                Slider(
                    value = profile.sharpness,
                    onValueChange = onUpdateSharpness,
                    valueRange = 0.0f..1.0f
                )

                // Scale Factor Slider
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(text = "Spatial Reconstruction Ratio", fontSize = 12.sp)
                    Text(text = String.format("%.2fx", profile.scaleFactor), fontSize = 12.sp, fontWeight = FontWeight.Bold)
                }
                Slider(
                    value = profile.scaleFactor,
                    onValueChange = onUpdateScale,
                    valueRange = 1.0f..2.0f,
                    steps = 3 // 1.0x, 1.25x, 1.33x, 1.5x, 2.0x
                )
            }
        }
    }
}

