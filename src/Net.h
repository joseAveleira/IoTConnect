#pragma once
#include <Arduino.h>
#include "Config.h"

// Funciones de conectividad WiFi
bool connectWifi(const AppConfig& cfg, uint32_t timeoutMs = 15000);
bool ensureWifi(uint32_t retryMs = 3000);

// Estado de la conexión
bool isWifiConnected();
