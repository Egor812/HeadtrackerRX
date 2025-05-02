#include "ppm.h"
#include "defines.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "frskybt.h"
#include <stdbool.h>
#include "esp_log.h"
#include "bt_client.h"


uint64_t tickCount; // счетчик таймера
volatile uint16_t alarmValue; // текущая установка таймера

volatile uint8_t channelIndex = 0; //номер передаваемого канала
volatile static uint16_t summChannelWidth = 0; //считаем суммарную длину по всем каналам
volatile static bool end = false; // передали 8 каналов
volatile bool pulseState = false;

//uint32_t last_receive = 0;


void setupPPMTimer();
bool IRAM_ATTR generatePPM(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *param);
void IRAM_ATTR generatePPMLogic();
void generateTest();
void IRAM_ATTR setMyAlarm(gptimer_handle_t timer);

void runPPM(void)
{
  gpio_set_direction(PPM_PIN, GPIO_MODE_DEF_OUTPUT);
  gpio_set_level(PPM_PIN, 0);
  setupPPMTimer();
}



void setMyAlarm(gptimer_handle_t timer)
{
  gptimer_get_raw_count(timer, &tickCount);
  gptimer_set_raw_count(timer, 0); // сбрасываем счетчик
  gptimer_alarm_config_t alarm_config = {
    .alarm_count = alarmValue, 
  };
  gptimer_set_alarm_action(timer, &alarm_config); // запускаем будильник
  //usb_serial_jtag_write_bytes("*", 1, 20 / portTICK_PERIOD_MS);
}

void setupPPMTimer()
{
   gptimer_handle_t gptimer = NULL;
    const gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000, // 1MHz
     //.intr_priority = 0,
      .flags = 0
      };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
      .on_alarm = generatePPM,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    gptimer_alarm_config_t alarm_config ={1000, 0, false}; //target count; reload count; disable autoreload
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}

bool generatePPM(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *param) {
  generatePPMLogic();
  setMyAlarm( timer );
  return 1;
}

void generateTest()
{

 if (pulseState)   {
    //usb_serial_jtag_write_bytes("-", 1, 20 / portTICK_PERIOD_MS);
    gpio_set_level(PPM_PIN, 1);
    alarmValue = 50000;
    pulseState = false; // следующий запуск - спад
  }  
  else{
    //usb_serial_jtag_write_bytes("+", 1, 20 / portTICK_PERIOD_MS);
    gpio_set_level(PPM_PIN, 0);
    alarmValue = 50000;
    pulseState = true; // следующий запуск - спад
  }
}

//функция обработчик прерывания
//
// PPM:
//   _      _           _______
// _| |____| |__| .....|       |_
//  |<-1-> |<-2->  ...        sync            
// значение канала - время между фронтами импульсов
// Высокий уровень (логическая 1) – короткий импульс (PPM_PULSE_WIDTH, обычно 300–500 мкс).
// Низкий уровень (логический 0) – пауза, определяющая длительность канала (например, 1000–2000 мкс).


void generatePPMLogic()
{
  uint16_t* channels; // = getChannels();

  if (pulseState) {
    // Высокий уровень (начало импульса)
    gpio_set_level(PPM_PIN, 1);
    alarmValue = PPM_PULSE_WIDTH;
    pulseState = false;
  } else {
      // Низкий уровень (пауза между каналами)
      gpio_set_level(PPM_PIN, 0);

      if (channelIndex >= 8) {
          // Синхроимпульс (конец кадра)
          alarmValue = PPM_FRAME - (summChannelWidth + 8 * PPM_PULSE_WIDTH);
          channelIndex = 0;
          summChannelWidth = 0;
      } else {
          // Пауза = длительность канала - PPM_PULSE_WIDTH
          channels = getChannels();
          alarmValue = channels[channelIndex] - PPM_PULSE_WIDTH;
          summChannelWidth += channels[channelIndex];
          channelIndex++;
      }
      pulseState = true;
  }
}