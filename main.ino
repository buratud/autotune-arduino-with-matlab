#include <core_esp8266_i2s.h>
#include <pins_arduino.h>
#include <I2S.h>
#define BUFFER_SIZE 256

int16_t buffer[BUFFER_SIZE];

void i2sInit()
{
  i2s_rxtx_begin(true, false); // This must be on top, I don't know why but moving it below make wave jakky
  i2s_set_bits(24);
  i2s_set_rate(44100);
}

void setup()
{
  Serial.begin(921600);
  i2sInit();
}

void loop()
{
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    if (i2s_read_sample(&buffer[i], nullptr, true))
    {
      Serial.println(buffer[i]);
    }
  }
}