#include <driver/i2s.h>
#define SAMPLE_RATE 30000
#define I2S_NUM I2S_NUM_0
#define I2S_SAMPLE_BITS I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_NUM 2
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN 512
#define I2S_WS 15
#define I2S_SD 13
#define I2S_SCK 2

#define BUFFER_SIZE 512

int32_t dataBuf[BUFFER_SIZE] = {0};

void setup()
{
  Serial.begin(921600);
  // I2S configuration
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_SAMPLE_BITS,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = I2S_DMA_BUF_COUNT,
      .dma_buf_len = I2S_DMA_BUF_LEN,
      .use_apll = false,
  };
  // I2S pinout
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD,
  };
  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
}
void loop()
{
  size_t bytesRead;
  i2s_read(I2S_NUM, &dataBuf, sizeof(dataBuf), &bytesRead, portMAX_DELAY);
  // Serial.println(bytesRead);
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    Serial.write(dataBuf[i]);
    // Serial.println(dataBuf[i]);
  }
}