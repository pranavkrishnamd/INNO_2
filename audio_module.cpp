#include "audio_module.h"
#include "pins.h"
#include "state_manager.h"
#include <Arduino.h>
#include <driver/i2s.h>

static Audio audio;
static bool  audioPlaying = false;

// DMA Buffer for INMP441 Microphone Recording
#define REC_SAMPLE_RATE   16000
#define REC_BUFFER_SIZE   (REC_SAMPLE_RATE * 2 * 8) // Up to 8 seconds of 16-bit PCM

static uint8_t* recBuffer = nullptr;
static size_t   recOffset = 0;
static bool     isRecording = false;

void Audio_init(BotState &bot) {
  // Speaker Amplifier Setup (MAX98357 - I2S1)
  audio.setPinout(PIN_AMP_BCLK, PIN_AMP_LRC, PIN_AMP_DIN);
  audio.setVolume(7); // Default volume (0-21)

  // Mic Setup (INMP441 - I2S0)
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = REC_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,
    .dma_buf_len          = 1024,
    .use_apll             = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = PIN_MIC_SCK,
    .ws_io_num    = PIN_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = PIN_MIC_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  if (!recBuffer) {
    recBuffer = (uint8_t*) ps_malloc(REC_BUFFER_SIZE);
  }
  Serial.println("Audio: Mic & Speaker initialized");
}

void Audio_startRecording(BotState &bot) {
  audio.stopSong();
  recOffset = 0;
  isRecording = true;
  bot.ai.recordStart = millis();
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("Audio: Recording started...");
}

uint8_t* Audio_stopRecording(BotState &bot, size_t &length) {
  isRecording = false;
  length = recOffset;
  Serial.printf("Audio: Recording stopped. Captured %d bytes\n", length);
  return recBuffer;
}

void Audio_loop() {
  audio.loop();
  
  if (isRecording && recBuffer) {
    size_t bytesRead = 0;
    if (recOffset + 1024 <= REC_BUFFER_SIZE) {
      i2s_read(I2S_NUM_0, recBuffer + recOffset, 1024, &bytesRead, portMAX_DELAY);
      recOffset += bytesRead;
    }
  }
}

void Audio_playURL(const char* url) {
  audio.stopSong();
  audioPlaying = audio.connecttohost(url);
}

bool Audio_isPlaying() {
  return audio.isRunning();
}

void Audio_stop() {
  audio.stopSong();
}

void Audio_setVolume(int volumePercent) {
  int vol = map(volumePercent, 0, 100, 0, 21);
  audio.setVolume(vol);
}