// STM32 UART regressions, controlled over native USB so UART traffic is independent.
#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "periph/uart/uart_hal.h"
#include "periph/usb_cdc/usb_cdc.h"

static UartState_t uart;
static usbd_cdc_state_t usb;
static char tx[2048];
static unsigned rx_count;
static unsigned rx_calls;
static unsigned char observed[512];
static bool delay_next_rx;
static unsigned masks_done;
static unsigned masked_flags;
static char raw_buffer[64];
static unsigned raw_calls;
static unsigned raw_primask;

static void spin_us(unsigned us) {
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000);
  while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
  }
}

static void mask_task(void *data) {
  rulos_irq_state_t irq = hal_start_atomic();
  spin_us(100000);
  // This fixture allocates UART TX first and RX second, with no other DMA users.
#if defined(RULOS_ARM_stm32h5)
  masked_flags = LL_DMA_IsActiveFlag_TC(GPDMA1, LL_DMA_CHANNEL_1) |
                 (LL_DMA_IsActiveFlag_HT(GPDMA1, LL_DMA_CHANNEL_1) << 1);
#else
  masked_flags = LL_DMA_IsActiveFlag_TC2(DMA1) | (LL_DMA_IsActiveFlag_HT2(DMA1) << 1);
#endif
  hal_end_atomic(irq);
  masks_done++;
}

static void raw_rx(uint8_t id, void *data, char *buf, size_t len) {
  raw_calls++;
  raw_primask = __get_PRIMASK();
}

static void uart_rx(UartState_t *u, void *data, char *buf, size_t len) {
  if (delay_next_rx) {
    delay_next_rx = false;
    spin_us(50000);
  }
  if (rx_count + len <= sizeof(observed)) {
    memcpy(observed + rx_count, buf, len);
  }
  rx_count += len;
  rx_calls++;
}

static void usb_rx(usbd_cdc_state_t *cdc, void *data, const uint8_t *buf, uint32_t len) {
  if (!len || !usbd_cdc_tx_ready(cdc)) {
    return;
  }
  unsigned out = 0;
  switch (buf[0]) {
    case 'I':
      out = snprintf(tx, sizeof(tx), "UART regression: %lu Hz\n", SystemCoreClock);
      break;
    case 'R':
      rx_count = rx_calls = masks_done = masked_flags = 0;
      memset(observed, 0, sizeof(observed));
      out = snprintf(tx, sizeof(tx), "RESET\n");
      break;
    case 'D':
      delay_next_rx = true;
      out = snprintf(tx, sizeof(tx), "DELAY\n");
      break;
    case 'M':
      schedule_us(20000, mask_task, NULL);
      out = snprintf(tx, sizeof(tx), "MASK\n");
      break;
    case 'E':
      LL_USART_DisableDMAReq_RX(USART1);
      out = snprintf(tx, sizeof(tx), "DMA PAUSED\n");
      break;
    case 'P':
      raw_calls = 0;
      hal_uart_start_rx(0, raw_rx, raw_buffer, sizeof(raw_buffer));
      out = snprintf(tx, sizeof(tx), "RAW\n");
      break;
    case 'A': {
      rulos_irq_state_t irq = hal_start_atomic();
      hal_uart_rx_cb_done(0);
      unsigned restored = __get_PRIMASK();
      hal_end_atomic(irq);
      out = snprintf(tx, sizeof(tx), "RAW %u MASK %u RESTORED %u\n", raw_calls, raw_primask,
                     restored);
      break;
    }
    case 'N':
      out = snprintf(tx, sizeof(tx), "RXNE %lu\n", LL_USART_IsActiveFlag_RXNE(USART1));
      LL_USART_ReceiveData8(USART1);
      LL_USART_EnableDMAReq_RX(USART1);
      break;
    case 'T':
      uart_write(&uart, "UART-TX-OK\n", 11);
      out = snprintf(tx, sizeof(tx), "TX\n");
      break;
    case 'Q':
      out = snprintf(tx, sizeof(tx), "RX %u CALLS %u MASKS %u FLAGS %u HEX ", rx_count, rx_calls,
                     masks_done, masked_flags);
      for (unsigned i = 0; i < r_min(rx_count, sizeof(observed)); i++) {
        out += snprintf(tx + out, sizeof(tx) - out, "%02x", observed[i]);
      }
      tx[out++] = '\n';
      break;
    default:
      out = snprintf(tx, sizeof(tx), "UNKNOWN\n");
  }
  usbd_cdc_write(cdc, tx, out);
}

int main(void) {
  rulos_hal_init();
  init_clock(1000, TIMER1);
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  uart_init(&uart, 0, 115200);
  uart_start_rx(&uart, uart_rx, NULL);
  usb.rx_cb = usb_rx;
  usbd_cdc_init(&usb);
  scheduler_run();
}
