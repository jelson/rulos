/* USER CODE BEGIN Header */
/**
******************************************************************************
  * @file           USB_Device/CDC_Standalone/USB_Device/Target/usbd_conf.c
  * @author         MCD Application Team
  * @brief          This file implements the board support package for the USB device library
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32.h"
#if defined(RULOS_ARM_stm32g4)
#include "stm32g4xx_hal.h"
#elif defined(RULOS_ARM_stm32h5)
#include "stm32h5xx_hal.h"
#else
#error "USB CDC not supported on this chip family"
#include <stophere>
#endif
#include "usbd_def.h"
#include "usbd_core.h"

#include "usbd_cdc.h"

#include "core/rulos.h"

/*
 * Family-specific USB peripheral name. On STM32G4 the USB FS device
 * controller is literally called `USB`; on STM32H5 it's the USB DRD
 * (Dual-Role Device) controller running in FS device mode, exposed
 * as `USB_DRD_FS`. The PCD HAL driver is otherwise identical across
 * the two families.
 */
#if defined(RULOS_ARM_stm32h5)
#define RULOS_USB_INSTANCE USB_DRD_FS
#define RULOS_USB_IRQn     USB_DRD_FS_IRQn
#define RULOS_USB_IRQHandler USB_DRD_FS_IRQHandler
#else
#define RULOS_USB_INSTANCE USB
#define RULOS_USB_IRQn     USB_LP_IRQn
#define RULOS_USB_IRQHandler USB_LP_IRQHandler
#endif

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

PCD_HandleTypeDef hpcd_USB_FS;

void Error_Handler(void) {
  // Hang here - trap will be caught by debugger
  __builtin_trap();
}

/**
  * @brief This function handles USB (device-mode FS) global interrupt.
  */
void RULOS_USB_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

// Stub - system clock already configured by rulos
void SystemClock_Config(void) {
}

// Stub - USB clock already configured in init_usb
void USBD_Clock_Config(void) {
}

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Exported function prototypes ----------------------------------------------*/

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* Private functions ---------------------------------------------------------*/
static USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status);
/* USER CODE BEGIN 1 */
static void SystemClockConfig_Resume(void);
extern void USBD_Clock_Config(void);
/* USER CODE END 1 */
extern void SystemClock_Config(void);

/*******************************************************************************
                       LL Driver Callbacks (PCD -> USB Device Library)
*******************************************************************************/
/* MSP Init */

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
  if(pcdHandle->Instance==RULOS_USB_INSTANCE)
  {
    /* Peripheral clock enable. The H5 DRD-FS and the G4 USB device
     * controller both bind __HAL_RCC_USB_CLK_ENABLE to the same
     * macro name. */
    __HAL_RCC_USB_CLK_ENABLE();

    /* Peripheral interrupt init */
    // Priority 6 is fine since SysTick is already at lowest priority (15)
    HAL_NVIC_SetPriority(RULOS_USB_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(RULOS_USB_IRQn);
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{
  if(pcdHandle->Instance==RULOS_USB_INSTANCE)
  {
    /* Peripheral clock disable */
    __HAL_RCC_USB_CLK_DISABLE();

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(RULOS_USB_IRQn);

    // Do NOT disable GPIOA here -- it's shared with UART, timers, SWD, etc.
  }
}

/**
  * @brief  Setup stage callback
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_SetupStageCallback_PreTreatment */

  /* USER CODE END  HAL_PCD_SetupStageCallback_PreTreatment */
  USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t *)hpcd->Setup);
  /* USER CODE BEGIN HAL_PCD_SetupStageCallback_PostTreatment */

  /* USER CODE END  HAL_PCD_SetupStageCallback_PostTreatment */
}

/**
  * @brief  Data Out stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint number
  * @retval None
  */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  /* USER CODE BEGIN HAL_PCD_DataOutStageCallback_PreTreatment */

  /* USER CODE END HAL_PCD_DataOutStageCallback_PreTreatment */
  USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
  /* USER CODE BEGIN HAL_PCD_DataOutStageCallback_PostTreatment */

  /* USER CODE END HAL_PCD_DataOutStageCallback_PostTreatment */
}

/**
  * @brief  Data In stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint number
  * @retval None
  */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  /* USER CODE BEGIN HAL_PCD_DataInStageCallback_PreTreatment */

  /* USER CODE END HAL_PCD_DataInStageCallback_PreTreatment */
  USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
  /* USER CODE BEGIN HAL_PCD_DataInStageCallback_PostTreatment  */

  /* USER CODE END HAL_PCD_DataInStageCallback_PostTreatment */
}

/**
  * @brief  SOF callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_SOFCallback_PreTreatment */

  /* USER CODE END HAL_PCD_SOFCallback_PreTreatment */
  USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData);
  /* USER CODE BEGIN HAL_PCD_SOFCallback_PostTreatment */

  /* USER CODE END HAL_PCD_SOFCallback_PostTreatment */
}

/**
  * @brief  Reset callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_ResetCallback_PreTreatment */

  /* USER CODE END HAL_PCD_ResetCallback_PreTreatment */
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  if ( hpcd->Init.speed != PCD_SPEED_FULL)
  {
    Error_Handler();
  }
    /* Set Speed. */
  USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);

  /* Reset Device. */
  USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
  /* USER CODE BEGIN HAL_PCD_ResetCallback_PostTreatment */

  /* USER CODE END HAL_PCD_ResetCallback_PostTreatment */
}

/**
  * @brief  Suspend callback.
  * When Low power mode is enabled the debug cannot be used (IAR, Keil doesn't support it)
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_SuspendCallback_PreTreatment */

  /* USER CODE END HAL_PCD_SuspendCallback_PreTreatment */
  /* Inform USB library that core enters in suspend Mode. */
  USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
  /* Enter in STOP mode. */
  /* USER CODE BEGIN 2 */
  if (hpcd->Init.low_power_enable)
  {
    /* Set SLEEPDEEP bit and SleepOnExit of Cortex System Control Register. */
    SCB->SCR |= (uint32_t)((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
  }
  /* USER CODE END 2 */
  /* USER CODE BEGIN HAL_PCD_SuspendCallback_PostTreatment */

  /* USER CODE END HAL_PCD_SuspendCallback_PostTreatment */
}

/**
  * @brief  Resume callback.
  * When Low power mode is enabled the debug cannot be used (IAR, Keil doesn't support it)
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_ResumeCallback_PreTreatment */

  /* USER CODE END HAL_PCD_ResumeCallback_PreTreatment */

  /* USER CODE BEGIN 3 */
  if (hpcd->Init.low_power_enable)
  {
    /* Reset SLEEPDEEP bit of Cortex System Control Register. */
    SCB->SCR &= (uint32_t)~((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
    SystemClockConfig_Resume();
  }
  /* USER CODE END 3 */

  USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData);
  /* USER CODE BEGIN HAL_PCD_ResumeCallback_PostTreatment */

  /* USER CODE END HAL_PCD_ResumeCallback_PostTreatment */
}

/**
  * @brief  ISOOUTIncomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint number
  * @retval None
  */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  /* USER CODE BEGIN HAL_PCD_ISOOUTIncompleteCallback_PreTreatment */

  /* USER CODE END HAL_PCD_ISOOUTIncompleteCallback_PreTreatment */
  USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
  /* USER CODE BEGIN HAL_PCD_ISOOUTIncompleteCallback_PostTreatment */

  /* USER CODE END HAL_PCD_ISOOUTIncompleteCallback_PostTreatment */
}

/**
  * @brief  ISOINIncomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint number
  * @retval None
  */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  /* USER CODE BEGIN HAL_PCD_ISOINIncompleteCallback_PreTreatment */

  /* USER CODE END HAL_PCD_ISOINIncompleteCallback_PreTreatment */
  USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
  /* USER CODE BEGIN HAL_PCD_ISOINIncompleteCallback_PostTreatment */

  /* USER CODE END HAL_PCD_ISOINIncompleteCallback_PostTreatment */
}

/**
  * @brief  Connect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_ConnectCallback_PreTreatment */

  /* USER CODE END HAL_PCD_ConnectCallback_PreTreatment */
  USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData);
  /* USER CODE BEGIN HAL_PCD_ConnectCallback_PostTreatment */

  /* USER CODE END HAL_PCD_ConnectCallback_PostTreatment */
}

/**
  * @brief  Disconnect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  /* USER CODE BEGIN HAL_PCD_DisconnectCallback_PreTreatment */

  /* USER CODE END HAL_PCD_DisconnectCallback_PreTreatment */
  USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData);
  /* USER CODE BEGIN HAL_PCD_DisconnectCallback_PostTreatment */

  /* USER CODE END HAL_PCD_DisconnectCallback_PostTreatment */
}

  /* USER CODE BEGIN LowLevelInterface */

  /* USER CODE END LowLevelInterface */

/*******************************************************************************
                       LL Driver Interface (USB Device Library --> PCD)
*******************************************************************************/

/**
  * @brief  Initializes the low level portion of the device driver.
  * @param  pdev: Device handle
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  /* Init USB Ip. */
  hpcd_USB_FS.pData = pdev;
  /* Link the driver to the stack. */
  pdev->pData = &hpcd_USB_FS;

  hpcd_USB_FS.Instance = RULOS_USB_INSTANCE;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;


  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN EndPoint_Configuration */
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x00 , PCD_SNG_BUF, 0x14);
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x80 , PCD_SNG_BUF, 0x54);
  /* USER CODE END EndPoint_Configuration */
  /* USER CODE BEGIN EndPoint_Configuration_CDC */
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_IN_EP, PCD_SNG_BUF, 0x94);
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_OUT_EP, PCD_SNG_BUF, 0xD4);
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_CMD_EP, PCD_SNG_BUF, 0x114);
  /* USER CODE END EndPoint_Configuration_CDC */
  return USBD_OK;
}

/**
  * @brief  De-Initializes the low level portion of the device driver.
  * @param  pdev: Device handle
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_DeInit(pdev->pData);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Starts the low level portion of the device driver.
  * @param  pdev: Device handle
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_Start(pdev->pData);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Stops the low level portion of the device driver.
  * @param  pdev: Device handle
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_Stop(pdev->pData);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Opens an endpoint of the low level driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @param  ep_type: Endpoint type
  * @param  ep_mps: Endpoint max packet size
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Closes an endpoint of the low level driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Close(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Flushes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Flush(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_SetStall(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Returns Stall condition.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval Stall (1: Yes, 0: No)
  */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef*) pdev->pData;

  if((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
  }
}

/**
  * @brief  Assigns a USB address to the device.
  * @param  pdev: Device handle
  * @param  dev_addr: Device address
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_SetAddress(pdev->pData, dev_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Transmits data over an endpoint.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @param  pbuf: Pointer to data to be sent
  * @param  size: Data size
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Prepares an endpoint for reception.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @param  pbuf: Pointer to data to be received
  * @param  size: Data size
  * @retval USBD status
  */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

/**
  * @brief  Returns the last transferred packet size.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint number
  * @retval Received Data Size
  */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef*) pdev->pData, ep_addr);
}

/**
  * @brief  Delays routine for the USB Device Library.
  * @param  Delay: Delay in ms
  * @retval None
  */
void USBD_LL_Delay(uint32_t Delay)
{
  // Use RULOS delay API (Delay is in ms, delay_us needs microseconds)
  delay_us(Delay * 1000);
}

/* USER CODE BEGIN 5 */
/**
  * @brief  Configures system clock after wake-up from USB resume callBack:
  *         enable HSI, PLL and select PLL as system clock source.
  * @retval None
  */
void SystemClockConfig_Resume(void)
{
  SystemClock_Config();
  USBD_Clock_Config();
}
/* USER CODE END 5 */

/**
  * @brief  Returns the USB status depending on the HAL status:
  * @param  hal_status: HAL status
  * @retval USB status
  */
USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status)
{
  USBD_StatusTypeDef usb_status = USBD_OK;

  switch (hal_status)
  {
    case HAL_OK :
      usb_status = USBD_OK;
    break;
    case HAL_ERROR :
      usb_status = USBD_FAIL;
    break;
    case HAL_BUSY :
      usb_status = USBD_BUSY;
    break;
    case HAL_TIMEOUT :
      usb_status = USBD_FAIL;
    break;
    default :
      usb_status = USBD_FAIL;
    break;
  }
  return usb_status;
}
