#include "MQTTRemote.h"
#include <algorithm>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define RETRY_CONNECT_WAIT_MS 3000

#define LAST_WILL_MSG "offline"

void MQTTRemote::onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  MQTTRemote *_this = static_cast<MQTTRemote *>(handler_args);
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MQTTRemoteLog::TAG, "Connected!");
    _this->_connected = true;
    xEventGroupSetBits(_this->_connection_state_changed_event_group, ConnectionState::Connected);

    // And publish that we are now online.
    _this->publishMessageVerbose(_this->_last_will_topic, "online", true);

    // Subscribe to all topics.
    for (const auto &subscription : _this->_subscriptions) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
      esp_mqtt_client_subscribe_single(client, subscription.first.c_str(), 0);
#else
      esp_mqtt_client_subscribe(client, subscription.first.c_str(), 0);
#endif
    }

    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGW(MQTTRemoteLog::TAG, "Disconnected.");
    _this->_connected = false;
    xEventGroupSetBits(_this->_connection_state_changed_event_group, ConnectionState::Disconnected);
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
  mqtt_cfg.session.last_will.qos = 0;
  mqtt_cfg.session.last_will.retain = 0;

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
  mqtt_cfg.lwt_qos = 0;
  mqtt_cfg.lwt_retain = 0;

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
  xEventGroupClearBits(_connection_state_changed_event_group, 0xFF);
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(_mqtt_client, MQTT_EVENT_ANY, onMqttEvent, this));
  ESP_ERROR_CHECK(esp_mqtt_client_start(_mqtt_client));

  _started = true;
}

void MQTTRemote::runTask(void *pvParams) {
  MQTTRemote *_this = static_cast<MQTTRemote *>(pvParams);
  while (1) {
    auto event_bits =
        xEventGroupWaitBits(_this->_connection_state_changed_event_group,
                            MQTTRemote::ConnectionState::Connected | MQTTRemote::ConnectionState::Disconnected, pdTRUE,
                            pdFALSE, portMAX_DELAY);
    if ((event_bits & MQTTRemote::ConnectionState::Connected) != 0) {
      _this->_on_connection_change(true);
    } else if ((event_bits & MQTTRemote::ConnectionState::Disconnected) != 0) {
      _this->_on_connection_change(false);
    }
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
