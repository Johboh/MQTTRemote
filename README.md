# MQTTRemote
[![GitHub release](https://img.shields.io/github/release/Johboh/MQTTRemote.svg)](https://github.com/Johboh/MQTTRemote/releases)

Arduino MQTT wrapper for setting up an MQTT connection.

The wrapper was created to reduce boilerplate of common MQTT setup code that I was repeated in various projects.

Given the MQTT host and credentials, it connects to the host and reconnect on connection loss. It provides methods for publishing messages as well as subscribing to topics.
On connection, it publish `online` to the `client-id/status` topic, and sets up a last will to publish `offline` to the same topic on connection loss/device offline. This is a common practice for devices running as Home Assistant nodes.

### API
Creating the instance:
```C++
MQTTRemote(String client_id, String host, int port, String username, String password, uint16_t max_message_size = 2048, bool receive_verbose = true)
```

Publishing a message:
```C++
bool publishMessage("topic", "message");
bool publishMessageVerbose("topic", "message"); // Prints to Serial.
```

Subscribing/unsubscribing to a topic:
```C++
// First parameter is topic, second one is the message.
typedef std::function<void(const char *, const char *)> SubscriptionCallback;

bool subscribe(String topic, SubscriptionCallback message_callback);
bool unsubscribe(String topic);
```

Call from main `loop()` to run:
```C++
void handle();
```

### Examples
See Example for [publishing and subscribing](examples/PublishAndSubscribe/PublishAndSubscribe.ino).

### Supported platforms
- ESP32 (tested with platform I/O [espressif32@5.3.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.6](https://github.com/espressif/arduino-esp32) on ESP32-S2 and ESP32-C3)
- ESP8266 (tested with platform I/O [espressif8266@4.0.1](https://github.com/platformio/platform-espressif8266) / [ardunio-core@3.0.2](https://github.com/esp8266/Arduino))

### Dependencies
- https://github.com/256dpi/arduino-mqtt @^2.5.1
