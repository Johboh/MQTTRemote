#include <Ardunio.h>
#include <MQTTRemote.h>

const char mqtt_client_id[] = "my-client";
const char mqtt_host[] = "192.168.1.1";
const char mqtt_username[] = "my-username";
const char mqtt_password[] = "my-password";

MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, 1883, mqtt_username, mqtt_password);
bool _was_connected = false;
unsigned long _last_publish_ms = 0;

void setup() { Serial.begin(115200); }

void loop() {
  _mqtt_remote.handle();

  // Subscribe to the topic "interesting/topic" on new connection.
  auto connected = _mqtt_remote.connected();
  if (!_was_connected && connected) {
    _mqtt_remote.subscribe("interesting/topic", [](const char *topic, const char *message) {
      Serial.println("Got message [" + message + "] on topic: " + topic);
    });
  } else if (_was_connected && !connected) {
    _mqtt_remote.unsubscribe();
  }
  _was_connected = connected;

  // Publish a message every 2 second.
  auto now = millis();
  if (_last_publish_ms - now > 2000) {
    _mqtt_remote.publishVerbose(_mqtt_remote.clientId() + "/my_topic", "my message, hello!");
    _last_publish_ms = now;
  }
}
