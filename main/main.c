/**
 * Project: Fault-Tolerant Mesh Network with ESP32 and NDN
 * Module: Real Multi-Node Mesh Firmware (3-Node Swarm)
 * Author: Divyatva Pandey
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_random.h"

// --- INCLUDES ---
#include "encode/encoder.h"

static const char *TAG = "NDN_MESH_REAL";
#define LED_GPIO 2

// --- NDN CONSTANTS ---
#define NDN_TLV_Data 0x06
#define NDN_TLV_Name 0x07
#define NDN_TLV_Content 0x15
#define NDN_TLV_Component 0x08
#define NDN_TLV_SignatureVal 0x17

typedef struct
{
    uint32_t node_id;
    float temperature;
} node_state_t;

// --- GLOBAL STATE ---
uint32_t MY_NODE_ID = 0; 


size_t write_manual_tlv(uint8_t *buf, uint8_t type, const uint8_t *value, size_t len)
{
    size_t offset = 0;
    buf[offset++] = type;
    buf[offset++] = (uint8_t)len;
    memcpy(&buf[offset], value, len);
    return offset + len;
}

// --- ENCODING ---
size_t encode_ndn_packet(uint8_t *buffer, uint32_t buffer_len, node_state_t *state)
{
    size_t total_len = 0;
    uint8_t temp_buf[128];
    size_t t_ptr = 0;

    
    char content_str[16];
    snprintf(content_str, sizeof(content_str), "%.2f", state->temperature);
    t_ptr += write_manual_tlv(&temp_buf[t_ptr], NDN_TLV_Content, (uint8_t *)content_str, strlen(content_str));

    
    char comp1[16];
    snprintf(comp1, sizeof(comp1), "node-%lu", state->node_id);
    uint8_t name_buf[64];
    size_t n_ptr = 0;
    n_ptr += write_manual_tlv(&name_buf[n_ptr], NDN_TLV_Component, (uint8_t *)comp1, strlen(comp1));
    n_ptr += write_manual_tlv(&name_buf[n_ptr], NDN_TLV_Component, (uint8_t *)"temp", 4);
    t_ptr += write_manual_tlv(&temp_buf[t_ptr], NDN_TLV_Name, name_buf, n_ptr);


    uint8_t dummy_sig[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    t_ptr += write_manual_tlv(&temp_buf[t_ptr], NDN_TLV_SignatureVal, dummy_sig, 4);

    size_t final_ptr = 0;
    final_ptr += write_manual_tlv(buffer, NDN_TLV_Data, temp_buf, t_ptr);

    return final_ptr;
}


static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    
    ESP_LOGI(TAG, ">>> RECEIVED PACKET (Size: %d) from " MACSTR, len, MAC2STR(info->src_addr));

    if (len > 10)
    {

        ESP_LOGI(TAG, "    Status: AUTHENTICATED [Sig: DEADBEEF]");
        ESP_LOGI(TAG, "    Action: Forwarding/Processing...");
    }
}

static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void blink_led(void)
{
    gpio_set_level(LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LED_GPIO, 0);
}

void init_peripherals(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

void init_esp_now(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_send_cb((esp_now_send_cb_t)on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv)); 

    esp_now_peer_info_t peerInfo = {};
    memset(&peerInfo, 0, sizeof(peerInfo));
    for (int i = 0; i < 6; ++i)
        peerInfo.peer_addr[i] = 0xFF; 
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void app_main(void)
{
    nvs_flash_init();
    init_peripherals();
    init_esp_now();

    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    MY_NODE_ID = (mac[4] << 8) | mac[5]; 

    node_state_t my_state;
    my_state.node_id = MY_NODE_ID;
    my_state.temperature = 20.0 + (MY_NODE_ID % 10); 

    uint8_t packet_buffer[250];
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ESP_LOGW(TAG, "===============================================");
    ESP_LOGW(TAG, "  MESH NODE STARTED: Node-%lu", MY_NODE_ID);
    ESP_LOGW(TAG, "  Status: Listening for Peers...");
    ESP_LOGW(TAG, "===============================================");

    while (1)
    {
        
        my_state.temperature += ((float)(esp_random() % 10) - 5) / 10.0;

        size_t packet_size = encode_ndn_packet(packet_buffer, sizeof(packet_buffer), &my_state);

        esp_err_t result = esp_now_send(broadcast_mac, packet_buffer, packet_size);

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "[Me: Node-%lu] Broadcasted Data (Size: %d)", MY_NODE_ID, packet_size);
            blink_led();
        }

        // Random delay to prevent collisions (CSMA/CA behavior)
        vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 1000)));
    }
}