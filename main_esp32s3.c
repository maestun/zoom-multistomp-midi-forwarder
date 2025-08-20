#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"

#define UART_PORT UART_NUM_1
#define UART_RX_PIN 44
#define UART_TX_PIN 43
#define BUF_SIZE 256

static const char *TAG = "MIDI_THRU";

void uart_midi_task(void *arg) {
    uint8_t data[3];
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, 3, pdMS_TO_TICKS(10));
        if (len > 0) {
            ESP_LOGI(TAG, "Received MIDI: %02X %02X %02X", data[0], data[1], data[2]);
            // TODO: Send to USB MIDI device


            
        }
    }
}

void usb_host_init_task(void *param) {
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    ESP_LOGI(TAG, "USB Host stack initialized");
    // You must create a USB MIDI class driver here

    // Wait forever
    vTaskDelay(portMAX_DELAY);
}

void app_main(void) {
    const uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_midi_task, "uart_midi_task", 2048, NULL, 10, NULL);
    xTaskCreate(usb_host_init_task, "usb_host", 4096, NULL, 10, NULL);
}
