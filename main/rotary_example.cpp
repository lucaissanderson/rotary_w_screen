#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <driver/gpio.h>
#include <stdlib.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "DFRobot_LCD.h"


static const char *TAG = "rotary";

#define S1_GPIO 5
#define S2_GPIO 6
#define KEY_GPIO 7
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

/* queues gpio when edge detected */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

volatile int16_t position = 0;
volatile int8_t direction = 0;

int16_t prev_position = 0;
int8_t prev_direction = 0;

volatile int S1_prev;

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    int S1_level, S2_level;
    for (;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)){
            switch (io_num) {
                case S1_GPIO:
                    S1_level = gpio_get_level(static_cast<gpio_num_t>(io_num));
                    S2_level = gpio_get_level(static_cast<gpio_num_t>(S2_GPIO));
                    if(S1_level != S1_prev && !S1_level){
                        if(S2_level){ // CW
                            direction = 1;
                            position = position + 1;
                        } else { // CCW
                            direction = 0;
                            position = position - 1;
                        }
                    }
                    S1_prev = S1_level;
                    break;
                case KEY_GPIO:
                    if(!gpio_get_level(static_cast<gpio_num_t>(io_num))) ESP_LOGI(TAG, "Button press!");
                    break;
                default:
                    ESP_LOGI(TAG, "Invalid GPIO dequeued");
            }
        }
    }
}


void print_task(void* arg) {
    /* instantiate LCD object */
    DFRobot_LCD lcd(16,2);
    lcd.init();
    while(1) {
        if(position != prev_position || direction != prev_direction){
            ESP_LOGI(TAG, "Direction:%s, Position %d", direction == 1 ? "Clockwise" : "Counter-Clockwise", position);
            /* print to screen, too */
            lcd.clear();
            lcd.setCursor(0,0);
            char disp_txt[50];
            snprintf(disp_txt,sizeof(disp_txt),"Pos: %d Dir: %s", static_cast<int>(position), (direction == 1 ? "CW" : "CCW"));
            lcd.printstr(disp_txt);
            // update prev states for next cycle
            prev_position = position;
	        prev_direction = direction;
	    }
	    vTaskDelay(pdMS_TO_TICKS(10));
    }
}


extern "C" void app_main(void)
{

    // configure S1
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << S1_GPIO) | (1ULL << S2_GPIO);
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(0);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(1);
    gpio_config(&io_conf);

    // configure KEY
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << KEY_GPIO);
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(0);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(1);
    gpio_config(&io_conf);

    S1_prev = gpio_get_level(static_cast<gpio_num_t>(S1_GPIO));

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 5, NULL);
    xTaskCreate(print_task, "print_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(static_cast<gpio_num_t>(S1_GPIO), gpio_isr_handler, (void*)S1_GPIO);
    gpio_isr_handler_add(static_cast<gpio_num_t>(KEY_GPIO), gpio_isr_handler, (void*)KEY_GPIO);


}
