// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "driver/gpio.h"
#include "driver/uart.h"
#include "mdf_common.h"
#include "mwifi.h"

// #define MEMORY_DEBUG
#define BUF_SIZE 512
#define GPIO_OUTPUT_IO_0 33
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO_0)

static const char *TAG = "no-router";
esp_netif_t *sta_netif;

/**
 * @brief uart initialization
 */
static mdf_err_t uart_initialize() {
  uart_config_t uart_config = {.baud_rate = CONFIG_UART_BAUD_RATE,
                               .data_bits = UART_DATA_8_BITS,
                               .parity = UART_PARITY_DISABLE,
                               .stop_bits = UART_STOP_BITS_1,
                               .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  MDF_ERROR_ASSERT(uart_param_config(CONFIG_UART_PORT_NUM, &uart_config));
  MDF_ERROR_ASSERT(uart_set_pin(CONFIG_UART_PORT_NUM, CONFIG_UART_TX_IO,
                                CONFIG_UART_RX_IO, UART_PIN_NO_CHANGE,
                                UART_PIN_NO_CHANGE));
  MDF_ERROR_ASSERT(uart_driver_install(CONFIG_UART_PORT_NUM, 2 * BUF_SIZE,
                                       2 * BUF_SIZE, 0, NULL, 0));
  return MDF_OK;
}

static void uart_handle_task(void *arg) {
  int recv_length = 0;
  mdf_err_t ret = MDF_OK;
  cJSON *json_root = NULL;
  cJSON *json_addr = NULL;
  cJSON *json_group = NULL;
  cJSON *json_data = NULL;
  cJSON *json_dest_addr = NULL;

  // Configure a temporary buffer for the incoming data
  uint8_t *data = (uint8_t *)MDF_MALLOC(BUF_SIZE);
  size_t size = MWIFI_PAYLOAD_LEN;
  char jsonstring[] = "hello";
  uint8_t dest_addr[MWIFI_ADDR_LEN] = {0};
  mwifi_data_type_t data_type = {0};
  uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};

  MDF_LOGI("Uart handle task is running");

  esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);

  /* uart initialization */
  MDF_ERROR_ASSERT(uart_initialize());

  while (1) {
    // memset(data, 0, BUF_SIZE);
    // recv_length = uart_read_bytes(CONFIG_UART_PORT_NUM, data, BUF_SIZE, 100 /
    // portTICK_PERIOD_MS);

    // if (recv_length <= 0) {
    //     continue;
    // }

    // ESP_LOGD("UART Recv data:", "%s", data);

    // json_root = cJSON_Parse((char *)data);
    // MDF_ERROR_CONTINUE(!json_root, "cJSON_Parse, data format error, data:
    // %s", data);

    // /**
    //  * @brief Check if it is a group address. If it is a group address,
    //  data_type.group = true.
    //  */
    // json_addr = cJSON_GetObjectItem(json_root, "dest_addr");
    // json_group = cJSON_GetObjectItem(json_root, "group");

    // if (json_addr) {
    //     data_type.group = false;
    //     json_dest_addr = json_addr;
    // } else if (json_group) {
    //     data_type.group = true;
    //     json_dest_addr = json_group;
    // } else {
    //     MDF_LOGW("Address not found");
    //     cJSON_Delete(json_root);
    //     continue;
    // }
    vTaskDelay(pdMS_TO_TICKS(1000));
    /**
     * @brief  Convert mac from string format to binary
     */
    do {
      uint32_t mac_data[MWIFI_ADDR_LEN] = {0};
      sscanf("ff:ff:ff:ff:ff:ff", MACSTR, mac_data, mac_data + 1, mac_data + 2,
             mac_data + 3, mac_data + 4, mac_data + 5);

      for (int i = 0; i < MWIFI_ADDR_LEN; i++) {
        dest_addr[i] = mac_data[i];
      }
    } while (0);

    // json_data = cJSON_GetObjectItem(json_root, "data");
    // char *recv_data = cJSON_PrintUnformatted(json_data);

    // size = asprintf(&jsonstring, "{\"src_addr\": \"" MACSTR "\", \"data\":
    // %s}",
    //                 MAC2STR(sta_mac), recv_data);
    ret = mwifi_write(dest_addr, &data_type, jsonstring, strlen(jsonstring),
                      true);
    // MDF_ERROR_GOTO(ret != MDF_OK, FREE_MEM, "<%s> mwifi_root_write",
    //                mdf_err_to_name(ret));
    MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_write",
                       mdf_err_to_name(ret));

    //   FREE_MEM:
    // MDF_FREE(recv_data);
    // cJSON_Delete(json_root);
  }

  MDF_LOGI("Uart handle task is exit");

  MDF_FREE(data);
  vTaskDelete(NULL);
}

static void node_read_task(void *arg) {
  mdf_err_t ret = MDF_OK;
  char *data = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
  size_t size = MWIFI_PAYLOAD_LEN;
  mwifi_data_type_t data_type = {0x0};
  uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};

  MDF_LOGI("Node read task is running");

  for (;;) {
    if (!mwifi_is_connected() && !(mwifi_is_started() && esp_mesh_is_root())) {
      vTaskDelay(500 / portTICK_RATE_MS);
      continue;
    }

    size = MWIFI_PAYLOAD_LEN;
    memset(data, 0, MWIFI_PAYLOAD_LEN);

    /**
     * @brief Pre-allocated memory to data and size must be specified when
     * passing in a level 1 pointer
     */
    ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
    MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_read", mdf_err_to_name(ret));
    if (!esp_mesh_is_root()) {
      MDF_LOGI("Node receive, addr: " MACSTR ", size: %d, data: %s",
               MAC2STR(src_addr), size, data);
      gpio_set_level(GPIO_OUTPUT_IO_0, 1);
      /* forwoad to uart */
      // uart_write_bytes(CONFIG_UART_PORT_NUM, data, size);
      // uart_write_bytes(CONFIG_UART_PORT_NUM, "\r\n", 2);
      vTaskDelay(pdMS_TO_TICKS(500));
      gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    }
  }

  MDF_LOGW("Node read task is exit");

  MDF_FREE(data);
  vTaskDelete(NULL);
}

/**
 * @brief printing system information
 */
static void print_system_info_timercb(void *timer) {
  uint8_t primary = 0;
  wifi_second_chan_t second = 0;
  mesh_addr_t parent_bssid = {0};
  uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
  wifi_sta_list_t wifi_sta_list = {0x0};

  esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
  esp_wifi_ap_get_sta_list(&wifi_sta_list);
  esp_wifi_get_channel(&primary, &second);
  esp_mesh_get_parent_bssid(&parent_bssid);

  MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR
           ", parent bssid: " MACSTR
           ", parent rssi: %d, node num: %d, free heap: %u",
           primary, esp_mesh_get_layer(), MAC2STR(sta_mac),
           MAC2STR(parent_bssid.addr), mwifi_get_parent_rssi(),
           esp_mesh_get_total_node_num(), esp_get_free_heap_size());

  for (int i = 0; i < wifi_sta_list.num; i++) {
    MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
  }

#ifdef MEMORY_DEBUG

  if (!heap_caps_check_integrity_all(true)) {
    MDF_LOGE("At least one heap is corrupt");
  }

  mdf_mem_print_heap();
  mdf_mem_print_record();
  mdf_mem_print_task();
#endif /**< MEMORY_DEBUG */
}

static mdf_err_t wifi_init() {
  mdf_err_t ret = nvs_flash_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    MDF_ERROR_ASSERT(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  MDF_ERROR_ASSERT(ret);

  MDF_ERROR_ASSERT(esp_netif_init());
  MDF_ERROR_ASSERT(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&sta_netif, NULL));
  MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
  MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
  MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
  MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
  MDF_ERROR_ASSERT(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));

  int8_t test_power = 0;
  esp_wifi_get_max_tx_power(&test_power);
  MDF_LOGI("max power %d", test_power);
  return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx) {
  MDF_LOGI("event_loop_cb, event: %d", event);

  switch (event) {
  case MDF_EVENT_MWIFI_STARTED:
    MDF_LOGI("MESH is started");
    break;

  case MDF_EVENT_MWIFI_PARENT_CONNECTED:
    MDF_LOGI("Parent is connected on station interface");

    if (esp_mesh_is_root()) {
      esp_netif_dhcpc_start(sta_netif);
    }

    break;

  case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
    MDF_LOGI("Parent is disconnected on station interface");
    break;

  default:
    break;
  }

  return MDF_OK;
}

void app_main() {
  mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
  mwifi_config_t config = {
      .channel = CONFIG_MESH_CHANNEL,
      .mesh_id = CONFIG_MESH_ID,
      .mesh_type = CONFIG_DEVICE_TYPE,
  };

  /**
   * @brief Set the log level for serial port printing.
   */
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set(TAG, ESP_LOG_DEBUG);

  gpio_config_t io_conf = {};
  // disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  // set as output mode
  io_conf.mode = GPIO_MODE_OUTPUT;
  // bit mask of the pins that you want to set,e.g.GPIO18/19
  io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  // disable pull-down mode
  io_conf.pull_down_en = 0;
  // disable pull-up mode
  io_conf.pull_up_en = 0;
  // configure GPIO with the given settings
  gpio_config(&io_conf);

  /**
   * @brief Initialize wifi mesh.
   */
  MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
  MDF_ERROR_ASSERT(wifi_init());
  MDF_ERROR_ASSERT(mwifi_init(&cfg));
  MDF_ERROR_ASSERT(mwifi_set_config(&config));
  MDF_ERROR_ASSERT(mwifi_start());

  /**
   * @brief select/extend a group memebership here
   *      group id can be a custom address
   */
  const uint8_t group_id_list[2][6] = {{0x01, 0x00, 0x5e, 0xae, 0xae, 0xae},
                                       {0x01, 0x00, 0x5e, 0xae, 0xae, 0xaf}};

  MDF_ERROR_ASSERT(
      esp_mesh_set_group_id((mesh_addr_t *)group_id_list,
                            sizeof(group_id_list) / sizeof(group_id_list[0])));

  /**
   * @brief Data transfer between wifi mesh devices
   */

  xTaskCreate(node_read_task, "node_read_task", 4 * 1024, NULL,
              CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

  /* Periodic print system information */
  TimerHandle_t timer =
      xTimerCreate("print_system_info", 10000 / portTICK_RATE_MS, true, NULL,
                   print_system_info_timercb);
  xTimerStart(timer, 0);

  /**
   * @brief uart handle task:
   *  receive json format data,eg:`{"dest_addr":"30:ae:a4:80:4c:3c","data":"send
   * data"}` forward data item to destination address in mesh network
   */
  if (config.mesh_type == MWIFI_MESH_ROOT)
    xTaskCreate(uart_handle_task, "uart_handle_task", 4 * 1024, NULL,
                CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
}
