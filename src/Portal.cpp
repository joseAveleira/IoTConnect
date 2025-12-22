#include "Portal.h"
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <cstring>

static WebServer server(80);
static DNSServer dnsServer;
static bool portalActive = false;
static unsigned long lastScanTime = 0;
static String scanResults = "";

// HTML del portal (PROGMEM para ahorrar RAM)
const char HTML_PORTAL[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{{APP_NAME}} Setup</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; margin-bottom: 30px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }
        input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { background: #007cba; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin-right: 10px; }
        button:hover { background: #005a8a; }
        .danger { background: #dc3545; }
        .danger:hover { background: #c82333; }
        .status { padding: 10px; margin: 10px 0; border-radius: 4px; background: #d4edda; color: #155724; }
        .hidden { display: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>{{APP_NAME}} Setup</h1>
        <form method="POST" action="/save">
            <div class="form-group">
                <label for="clientid">Client ID:</label>
                <input type="text" id="clientid" name="clientid" value="{{CLIENT_ID}}" required>
            </div>
            
            <div class="form-group">
                <label for="token">Token:</label>
                <input type="password" id="token" name="token" value="{{TOKEN}}" required>
            </div>
            
            <div class="form-group">
                <label for="publicid">Public ID:</label>
                <input type="text" id="publicid" name="publicid" value="{{PUBLIC_ID}}" required>
            </div>
            
            <div class="form-group">
                <label for="ssid_select">Red WiFi (escaneadas):</label>
                <select id="ssid_select">
                    <option value="">Cargando redes...</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="ssid">SSID (manual):</label>
                <input type="text" id="ssid" name="ssid" value="{{SSID}}" required>
            </div>
            
            <div class="form-group">
                <label for="pass">Contraseña WiFi:</label>
                <input type="password" id="pass" name="pass" value="{{PASS}}">
            </div>
            
            <button type="submit">Guardar y Conectar</button>
            <button type="button" class="danger" onclick="resetConfig()">Reset Config</button>
        </form>
        
        <div id="status" class="status hidden"></div>
    </div>

    <script>
        function getUrlParam(name) {
            const urlParams = new URLSearchParams(window.location.search);
            return urlParams.get(name) || urlParams.get(name.toLowerCase());
        }
        
        window.onload = function() {
            const clientId = getUrlParam('clientId') || getUrlParam('clientid');
            const publicId = getUrlParam('publicId') || getUrlParam('publicid');
            const token = getUrlParam('token');
            
            if (clientId && !document.getElementById('clientid').value) {
                document.getElementById('clientid').value = clientId;
            }
            if (publicId && !document.getElementById('publicid').value) {
                document.getElementById('publicid').value = publicId;
            }
            if (token && !document.getElementById('token').value) {
                document.getElementById('token').value = token;
            }
            
            loadNetworks();
        };
        
        function loadNetworks() {
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    const select = document.getElementById('ssid_select');
                    select.innerHTML = '<option value="">Seleccionar red escaneada...</option>';
                    
                    data.networks.forEach(network => {
                        const option = document.createElement('option');
                        option.value = network.ssid;
                        option.textContent = network.ssid + ' (' + network.rssi + 'dBm)' + (network.enc ? ' 🔒' : '');
                        select.appendChild(option);
                    });
                })
                .catch(err => {
                    console.error('Error cargando redes:', err);
                    document.getElementById('ssid_select').innerHTML = '<option value="">Error cargando redes</option>';
                });
        }
        
        document.getElementById('ssid_select').onchange = function() {
            if (this.value) {
                document.getElementById('ssid').value = this.value;
            }
        };
        
        function resetConfig() {
            if (confirm('¿Estás seguro de que quieres resetear la configuración?')) {
                fetch('/reset', { method: 'POST' })
                    .then(() => {
                        document.getElementById('status').textContent = 'Configuración reseteada. Reiniciando...';
                        document.getElementById('status').classList.remove('hidden');
                        setTimeout(() => location.reload(), 2000);
                    });
            }
        }
    </script>
</body>
</html>
)html";

const char HTML_CONNECTING[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{{APP_NAME}} - Conectando</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .spinner { border: 4px solid #f3f3f3; border-top: 4px solid #007cba; border-radius: 50%; width: 40px; height: 40px; animation: spin 1s linear infinite; margin: 20px auto; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="container">
        <h1>Conectando...</h1>
        <div class="spinner"></div>
        <p>Configuración guardada. Conectando a WiFi y MQTT...</p>
        <p>El dispositivo se reiniciará automáticamente.</p>
    </div>
    <script>
        setTimeout(() => { window.location.href = '/'; }, 10000);
    </script>
</body>
</html>
)html";

String replaceTemplate(const char* tmpl, const AppConfig& cfg) {
  String html = String(tmpl);
  html.replace("{{APP_NAME}}", g_appName);
  html.replace("{{CLIENT_ID}}", cfg.clientId);
  html.replace("{{TOKEN}}", cfg.token);
  html.replace("{{PUBLIC_ID}}", cfg.publicId);
  html.replace("{{SSID}}", cfg.ssid);
  html.replace("{{PASS}}", cfg.pass);
  return html;
}

void handleRoot() {
  bool hasQRData = false;
  
  if (server.hasArg("clientId") || server.hasArg("clientid")) {
    const char* clientId = server.hasArg("clientId") ? server.arg("clientId").c_str() : server.arg("clientid").c_str();
    strlcpy(g_cfg.clientId, clientId, sizeof(g_cfg.clientId));
    Serial.printf("[CFG] Client ID desde GET: %s\n", g_cfg.clientId);
    hasQRData = true;
  }
  
  if (server.hasArg("token")) {
    strlcpy(g_cfg.token, server.arg("token").c_str(), sizeof(g_cfg.token));
    Serial.printf("[CFG] Token desde GET: %s\n", g_cfg.token);
    hasQRData = true;
  }
  
  if (server.hasArg("publicId") || server.hasArg("publicid")) {
    const char* publicId = server.hasArg("publicId") ? server.arg("publicId").c_str() : server.arg("publicid").c_str();
    strlcpy(g_cfg.publicId, publicId, sizeof(g_cfg.publicId));
    Serial.printf("[CFG] Public ID desde GET: %s\n", g_cfg.publicId);
    hasQRData = true;
  }

  if (hasQRData) {
    Serial.println("[PORTAL] Cliente conectado con datos QR");
  }

  String html = replaceTemplate(HTML_PORTAL, g_cfg);
  server.send(200, "text/html", html);
}

void handleScan() {
  unsigned long now = millis();
  
  if (now - lastScanTime < 3000 && scanResults.length() > 0) {
    server.send(200, "application/json", scanResults);
    return;
  }

  Serial.println("[NET] Escaneando redes WiFi...");
  
  WiFi.mode(WIFI_AP_STA);
  
  int n = WiFi.scanNetworks();
  
  DynamicJsonDocument doc(2048);
  JsonArray networks = doc.createNestedArray("networks");
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
  }
  
  serializeJson(doc, scanResults);
  lastScanTime = now;
  
  Serial.printf("[NET] Encontradas %d redes\n", n);
  server.send(200, "application/json", scanResults);
}

void handleSave() {
  Serial.println("[CFG] Guardando configuración desde POST");
  
  if (server.hasArg("clientid")) {
    strlcpy(g_cfg.clientId, server.arg("clientid").c_str(), sizeof(g_cfg.clientId));
  }
  if (server.hasArg("token")) {
    strlcpy(g_cfg.token, server.arg("token").c_str(), sizeof(g_cfg.token));
  }
  if (server.hasArg("publicid")) {
    strlcpy(g_cfg.publicId, server.arg("publicid").c_str(), sizeof(g_cfg.publicId));
  }
  if (server.hasArg("ssid")) {
    strlcpy(g_cfg.ssid, server.arg("ssid").c_str(), sizeof(g_cfg.ssid));
  }
  if (server.hasArg("pass")) {
    strlcpy(g_cfg.pass, server.arg("pass").c_str(), sizeof(g_cfg.pass));
  }
  
  g_cfg.confirmed = true;
  
  if (saveConfig(g_cfg)) {
    server.send(200, "text/html", HTML_CONNECTING);
    Serial.println("[CFG] Configuración guardada, saliendo del portal");
  } else {
    server.send(500, "text/plain", "Error guardando configuración");
  }
}

void handleReset() {
  Serial.println("[CFG] Reset solicitado desde web");
  clearConfig();
  server.send(200, "text/plain", "Configuración reseteada. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void startPortal() {
  if (portalActive) return;
  
  Serial.println("[NET] Iniciando portal cautivo...");
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(g_apName, "");
  
  Serial.printf("[NET] AP iniciado: %s en 192.168.4.1\n", g_apName);
  
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  
  server.on("/generate_204", handleCaptivePortal);
  server.on("/hotspot-detect.html", handleCaptivePortal);
  server.on("/favicon.ico", handleCaptivePortal);
  
  server.onNotFound(handleCaptivePortal);
  
  server.begin();
  portalActive = true;
  
  Serial.println("[NET] Portal cautivo activo en http://192.168.4.1/");
}

void stopPortal() {
  if (!portalActive) return;
  
  Serial.println("[NET] Deteniendo portal cautivo...");
  
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  
  portalActive = false;
  
  Serial.println("[NET] Portal cautivo detenido");
}

void portalLoop() {
  if (!portalActive) return;
  
  dnsServer.processNextRequest();
  server.handleClient();
}

bool isPortalActive() {
  return portalActive;
}
