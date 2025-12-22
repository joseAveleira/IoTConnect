#pragma once
#include <Arduino.h>

struct AppConfig {
  char ssid[64];
  char pass[64];
  char clientId[64];
  char token[64];
  char publicId[64];
  bool confirmed;
};

extern AppConfig g_cfg;

// Funciones de configuración NVS
bool loadConfig(AppConfig& cfg);
bool saveConfig(const AppConfig& cfg);
void clearConfig();

// Constantes MQTT (puedes cambiarlas según tu servidor)
constexpr const char* MQTT_HOST = "joseaveleira.es";
constexpr uint16_t    MQTT_PORT = 1883;

// Nombres del portal (configurables desde IoTConnect)
extern const char* g_apName;
extern const char* g_appName;

// Función para establecer los nombres del portal
void setPortalNames(const char* apName, const char* appName);
