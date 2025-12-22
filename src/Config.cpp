#include "Config.h"
#include <Preferences.h>
#include <cstring>

AppConfig g_cfg = {};

// Nombres del portal (valores por defecto)
const char* g_apName = "IoT-Setup";
const char* g_appName = "IoT Connect";

static Preferences prefs;
static const char* NAMESPACE = "iotconnect";

void setPortalNames(const char* apName, const char* appName) {
  g_apName = apName;
  g_appName = appName;
  Serial.printf("[CFG] Portal configurado: AP='%s', App='%s'\n", g_apName, g_appName);
}

bool loadConfig(AppConfig& cfg) {
  Serial.println("[CFG] Cargando configuración desde NVS");
  
  if (!prefs.begin(NAMESPACE, true)) {
    Serial.println("[CFG] Error abriendo NVS para lectura");
    return false;
  }

  strlcpy(cfg.ssid, prefs.getString("ssid", "").c_str(), sizeof(cfg.ssid));
  strlcpy(cfg.pass, prefs.getString("pass", "").c_str(), sizeof(cfg.pass));
  strlcpy(cfg.clientId, prefs.getString("clientId", "").c_str(), sizeof(cfg.clientId));
  strlcpy(cfg.token, prefs.getString("token", "").c_str(), sizeof(cfg.token));
  strlcpy(cfg.publicId, prefs.getString("publicId", "").c_str(), sizeof(cfg.publicId));
  cfg.confirmed = prefs.getBool("confirmed", false);

  prefs.end();

  Serial.printf("[CFG] Config cargada: ssid='%s', clientId='%s', publicId='%s', confirmed=%s\n",
                cfg.ssid, cfg.clientId, cfg.publicId, cfg.confirmed ? "true" : "false");
  
  return true;
}

bool saveConfig(const AppConfig& cfg) {
  Serial.println("[CFG] Guardando configuración en NVS");
  
  if (!prefs.begin(NAMESPACE, false)) {
    Serial.println("[CFG] Error abriendo NVS para escritura");
    return false;
  }

  bool success = true;
  success &= prefs.putString("ssid", cfg.ssid);
  success &= prefs.putString("pass", cfg.pass);
  success &= prefs.putString("clientId", cfg.clientId);
  success &= prefs.putString("token", cfg.token);
  success &= prefs.putString("publicId", cfg.publicId);
  success &= prefs.putBool("confirmed", cfg.confirmed);

  prefs.end();

  if (success) {
    Serial.println("[CFG] Configuración guardada exitosamente");
  } else {
    Serial.println("[CFG] Error guardando configuración");
  }

  return success;
}

void clearConfig() {
  Serial.println("[CFG] Limpiando configuración NVS");
  
  if (!prefs.begin(NAMESPACE, false)) {
    Serial.println("[CFG] Error abriendo NVS para limpiar");
    return;
  }

  prefs.clear();
  prefs.end();

  memset(&g_cfg, 0, sizeof(g_cfg));
  
  Serial.println("[CFG] Configuración limpiada");
}
