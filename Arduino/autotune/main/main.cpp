#include "driver/uart.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "soc/clk_tree_defs.h"
#include <inttypes.h>
#include "unity.h"
#include <cstring>
#include "esp_timer.h"

#define I2S_SAMPLE_BITS I2S_DATA_BIT_WIDTH_24BIT
#define I2S_SAMPLE_BITS_WIDTH I2S_SLOT_BIT_WIDTH_AUTO
#define SAMPLE_RATE 16000
#define I2S_WS GPIO_NUM_15
#define I2S_SD GPIO_NUM_13
#define I2S_SCK GPIO_NUM_2
#define BUFFER_SIZE 240
#define DMA_NUM 6

int32_t dataBuf[BUFFER_SIZE] = {0};
bool available = false;
esp_event_loop_handle_t loop_handle;

void start_i2s();
void register_event();

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
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        printf("%" PRId32 "\n", int32to24(dataBuf[i]));
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
    start_i2s();
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