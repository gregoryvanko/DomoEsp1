#ifndef WIFI_AUTO_H
#define WIFI_AUTO_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <functional>

// Connexion WiFi automatique pour ESP32.
//
// - Sans identifiants en mémoire : l'ESP32 crée un point d'accès OUVERT (sans mot de passe).
//   Une fois connecté dessus, une page web (http://192.168.4.1, ouverte automatiquement
//   sur la plupart des téléphones) permet de saisir le SSID et le mot de passe du réseau.
// - Les identifiants sont enregistrés en mémoire flash (NVS) puis l'ESP32 redémarre
//   et se connecte au réseau. Ils sont retrouvés après une coupure de courant.
// - Une reconnexion automatique est gérée si le signal WiFi est perdu.
// - Un appui long sur le bouton (défaut : BOOT, GPIO 0) efface les identifiants
//   et redémarre l'ESP32 en mode point d'accès.
//
// Utilisation :
//   WifiAuto wifi;                 // ou WifiAuto wifi(4, "MonESP32");
//   void setup() { wifi.begin(); }
//   void loop()  { wifi.update(); }
class WifiAuto {
public:
  using Callback = std::function<void()>;

  // buttonPin : GPIO du bouton de réinitialisation (défaut : 0 = bouton BOOT)
  // apName    : nom (SSID) du point d'accès de configuration
  WifiAuto(uint8_t buttonPin = 0, const char* apName = "ESP32-WifiAuto");

  // Le WebServer garde des références internes : l'objet ne doit pas être copié
  WifiAuto(const WifiAuto&)            = delete;
  WifiAuto& operator=(const WifiAuto&) = delete;

  // --- Réglages optionnels (à appeler AVANT begin()) ---

  // Bouton actif à l'état bas (défaut, ex. BOOT) ou à l'état haut.
  // usePullUp : active la résistance de pull-up interne (uniquement si activeLow = true).
  void setButton(uint8_t pin, bool activeLow = true, bool usePullUp = true);
  // Durée d'appui (ms) pour effacer la configuration (défaut : 2000)
  void setResetHoldTime(uint32_t ms);
  // Délai (ms) entre deux tentatives de reconnexion (défaut : 10000)
  void setReconnectInterval(uint32_t ms);
  // Réseau tout juste saisi dans le portail (donc pas encore validé) : nombre de nouvelles
  // tentatives (défaut : 2) et durée (ms) de chaque tentative (défaut : 15000). Si elles
  // échouent toutes, les identifiants sont effacés et le portail est rouvert.
  // Un réseau déjà validé une fois n'est jamais effacé : on se reconnecte indéfiniment.
  void setRetryPolicy(uint8_t retries, uint32_t attemptTimeoutMs);
  // Active (défaut) ou coupe les logs sur le port série. Peut être appelé à tout moment.
  void setDebug(bool enabled);
  // Nom d'hôte de l'ESP32 sur le réseau
  void setHostname(const char* hostname);

  // Fonctions appelées à la connexion / à la perte du WiFi
  void onConnected(Callback callback);
  void onDisconnected(Callback callback);

  // Démarre la librairie : charge la configuration puis lance la connexion
  // (mode station) ou le portail de configuration (mode point d'accès).
  // Non bloquant. À appeler dans setup().
  void begin();

  // Gère le bouton, le portail, la reconnexion. À appeler à chaque tour de loop().
  void update();

  // Efface le SSID et le mot de passe en mémoire, sans redémarrer
  void clearCredentials();
  // Efface la configuration puis redémarre l'ESP32 (relance en mode point d'accès)
  void resetAndRestart();

  // --- État ---
  bool isConnected() const;      // connecté au réseau WiFi
  bool isPortalActive() const;   // mode point d'accès / portail de configuration
  bool hasCredentials() const;   // SSID enregistré en mémoire
  String getSSID() const;        // SSID enregistré
  IPAddress getLocalIP() const;  // IP obtenue sur le réseau (ou IP du portail)
  // Dernier motif de déconnexion WiFi (code ESP-IDF, 0 = aucun) et sa description
  uint8_t getLastDisconnectReason() const;
  String  getLastDisconnectReasonText() const;

private:
  // Configuration
  uint8_t  _buttonPin;
  bool     _buttonActiveLow = true;
  bool     _buttonPullUp    = true;
  uint32_t _resetHoldMs     = 2000;
  uint32_t _reconnectMs     = 10000;
  uint8_t  _maxRetries      = 2;
  uint32_t _attemptTimeoutMs = 15000;
  bool     _debug           = true;
  String   _apName;
  String   _hostname;

  // Identifiants chargés depuis la mémoire
  String _ssid;
  String _password;
  bool   _unverified = false;   // identifiants saisis mais jamais connectés avec succès
  String _failedSsid;           // dernier SSID rejeté, affiché sur le portail
  uint8_t _retryCount = 0;
  volatile uint8_t _lastDisconnectReason = 0;  // écrit par la tâche WiFi
  bool _eventRegistered = false;

  Preferences _prefs;
  WebServer   _server;
  DNSServer   _dns;

  bool     _portalActive = false;
  bool     _wasConnected = false;
  uint32_t _lastAttemptMs = 0;
  bool     _pressed        = false;
  uint32_t _pressStartMs   = 0;   // instant (ms) du début de l'appui
  bool     _restartPending = false;
  uint32_t _restartAtMs    = 0;   // instant (ms) du redémarrage planifié

  Callback _onConnected;
  Callback _onDisconnected;

  void loadCredentials();
  void saveCredentials(const String& ssid, const String& password);
  void markVerified();

  void debugPrintf(const char* format, ...) const __attribute__((format(printf, 2, 3)));
  void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info);
  static const char* disconnectReasonText(uint8_t reason);

  void startStation();
  void fallbackToPortal();
  void startPortal();

  void handleButton();
  void handleStation();

  void handleRoot();
  void handleSave();
  void handleNotFound();
};

#endif
