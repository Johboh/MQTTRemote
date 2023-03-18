#ifndef __MQTT_REMOTE_H__
#define __MQTT_REMOTE_H__

#include "IMQTTRemote.h"
#include <Arduino.h>
#include <MQTT.h>
#include <functional>
#include <map>
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#else
#error "Unsupported hardware. Sorry!"
#endif

#define KEEP_ALIVE_S 10

/**
 * @brief MQTT wrapper for setting up MQTT connection (and will) and provide API for sending and subscribing to
 * messages.
 */
class MQTTRemote : public IMQTTRemote {
public:
  // First parameter is topic, second one is the message.
  typedef std::function<void(const char *, const char *)> SubscriptionCallback;

public:
  /**
   * @brief Construct a new Remote object
   *
   * @param client_id Base ID for this device. This is used for the last will / status
   * topic.Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * is also used as client ID for the MQTT connection. This has to be [a-zA-Z0-9_] only and unique among all MQTT
   * clients on the server. It should also be stable across connections.
   * @param host MQTT hostname or IP for MQTT server.
   * @param port MQTT port number.
   * @param username MQTT username.
   * @param password MQTT password.
   * @param max_message_size the max message size one can send. The larger to more memory/RAM is needed.
   * @param keep_alive keep alive interval in seconds.
   * @param receive_verbose if true, will print on Serial on message received. Publish verbosity is controlled by the
   * which publish method that is used. Connection information on setup will always be printed out.
   */
  MQTTRemote(String client_id, String host, int port, String username, String password,
             uint16_t max_message_size = 2048, uint32_t keep_alive = KEEP_ALIVE_S, bool receive_verbose = true);

  /**
   * Call from Arduino loop() function in main.
   */
  void handle();

  /**
   * @brief Publish a message.
   *
   * @param topic the topic to publish to.
   * @param message The message to send. This cannot be larger than the value set for max_message_size in the
   * constructor.
   * @param retain True to set this message as retained.
   * @returns true on success, or false on failure.
   */
  bool publishMessage(String topic, String message, bool retain = false) override;

  /**
   * Same as publishMessage(), but will print the message and topic and the result on serial.
   */
  bool publishMessageVerbose(String topic, String message, bool retain = false) override;

  /**
   * @brief returns if there is a connection to the MQTT server.
   */
  bool connected() override { return _mqtt_client.connected(); }

  /**
   * @brief Subscribe to a topic. The callback will be invoked on every new message.
   * There can only be one callback per topic. If trying to subscribe to an already subscribe topic, it will be ignored.
   * Don't do have operations in the callback or delays as this will block the MQTT callback.
   * Subscribe can only be called once there is a connection!
   *
   * @param message_callback a message callback with the topic and the message. The topic is repeated for convinience,
   * but it will always be for the subscribed topic.
   */
  bool subscribe(String topic, SubscriptionCallback message_callback) override;

  /**
   * @brief Unsubscribe a topic.
   */
  bool unsubscribe(String topic) override;

  /**
   * @brief The client ID for this device. This is used for the last will / status
   * topic.Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * has to be [a-zA-Z0-9_] only.
   */
  String &clientId() override { return _client_id; }

private:
  void onMessage(MQTTClient *client, char topic_cstr[], char message_cstr[], int message_size);
  void setupWill();

private:
  String _client_id;
  String _host;
  String _username;
  String _password;
  bool _receive_verbose;
  WiFiClient _wifi_client;
  MQTTClient _mqtt_client;
  std::map<String, SubscriptionCallback> _subscriptions;
  unsigned long _last_connection_attempt_timestamp_ms = 0;
};

#endif // __MQTT_REMOTE_H__