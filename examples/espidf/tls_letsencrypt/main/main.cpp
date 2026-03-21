#include <MQTTRemote.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "example"

const char mqtt_client_id[] = "example";
const char mqtt_host[] = "mqtts://test.mosquitto.org";
int mqtt_port = 8886; // mqtts using letsencrypt, see more at test.mosquitto.org
const char mqtt_username[] = "";
const char mqtt_password[] = "";

#define PIN_LED GPIO_NUM_14

extern const uint8_t isrgrootx1_pem_start[] asm("_binary_isrgrootx1_pem_start");
extern const uint8_t isrgrootx1_pem_end[] asm("_binary_isrgrootx1_pem_end");
const size_t certificate_len = isrgrootx1_pem_end - isrgrootx1_pem_start;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
esp_mqtt_client_config_t::broker_t::verification_t verification = {
    .use_global_ca_store = false,
    .crt_bundle_attach = nullptr,
    .certificate = (const char *)isrgrootx1_pem_start,
    .certificate_len = certificate_len,
    .psk_hint_key = nullptr,
    .skip_cert_common_name_check = false,
    .alpn_protos = nullptr,
    .common_name = nullptr,
    .ciphersuites_list = nullptr,
};
#else
esp_mqtt_client_config_t::broker_t::verification_t verification = {
    .certificate = (const char *)isrgrootx1_pem_start,
    .certificate_len = certificate_len,
};
#endif

MQTTRemote::Configuration configuration = {
    .rx_buffer_size = 2048, .tx_buffer_size = 2048, .keep_alive_s = 10, .verification = verification};
MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, mqtt_port, mqtt_username, mqtt_password, configuration);

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
    _mqtt_remote.start([](bool connected) {
      _mqtt_remote.publishMessageVerbose(_mqtt_remote.clientId() + "/initial_message", "oh hello!");
    });

    // Start task for periodically publishing messages.
    xTaskCreate(mqttMessageTask, "mqttMessageTask", 2048, NULL, 15, NULL);

  } else {
    ESP_LOGE(TAG, "Failed to connect");
  }

  // Run forever.
  while (1) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}
