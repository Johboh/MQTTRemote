#include <Arduino.h>
#include <MQTTRemote.h>
#include <WiFi.h>
#include <string>

/**
 * @brief ESP32 example version
 * Connects to an MQTT broker and publishes and subscribes to topics.
 */

const char wifi_ssid[] = "my-wifi-ssid";
const char wifi_password[] = "my-wifi-password";
const char mqtt_client_id[] = "my-client";
const char mqtt_host[] = "192.168.1.1";
const char mqtt_username[] = "my-username";
const char mqtt_password[] = "my-password";

MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, 1883, mqtt_username, mqtt_password,
                        {.buffer_size = 2048, .keep_alive_s = 10, .receive_verbose = true});

bool _was_connected = false;
unsigned long _last_publish_ms = 0;

void setup() {
  Serial.begin(115200);

  // Setup WiFI
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("have wifi");
  Serial.print("IP number: ");
  Serial.println(WiFi.localIP());

  _mqtt_remote.start([](bool connected) {
    if (connected) {
      _mqtt_remote.subscribe(
          _mqtt_remote.clientId() + "/interesting/topic", [](std::string topic, std::string message) {
            Serial.println(("Got message [" + message + "] on topic: " + topic).c_str());

            _mqtt_remote.publishMessageVerbose(_mqtt_remote.clientId() + "/initial_message", "oh hello!");
          });
    }
  });
}

void loop() {
  // Publish a message every 5 seconds.
  auto now = millis();
  auto connected = _mqtt_remote.connected();
  if (connected && (now - _last_publish_ms > 5000)) {
    bool retain = false;
    uint8_t qos = 0;
    _mqtt_remote.publishMessageVerbose(_mqtt_remote.clientId() + "/my_topic", "my message, hello!", retain, qos);
    _last_publish_ms = now;
  }
}
