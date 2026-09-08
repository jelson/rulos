#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int uart_port_t;
typedef unsigned TickType_t;
typedef void *QueueHandle_t;
typedef struct {
  int type;
} uart_event_t;
typedef struct {
  int baud_rate;
  int data_bits;
  int parity;
  int stop_bits;
  int flow_ctrl;
  int source_clk;
} uart_config_t;

enum {
  UART_NUM_0,
  UART_NUM_1,
  UART_NUM_2,
  UART_DATA_8_BITS,
  UART_PARITY_DISABLE,
  UART_STOP_BITS_1,
  UART_HW_FLOWCTRL_DISABLE,
  UART_SCLK_APB,
  UART_PIN_NO_CHANGE,
  portTICK_PERIOD_MS = 1,
  configMAX_PRIORITIES = 16,
};
#define portMAX_DELAY         UINT32_MAX
#define ESP_ERROR_CHECK(expr) assert((expr) == 0)

int uart_driver_install(uart_port_t port, int rx_len, int tx_len, int queue_len,
                        QueueHandle_t *queue, int flags);
int uart_param_config(uart_port_t port, const uart_config_t *config);
int uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts);
int uart_write_bytes(uart_port_t port, const void *buf, size_t len);
int uart_get_buffered_data_len(uart_port_t port, size_t *len);
int uart_read_bytes(uart_port_t port, void *buf, uint32_t len, TickType_t wait);
void vTaskDelay(TickType_t ticks);
int xQueueReceive(QueueHandle_t queue, void *event, TickType_t wait);
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack_size, void *arg,
                unsigned priority, void *handle);
