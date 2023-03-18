#include "MQTTRemote.h"

#define RETRY_CONNECT_WAIT_MS 1000

MQTTRemote::MQTTRemote(String client_id, String host, int port, String username, String password,
                       uint16_t max_message_size, uint32_t keep_alive, bool receive_verbose)
    : _client_id(client_id), _host(host), _username(username), _password(password), _receive_verbose(receive_verbose),
      _mqtt_client(max_message_size) {
  _mqtt_client.begin(_host.c_str(), port, _wifi_client);
  _mqtt_client.setKeepAlive(keep_alive);
  std::function<void(MQTTClient * client, char topic[], char bytes[], int length)> callback =
      std::bind(&MQTTRemote::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4);
  _mqtt_client.onMessageAdvanced(callback);
}

void MQTTRemote::handle() {
  auto now = millis();
  auto connected = _mqtt_client.connected();

  if (!connected && now - _last_connection_attempt_timestamp_ms > RETRY_CONNECT_WAIT_MS) {
    _subscriptions.clear();
    Serial.print("MQTTRemote: Client not connected. Trying to connect... ");
    setupWill();
    auto r = _mqtt_client.connect(_client_id.c_str(), _username.c_str(), _password.c_str());
    if (r) {
      Serial.println("success!");

      // And publish that we are now online.
      publishMessageVerbose(_client_id + "/status", "online", true);
    } else {
      Serial.println("failed :(, rc=" + String(_mqtt_client.lastError()));
    }
    _last_connection_attempt_timestamp_ms = now;
  } else if (connected) {
    _mqtt_client.loop();
  }
}

bool MQTTRemote::publishMessage(String topic, String message, bool retain) {
  if (!connected()) {
    Serial.println("MQTTRemote: Wanted to publish to topic " + topic + ", but no connection to server.");
    return false;
  }
  return _mqtt_client.publish(topic, message, retain, 0);
}

bool MQTTRemote::publishMessageVerbose(String topic, String message, bool retain) {
  if (!connected()) {
    Serial.println("MQTTRemote: Wanted to publish to topic " + topic + ", but no connection to server.");
    return false;
  }
  Serial.print("MQTTRemote: About to publish message '" + message + "' on topic '" + topic + "'...: ");
  bool r = publishMessage(topic, message, retain);
  Serial.println(String(r));
  return r;
}

bool MQTTRemote::subscribe(String topic, SubscriptionCallback message_callback) {
  if (_subscriptions.count(topic) > 0) {
    Serial.println("MQTTRemote: Err: Topic " + topic + " is already subscribed to.");
    return false;
  }
  if (!connected()) {
    Serial.println("MQTTRemote: Err: Can only subscribe when connected.");
    return false;
  }

  _subscriptions.emplace(topic, message_callback);
  return _mqtt_client.subscribe(topic);
}

bool MQTTRemote::unsubscribe(String topic) {
  _subscriptions.erase(topic);
  return _mqtt_client.unsubscribe(topic);
}

void MQTTRemote::onMessage(MQTTClient *client, char topic_cstr[], char message_cstr[], int message_size) {
  String topic = String(topic_cstr);
  if (_receive_verbose) {
    Serial.print("Received message with topic " + topic);
  }
  if (auto subscription = _subscriptions.find(topic); subscription != _subscriptions.end()) {
    if (_receive_verbose) {
      Serial.print(" (callback found) ");
    }
    subscription->second(topic_cstr, message_cstr);
  } else {
    if (_receive_verbose) {
      Serial.print(" (NO callback found) ");
    }
  }
  if (_receive_verbose) {
    Serial.println("and size: " + String(message_size));
  }
}

void MQTTRemote::setupWill() { _mqtt_client.setWill(String(_client_id + "/status").c_str(), "offline", true, 0); }
