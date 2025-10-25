#include <stdint.h>
#include <stddef.h>
#include "core/hardware.h"

void *USBD_malloc(uint32_t size);
void USBD_free(void *ptr);

/*---------- -----------*/
#define USBD_MAX_NUM_INTERFACES     1U
/*---------- -----------*/
#define USBD_MAX_NUM_CONFIGURATION     1U
/*---------- -----------*/
#define USBD_MAX_STR_DESC_SIZ     100U
/*---------- -----------*/
#define USBD_DEBUG_LEVEL     0U
/*---------- -----------*/
#define USBD_LPM_ENABLED     0U
/*---------- -----------*/
#define USBD_SELF_POWERED     1U

