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
   * Additional configuration where most user can go with defaults.
   */
  struct Configuration {
    /**
     * Maximum message size, in bytes, for incoming and outgoing messages. Messages larger than this will be truncated.
     * This will be allocated on the heap upon MQTTRemote object creation.
     */
    uint32_t buffer_size = 1024;

    /**
     * MQTT keep alive interval, in seconds. If the client fails to communicate with the broker within the specified
     * Keep Alive period, the LWT/Last Will message is sent (by the broker).
     */
    uint32_t keep_alive_s = 10;

    /**
     * if true, will print on Serial on message received. Publish verbosity is controlled by the
     * which publish method that is used. Connection information on setup will always be printed out.
     */
    bool receive_verbose = false;
  };

  /**
   * @brief Construct a new MQTTRemote object
   *
   * @param client_id Base ID for this device. This is used for the last will / status
   * topic. Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * is also used as client ID for the MQTT connection. This has to be [a-zA-Z0-9_] only and unique among all MQTT
   * clients on the server. It should also be stable across connections.
   * @param host MQTT hostname or IP for MQTT server.
   * @param port MQTT port number.
   * @param username MQTT username.
   * @param password MQTT password.
   */
  MQTTRemote(std::string client_id, std::string host, int port, std::string username, std::string password)
      : MQTTRemote(std::move(client_id), std::move(host), port, std::move(username), std::move(password),
                   Configuration{}) {}

  /**
   * @brief Construct a new MQTTRemote object
   *
   * @param client_id Base ID for this device. This is used for the last will / status
   * topic. Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * is also used as client ID for the MQTT connection. This has to be [a-zA-Z0-9_] only and unique among all MQTT
   * clients on the server. It should also be stable across connections.
   * @param host MQTT hostname or IP for MQTT server.
   * @param port MQTT port number.
   * @param username MQTT username.
   * @param password MQTT password.
   * @param configuration Additional configuration where most user can go with defaults.
   */
  MQTTRemote(std::string client_id, std::string host, int port, std::string username, std::string password,
             Configuration configuration);

  /**
   * Call from Arduino loop() function in main.
   */
  void handle();

  /**
   * @brief Set optional callback on connect state change. Will be called when the client is connected
   * to server (every time, so expect calls on reconnection), and on disconnect. The parameter will be true on new
   * connection and false on disconnection. Set to {} to clear callback.
   */
  void setOnConnectionChange(std::function<void(bool connected)> callback = {}) { _on_connection_change = callback; };

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
  bool _was_connected = false;
  std::function<void(bool)> _on_connection_change;
  std::map<std::string, SubscriptionCallback> _subscriptions;
  unsigned long _last_connection_attempt_timestamp_ms = 0;
};

#endif // __MQTT_REMOTE_H__