#pragma once
#include <Arduino.h>

// Funciones del portal cautivo
void startPortal();
void stopPortal(); 
void portalLoop();

// Estado del portal
bool isPortalActive();
