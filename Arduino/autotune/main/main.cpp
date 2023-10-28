#include <inttypes.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "soc/clk_tree_defs.h"
#include "unity.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"

#define BROADCAST_PORT 8080

#define EXAMPLE_ESP_WIFI_SSID "SSID"
#define EXAMPLE_ESP_WIFI_PASS "PASSWORD"
#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define I2S_SAMPLE_BITS I2S_DATA_BIT_WIDTH_24BIT
#define I2S_SAMPLE_BITS_WIDTH I2S_SLOT_BIT_WIDTH_AUTO
#define SAMPLE_RATE 16000
#define I2S_WS GPIO_NUM_15
#define I2S_SD GPIO_NUM_13
#define I2S_SCK GPIO_NUM_2
#define BUFFER_SIZE 240
#define DMA_NUM 6

static EventGroupHandle_t s_wifi_event_group;

static const char *WIFI_TAG = "wifi station";
static const char *SOCKET_TAG = "socket";

static int s_retry_num = 0;

int32_t dataBuf[BUFFER_SIZE] = {0};
bool available = false;
esp_event_loop_handle_t loop_handle;
int sock;
struct sockaddr_in broadcast_addr;

void start_i2s();
void register_event();
void connect_wifi();
void create_socket();
static int32_t int32to24(int32_t number)
{
    number >>= 8;
    if ((number >> 23) & 1)
    {
        number |= 0xFF000000;
    }
    else
    {
        number &= 0x00FFFFFF;
    }
    return number;
}

void bufferFilled(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
     int send_result = sendto(sock, dataBuf, sizeof(dataBuf), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    if (send_result < 0)
    {
        ESP_LOGE(SOCKET_TAG, "Failed to send UDP broadcast with error code: %d", send_result);
        perror("sendto");
        // Handle the error
    } else {
        ESP_LOGI(SOCKET_TAG, "Send with size of: %u", sizeof(dataBuf));
    }
}

IRAM_ATTR bool i2s_rx_queue_received_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    memcpy(dataBuf, event->data, event->size);
    available = true;
   
    TEST_ESP_OK(esp_event_isr_post("Filled", 1, NULL, 0U, NULL));
    return false;
}

IRAM_ATTR bool i2s_rx_queue_overflow_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    // printf("Crashed\n");
    return false;
}

extern "C" void app_main()
{
    TEST_ESP_OK(uart_set_baudrate(UART_NUM_0, 921600));
    register_event();
    connect_wifi();
    start_i2s();
    create_socket();
}

void start_i2s()
{
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_NUM,
        .dma_frame_num = BUFFER_SIZE,
        .auto_clear = false,
    };
    /* Allocate a new RX channel and get the handle of this channel */
    TEST_ESP_OK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_384,
        },
        .slot_cfg = {
            .data_bit_width = I2S_SAMPLE_BITS,
            .slot_bit_width = I2S_SAMPLE_BITS_WIDTH,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_SAMPLE_BITS,
            .ws_pol = false,
            .bit_shift = true,
            .msb_right = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    TEST_ESP_OK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    i2s_event_callbacks_t cbs = {
        .on_recv = i2s_rx_queue_received_callback,
        .on_recv_q_ovf = i2s_rx_queue_overflow_callback,
        .on_sent = NULL,
        .on_send_q_ovf = NULL,
    };

    TEST_ESP_OK(i2s_channel_register_event_callback(rx_handle, &cbs, NULL));
    TEST_ESP_OK(i2s_channel_enable(rx_handle));
}

void register_event()
{
    TEST_ESP_OK(esp_event_loop_create_default());
    TEST_ESP_OK(esp_event_handler_register("Filled", 1, bufferFilled, NULL));
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void connect_wifi()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold = {
                .rssi = 0,
                .authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID: %s",
                 EXAMPLE_ESP_WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID: %s,",
                 EXAMPLE_ESP_WIFI_SSID);
    }
    else
    {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }
}

void create_socket()
{
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("192.168.1.255");
    if (sock < 0)
    {
        ESP_LOGE(SOCKET_TAG, "Failed to create socket");
        close(sock);
        return;
    }
    int enableBroadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enableBroadcast, sizeof(enableBroadcast)) < 0)
    {
        ESP_LOGE(SOCKET_TAG, "Failed to set sock options: errno %d", errno);
        close(sock);
        return;
    }
}