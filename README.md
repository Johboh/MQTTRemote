# MQTTRemote
[![Test](https://github.com/Johboh/MQTTRemote/actions/workflows/test.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/MQTTRemote)
[![Test](https://github.com/Johboh/MQTTRemote/actions/workflows/esp_upload_component.yaml/badge.svg)](https://components.espressif.com/components/johboh/mqttremote)
[![GitHub release](https://img.shields.io/github/release/Johboh/MQTTRemote.svg)](https://github.com/Johboh/MQTTRemote/releases)

Arduino (using Arduino IDE or Platform I/O) and ESP-IDF (using Espressif IoT Development Framework or Platform I/O) compatible library MQTT wrapper for setting up an MQTT connection.

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
typedef std::function<void(std::string *, std::string *)> SubscriptionCallback;

bool subscribe(String topic, SubscriptionCallback message_callback);
bool unsubscribe(String topic);
```

Call from main `loop()` to run:
```C++
void handle();
```

### Installation
#### Platform I/O (Arduino or ESP-IDF):
Add the following to `libs_deps`:
```
   Johboh/MQTTRemote
```
#### Espressif IoT Development Framework:
In your existing `idf_component.yml` or in a new `idf_component.yml` next to your main component:
```
dependencies:
  johboh/MQTTRemote:
    version: ">=4.0.0"
```

### Examples
See Example for [publishing and subscribing](examples/PublishAndSubscribe/PublishAndSubscribe.ino).

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with platform I/O [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@5.1.1](https://github.com/espressif/esp-idf) on ESP32-S2 and ESP32-C3)
- ESP8266 (tested with platform I/O [espressif8266@4.2.1](https://github.com/platformio/platform-espressif8266) / [ardunio-core@3.1.2](https://github.com/esp8266/Arduino))

Newer version most probably work too, but they have not been verified.

### Dependencies
- For Arduino: https://github.com/256dpi/arduino-mqtt @^2.5.1
