# MQTTRemote
MQTT wrapper for setting up an MQTT connection.

The wrapper was created to reduce boilerplate of common MQTT setup code that I was repeated in various projects.

Given the MQTT host and credentials, it connects to the host and reconnect on connection loss. It provides methods for publishing messages as well as subscribing to topics.
On connection, it publish `online` to the `client-id/status` topic, and sets up a last will to publish `offline` to the same topic on connection loss/device offline. This is a common practice for devices running as Home Assistant nodes.
