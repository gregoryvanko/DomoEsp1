# WifiAuto

Librairie de connexion WiFi automatique pour ESP32 (Arduino / PlatformIO).

Au premier démarrage, l'ESP32 crée son propre point d'accès WiFi pour permettre
de lui indiquer, via une page web, le réseau auquel se connecter. Ce réseau est
ensuite enregistré en mémoire flash et retrouvé après une coupure de courant.
La librairie surveille aussi la connexion et se reconnecte seule en cas de
perte du signal.

## Fonctionnement

1. **Premier démarrage (pas d'identifiants en mémoire) :** l'ESP32 crée un
   point d'accès **ouvert** (sans mot de passe), nommé `ESP32-WifiAuto` par
   défaut. En s'y connectant avec un téléphone ou un ordinateur, la page de
   configuration s'ouvre automatiquement sur la plupart des téléphones, sinon
   elle est accessible à `http://192.168.1.1`.
2. **Saisie du réseau :** la page propose un champ SSID et un champ mot de
   passe (vide pour un réseau ouvert). Après validation, l'ESP32 enregistre
   ces identifiants en mémoire flash (NVS) et redémarre.
3. **Connexion :** l'ESP32 se connecte au réseau indiqué. Si les identifiants
   n'ont encore jamais fonctionné, plusieurs tentatives sont faites ; en cas
   d'échec de toutes, ils sont effacés et le portail de configuration se
   rouvre automatiquement. Une fois la connexion réussie une première fois,
   les identifiants sont considérés comme valides et ne sont plus effacés
   automatiquement.
4. **Démarrages suivants :** les identifiants enregistrés sont relus et
   utilisés directement, sans repasser par le portail.
5. **Reconnexion automatique :** si le signal WiFi est perdu, l'ESP32
   retente de se connecter périodiquement jusqu'au retour du réseau.
6. **Bouton de réinitialisation :** un appui maintenu 2 secondes (par défaut)
   sur un bouton (le bouton BOOT, GPIO 0, par défaut) efface les identifiants
   enregistrés et redémarre l'ESP32 en mode point d'accès.
7. **Logs :** chaque étape (démarrage du portail, tentative de connexion,
   perte de signal, motif de déconnexion, etc.) est journalisée sur le port
   série, désactivable pour la production.

## Installation

Placer les fichiers `WifiAuto.h` et `WifiAuto.cpp` dans `lib/WifiAuto/` du
projet PlatformIO. Aucune dépendance externe : la librairie utilise
uniquement les librairies fournies par le framework Arduino ESP32 (`WiFi`,
`WebServer`, `DNSServer`, `Preferences`).

## Utilisation minimale

```cpp
#include <Arduino.h>
#include "WifiAuto.h"

WifiAuto wifi;   // bouton BOOT (GPIO 0), point d'accès "ESP32-WifiAuto"

void setup() {
  Serial.begin(115200);
  wifi.begin();
}

void loop() {
  wifi.update();   // à appeler à chaque tour de loop(), sans délai bloquant
}
```

## Constructeur

```cpp
WifiAuto(uint8_t buttonPin = 0, const char* apName = "ESP32-WifiAuto");
```

| Paramètre  | Description                                              | Défaut            |
|------------|-----------------------------------------------------------|--------------------|
| `buttonPin`| GPIO du bouton de réinitialisation                        | `0` (bouton BOOT)  |
| `apName`   | Nom (SSID) du point d'accès de configuration               | `"ESP32-WifiAuto"` |

```cpp
WifiAuto wifi(4, "MonESP32");   // bouton sur GPIO 4, point d'accès "MonESP32"
```

> L'objet `WifiAuto` ne doit être déclaré qu'une seule fois (variable
> globale) et ne peut pas être copié.

## Options (à régler avant `begin()`)

Toutes les méthodes ci-dessous se règlent avant l'appel à `wifi.begin()`
dans `setup()`, sauf mention contraire.

```cpp
WifiAuto wifi;

void setup() {
  Serial.begin(115200);

  // Bouton de réinitialisation sur une autre GPIO, actif à l'état haut,
  // sans résistance de pull-up interne (ex. bouton avec résistance externe)
  wifi.setButton(4, false, false);

  // Durée d'appui (ms) requise pour effacer la configuration (défaut : 2000)
  wifi.setResetHoldTime(3000);

  // Délai (ms) entre deux tentatives de reconnexion une fois le réseau validé
  // (défaut : 10000)
  wifi.setReconnectInterval(15000);

  // Politique de nouvelle tentative pour un réseau tout juste saisi (jamais
  // encore validé) : nombre de tentatives supplémentaires et durée (ms) de
  // chaque tentative. Si toutes échouent, les identifiants sont effacés et
  // le portail est rouvert. (défauts : 2 tentatives, 15000 ms chacune)
  wifi.setRetryPolicy(2, 15000);

  // Active (défaut) ou coupe les logs sur le port série. Peut aussi être
  // appelé à tout moment, y compris après begin()
  wifi.setDebug(false);

  // Adresse IP de l'ESP32 en mode point d'accès (défaut : 192.168.1.1)
  wifi.setPortalIP(IPAddress(10, 0, 0, 1));

  // Nom d'hôte de l'ESP32 sur le réseau une fois connecté
  wifi.setHostname("esp32-salon");

  // Fonctions appelées à la connexion et à la perte du WiFi
  wifi.onConnected([]() {
    Serial.println("WiFi connecte !");
  });
  wifi.onDisconnected([]() {
    Serial.println("WiFi perdu...");
  });

  wifi.begin();
}

void loop() {
  wifi.update();
}
```

### Détail des options

| Méthode | Rôle | Défaut |
|---|---|---|
| `setButton(pin, activeLow, usePullUp)` | GPIO du bouton, polarité et pull-up interne | GPIO 0, actif bas, pull-up activée |
| `setResetHoldTime(ms)` | Durée d'appui pour effacer la configuration | 2000 ms |
| `setReconnectInterval(ms)` | Délai entre deux tentatives de reconnexion (réseau déjà validé) | 10000 ms |
| `setRetryPolicy(retries, attemptTimeoutMs)` | Tentatives et délai avant retour au portail (réseau jamais validé) | 2 tentatives, 15000 ms |
| `setDebug(enabled)` | Active ou coupe les logs sur le port série | activé |
| `setPortalIP(ip)` | Adresse IP de l'ESP32 en mode point d'accès | `192.168.1.1` |
| `setHostname(hostname)` | Nom d'hôte sur le réseau WiFi | (non défini) |
| `onConnected(callback)` | Fonction appelée à chaque connexion réussie | (aucune) |
| `onDisconnected(callback)` | Fonction appelée à chaque perte de connexion | (aucune) |

## Méthodes principales

| Méthode | Rôle |
|---|---|
| `begin()` | Démarre la librairie (à appeler dans `setup()`) |
| `update()` | Gère le bouton, le portail et la reconnexion (à appeler dans `loop()`, sans délai bloquant) |
| `clearCredentials()` | Efface le SSID et le mot de passe en mémoire, sans redémarrer |
| `resetAndRestart()` | Efface la configuration puis redémarre en mode point d'accès (équivalent à l'appui long sur le bouton) |

## État

```cpp
if (wifi.isConnected()) {
  Serial.println(wifi.getLocalIP());
}

if (wifi.isPortalActive()) {
  Serial.println("En attente de configuration...");
}

Serial.println(wifi.hasCredentials() ? "Identifiants en memoire" : "Aucun identifiant");
Serial.println(wifi.getSSID());

Serial.printf("Dernier motif de deconnexion : %u (%s)\n",
              wifi.getLastDisconnectReason(),
              wifi.getLastDisconnectReasonText().c_str());
```

| Méthode | Rôle |
|---|---|
| `isConnected()` | `true` si connecté au réseau WiFi |
| `isPortalActive()` | `true` en mode point d'accès / portail de configuration |
| `hasCredentials()` | `true` si un SSID est enregistré en mémoire |
| `getSSID()` | SSID actuellement enregistré |
| `getLocalIP()` | IP obtenue sur le réseau (ou IP du portail en mode point d'accès) |
| `getLastDisconnectReason()` | Code du dernier motif de déconnexion (ESP-IDF, `0` = aucun) |
| `getLastDisconnectReasonText()` | Description en français du dernier motif de déconnexion |

## Réinitialiser la configuration

Deux façons équivalentes :

- **Matériel :** maintenir le bouton configuré (BOOT par défaut) pendant la
  durée réglée par `setResetHoldTime()` (2 s par défaut).
- **Logiciel :** appeler `wifi.resetAndRestart();` depuis le code.

Dans les deux cas, le SSID et le mot de passe sont effacés de la mémoire
flash et l'ESP32 redémarre en mode point d'accès.

## Logs sur le port série

Avec `Serial.begin(...)` actif, chaque étape est journalisée avec le
préfixe `[WifiAuto]` : démarrage du portail, connexion d'un client,
tentative de connexion, connexion réussie (avec IP), perte de signal
(avec motif), nouvelle tentative, retour au portail, effacement des
identifiants, redémarrage.

Les événements bas niveau du driver WiFi (association, IP obtenue,
déconnexion avec son motif, client connecté ou déconnecté du portail) sont
également journalisés, préfixés par `[evt]`.

`wifi.setDebug(false);` coupe l'ensemble de ces logs, par exemple pour une
version de production.

## Remarques

- Le bouton BOOT (GPIO 0) ne doit pas être maintenu enfoncé pendant la mise
  sous tension de l'ESP32, sous peine de démarrer en mode téléchargement du
  firmware.
- Si le mot de passe saisi est refusé après toutes les tentatives, le
  portail se rouvre automatiquement : il n'est pas nécessaire d'utiliser le
  bouton dans ce cas.
- Une alimentation insuffisante (câble ou port USB faible) peut déclencher
  le détecteur de sous-tension de l'ESP32 (`Brownout detector was
  triggered`) au démarrage du WiFi. Ce n'est pas lié à la librairie : il
  faut alors changer de câble, de port USB, ou d'alimentation.
