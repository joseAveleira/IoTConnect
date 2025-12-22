#include "Net.h"
#include <WiFi.h>

static unsigned long lastRetryTime = 0;

bool connectWifi(const AppConfig& cfg, uint32_t timeoutMs) {
  if (strlen(cfg.ssid) == 0) {
    Serial.println("[NET] Error: SSID vacío");
    return false;
  }

  Serial.printf("[NET] Conectando a WiFi: %s\n", cfg.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.pass);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[NET] WiFi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  
  Serial.printf("[NET] Error conectando WiFi (timeout %dms)\n", timeoutMs);
  return false;
}

bool ensureWifi(uint32_t retryMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  
  unsigned long now = millis();
  if (now - lastRetryTime < retryMs) return false;
  
  lastRetryTime = now;
  Serial.println("[NET] WiFi desconectado, reintentando...");
  WiFi.reconnect();
  return false;
}

bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
