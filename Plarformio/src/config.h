#pragma once
// ── Setup ──────────────────────────────────────────────────────────────────────
constexpr int32_t CONFIG_BAUDRATE = 115200;
constexpr int32_t CONFIG_MESURE_INTERVAL = 60000;
constexpr uint8_t CONFIG_DEBOUNCE_MS= 50;
constexpr const char* CONFIG_WIFI_ESP32SSID = "ESP32-Garage";
// ── GPIO ──────────────────────────────────────────────────────────────────────-
constexpr uint8_t CONFIG_PIN_BUTTON_BOOT = 0;
constexpr uint8_t CONFIG_PIN_BUTTON1 = 13;
constexpr uint8_t CONFIG_PIN_BUTTON2 = 14;
constexpr uint8_t CONFIG_PIN_BUTTON3 = 16;
constexpr uint8_t CONFIG_PIN_BUTTON4 = 17;
constexpr uint8_t CONFIG_PIN_BUTTON5 = 18;
constexpr uint8_t CONFIG_PIN_BUTTON6 = 19;
constexpr uint8_t CONFIG_PIN_BUTTON7 = 21;
constexpr uint8_t CONFIG_PIN_BUTTON8 = 22;
constexpr uint8_t CONFIG_PIN_TEMPERATURE = 23;
// ── MQTT ──────────────────────────────────────────────────────────────────────-
constexpr const char* CONFIG_MQTT_TOPIC_STATUS = "Domo/status";
constexpr const char* CONFIG_MQTT_TOPIC_GET = "Domo/Get";
constexpr const char* CONFIG_MQTT_TOPIC_PIN = "Domo/Pin";
constexpr const char* CONFIG_MQTT_TOPIC_TEMPERATURE = "Domo/Temp1";
constexpr const char* CONFIG_MQTT_PAYLOAD_ALL = "All";