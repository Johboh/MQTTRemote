# MQTTRemote
[![PlatformIO CI](https://github.com/Johboh/MQTTRemote/actions/workflows/platformio.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/MQTTRemote)
[![ESP-IDF CI](https://github.com/Johboh/MQTTRemote/actions/workflows/espidf.yaml/badge.svg)](https://components.espressif.com/components/johboh/mqttremote)
[![Arduino IDE](https://github.com/Johboh/MQTTRemote/actions/workflows/arduino_cli.yaml/badge.svg)](https://github.com/Johboh/MQTTRemote/actions/workflows/arduino_cli.yaml)
[![GitHub release](https://img.shields.io/github/release/Johboh/MQTTRemote.svg)](https://github.com/Johboh/MQTTRemote/releases)
[![Clang-format](https://github.com/Johboh/MQTTRemote/actions/workflows/clang-format.yaml/badge.svg)](https://github.com/Johboh/MQTTRemote)

Arduino (using Arduino IDE or PlatformIO) and ESP-IDF (using Espressif IoT Development Framework or PlatformIO) compatible library MQTT wrapper for setting up an MQTT connection.

The wrapper was created to reduce boilerplate of common MQTT setup code that I was repeated in various projects.

Given the MQTT host and credentials, it connects to the host and reconnect on connection loss. It provides methods for publishing messages as well as subscribing to topics.
On connection, it publish `online` to the `client-id/status` topic, and sets up a last will to publish `offline` to the same topic on connection loss/device offline. This is a common practice for devices running as Home Assistant nodes.

### Installation
#### PlatformIO ESP32 (Arduino or ESP-IDF):
Add the following to `libs_deps`:
```
   Johboh/MQTTRemote
```
#### PlatformIO ESP8266 (Arduino only):
Add the following to `libs_deps`:
```
   Johboh/MQTTRemote8266
```
#### Arduino IDE:
Search for `MQTTRemote` by `johboh` in the library manager.
#### Espressif IoT Development Framework:
In your existing `idf_component.yml` or in a new `idf_component.yml` next to your main component:
```
dependencies:
  johboh/mqttremote:
    version: ">=6.0.1"
```

#### Arduino IDE:
Search for `MQTTRemote` by `johboh` in the library manager. See note about version above.

__Note__: Need ESP32 core v3.0.3 until [this issue](https://github.com/espressif/arduino-esp32/issues/10084) has been fixed. If you get issues with `undefined reference to `lwip_hook_ip6_input'`, try a different ESP32 core version. Need at least 3+ for C++17 support.

### Examples
- [Using Arduino IDE/CLI](examples/arduino/legacy_stack/publish_and_subscribe/publish_and_subscribe.ino)
- [Platform IO Arduino ESP32](examples/arduino/espidf_stack/publish_and_subscribe/publish_and_subscribe.ino)
- [Platform IO Arduino ESP8266](examples/arduino/legacy_stack/publish_and_subscribe/publish_and_subscribe.ino)
- [ESP-IDF framework](examples/espidf/publish_and_subscribe/main/main.cpp)

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with PlatformIO [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@4.4.6](https://github.com/espressif/esp-idf) / [ESP-IDF@5.1.2](https://github.com/espressif/esp-idf) on ESP32-S2 and ESP32-C3)
- ESP8266 (tested with PlatformIO [espressif8266@4.2.1](https://github.com/platformio/platform-espressif8266) / [ardunio-core@3.1.2](https://github.com/esp8266/Arduino))

Newer version most probably work too, but they have not been verified.

### Dependencies
- For Arduino ESP8266 only: https://github.com/256dpi/arduino-mqtt @^2.5.1

## Websockets
To use websockets, you need to build ESP-IDF with `MQTT_TRANSPORT_WEBSOCKET` (default enabled) and optionally `MQTT_TRANSPORT_WEBSOCKET_SECURE` set (default enabled).

## TLS
ESP-IDF must be built with `MQTT_TRANSPORT_SSL` set (default enabled). If using TLS over websockets, `MQTT_TRANSPORT_WEBSOCKET_SECURE` must be set (default enabled).

To use TLS (mqtts or wss), you can either use the bundled certificates with ESP-IDF, or provide your own certificate.

You can read more about this here https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/protocols/esp_crt_bundle.html

To use the bundeled certificates, make sure to build with the `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` set (default enabled). Normally, the certificates are validated and this is recommended to do, but if you use your own local certificates and do not want to bundle it, you can override this `CONFIG_ESP_TLS_INSECURE` (default disabled), but use caution.

You also need to configure the `MQTTRemote::Configuration` accordingly.

Also see `MQTTRemote::Configuration::verification` for examples on how to configure for self-signed certificates and check out [TLS using LetsEncrypt example](examples/espidf/tls_letsencrypt/main/main.cpp)