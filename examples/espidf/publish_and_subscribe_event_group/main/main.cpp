#include "credentials.h"
#include <MQTTRemote.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#define TAG "example"

#define PIN_LED GPIO_NUM_14

auto _conntect_state_changed_event_group = xEventGroupCreate();
MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, 1883, mqtt_username, mqtt_password,
                        MQTTRemote::Configuration{.rx_buffer_size = 2048, .tx_buffer_size = 2048, .keep_alive_s = 10});

void blinkAndSerialTask(void *pvParameters) {
  bool swap = false;
  while (1) {
    gpio_set_level(PIN_LED, swap);
    swap = !swap;
    ESP_LOGI(TAG, "Hello");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void mqttMessageTask(void *pvParameters) {
  bool retain = false;
  uint8_t qos = 0;
  while (1) {
    _mqtt_remote.publishMessageVerbose(_mqtt_remote.clientId() + "/hello", "world", retain, qos);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void mqttConnectionStateChangedTask(void *pvParameters) {
  while (1) {
    // Waiting forever for a change.
    auto bits = xEventGroupWaitBits(_mqtt_secondary_event_group,
                                    MQTTRemote::ConnectionState::Connected | MQTTRemote::ConnectionState::Disconnected,
                                    pdTRUE, pdFALSE, portMAX_DELAY);
    auto connected = (bits & MQTTRemote::ConnectionState::Connected) != 0;
    auto disconnected = (bits & MQTTRemote::ConnectionState::Connected) != 0;

    if (connected) {
      ESP_LOGI(TAG, "MQTT Connected");
      _mqtt_remote.publishMessageVerbose(_mqtt_remote.clientId() + "/initial_message", "oh hello!");
    } else if (disconnected) {
      ESP_LOGI(TAG, "MQTT Disconnected");
    }
  }
}

extern "C" {
void app_main();
}

void app_main(void) {
  // Setup led and blinking led task
  gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_LED, 1);
  xTaskCreate(blinkAndSerialTask, "blinkAndSerialTask", 2048, NULL, 15, NULL);

  // TODO (you): You need to connect to WiFi here first.
  // For a simple one line utility, see https://github.com/johboh/ConnectionHelper
  // Once connected to wifi, continue with below.

  // Connect to WIFI
  auto connected = true; // TODO (you): You need to connect to WiFi here first.
  if (connected) {
    // Connected to WIFI.

    // Subscribe to to the /set topic under our client ID.
    _mqtt_remote.subscribe(_mqtt_remote.clientId() + "/set", [](const std::string &topic, const std::string &message) {
      ESP_LOGI(TAG, "Topic: %s, Message: %s", topic.c_str(), message.c_str());
    });

    // Start MQTT
    _mqtt_remote.start(_mqtt_secondary_event_group);

    // Start task for periodically publishing messages.
    xTaskCreate(mqttMessageTask, "mqttMessageTask", 2048, NULL, 15, NULL);

    // Start task for connection state change.
    xTaskCreate(mqttConnectionStateChangedTask, "mqttConnectionStateChangedTask", 2048, NULL, 15, NULL);

  } else {
    ESP_LOGE(TAG, "Failed to connect");
  }

  // Run forever.
  while (1) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}
