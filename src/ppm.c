// PPM:
//   _      _          
// _| |____| |__| .....
//  |<-1-> |<-2->  ...        sync            
// значение канала - время между фронтами импульсов
// Высокий уровень (логическая 1) – короткий импульс (PPM_PULSE_WIDTH, обычно 300–500 мкс).
// Низкий уровень (логический 0) – пауза, определяющая длительность канала (например, 1000–2000 мкс).
// В конце низкий уровень, чтобы длина пакет стала 22500мкс

#include "ppm.h"
#include "defines.h"
//#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "frskybt.h"
#include <stdbool.h>
#include "esp_log.h"
//#include "bt_client.h"
#include <esp_timer.h>
#include "driver/rmt_tx.h"

uint16_t* channels;

//rmt
rmt_channel_handle_t tx_chan = NULL;
rmt_encoder_handle_t ppm_encoder = NULL;
bool rmt_on_duty = false;
int64_t last_frame_time = 0;


// Колбэк для события завершения передачи
static bool on_rmt_transmit_done(rmt_channel_handle_t chan, const rmt_tx_done_event_data_t *edata, void *user_data) {
    rmt_on_duty = false;
    return false; // Не требуется повторный вызов
}

void setupRMTChannel() {
    rmt_tx_channel_config_t tx_chan_cfg = {
        .gpio_num = PPM_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,  // 1 МГц = 1 мкс/такт
        .mem_block_symbols = 64,
        .trans_queue_depth = 4
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_cfg, &tx_chan));

    rmt_tx_event_callbacks_t cbs = {
        .on_trans_done = on_rmt_transmit_done, // Указываем колбэк
    };
    ESP_ERROR_CHECK(rmt_tx_register_event_callbacks(tx_chan, &cbs, NULL));

    // Включение канала
    ESP_ERROR_CHECK(rmt_enable(tx_chan));
}


//--------------------------------------------------
// Настройка энкодера (без энкодера)
//--------------------------------------------------
void setupRMTEncoder() {
    rmt_copy_encoder_config_t copy_encoder_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_cfg, &ppm_encoder));
}

void generatePPM() {
    rmt_on_duty = true;
    last_frame_time = esp_timer_get_time();
    //ESP_LOGI("PPM", "at %lld us [%04d]", esp_timer_get_time(),channels[0]);
    rmt_symbol_word_t symbols[PPM_CHANNELS] = {0};
    uint32_t total_time = 0;

    // Заполнение импульсов и пауз
    for (int i = 0; i < PPM_CHANNELS; i++) {
        // Импульс (высокий уровень)
        symbols[i].level0 = 1;
        symbols[i].duration0 = PPM_PULSE_WIDTH;
        // Пауза (низкий уровень)
        symbols[i].level1 = 0;
        symbols[i].duration1 = channels[i] - PPM_PULSE_WIDTH;
        //symbols[i].duration1 = 1000 - PPM_PULSE_WIDTH;
        total_time += channels[i]+PPM_PULSE_WIDTH;
        //total_time += 1500;
    }

    // Синхроимпульс (конец кадра)
    symbols[PPM_CHANNELS-1].duration1 += PPM_FRAME - total_time;

    // Отправка данных
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0, // Без зацикливания
    };
    ESP_ERROR_CHECK(rmt_transmit(tx_chan, ppm_encoder, symbols, sizeof(symbols), &tx_cfg));
}


//---------------------------------------------------------------------
// Задача PPM (высокий приоритет)
//---------------------------------------------------------------------
void ppmTask(void *pvParameters) {
    gpio_set_direction(PPM_PIN, GPIO_MODE_DEF_OUTPUT);
    gpio_set_level(PPM_PIN, 0);
    setupRMTChannel();
    setupRMTEncoder();
    while (1) {
        channels = getChannels();
        generatePPM();
        while ( !rmt_on_duty && (esp_timer_get_time() - last_frame_time >= PPM_FRAME) ) {
            taskYIELD();  // Даём время другим задачам
        }
    }

}