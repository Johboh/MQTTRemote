#ifndef __I_MQTT_REMOTE_H__
#define __I_MQTT_REMOTE_H__

#include <Arduino.h>

/**
 * @brief Inteface to separate the concerns between the actual MQTT implementation that has a lifecycle, connection
 * handling, and consumers who only need the API itself.
 */
class IMQTTRemote {
public:
  /**
   * @brief Publish a message.
   *
   * @param topic the topic to publish to.
   * @param message The message to send. This cannot be larger than the value set for max_message_size in the
   * constructor.
   * @param retain True to set this message as retained.
   * @returns true on success, or false on failure.
   */
  virtual bool publishMessage(String topic, String message, bool retain = false) = 0;

  /**
   * Same as publishMessage(), but will print the message and topic and the result on Serial.
   */
  virtual bool publishMessageVerbose(String topic, String message, bool retain = false) = 0;

  /**
   * @brief Subscribe to a topic. The callback will be invoked on every new message.
   * There can only be one callback per topic. If trying to subscribe to an already subscribe topic, it will be ignored.
   * Don't do have operations in the callback or delays as this will block the MQTT callback.
   *
   * @param message_callback a message callback with the topic and the message. The topic is repeated for convinience,
   * but it will always be for the subscribed topic.
   */
  virtual bool subscribe(String topic, std::function<void(const char *, const char *)> message_callback) = 0;

  /**
   * @brief Unsubscribe a topic.
   */
  virtual bool unsubscribe(String topic) = 0;

  /**
   * @brief returns if there is a connection to the MQTT server.
   */
  virtual bool connected() = 0;

  /**
   * @brief The client ID for this device. This is used for the last will / status
   * topic.Example, if this is "esp_now_router", then the status/last will topic will be "esp_now_router/status". This
   * has to be [a-zA-Z0-9_] only.
   */
  virtual String &clientId() = 0;
};

#endif // __I_MQTT_REMOTE_H__