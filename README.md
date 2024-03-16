# MQTTRemote
[![PlatformIO CI](https://github.com/Johboh/MQTTRemote/actions/workflows/platformio.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/MQTTRemote)
[![ESP-IDF CI](https://github.com/Johboh/MQTTRemote/actions/workflows/espidf.yaml/badge.svg)](https://components.espressif.com/components/johboh/mqttremote)
[![GitHub release](https://img.shields.io/github/release/Johboh/MQTTRemote.svg)](https://github.com/Johboh/MQTTRemote/releases)
[![Clang-format](https://github.com/Johboh/MQTTRemote/actions/workflows/clang-format.yaml/badge.svg)](https://github.com/Johboh/MQTTRemote)

Arduino (using Arduino IDE or PlatformIO) and ESP-IDF (using Espressif IoT Development Framework or PlatformIO) compatible library MQTT wrapper for setting up an MQTT connection.

The wrapper was created to reduce boilerplate of common MQTT setup code that I was repeated in various projects.

Given the MQTT host and credentials, it connects to the host and reconnect on connection loss. It provides methods for publishing messages as well as subscribing to topics.
On connection, it publish `online` to the `client-id/status` topic, and sets up a last will to publish `offline` to the same topic on connection loss/device offline. This is a common practice for devices running as Home Assistant nodes.

### Installation
#### PlatformIO (Arduino or ESP-IDF):
Add the following to `libs_deps`:
```
   Johboh/MQTTRemote
```
#### Espressif IoT Development Framework:
In your existing `idf_component.yml` or in a new `idf_component.yml` next to your main component:
```
dependencies:
  johboh/MQTTRemote:
    version: ">=4.0.1"
```

### Examples
- [Arduino framework](examples/arduino/publish_and_subscribe/PublishAndSubscribe.ino)
- [ESP-IDF framework](examples/espidf/publish_and_subscribe/main/main.cpp)

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with PlatformIO [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@4.4.6](https://github.com/espressif/esp-idf) / [ESP-IDF@5.1.2](https://github.com/espressif/esp-idf) on ESP32-S2 and ESP32-C3)
- ESP8266 (tested with PlatformIO [espressif8266@4.2.1](https://github.com/platformio/platform-espressif8266) / [ardunio-core@3.1.2](https://github.com/esp8266/Arduino))

Newer version most probably work too, but they have not been verified.

### Dependencies
- For Arduino: https://github.com/256dpi/arduino-mqtt @^2.5.1
