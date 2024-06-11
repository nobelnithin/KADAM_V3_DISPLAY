#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "image.h"
#include <string.h>
#include "esp_timer.h"
#include "ssd1306_i2c_new.c"

#define TAG "SSD1306"
#define TAG "MAX17260"

#define CONFIG_SDA_GPIO 1
#define CONFIG_SCL_GPIO 2
#define CONFIG_RESET_GPIO 0

#define BTN_UP GPIO_NUM_14
#define BTN_DOWN GPIO_NUM_17
#define BTN_PWR GPIO_NUM_15
#define BTN_OK GPIO_NUM_16


SSD1306_t dev;
bool isDisp_menu;
bool isDisp_menu1=true;
bool isDisp_menu2=false;
bool isDisp_setup;
bool isDisp_walk;

xQueueHandle BTN_UPQueue;
xQueueHandle BTN_DOWNQueue;
xQueueHandle BTN_PWRQueue;
xQueueHandle BTN_OKQueue;
xQueueHandle DISPQueue;

uint32_t pre_time_up = 0;
uint32_t pre_time_down = 0;
uint32_t pre_time_pwr = 0;
uint32_t pre_time_ok = 0;
uint64_t pre_time = 0;
uint64_t intr_time = 0;
uint64_t curr_time = 0;
int freq_list_index = 0;
float soc =0;
int freq_list[11] = {0,1,2,3,4,5,6,7,8,9,10};


// void max_17260_init(void *params)
// {
//     uint16_t Soc = 0x00;

//         uint8_t data[4];
//         i2c_cmd_handle_t cmd;
//         cmd = i2c_cmd_link_create();
// 		ESP_ERROR_CHECK(i2c_master_start(cmd));
// 		ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (I2C_ADDRESS << 1) | I2C_MASTER_WRITE, 1));
// 		ESP_ERROR_CHECK(i2c_master_write_byte(cmd, 0x06, 4));
// 		ESP_ERROR_CHECK(i2c_master_stop(cmd));
// 		ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_PERIOD_MS));
// 		i2c_cmd_link_delete(cmd);
//         while(1)
//         {
//             cmd = i2c_cmd_link_create();
//             ESP_ERROR_CHECK(i2c_master_start(cmd));
//             ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (I2C_ADDRESS << 1) | I2C_MASTER_READ, 1));
// 		    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data,   0));
// 		    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data+1, 0));
//             ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data+2,   0));
// 		    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data+3, 1));
//             ESP_ERROR_CHECK(i2c_master_stop(cmd));
//             ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS));
//             i2c_cmd_link_delete(cmd);
    
//             Soc = (data[2]<<8) | data[3];
            
//         //    printf("charge: %u\n",Soc);
//             printf("%u \n",data[1]);
//             vTaskDelay(1000/portTICK_PERIOD_MS);
//         }

// }

void display_logo()
{
    ssd1306_init(&dev, 128, 64);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ssd1306_bitmaps(&dev, 0, 0, biostim_logo, 128, 64, false);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ssd1306_clear_screen(&dev, false);
}

void disp_menu()
{
    isDisp_setup = false;
    isDisp_menu=true;
    char str[4];
    char display_str[20];
    int frequency = freq_list_index;
    snprintf(str,sizeof(str),"%d",frequency);
    if(soc>75.0)
    {
        ssd1306_bitmaps(&dev, 100, 5, battery_100, 32, 17, false);        
    }
    else if(soc<75.0&&soc>50.0)
    {
        ssd1306_bitmaps(&dev, 100, 5, battery_75, 32, 17, false);
    }
    else if(soc<50.0&&soc>25.0)
    {
        ssd1306_bitmaps(&dev, 100, 5, battery_50, 32, 17, false);
    }
        else if(soc<25.0)
    {
        ssd1306_bitmaps(&dev, 100, 5, battery_low, 32, 17, false);
    }
    snprintf(display_str,sizeof(display_str),"Setup         %s",str);
    if(isDisp_menu1)
    {
        
        ssd1306_display_text(&dev,3, display_str,strlen(display_str),true);
        ssd1306_display_text(&dev, 5, "Walk         ", 16, false);
    }
    if(isDisp_menu2)
    {
        
        ssd1306_display_text(&dev,3, display_str,strlen(display_str),false);
        ssd1306_display_text(&dev, 5, "Walk           ", 16, true);
    }

}

void disp_setup()
{
    isDisp_menu=false;
    isDisp_walk=false;
    isDisp_setup=true;
    
    
    //printf("freq %d\n",freq_list_index);
    if(freq_list_index>0)
    {
            ssd1306_bitmaps(&dev, 50, 15, flash, 32, 26, false);
            ssd1306_bitmaps(&dev, 106, 35,str_num[freq_list_index-1], 16,9,false);
    }
}

// void display(void *params)
// {
//     while(1)
//     {
//         if(isDisp_menu)
//         {
//             printf("menu func called\n");
//             disp_menu();
//         }
//         if(isDisp_setup)
//         {
//             printf("set func called\n");
//             disp_setup();
//         }
//     vTaskDelay(10/portTICK_PERIOD_MS);
//     }

// }


void BTN_UPTask(void *param)
{
    gpio_set_direction(BTN_UP, GPIO_MODE_INPUT);
    gpio_set_intr_type(BTN_UP, GPIO_INTR_NEGEDGE);
    int BTN_NUMBER = 0;
    while (1)
    {
        if (xQueueReceive(BTN_UPQueue, &BTN_NUMBER, portMAX_DELAY))
        {
            printf("Button up pressed\n");
            if(isDisp_menu)
            {
                printf("disp menu 1 true\n");
                isDisp_menu1=true;
                isDisp_menu2=false;
                disp_menu();

            }
            if(isDisp_setup)
            {
                
                if(freq_list_index<8)
                {
                    freq_list_index++;
                }
                disp_setup();
            }

            
            xQueueReset(BTN_UPQueue);
        }
    }
}

void BTN_DOWNTask(void *params)
{
    gpio_set_direction(BTN_DOWN, GPIO_MODE_INPUT);
    gpio_set_intr_type(BTN_DOWN, GPIO_INTR_NEGEDGE);
    int BTN_NUMBER = 0;
    while (1)
    {

        if (xQueueReceive(BTN_DOWNQueue, &BTN_NUMBER, portMAX_DELAY))
        {
            printf("Down_button_works\n");
            if(isDisp_menu)
            {
                isDisp_menu1=false;
                isDisp_menu2=true;
            }
            
            xQueueReset(BTN_DOWNQueue);
        }
    }
}

void BTN_PWRTask(void *params)
{
    gpio_set_direction(BTN_OK, GPIO_MODE_INPUT);
    gpio_set_intr_type(BTN_OK, GPIO_INTR_NEGEDGE);
    int BTN_NUMBER = 0;
    while (1)
    {
        if (xQueueReceive(BTN_PWRQueue, &BTN_NUMBER, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Task BTN_PWRTask: Button power pressed (15)!");
            
            xQueueReset(BTN_PWRQueue);
        }
    }
}

void BTN_OKTask(void *params)
{
    gpio_set_direction(BTN_PWR, GPIO_MODE_INPUT);
    gpio_set_intr_type(BTN_PWR, GPIO_INTR_NEGEDGE);
    int BTN_NUMBER = 0;
    while (1)
    {
        if (xQueueReceive(BTN_OKQueue, &BTN_NUMBER, portMAX_DELAY))
        {
            if(isDisp_menu)
            {
                if(isDisp_menu1)
                {
                    printf("disp setup true\n");
                    isDisp_setup=true;
                    isDisp_menu = false;
                    ssd1306_clear_screen(&dev, false);
                    ssd1306_bitmaps(&dev, 0, 0, setup_1, 128, 64, false);
                    disp_setup();
                }
                if(isDisp_menu2)
                {
                    printf("disp walk true\n");
                    isDisp_walk=true;
                }
            }
            else if(isDisp_setup)
            {

                ssd1306_clear_screen(&dev, false);
                disp_menu();
            }

            
            xQueueReset(BTN_OKQueue);
        }
    }
}

void get_soc(void *params)
{
while(1)
{
    soc = max17260_read_soc();
    if (soc >= 0) 
    {
        ESP_LOGI(TAG, "State of Charge: %.2f%%", soc);

    }
    vTaskDelay(5000/portTICK_PERIOD_MS);

}
}
static void IRAM_ATTR BTN_UP_interrupt_handler(void *args)
{
    
    int pinNumber = (int)args;
    if(esp_timer_get_time() - pre_time_up > 500000)
    {
        xQueueSendFromISR(BTN_UPQueue, &pinNumber, NULL);

    }
    pre_time_up= esp_timer_get_time();
}

static void IRAM_ATTR BTN_DOWN_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    if(esp_timer_get_time() - pre_time_down > 500000)
    {
        xQueueSendFromISR(BTN_DOWNQueue, &pinNumber, NULL);
    }
    pre_time_down = esp_timer_get_time();
}

static void IRAM_ATTR BTN_PWR_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    if(esp_timer_get_time() - pre_time_pwr > 500000){
    xQueueSendFromISR(BTN_PWRQueue, &pinNumber, NULL);
    }
    pre_time_pwr = esp_timer_get_time();
}

static void IRAM_ATTR BTN_OK_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    if(esp_timer_get_time() - pre_time_ok > 500000){
    xQueueSendFromISR(BTN_OKQueue, &pinNumber, NULL);
    intr_time = esp_timer_get_time();
    }
    pre_time_ok = esp_timer_get_time();
}

void app_main(void)
{
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    display_logo();
    disp_menu();
    BTN_UPQueue = xQueueCreate(10, sizeof(int));
    BTN_DOWNQueue = xQueueCreate(10, sizeof(int));
    BTN_PWRQueue = xQueueCreate(10, sizeof(int));
    BTN_OKQueue = xQueueCreate(10, sizeof(int));
    DISPQueue = xQueueCreate(10, sizeof(int));

    if (BTN_UPQueue == NULL || BTN_DOWNQueue == NULL || BTN_PWRQueue == NULL || BTN_OKQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button queues");
        return;
    }

    // float soc = max17260_read_soc();
    // if (soc >= 0) 
    // {
    //     ESP_LOGI(TAG, "State of Charge: %.2f%%", soc);
    // }

    // gpio_set_direction(BTN_UP, GPIO_MODE_INPUT);
    // gpio_set_intr_type(BTN_UP, GPIO_INTR_NEGEDGE);
    // gpio_set_direction(BTN_DOWN, GPIO_MODE_INPUT);
    // gpio_set_intr_type(BTN_DOWN, GPIO_INTR_NEGEDGE);
    // gpio_set_direction(BTN_PWR, GPIO_MODE_INPUT);
    // gpio_set_intr_type(BTN_PWR, GPIO_INTR_NEGEDGE);
    // gpio_set_direction(BTN_OK, GPIO_MODE_INPUT);
    // gpio_set_intr_type(BTN_OK, GPIO_INTR_NEGEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_UP, BTN_UP_interrupt_handler, (void *)BTN_UP);
    gpio_isr_handler_add(BTN_DOWN, BTN_DOWN_interrupt_handler, (void *)BTN_DOWN);
    gpio_isr_handler_add(BTN_PWR, BTN_PWR_interrupt_handler, (void *)BTN_PWR);
    gpio_isr_handler_add(BTN_OK, BTN_OK_interrupt_handler, (void *)BTN_OK);

    xTaskCreate(BTN_UPTask, "BTN_UPTask", 2048, NULL, 1, NULL);
    xTaskCreate(BTN_DOWNTask, "BTN_DOWNTask", 2048, NULL, 1, NULL);
    xTaskCreate(BTN_PWRTask, "BTN_PWRTask", 2048, NULL, 1, NULL);
    xTaskCreate(BTN_OKTask, "BTN_OKTask", 2048, NULL, 1, NULL);
    xTaskCreate(get_soc, "get soc",2048, NULL, 1, NULL);
    // xTaskCreate(display,"display",2048,NULL,1,NULL);

    // i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    // display_logo();
    // disp_menu();
}
