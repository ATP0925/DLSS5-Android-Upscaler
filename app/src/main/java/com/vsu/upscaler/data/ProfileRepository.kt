package com.vsu.upscaler.data

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject

class ProfileRepository(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("vsu_profiles", Context.MODE_PRIVATE)

    private val defaultProfiles = listOf(
        GameProfile("com.dts.freefiremax", "Free Fire MAX", 1.33f, 0.85f, true, true),
        GameProfile("com.dts.freefireth", "Free Fire India", 1.33f, 0.85f, true, true),
        GameProfile("com.pubg.imobile", "BGMI (Battlegrounds India)", 1.25f, 0.80f, true, true),
        GameProfile("com.activision.callofduty.shooter", "Call of Duty: Mobile", 1.20f, 0.75f, true, false),
        GameProfile("com.miHoYo.GenshinImpact", "Genshin Impact", 1.50f, 0.90f, true, true)
    )

    private val _profilesFlow = MutableStateFlow<List<GameProfile>>(loadProfiles())
    val profilesFlow: StateFlow<List<GameProfile>> = _profilesFlow.asStateFlow()

    private fun loadProfiles(): List<GameProfile> {
        val jsonString = prefs.getString("profiles_json", null) ?: return defaultProfiles
        return try {
            val array = JSONArray(jsonString)
            val list = mutableListOf<GameProfile>()
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                list.add(
                    GameProfile(
                        packageName = obj.getString("packageName"),
                        appName = obj.getString("appName"),
                        scaleFactor = obj.getDouble("scaleFactor").toFloat(),
                        sharpness = obj.getDouble("sharpness").toFloat(),
                        isEnabled = obj.getBoolean("isEnabled"),
                        isVulkanNative = obj.optBoolean("isVulkanNative", true)
                    )
                )
            }
            list
        } catch (e: Exception) {
            defaultProfiles
        }
    }

    fun saveProfile(updated: GameProfile) {
        val current = _profilesFlow.value.toMutableList()
        val index = current.indexOfFirst { it.packageName == updated.packageName }
        if (index != -1) {
            current[index] = updated
        } else {
            current.add(updated)
        }
        _profilesFlow.value = current
        persistProfiles(current)
    }

    fun getProfile(packageName: String): GameProfile? {
        return _profilesFlow.value.find { it.packageName == packageName }
    }

    private fun persistProfiles(list: List<GameProfile>) {
        val array = JSONArray()
        list.forEach { p ->
            val obj = JSONObject().apply {
                put("packageName", p.packageName)
                put("appName", p.appName)
                put("scaleFactor", p.scaleFactor.toDouble())
                put("sharpness", p.sharpness.toDouble())
                put("isEnabled", p.isEnabled)
                put("isVulkanNative", p.isVulkanNative)
            }
            array.put(obj)
        }
        prefs.edit().putString("profiles_json", array.toString()).apply()
    }
}

