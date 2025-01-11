#include "MQTTRemote.h"
#include <esp_err.h>
#include <esp_log.h>

#define RETRY_CONNECT_WAIT_MS 3000

#define LAST_WILL_MSG "offline"

void MQTTRemote::onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  MQTTRemote *_this = static_cast<MQTTRemote *>(handler_args);
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    _this->_connected = true;
    ESP_LOGI(MQTTRemoteLog::TAG, "Connected!");

    // And publish that we are now online.
    _this->publishMessageVerbose(_this->_client_id + "/status", "online", true);

    // Subscribe to all topics.
    for (const auto &subscription : _this->_subscriptions) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
      esp_mqtt_client_subscribe_single(client, subscription.first.c_str(), 0);
#else
      esp_mqtt_client_subscribe(client, subscription.first.c_str(), 0);
#endif
    }

    if (_this->_on_connected) {
      _this->_on_connected();
    }

    break;

  case MQTT_EVENT_DISCONNECTED:
    _this->_connected = false;
    ESP_LOGW(MQTTRemoteLog::TAG, "Disconnected.");
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGE(MQTTRemoteLog::TAG, "MQTT_EVENT_ERROR: %s", strerror(event->error_handle->esp_transport_sock_errno));
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGV(MQTTRemoteLog::TAG, "MQTT_EVENT_SUBSCRIBED");
    break;

  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGV(MQTTRemoteLog::TAG, "MQTT_EVENT_UNSUBSCRIBED");
    break;

  case MQTT_EVENT_PUBLISHED:
    ESP_LOGV(MQTTRemoteLog::TAG, "MQTT_EVENT_PUBLISHED");
    break;

  case MQTT_EVENT_DATA: {
    std::string topic = std::string(event->topic, event->topic_len);
    std::string msg = std::string(event->data, event->data_len);
    ESP_LOGV(MQTTRemoteLog::TAG, "Received message with topic %s and payload size %d", topic.c_str(), event->data_len);
    if (auto subscription = _this->_subscriptions.find(topic); subscription != _this->_subscriptions.end()) {
      ESP_LOGV(MQTTRemoteLog::TAG, "callback found");
      subscription->second(topic, msg);
    } else {
      ESP_LOGV(MQTTRemoteLog::TAG, "NO callback found");
    }
    break;
  }

  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGV(MQTTRemoteLog::TAG, "Trying to connect...");
    break;

  case MQTT_EVENT_DELETED:
    ESP_LOGV(MQTTRemoteLog::TAG, "MQTT_EVENT_DELETED");
    break;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  case MQTT_USER_EVENT:
    ESP_LOGV(MQTTRemoteLog::TAG, "MQTT_USER_EVENT");
    break;
#endif

  default:
    break;
  }
}

MQTTRemote::MQTTRemote(std::string client_id, std::string host, int port, std::string username, std::string password,
                       uint16_t max_message_size, uint32_t keep_alive)
    : _client_id(client_id), _last_will_topic(_client_id + "/status") {

  esp_mqtt_client_config_t mqtt_cfg = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  mqtt_cfg.broker.address.hostname = host.c_str();
  mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP; // TODO: Support TLS
  mqtt_cfg.broker.address.port = port;

  mqtt_cfg.buffer.size = max_message_size;

  mqtt_cfg.credentials.username = username.c_str();
  mqtt_cfg.credentials.client_id = client_id.c_str();
  mqtt_cfg.credentials.authentication.password = password.c_str();

  mqtt_cfg.network.reconnect_timeout_ms = RETRY_CONNECT_WAIT_MS;
  mqtt_cfg.network.disable_auto_reconnect = false;

  mqtt_cfg.session.keepalive = keep_alive;
  mqtt_cfg.session.disable_keepalive = false;

  mqtt_cfg.session.last_will.topic = _last_will_topic.c_str();
  mqtt_cfg.session.last_will.msg = LAST_WILL_MSG;
  mqtt_cfg.session.last_will.qos = 0;
  mqtt_cfg.session.last_will.retain = 0;
#else
  mqtt_cfg.host = host.c_str();
  mqtt_cfg.transport = MQTT_TRANSPORT_OVER_TCP; // TODO: Support TLS
  mqtt_cfg.port = port;

  mqtt_cfg.buffer_size = max_message_size;

  mqtt_cfg.username = username.c_str();
  mqtt_cfg.client_id = client_id.c_str();
  mqtt_cfg.password = password.c_str();

  mqtt_cfg.reconnect_timeout_ms = RETRY_CONNECT_WAIT_MS;
  mqtt_cfg.disable_auto_reconnect = false;

  mqtt_cfg.keepalive = keep_alive;
  mqtt_cfg.disable_keepalive = false;

  mqtt_cfg.lwt_topic = _last_will_topic.c_str();
  mqtt_cfg.lwt_msg = LAST_WILL_MSG;
  mqtt_cfg.lwt_msg_len = sizeof(LAST_WILL_MSG) - 1;
  mqtt_cfg.lwt_qos = 0;
  mqtt_cfg.lwt_retain = 0;
#endif

  _mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
}

void MQTTRemote::start(std::function<void()> on_connected) {
  if (_started) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Already started, cannot start again.");
    return;
  }
  _on_connected = on_connected;

  ESP_ERROR_CHECK(esp_mqtt_client_register_event(_mqtt_client, MQTT_EVENT_ANY, onMqttEvent, this));
  ESP_ERROR_CHECK(esp_mqtt_client_start(_mqtt_client));
}

bool MQTTRemote::publishMessage(std::string topic, std::string message, bool retain, uint8_t qos) {
  if (!connected()) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Not connected to server when trying to publish to topic %s.", topic.c_str());
    return false;
  }
  return esp_mqtt_client_publish(_mqtt_client, topic.c_str(), message.c_str(), message.length(), qos, retain) >= 0;
}

bool MQTTRemote::publishMessageVerbose(std::string topic, std::string message, bool retain, uint8_t qos) {
  if (!connected()) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Not connected to server when trying to publish to topic %s.", topic.c_str());
    return false;
  }

  ESP_LOGI(MQTTRemoteLog::TAG, "About to publish message '%s' on topic '%s'...", message.c_str(), topic.c_str());
  bool r = publishMessage(topic, message, retain, qos);
  ESP_LOGI(MQTTRemoteLog::TAG, "Publish result: %s", (r ? "success" : "failure"));
  return r;
}

bool MQTTRemote::subscribe(std::string topic, IMQTTRemote::SubscriptionCallback message_callback) {
  if (_subscriptions.count(topic) > 0) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Topic %s is already subscribed to.", topic.c_str());
    return false;
  }

  _subscriptions.emplace(topic, message_callback);

  if (!connected()) {
    ESP_LOGI(MQTTRemoteLog::TAG, "Not connected. Will subscribe once connected.");
    return false;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  return esp_mqtt_client_subscribe_single(_mqtt_client, topic.c_str(), 0) >= 0;
#else
  return esp_mqtt_client_subscribe(_mqtt_client, topic.c_str(), 0) >= 0;
#endif
}

bool MQTTRemote::unsubscribe(std::string topic) {
  _subscriptions.erase(topic);
  return esp_mqtt_client_unsubscribe(_mqtt_client, topic.c_str()) >= 0;
}
