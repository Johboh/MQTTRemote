#ifndef __MQTT_REMOTE_H__
#define __MQTT_REMOTE_H__

#include "IMQTTRemote.h"

#include <MQTT.h>
#include <functional>
#include <map>
#include <string>
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#else
#error "Unsupported hardware. Sorry!"
#endif

/**
 * @brief MQTT wrapper for setting up MQTT connection (and will) and provide API for sending and subscribing to
 * messages.
 */
class MQTTRemote : public IMQTTRemote {
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
   * @param max_message_size the max message size one can send. The larger to more memory/RAM is needed. Default: 2048
   * bytes
   * @param keep_alive keep alive interval in seconds. Default: 10 seconds
   * @param receive_verbose if true, will print on Serial on message received. Publish verbosity is controlled by the
   * which publish method that is used. Connection information on setup will always be printed out.
   */
  MQTTRemote(std::string client_id, std::string host, int port, std::string username, std::string password,
             uint16_t max_message_size = 2048, uint32_t keep_alive = 10, bool receive_verbose = true);

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
   * @param qos quality of service for published message (0 (default), 1 or 2)
   * @returns true on success, or false on failure.
   */
  bool publishMessage(std::string topic, std::string message, bool retain = false, uint8_t qos = 0) override;

  /**
   * Same as publishMessage(), but will print the message and topic and the result on serial.
   */
  bool publishMessageVerbose(std::string topic, std::string message, bool retain = false, uint8_t qos = 0) override;

  /**
   * @brief returns if there is a connection to the MQTT server.
   */
  bool connected() override { return _mqtt_client.connected(); }

  /**
   * @brief Subscribe to a topic. The callback will be invoked on every new message.
   * There can only be one callback per topic. If trying to subscribe to an already subscribe topic, it will be ignored.
   * Don't do heavy operations in the callback or delays as this will block the MQTT callback.
   *
   * Can be called before being connected. All subscriptions will be (re-)subscribed to once a connection is
   * (re-)established.
   *
   * @param message_callback a message callback with the topic and the message. The topic is repeated for convinience,
   * but it will always be for the subscribed topic.
   * @return true if an subcription was successul. Will return false if there is no active MQTT connection. In this
   * case, the subscription will be performed once connected. Will retun false if this subscription is already
   * subscribed to.
   */
  bool subscribe(std::string topic, IMQTTRemote::SubscriptionCallback message_callback) override;

  /**
   * @brief Unsubscribe a topic.
   */
  bool unsubscribe(std::string topic) override;

  /**
   * @brief The client ID for this device. This is used for the last will / status
   * topic.Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * has to be [a-zA-Z0-9_] only.
   */
  std::string &clientId() override { return _client_id; }

private:
  void onMessage(MQTTClient *client, char topic_cstr[], char message_cstr[], int message_size);
  void setupWill();

private:
  std::string _client_id;
  std::string _host;
  std::string _username;
  std::string _password;
  bool _receive_verbose;
  WiFiClient _wifi_client;
  MQTTClient _mqtt_client;
  std::map<std::string, SubscriptionCallback> _subscriptions;
  unsigned long _last_connection_attempt_timestamp_ms = 0;
};

#endif // __MQTT_REMOTE_H__