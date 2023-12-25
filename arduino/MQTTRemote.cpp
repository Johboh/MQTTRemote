#include "MQTTRemote.h"

#define RETRY_CONNECT_WAIT_MS 3000

MQTTRemote::MQTTRemote(std::string client_id, std::string host, int port, std::string username, std::string password,
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

  if (!connected && (now - _last_connection_attempt_timestamp_ms > RETRY_CONNECT_WAIT_MS)) {
    Serial.print("MQTTRemote: Client not connected. Trying to connect... ");
    setupWill();
    auto r = _mqtt_client.connect(_client_id.c_str(), _username.c_str(), _password.c_str());
    if (r) {
      Serial.println("success!");

      // And publish that we are now online.
      publishMessageVerbose(_client_id + "/status", "online", true);

      // Subscribe to all topics.
      for (const auto &subscription : _subscriptions) {
        _mqtt_client.subscribe(subscription.first.c_str());
      }
    } else {
      Serial.println(("failed :(, rc=" + std::to_string(_mqtt_client.lastError())).c_str());
    }
    _last_connection_attempt_timestamp_ms = now;
  } else if (connected) {
    _mqtt_client.loop();
  }
}

bool MQTTRemote::publishMessage(std::string topic, std::string message, bool retain) {
  if (!connected()) {
    Serial.println(("MQTTRemote: Wanted to publish to topic " + topic + ", but no connection to server.").c_str());
    return false;
  }
  return _mqtt_client.publish(topic.c_str(), message.c_str(), retain, 0);
}

bool MQTTRemote::publishMessageVerbose(std::string topic, std::string message, bool retain) {
  if (!connected()) {
    Serial.println(("MQTTRemote: Wanted to publish to topic " + topic + ", but no connection to server.").c_str());
    return false;
  }
  Serial.print(("MQTTRemote: About to publish message '" + message + "' on topic '" + topic + "'...: ").c_str());
  bool r = publishMessage(topic, message, retain);
  Serial.println(std::to_string(r).c_str());
  return r;
}

bool MQTTRemote::subscribe(std::string topic, IMQTTRemote::SubscriptionCallback message_callback) {
  if (_subscriptions.count(topic) > 0) {
    Serial.println(("MQTTRemote: Warning: Topic " + topic + " is already subscribed to.").c_str());
    return false;
  }

  _subscriptions.emplace(topic, message_callback);

  if (!connected()) {
    Serial.println("MQTTRemote: Not connected. Will subscribe once connected.");
    return false;
  }

  return _mqtt_client.subscribe(topic.c_str());
}

bool MQTTRemote::unsubscribe(std::string topic) {
  _subscriptions.erase(topic);
  return _mqtt_client.unsubscribe(topic.c_str());
}

void MQTTRemote::onMessage(MQTTClient *client, char topic_cstr[], char message_cstr[], int message_size) {
  std::string topic = std::string(topic_cstr);
  if (_receive_verbose) {
    Serial.print(("Received message with topic " + topic).c_str());
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
    Serial.println(("and size: " + std::to_string(message_size)).c_str());
  }
}

void MQTTRemote::setupWill() { _mqtt_client.setWill(std::string(_client_id + "/status").c_str(), "offline", true, 0); }
