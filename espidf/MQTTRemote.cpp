#include "MQTTRemote.h"
#include <algorithm>
#include <cstring>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

#define RETRY_CONNECT_WAIT_MS 3000

#define LAST_WILL_MSG "offline"

static int subscribeTopic(esp_mqtt_client_handle_t client, const std::string &topic) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  return esp_mqtt_client_subscribe_single(client, topic.c_str(), 0);
#else
  return esp_mqtt_client_subscribe(client, topic.c_str(), 0);
#endif
}

void MQTTRemote::onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  MQTTRemote *_this = static_cast<MQTTRemote *>(handler_args);
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED: {
    ESP_LOGI(MQTTRemoteLog::TAG, "Connected!");

    // Mark as connected and take a snapshot of the subscriptions under the same lock. Any subscribe() call that
    // happens after this will see that we are connected and subscribe by itself, and any call that happened before
    // is part of the snapshot. This way no subscription is lost or sent twice.
    std::vector<std::string> topics;
    {
      std::lock_guard<std::mutex> lock(_this->_subscriptions_mutex);
      _this->_connected = true;
      topics.reserve(_this->_subscriptions.size());
      for (const auto &subscription : _this->_subscriptions) {
        topics.push_back(subscription.first);
      }
    }

    // And publish that we are now online.
    _this->publishMessageVerbose(_this->_last_will_topic, "online", true);

    // Subscribe to all topics.
    for (const auto &topic : topics) {
      subscribeTopic(client, topic);
    }

    // Notify last, so that any listener sees a fully set up connection.
    _this->setConnectionState(ConnectionState::Connected);
    break;
  }

  case MQTT_EVENT_DISCONNECTED: {
    ESP_LOGW(MQTTRemoteLog::TAG, "Disconnected.");
    {
      std::lock_guard<std::mutex> lock(_this->_subscriptions_mutex);
      _this->_connected = false;
    }
    _this->setConnectionState(ConnectionState::Disconnected);
    break;
  }

  case MQTT_EVENT_ERROR:
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
      ESP_LOGE(MQTTRemoteLog::TAG, "MQTT_EVENT_ERROR: Connection refused by broker, return code: %d",
               (int)event->error_handle->connect_return_code);
    } else {
      ESP_LOGE(MQTTRemoteLog::TAG, "MQTT_EVENT_ERROR: type: %d, errno: %d (%s)", (int)event->error_handle->error_type,
               event->error_handle->esp_transport_sock_errno, strerror(event->error_handle->esp_transport_sock_errno));
    }
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
    if (event->current_data_offset > 0) {
      // Continuation of a fragmented message that was already dropped below.
      ESP_LOGV(MQTTRemoteLog::TAG, "Ignoring fragment at offset %d", event->current_data_offset);
      break;
    }
    if (event->data_len < event->total_data_len) {
      // Message did not fit in the RX buffer and will arrive in multiple fragments. Drop it instead of delivering a
      // truncated message to the subscriber.
      ESP_LOGW(MQTTRemoteLog::TAG,
               "Dropping message on topic %.*s: payload size %d does not fit in RX buffer. Increase rx_buffer_size.",
               event->topic_len, event->topic, event->total_data_len);
      break;
    }
    std::string topic = std::string(event->topic, event->topic_len);
    std::string msg = std::string(event->data, event->data_len);
    ESP_LOGV(MQTTRemoteLog::TAG, "Received message with topic %s and payload size %d", topic.c_str(), event->data_len);
    _this->onMessage(topic, msg);
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
                       Configuration configuration)
    : _client_id(client_id), _last_will_topic(_client_id + "/status") {

  esp_mqtt_client_config_t mqtt_cfg = {};

  esp_mqtt_transport_t transport = MQTT_TRANSPORT_OVER_TCP;
  if (configuration.transport) {
    transport = *configuration.transport;
  } else {
    // try to deduce from schema.
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    if (host.rfind("mqtt://", 0) == 0) {
      transport = MQTT_TRANSPORT_OVER_TCP;
      host = host.substr(7);
    } else if (host.rfind("mqtts://", 0) == 0) {
      transport = MQTT_TRANSPORT_OVER_SSL;
      host = host.substr(8);
    } else if (host.rfind("ws://", 0) == 0) {
      transport = MQTT_TRANSPORT_OVER_WS;
      host = host.substr(5);
    } else if (host.rfind("wss://", 0) == 0) {
      transport = MQTT_TRANSPORT_OVER_WSS;
      host = host.substr(6);
    }
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  mqtt_cfg.broker.address.hostname = host.c_str();
  mqtt_cfg.broker.address.transport = transport;
  if (transport == MQTT_TRANSPORT_OVER_SSL || transport == MQTT_TRANSPORT_OVER_WSS) {
    memcpy(&mqtt_cfg.broker.verification, &configuration.verification, sizeof(configuration.verification));
    ESP_LOGI(MQTTRemoteLog::TAG, "Using TLS verification");
    ESP_LOGI(MQTTRemoteLog::TAG, " -- use_global_ca_store: %d", mqtt_cfg.broker.verification.use_global_ca_store);
    ESP_LOGI(MQTTRemoteLog::TAG, " -- skip_cert_common_name_check: %d",
             mqtt_cfg.broker.verification.skip_cert_common_name_check);
  }
  mqtt_cfg.broker.address.port = port;

  mqtt_cfg.buffer.size = configuration.rx_buffer_size;
  mqtt_cfg.buffer.out_size = configuration.tx_buffer_size;

  mqtt_cfg.credentials.username = username.c_str();
  mqtt_cfg.credentials.client_id = client_id.c_str();
  mqtt_cfg.credentials.authentication.password = password.c_str();

  mqtt_cfg.network.reconnect_timeout_ms = RETRY_CONNECT_WAIT_MS;
  mqtt_cfg.network.disable_auto_reconnect = false;

  mqtt_cfg.session.keepalive = configuration.keep_alive_s;
  mqtt_cfg.session.disable_keepalive = false;

  mqtt_cfg.session.last_will.topic = _last_will_topic.c_str();
  mqtt_cfg.session.last_will.msg = LAST_WILL_MSG;
  mqtt_cfg.session.last_will.qos = 1;
  mqtt_cfg.session.last_will.retain = 1;

  if (configuration.task_size) {
    mqtt_cfg.task.stack_size = *configuration.task_size;
  }
#else
  mqtt_cfg.host = host.c_str();
  mqtt_cfg.transport = transport;
  if (transport == MQTT_TRANSPORT_OVER_SSL || transport == MQTT_TRANSPORT_OVER_WSS) {
    mqtt_cfg.use_global_ca_store = configuration.verification.use_global_ca_store;
    mqtt_cfg.cert_pem = configuration.verification.certificate;
    mqtt_cfg.cert_len = configuration.verification.certificate_len;
    mqtt_cfg.skip_cert_common_name_check = configuration.verification.skip_cert_common_name_check;
    mqtt_cfg.psk_hint_key = configuration.verification.psk_hint_key;
    mqtt_cfg.alpn_protos = configuration.verification.alpn_protos;
    ESP_LOGI(MQTTRemoteLog::TAG, "Using TLS verification");
    ESP_LOGI(MQTTRemoteLog::TAG, " -- use_global_ca_store: %d", mqtt_cfg.use_global_ca_store);
    ESP_LOGI(MQTTRemoteLog::TAG, " -- skip_cert_common_name_check: %d", mqtt_cfg.skip_cert_common_name_check);
  }
  mqtt_cfg.port = port;

  mqtt_cfg.buffer_size = configuration.rx_buffer_size;
  mqtt_cfg.out_buffer_size = configuration.tx_buffer_size;

  mqtt_cfg.username = username.c_str();
  mqtt_cfg.client_id = client_id.c_str();
  mqtt_cfg.password = password.c_str();

  mqtt_cfg.reconnect_timeout_ms = RETRY_CONNECT_WAIT_MS;
  mqtt_cfg.disable_auto_reconnect = false;

  mqtt_cfg.keepalive = configuration.keep_alive_s;
  mqtt_cfg.disable_keepalive = false;

  mqtt_cfg.lwt_topic = _last_will_topic.c_str();
  mqtt_cfg.lwt_msg = LAST_WILL_MSG;
  mqtt_cfg.lwt_msg_len = sizeof(LAST_WILL_MSG) - 1;
  mqtt_cfg.lwt_qos = 1;
  mqtt_cfg.lwt_retain = 1;

  if (configuration.task_size) {
    mqtt_cfg.task_stack = *configuration.task_size;
  }
#endif

  _mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
}

void MQTTRemote::start(std::function<void(bool)> on_connection_change, unsigned long task_size, uint8_t task_priority) {
  if (_started) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Already started, cannot start again.");
    return;
  }

  _connection_state_changed_event_group = xEventGroupCreate();

  _on_connection_change = on_connection_change;
  if (_on_connection_change) {
    xTaskCreate(&runTask, "MQTTRemote_main_task", task_size, this, task_priority, NULL);
  }

  startInternal();
}

void MQTTRemote::start(EventGroupHandle_t connection_state_changed_event_group) {
  if (_started) {
    ESP_LOGW(MQTTRemoteLog::TAG, "Already started, cannot start again.");
    return;
  }

  _connection_state_changed_event_group = connection_state_changed_event_group;

  startInternal();
}

void MQTTRemote::startInternal() {
  if (_connection_state_changed_event_group != nullptr) {
    // Only clear our own bits, the event group might be shared with the user.
    xEventGroupClearBits(_connection_state_changed_event_group,
                         ConnectionState::Connected | ConnectionState::Disconnected);
  }
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(_mqtt_client, MQTT_EVENT_ANY, onMqttEvent, this));
  ESP_ERROR_CHECK(esp_mqtt_client_start(_mqtt_client));

  _started = true;
}

void MQTTRemote::setConnectionState(ConnectionState state) {
  if (_connection_state_changed_event_group == nullptr) {
    return;
  }
  // Only touch our own bits, the event group might be shared with the user.
  const EventBits_t other =
      state == ConnectionState::Connected ? ConnectionState::Disconnected : ConnectionState::Connected;
  xEventGroupClearBits(_connection_state_changed_event_group, other);
  xEventGroupSetBits(_connection_state_changed_event_group, state);
}

void MQTTRemote::runTask(void *pvParams) {
  MQTTRemote *_this = static_cast<MQTTRemote *>(pvParams);
  while (1) {
    xEventGroupWaitBits(_this->_connection_state_changed_event_group,
                        MQTTRemote::ConnectionState::Connected | MQTTRemote::ConnectionState::Disconnected, pdTRUE,
                        pdFALSE, portMAX_DELAY);
    // Report the actual current state rather than which bit was set. If the state changed multiple times since we
    // last woke up, this guarantees that the latest state is the one reported.
    _this->_on_connection_change(_this->_connected);
  }
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
  bool connected;
  {
    // Check connection state under the same lock as the MQTT_EVENT_CONNECTED handler, see comment there.
    std::lock_guard<std::mutex> lock(_subscriptions_mutex);
    if (_subscriptions.count(topic) > 0) {
      ESP_LOGW(MQTTRemoteLog::TAG, "Topic %s is already subscribed to.", topic.c_str());
      return false;
    }
    _subscriptions.emplace(topic, std::move(message_callback));
    connected = _connected;
  }

  if (!connected) {
    ESP_LOGI(MQTTRemoteLog::TAG, "Not connected. Will subscribe once connected.");
    return false;
  }

  return subscribeTopic(_mqtt_client, topic) >= 0;
}

bool MQTTRemote::unsubscribe(std::string topic) {
  {
    std::lock_guard<std::mutex> lock(_subscriptions_mutex);
    _subscriptions.erase(topic);
  }
  return esp_mqtt_client_unsubscribe(_mqtt_client, topic.c_str()) >= 0;
}

void MQTTRemote::onMessage(const std::string &topic, const std::string &message) {
  // Copy the callback while holding the lock, then invoke it without the lock. This allows the callback to safely call
  // subscribe()/unsubscribe() (even on its own topic) without deadlocking or destroying itself while running.
  SubscriptionCallback callback;
  {
    std::lock_guard<std::mutex> lock(_subscriptions_mutex);
    if (auto subscription = _subscriptions.find(topic); subscription != _subscriptions.end()) {
      callback = subscription->second;
    }
  }

  if (callback) {
    ESP_LOGV(MQTTRemoteLog::TAG, "callback found");
    callback(topic, message);
  } else {
    ESP_LOGV(MQTTRemoteLog::TAG, "NO callback found");
  }
}
