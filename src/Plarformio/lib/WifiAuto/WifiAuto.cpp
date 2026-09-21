#include "WifiAuto.h"
#include <stdarg.h>

// Espace de noms et clés en mémoire flash (NVS)
static const char* NVS_NAMESPACE = "wifiauto";
static const char* NVS_KEY_SSID  = "ssid";
static const char* NVS_KEY_PASS  = "pass";
static const char* NVS_KEY_UNVERIFIED = "unverified";  // identifiants jamais validés par une connexion

// Délai avant redémarrage après l'enregistrement, pour laisser partir la réponse HTTP
static const uint32_t RESTART_DELAY_MS = 1500;

static const char PAGE_HEAD[] PROGMEM =
  "<!DOCTYPE html><html lang='fr'><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>Configuration WiFi</title><style>"
  "body{font-family:sans-serif;background:#f2f2f2;margin:0;padding:16px}"
  ".card{max-width:380px;margin:24px auto;background:#fff;padding:20px;border-radius:10px;"
  "box-shadow:0 2px 8px rgba(0,0,0,.15)}"
  "h1{font-size:20px;margin-top:0}label{display:block;margin:14px 0 4px;font-size:14px}"
  "input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:10px;"
  "font-size:16px;border:1px solid #bbb;border-radius:6px}"
  "button{width:100%;margin-top:20px;padding:12px;font-size:16px;border:0;border-radius:6px;"
  "background:#0a7cff;color:#fff}.err{color:#c00}.opt{font-size:13px;margin-top:8px}"
  "</style></head><body><div class='card'>";

static const char PAGE_TAIL[] PROGMEM = "</div></body></html>";

static const char PAGE_FORM[] PROGMEM =
  "<h1>Configuration WiFi</h1>"
  "<form method='POST' action='/save'>"
  "<label for='s'>Nom du réseau (SSID)</label>"
  "<input id='s' name='ssid' type='text' maxlength='32' required "
  "autocapitalize='none' autocomplete='off'>"
  "<label for='p'>Mot de passe</label>"
  "<input id='p' name='password' type='password' maxlength='63' autocomplete='off'>"
  "<div class='opt'><input id='c' type='checkbox' "
  "onclick=\"document.getElementById('p').type=this.checked?'text':'password'\">"
  " <label for='c' style='display:inline'>Afficher le mot de passe</label></div>"
  "<button type='submit'>Enregistrer</button></form>";

// Le SSID vient de l'utilisateur : on l'échappe avant de l'insérer dans la page
static String htmlEscape(const String& in) {
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    switch (in[i]) {
      case '&':  out += "&amp;";  break;
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += in[i];
    }
  }
  return out;
}

WifiAuto::WifiAuto(uint8_t buttonPin, const char* apName)
  : _buttonPin(buttonPin), _apName(apName), _server(80) {}

void WifiAuto::setButton(uint8_t pin, bool activeLow, bool usePullUp) {
  _buttonPin       = pin;
  _buttonActiveLow = activeLow;
  _buttonPullUp    = usePullUp;
}

void WifiAuto::setResetHoldTime(uint32_t ms)      { _resetHoldMs = ms; }
void WifiAuto::setReconnectInterval(uint32_t ms)  { _reconnectMs = ms; }
void WifiAuto::setRetryPolicy(uint8_t retries, uint32_t attemptTimeoutMs) {
  _maxRetries = retries;
  _attemptTimeoutMs = attemptTimeoutMs;
}
void WifiAuto::setDebug(bool enabled) { _debug = enabled; }
void WifiAuto::setHostname(const char* hostname)  { _hostname = hostname; }
void WifiAuto::onConnected(Callback callback)     { _onConnected = callback; }
void WifiAuto::onDisconnected(Callback callback)  { _onDisconnected = callback; }

void WifiAuto::begin() {
  if (_buttonActiveLow && _buttonPullUp) {
    pinMode(_buttonPin, INPUT_PULLUP);
  } else {
    pinMode(_buttonPin, INPUT);
  }

  // Les identifiants sont gérés par cette librairie : on évite que le driver WiFi
  // réécrive lui-même en flash à chaque WiFi.begin()
  WiFi.persistent(false);

  if (!_eventRegistered) {
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
      onWifiEvent(event, info);
    });
    _eventRegistered = true;
  }

  loadCredentials();

  if (_ssid.length() > 0) {
    startStation();
  } else {
    startPortal();
  }
}

void WifiAuto::update() {
  handleButton();

  if (_restartPending && (int32_t)(millis() - _restartAtMs) >= 0) {
    debugPrintf("Redemarrage");
    ESP.restart();
  }

  if (_portalActive) {
    _dns.processNextRequest();
    _server.handleClient();
  } else {
    handleStation();
  }
}

void WifiAuto::clearCredentials() {
  _prefs.begin(NVS_NAMESPACE, false);
  _prefs.clear();
  _prefs.end();
  _ssid = "";
  _password = "";
  debugPrintf("Identifiants effaces");
}

void WifiAuto::resetAndRestart() {
  clearCredentials();
  debugPrintf("Redemarrage en mode point d'acces");
  delay(100);
  ESP.restart();
}

bool WifiAuto::isConnected() const     { return WiFi.status() == WL_CONNECTED; }
bool WifiAuto::isPortalActive() const  { return _portalActive; }
bool WifiAuto::hasCredentials() const  { return _ssid.length() > 0; }
String WifiAuto::getSSID() const       { return _ssid; }

IPAddress WifiAuto::getLocalIP() const {
  return _portalActive ? WiFi.softAPIP() : WiFi.localIP();
}

uint8_t WifiAuto::getLastDisconnectReason() const {
  return _lastDisconnectReason;
}

String WifiAuto::getLastDisconnectReasonText() const {
  return disconnectReasonText(_lastDisconnectReason);
}

// ---------------------------------------------------------------------------
// Logs et événements WiFi
// ---------------------------------------------------------------------------

// Écrit une ligne sur le port série (préfixe [WifiAuto]) si le debug est activé
void WifiAuto::debugPrintf(const char* format, ...) const {
  if (!_debug) return;

  char buffer[192];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.print("[WifiAuto] ");
  Serial.println(buffer);
}

// Codes de motif de déconnexion (wifi_err_reason_t de l'ESP-IDF)
const char* WifiAuto::disconnectReasonText(uint8_t reason) {
  switch (reason) {
    case 0:   return "aucun";
    case 1:   return "motif non specifie";
    case 2:   return "authentification expiree";
    case 3:   return "deconnecte par le point d'acces";
    case 4:   return "association expiree";
    case 5:   return "point d'acces sature";
    case 8:   return "deconnexion volontaire";
    case 15:  return "echange de cles echoue (mot de passe incorrect probable)";
    case 16:  return "mise a jour de la cle de groupe expiree";
    case 200: return "signal perdu (beacon timeout)";
    case 201: return "reseau introuvable";
    case 202: return "authentification refusee (mot de passe incorrect probable)";
    case 203: return "association refusee";
    case 204: return "delai de negociation depasse (mot de passe incorrect probable)";
    case 205: return "connexion echouee";
    default:  return "motif inconnu";
  }
}

// Appelée par le driver WiFi (dans sa propre tâche) à chaque événement
void WifiAuto::onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      debugPrintf("[evt] WiFi station demarre");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      debugPrintf("[evt] Associe au point d'acces (canal %u)", info.wifi_sta_connected.channel);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      debugPrintf("[evt] Adresse IP obtenue : %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      _lastDisconnectReason = info.wifi_sta_disconnected.reason;
      debugPrintf("[evt] Deconnecte, motif %u : %s", _lastDisconnectReason,
                  disconnectReasonText(_lastDisconnectReason));
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
      const uint8_t* m = info.wifi_ap_staconnected.mac;
      debugPrintf("[evt] Client connecte au portail : %02X:%02X:%02X:%02X:%02X:%02X",
                  m[0], m[1], m[2], m[3], m[4], m[5]);
      break;
    }
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
      const uint8_t* m = info.wifi_ap_stadisconnected.mac;
      debugPrintf("[evt] Client deconnecte du portail : %02X:%02X:%02X:%02X:%02X:%02X",
                  m[0], m[1], m[2], m[3], m[4], m[5]);
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Mémoire
// ---------------------------------------------------------------------------

void WifiAuto::loadCredentials() {
  // Ouverture en lecture/écriture : en lecture seule, l'espace de noms doit déjà exister
  // (erreur NOT_FOUND au tout premier démarrage)
  _prefs.begin(NVS_NAMESPACE, false);
  // isKey() évite les erreurs "NOT_FOUND" affichées par getString() quand la clé n'existe pas
  _ssid     = _prefs.isKey(NVS_KEY_SSID) ? _prefs.getString(NVS_KEY_SSID, "") : "";
  _password = _prefs.isKey(NVS_KEY_PASS) ? _prefs.getString(NVS_KEY_PASS, "") : "";
  _unverified = _prefs.getBool(NVS_KEY_UNVERIFIED, false);
  _prefs.end();
}

// Les nouveaux identifiants sont marqués "non validés" tant qu'aucune connexion n'a réussi
void WifiAuto::saveCredentials(const String& ssid, const String& password) {
  _prefs.begin(NVS_NAMESPACE, false);
  _prefs.putString(NVS_KEY_SSID, ssid);
  _prefs.putString(NVS_KEY_PASS, password);
  _prefs.putBool(NVS_KEY_UNVERIFIED, true);
  _prefs.end();
}

void WifiAuto::markVerified() {
  _unverified = false;
  _prefs.begin(NVS_NAMESPACE, false);
  _prefs.putBool(NVS_KEY_UNVERIFIED, false);
  _prefs.end();
}

// ---------------------------------------------------------------------------
// Modes de fonctionnement
// ---------------------------------------------------------------------------

void WifiAuto::startStation() {
  _portalActive = false;

  // Le nom d'hôte doit être défini avant WiFi.mode()
  if (_hostname.length() > 0) {
    WiFi.setHostname(_hostname.c_str());
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  debugPrintf("Connexion a %s", _ssid.c_str());
  WiFi.begin(_ssid.c_str(), _password.c_str());
  _lastAttemptMs = millis();
  _retryCount = 0;
}

// Le réseau saisi est injoignable (mot de passe faux ?) : on l'oublie et on rouvre le portail
void WifiAuto::fallbackToPortal() {
  debugPrintf("Echec de connexion a %s (dernier motif : %s), retour au portail de configuration",
              _ssid.c_str(), disconnectReasonText(_lastDisconnectReason));

  _failedSsid = _ssid;
  clearCredentials();
  WiFi.disconnect();
  _wasConnected = false;
  startPortal();
}

void WifiAuto::startPortal() {
  _portalActive = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(_apName.c_str());  // réseau ouvert : pas de mot de passe

  // Toute résolution DNS pointe vers l'ESP32 : le portail s'ouvre automatiquement
  _dns.start(53, "*", WiFi.softAPIP());

  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/save", HTTP_POST, [this]() { handleSave(); });
  _server.onNotFound([this]() { handleNotFound(); });
  _server.begin();

  debugPrintf("Point d'acces \"%s\" actif - page de configuration : http://%s",
              _apName.c_str(), WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------------------
// Bouton : appui long -> effacement + redémarrage
// ---------------------------------------------------------------------------

void WifiAuto::handleButton() {
  bool pressed = (digitalRead(_buttonPin) == (_buttonActiveLow ? LOW : HIGH));

  if (!pressed) {
    _pressed = false;
    return;
  }

  if (!_pressed) {
    _pressed = true;
    _pressStartMs = millis();
    return;
  }

  if (millis() - _pressStartMs >= _resetHoldMs) {
    resetAndRestart();
  }
}

// ---------------------------------------------------------------------------
// Mode station : suivi de l'état et reconnexion automatique
// ---------------------------------------------------------------------------

void WifiAuto::handleStation() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected && !_wasConnected) {
    _wasConnected = true;
    if (_unverified) markVerified();
    debugPrintf("Connecte, IP : %s", WiFi.localIP().toString().c_str());
    if (_onConnected) _onConnected();
    return;
  }

  if (!connected && _wasConnected) {
    _wasConnected = false;
    _lastAttemptMs = millis();
    debugPrintf("Connexion perdue (motif : %s)", disconnectReasonText(_lastDisconnectReason));
    if (_onDisconnected) _onDisconnected();
    return;
  }

  if (connected) return;

  // Réseau jamais validé : durée limitée, puis retour au portail
  if (_unverified) {
    if (millis() - _lastAttemptMs < _attemptTimeoutMs) return;
    if (_retryCount >= _maxRetries) {
      fallbackToPortal();
      return;
    }
    _retryCount++;
    debugPrintf("Nouvelle tentative %u/%u", _retryCount, _maxRetries);
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _password.c_str());
    _lastAttemptMs = millis();
    return;
  }

  // Réseau déjà validé : filet de sécurité en plus de setAutoReconnect(), on relance
  // périodiquement la connexion tant que le réseau est injoignable
  if (millis() - _lastAttemptMs >= _reconnectMs) {
    debugPrintf("Tentative de reconnexion...");
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _password.c_str());
    _lastAttemptMs = millis();
  }
}

// ---------------------------------------------------------------------------
// Pages web du portail
// ---------------------------------------------------------------------------

void WifiAuto::handleRoot() {
  String html = FPSTR(PAGE_HEAD);
  if (_failedSsid.length() > 0) {
    html += "<p class='err'>Connexion impossible au réseau <b>" + htmlEscape(_failedSsid) +
            "</b>. Vérifiez le nom et le mot de passe puis réessayez.</p>";
  }
  html += FPSTR(PAGE_FORM);
  html += FPSTR(PAGE_TAIL);
  _server.send(200, "text/html; charset=utf-8", html);
}

void WifiAuto::handleSave() {
  String ssid     = _server.arg("ssid");
  String password = _server.arg("password");

  String error;
  if (ssid.length() == 0 || ssid.length() > 32) {
    error = "Le SSID doit contenir entre 1 et 32 caractères.";
  } else if (password.length() > 0 && (password.length() < 8 || password.length() > 63)) {
    error = "Le mot de passe doit contenir entre 8 et 63 caractères (ou être vide pour un réseau ouvert).";
  }

  String html = FPSTR(PAGE_HEAD);
  if (error.length() > 0) {
    html += "<h1>Erreur</h1><p class='err'>" + error + "</p><p><a href='/'>Retour</a></p>";
    html += FPSTR(PAGE_TAIL);
    _server.send(400, "text/html; charset=utf-8", html);
    return;
  }

  saveCredentials(ssid, password);
  debugPrintf("Identifiants enregistres pour %s", ssid.c_str());

  html += "<h1>Enregistré</h1><p>L'ESP32 redémarre et va se connecter au réseau "
          "<b>WiFi choisi</b>. Vous pouvez fermer cette page.</p>";
  html += FPSTR(PAGE_TAIL);
  _server.send(200, "text/html; charset=utf-8", html);

  _restartPending = true;
  _restartAtMs = millis() + RESTART_DELAY_MS;
}

// Redirige toute URL inconnue (tests de connectivité des téléphones, etc.) vers le portail
void WifiAuto::handleNotFound() {
  _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  _server.send(302, "text/plain", "");
}
