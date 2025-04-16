#ifndef __MQTT_REMOTE_H__
#define __MQTT_REMOTE_H__

#include "IMQTTRemote.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <functional>
#include <map>
#include <mqtt_client.h>
#include <optional>
#include <string>

namespace MQTTRemoteLog {
const char TAG[] = "MQTTRemote";
} // namespace MQTTRemoteLog

namespace MQTTRemoteDefaults {
const uint32_t CONNECTION_STATUS_STACK_SIZE = 4096;
const uint32_t CONNECTION_STATUS_TASK_PRIORITY = 7;
} // namespace MQTTRemoteDefaults

/**
 * @brief MQTT wrapper for setting up MQTT connection (and will) and provide API for sending and subscribing to
 * messages.
 */
class MQTTRemote : public IMQTTRemote {
public:
  enum ConnectionState : uint8_t {
    Connected = BIT0,
    Disconnected = BIT1,
  };

  /**
   * Additional configuration where most user can go with defaults.
   */
  struct Configuration {
// ESP-IDF 4.4 backward compatibility
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    typedef esp_mqtt_client_config_t::broker_t::verification_t verification_t;
#else
    // See
    // https://github.com/espressif/esp-mqtt/blob/ae53d799da294f03ef65c33e88fa33648e638134/include/mqtt_client.h#L244
    struct verification_t {
      bool use_global_ca_store = false;
      esp_err_t (*crt_bundle_attach)(void *conf) = nullptr;
      const char *certificate = nullptr;
      size_t certificate_len = 0;
      bool skip_cert_common_name_check = false;
      const struct psk_key_hint *psk_hint_key = nullptr;
      const char **alpn_protos = nullptr;
    };
#endif

    /**
     * Maximum message size, in bytes, for incoming messages. Messages larger than this will be truncated.
     * This will be allocated on the heap upon MQTTRemote object creation.
     */
    uint32_t rx_buffer_size = 1024;

    /**
     * Maximum message size, in bytes, for outgoing messages. Messages larger than this will be truncated.
     * This will be allocated on the heap upon MQTTRemote object creation.
     */
    uint32_t tx_buffer_size = 1024;

    /**
     * Task size to use for the ESP-IDF MQTT task. If you are handling large and/or many messages (queues), you might
     * need to increase this.
     * Normally defaults to 6144 bytes, setting this to non std::opt will override setting from menuconfig.
     */
    std::optional<uint32_t> task_size = 4096;

    /**
     * MQTT keep alive interval, in seconds. If the client fails to communicate with the broker within the specified
     * Keep Alive period, the LWT/Last Will message is sent (by the broker).
     */
    uint32_t keep_alive_s = 10;

    /**
     * Values, see esp_mqtt_transport_t:
     * - MQTT_TRANSPORT_OVER_TCP = mqtt
     * - MQTT_TRANSPORT_OVER_SSL = mqtts (TLS)
     * - MQTT_TRANSPORT_OVER_WS = ws (websockets)
     * - MQTT_TRANSPORT_OVER_WSS = wss (websockets with TLS)
     *
     * If not set, will try to decude the transport to use given the host, based on schemes `mqtt`, `mqtts`, `ws`,
     * `wss`. If no scheme is found, will default to `mqtt`.
     */
    std::optional<esp_mqtt_transport_t> transport = std::nullopt;

    /**
     * If using TLS, this configures the certificate verification. Must be set if using TLS.
     *
     * With this, you can configure to use your own private certificate, or using the global bundle.
     *
     * Example to use the global bundle:
     * ```cpp
     *   #include <esp_rls.h>
     *
     *  .vertification = {
     *    .use_global_ca_store = true,
     *  },
     * ```
     *
     * Then before calling start():
     *
     * ```cpp
     *   mbedtls_ssl_config conf;
     *   mbedtls_ssl_config_init(&conf);
     *   esp_tls_init_global_ca_store();
     * ```
     * Here you have the option to use other TLS implementation like mbedtls or wolfssl.
     *
     *
     * Example how to use letsencrypt (or similary for your own certificates):
     * 1. Download the ISRG Root X1 PEM file from https://letsencrypt.org/certs/isrgrootx1.pem
     * 2. Add in main directory
     * 3. Add `target_add_binary_data(${COMPONENT_TARGET} "isrgrootx1.pem" TEXT)` to your CMakeLists.txt
     *
     * Setup verification as follows:
     * ```cpp
     *   extern const uint8_t isrgrootx1_pem_start[] asm("_binary_isrgrootx1_pem_start");
     *
     *  .vertification = {
     *    .certificate = (const char *)isrgrootx1_pem_start,
     *  },
     * ```
     *
     * If using your own certificate, you might need to set `skip_cert_common_name_check` to true in the verification.
     */
    verification_t verification = {};
  };

  /**
   * @brief Construct a new MQTTRemote object
   *
   * To set log level for this object, use: esp_log_level_set(MQTTRemoteLog::TAG, ESP_LOG_*);
   *
   * A call to start() most follow.
   *
   * @param client_id Base ID for this device. This is used for the last will / status
   * topic. Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * is also used as client ID for the MQTT connection. This has to be [a-zA-Z0-9_] only and unique among all MQTT
   * clients on the server. It should also be stable across connections.
   * @param host MQTT hostname or IP for MQTT server. Supports `mqtt`, `mqtts`, `ws`, `wss` schemes. For example,
   * mqtts://hostname or just hostname/IP. If not using schemes, transport must be set in Configuration. Remember to
   * also specify the correct port to use for your schema.
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
   * To set log level for this object, use: esp_log_level_set(MQTTRemoteLog::TAG, ESP_LOG_*);
   *
   * A call to start() most follow.
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
   * @brief Call once there is a WIFI connection on which the host can be reached.
   * Will connect to the server and setup any subscriptions as well as start the MQTT loop.
   * @param on_connection_change optional callback on connection state change. Will be called when the client is
   * connected to server (every time, so expect calls on reconnection), and on disconnect. The parameter will be true on
   * new connection and false on disconnection. This callback will run from a dedicated task. Task size and priority can
   * be set.
   * @param task_size the stack size for the task that will call the on_connection_change callback, if set.
   * @param task_priority the priority for the task that will call the on_connection_change callback, if set.
   *
   * NOTE: Can only be called once WIFI has been setup! ESP-IDF will assert otherwise.
   */
  void start(std::function<void(bool)> on_connection_change = {},
             unsigned long task_size = MQTTRemoteDefaults::CONNECTION_STATUS_STACK_SIZE,
             uint8_t task_priority = MQTTRemoteDefaults::CONNECTION_STATUS_TASK_PRIORITY);

  /**
   * @brief Call once there is a WIFI connection on which the host can be reached.
   * Will connect to the server and setup any subscriptions as well as start the MQTT loop.
   * @param connection_state_changed_event_group optional event group for connection state change. Will be be set with
   * ConnectionState::Connected when the client is connected to server (every time, so expect re-setting on
   * reconnection), and ConnectionState::Disconnected on disconnect.
   *
   * NOTE: Can only be called once WIFI has been setup! ESP-IDF will assert otherwise.
   */
  void start(EventGroupHandle_t connection_state_changed_event_group);

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
  bool connected() override { return _connected; }

  /**
   * @brief Subscribe to a topic. The callback will be invoked on every new message.
   * There can only be one callback per topic. If trying to subscribe to an already subscribe topic, it will be ignored.
   * Don't do heavy operations in the callback or delays as this will block the MQTT callback.
   *
   * Can be called before being connected. All subscriptions will be (re-)subscribed to once a connection is
   * (re-)established.
   *
   * @param message_callback a message callback with the topic and the message. The topic is repeated for convenience,
   * but it will always be for the subscribed topic.
   * @return true if a subcription was successful. Will return false if there is no active MQTT connection. In this
   * case, the subscription will be performed once connected. Will return false if this subscription is already
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
  void startInternal();

  /*
   * @brief Event handler registered to receive MQTT events
   *
   *  This function is called by the MQTT client event loop.
   *
   * @param handler_args user data registered to the event.
   * @param base Event base for the handler(always MQTT Base in this example).
   * @param event_id The id for the received event.
   * @param event_data The data for the event, esp_mqtt_event_handle_t.
   */
  static void onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  static void runTask(void *pvParams);

private:
  bool _started = false;
  std::string _client_id;
  bool _connected = false;
  std::string _last_will_topic;
  esp_mqtt_client_handle_t _mqtt_client;
  std::function<void(bool)> _on_connection_change;
  EventGroupHandle_t _connection_state_changed_event_group;
  std::map<std::string, SubscriptionCallback> _subscriptions;
};

#endif // __MQTT_REMOTE_H__