/*
 * ed-overlay.c
 *
 *  Created on: Jul 5, 2024
 *      Author: colorbass
 */
#include <edcan_config.h>
#include <edcan.h>
#include "string.h"
#include <stdio.h>
#include "can.h"

uint8_t ED_OwnID;
uint8_t ED_SecondID = 0xFF;

//edcan_config.h
//#define BUFFER_SIZE 128

uint8_t can_error; // выставляется в HAL_CAN_ErrorCallback, разбирается в EDCAN_Loop()

uint8_t RxData[8] = {0,};
CAN_RxHeaderTypeDef RxHeader;
//EDCAN_frameId_t RxExtId;
EDCAN_RxFrame_t RxFrame;

uint32_t lastalivepackettime;

#define EDCAN_REG_SYS_STATUS 0x00

InfoBlock_t *InfoBlock = (InfoBlock_t *)(VERSION_OFFSET);

/* ------------------------------------------------------------------------------------------------
 * Vector base / memory remap helpers for OpenBLT integration
 * ------------------------------------------------------------------------------------------------
 * F0 (Cortex-M0): no VTOR, so OpenBLT copies the user vector table to SRAM (0x20000000) and remaps
 * SRAM to 0x00000000 before jumping to the app. When starting from debugger (no bootloader jump),
 * we do the same here so interrupts work with an app located at 0x08008000.
 *
 * F1 (Cortex-M3): VTOR exists, so we just point it to the application's vector table.
 */
static uint8_t EDCAN_VectorTableIsPlausible(const uint32_t *vt)
{
  const uint32_t sp = vt[0];
  const uint32_t reset = vt[1];
  return (uint8_t)((((sp & 0x2FFE0000u) == 0x20000000u) &&
                    ((reset & 0xFF000000u) == 0x08000000u)) ? 1u : 0u);
}

void EDCAN_VectorBaseConfigF0(void)
{
#if defined(SYSCFG_CFGR1_MEM_MODE) && defined(SYSCFG)
  extern const uint32_t g_pfnVectors[];
  enum { VECTOR_TABLE_SIZE_BYTES = 0xC0u };
  enum { VECTOR_TABLE_WORDS = (VECTOR_TABLE_SIZE_BYTES / 4u) };

  const uint32_t * const sramVectors = (const uint32_t *)0x20000000u;
  const uint32_t * const flashVectors = g_pfnVectors;

  if (EDCAN_VectorTableIsPlausible(sramVectors) == 0u)
  {
    if (EDCAN_VectorTableIsPlausible(flashVectors) == 0u)
    {
      Error_Handler();
      return;
    }
    volatile uint32_t *dst = (volatile uint32_t *)0x20000000u;
    for (uint32_t i = 0; i < (uint32_t)VECTOR_TABLE_WORDS; i++)
    {
      dst[i] = flashVectors[i];
    }
  }

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  MODIFY_REG(SYSCFG->CFGR1, SYSCFG_CFGR1_MEM_MODE,
             (SYSCFG_CFGR1_MEM_MODE_0 | SYSCFG_CFGR1_MEM_MODE_1)); /* 0b11: SRAM at 0x0 */
  __DSB();
  __ISB();
#else
  /* Not an STM32F0-style SYSCFG memory remap target. */
  (void)0;
#endif
}

void EDCAN_VectorBaseConfigF1(void)
{
#if defined(SCB_VTOR_TBLOFF_Msk)
  extern const uint32_t g_pfnVectors[];
  SCB->VTOR = (uint32_t)g_pfnVectors;
  __DSB();
  __ISB();
#else
  /* Cortex-M0 has no VTOR; use EDCAN_VectorBaseConfigF0() instead. */
  (void)0;
#endif
}

#ifdef ED_CANx


/**
  * @brief  CAN Interrupt Handler for EDCAN (CAN1)
  *
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
        EDCAN_ExtId_u extid;
        extid.raw = RxHeader.ExtId;
        RxFrame.ExtID = extid.f;
        RxFrame.DLC = RxHeader.DLC;
        memcpy(RxFrame.data, RxData, RxHeader.DLC);

    	if((RxFrame.ExtID.DestinationID == ED_OwnID) || (RxFrame.ExtID.DestinationID == 0xFF) || (RxFrame.ExtID.DestinationID == ED_SecondID)){
    		//Мгновенная перезагрузка (только Write)
#ifndef EDCAN_RESET_REG
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == 0x26) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#else		//Custom reset register
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == EDCAN_RESET_REG) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#endif
    		EDCAN_RxBufferAdd (&RxFrame);
//			EDCAN_ExchangeRxBuffer();
    	}
    }
}
#endif

#ifdef ED_CAN1


/**
  * @brief  CAN Interrupt Handler for EDCAN (CAN1)
  *
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
        EDCAN_ExtId_u extid;
        extid.raw = RxHeader.ExtId;
        RxFrame.ExtID = extid.f;
        RxFrame.DLC = RxHeader.DLC;
        memcpy(RxFrame.data, RxData, RxHeader.DLC);

    	if((RxFrame.ExtID.DestinationID == ED_OwnID) || (RxFrame.ExtID.DestinationID == 0xFF) || (RxFrame.ExtID.DestinationID == ED_SecondID)){
    		//Мгновенная перезагрузка (только Write)
#ifndef EDCAN_RESET_REG
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == 0x26) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#else		//Custom reset register
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == EDCAN_RESET_REG) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#endif
    		EDCAN_RxBufferAdd (&RxFrame);
//			EDCAN_ExchangeRxBuffer();
    	}
    }
}
#endif

#ifdef ED_CAN2

/**
  * @brief  CAN Interrupt Handler for EDCAN (CAN2)
  *
  */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan){
    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &RxHeader, RxData) == HAL_OK)
    {
        EDCAN_ExtId_u extid;
        extid.raw = RxHeader.ExtId;
        RxFrame.ExtID = extid.f;
        RxFrame.DLC = RxHeader.DLC;
        memcpy(RxFrame.data, RxData, RxHeader.DLC);

    	if((RxFrame.ExtID.DestinationID == ED_OwnID) || (RxFrame.ExtID.DestinationID == 0xFF) || (RxFrame.ExtID.DestinationID == ED_SecondID)){
    		//Мгновенная перезагрузка (только Write)
#ifndef EDCAN_RESET_REG
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == 0x26) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#else		//Custom reset register
    		if((RxFrame.ExtID.PacketType == ED_WRITE) && (RxFrame.ExtID.RegisterAddress == EDCAN_RESET_REG) && (RxFrame.DLC >= 1)){
    			if(RxFrame.data[0] == 0x66) NVIC_SystemReset();
    		}
#endif
    		EDCAN_RxBufferAdd (&RxFrame);
//			EDCAN_ExchangeRxBuffer();
    	}
    }
}
#endif

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan_){
	if (hcan_->Instance == ED_CAN_INSTANCE.Instance){
		EDCAN_ExchangeTxBuffer();
	}
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan_){
	if (hcan_->Instance == ED_CAN_INSTANCE.Instance){
		EDCAN_ExchangeTxBuffer();
	}
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan_){
	if (hcan_->Instance == ED_CAN_INSTANCE.Instance){
		EDCAN_ExchangeTxBuffer();
	}
}

/**
  * @brief  EDCAN Initialization function
  *
  * @param  _OwnID: EDCAN Device ID
  */
void EDCAN_Init(uint8_t _OwnID){
	ED_OwnID = _OwnID;

	EDCAN_DeviceInfo.device_id = DEVICE_ID;
	EDCAN_DeviceInfo.fwver_major = FWVER_MAJOR;
	EDCAN_DeviceInfo.fwver_minor = FWVER_MINOR;
	EDCAN_DeviceInfo.fwver_patch = FWVER_PATCH;
	EDCAN_DeviceInfo.board_type = InfoBlock->boardVersion;
	EDCAN_DeviceInfo.station_type = InfoBlock->stationType;
	EDCAN_DeviceInfo.rsvd = 0;
	EDCAN_DeviceInfo.serial_number = InfoBlock->serialNumber;
	EDCAN_DeviceInfo.uptime = 0;
};

/**
  * @brief  Get board version from device info structure in flash
  *
  * @retval Board version (boardVersion field from InfoBlock_t structure)
  */
uint8_t EDCAN_GetBoardVersion(void)
{
	return (uint8_t)(InfoBlock->boardVersion);
}

/**
  * @brief  EDCAN Set second ID (for receiving data)
  *
  * @param  _SecondID: EDCAN Second ID
  */
void EDCAN_SetSecondID(uint8_t _SecondID){
	ED_SecondID = _SecondID;
};


/**
  * @brief  CAN Reinitialization function
  *
  *
  */
void CAN_ReInit(){

	HAL_CAN_Stop(&ED_CAN_INSTANCE);

#ifdef ED_CANx
	MX_CAN_Init();
#endif

#ifdef ED_CAN1
	MX_CAN1_Init();
#endif

#ifdef ED_CAN2
	MX_CAN2_Init();
#endif

	EDCAN_FilterInit();
	HAL_CAN_Start(&ED_CAN_INSTANCE);

#ifdef ED_CANx
	HAL_CAN_ActivateNotification(&ED_CAN_INSTANCE, CAN_IT_RX_FIFO0_MSG_PENDING | /*CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE |*/ CAN_IT_TX_MAILBOX_EMPTY);
#endif

#ifdef ED_CAN1
	HAL_CAN_ActivateNotification(&ED_CAN_INSTANCE, CAN_IT_RX_FIFO0_MSG_PENDING | /*CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE |*/ CAN_IT_TX_MAILBOX_EMPTY);
#endif

#ifdef ED_CAN2
	HAL_CAN_ActivateNotification(&ED_CAN_INSTANCE, CAN_IT_RX_FIFO1_MSG_PENDING | /*CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE |*/ CAN_IT_TX_MAILBOX_EMPTY);
#endif

}

/**
  * @brief  EDCAN Initialization function
  *
  * @param  _OwnID: EDCAN Device ID
  *
  * @retval HAL status
  */
void EDCAN_FilterInit(){
	// Обязательно обнулять: SlaveStartFilterBank ниже присваивается только под
	// #ifdef ED_CAN2, а HAL на STM32F1 с двумя CAN пишет это поле в CAN1->FMR
	// (CAN2SB) БЕЗУСЛОВНО, при любом инстансе (stm32f1xx_hal_can.c: CLEAR_BIT/
	// SET_BIT по CAN_FMR_CAN2SB). Без инициализации туда уезжал мусор со стека и
	// границa банков фильтров между CAN1 и CAN2 съезжала - один из интерфейсов
	// оставался вообще без фильтров и переставал принимать.
	CAN_FilterTypeDef  sFilterConfig = {0};
	// 14 - значение по сбросу, оно же используется в CAN2-фильтрах проектов.
	sFilterConfig.SlaveStartFilterBank = 14;

	//Filter for Own ID

	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = (uint16_t)(ED_OwnID<<3)|0b100;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = (uint16_t)(0xFF<<3)|0b100;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;

#ifdef ED_CAN2
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO1;
	sFilterConfig.SlaveStartFilterBank = 14;
	sFilterConfig.FilterBank = 14;
#endif

	if(HAL_CAN_ConfigFilter(&ED_CAN_INSTANCE, &sFilterConfig) != HAL_OK){
		Error_Handler();
	}

	// Filter for broadcast ID

	sFilterConfig.FilterBank = 1;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = (uint16_t)(0xFF<<3)|0b100;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = (uint16_t)(0xFF<<3)|0b100;

#ifdef ED_CAN2
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO1;
	sFilterConfig.SlaveStartFilterBank = 14;
	sFilterConfig.FilterBank = 15;
#endif

	if(HAL_CAN_ConfigFilter(&ED_CAN_INSTANCE, &sFilterConfig) != HAL_OK)
	{
	    Error_Handler();
	}

	// Filter for second ID
	if(ED_SecondID != 0xFF){

		sFilterConfig.FilterBank = 2;
		sFilterConfig.FilterIdHigh = 0x0000;
		sFilterConfig.FilterIdLow = (uint16_t)(ED_SecondID<<3)|0b100;
		sFilterConfig.FilterMaskIdHigh = 0x0000;
		sFilterConfig.FilterMaskIdLow = (uint16_t)(0xFF<<3)|0b100;

	#ifdef ED_CAN2
		sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO1;
		sFilterConfig.SlaveStartFilterBank = 14;
		sFilterConfig.FilterBank = 16;
	#endif

		if(HAL_CAN_ConfigFilter(&ED_CAN_INSTANCE, &sFilterConfig) != HAL_OK)
		{
			Error_Handler();
		}
	}

}

/**
  * @brief  CAN Error interrupt callback
  */

//На данный момент функция не используется

//void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
//{
//	//TODO: Check error flags
//	/* Set can_error flag */
//	//if (HAL_CAN_GetError(hcan) == HAL_CAN_ERROR_ACK)
//    can_error=1;
//}

/**
  * @brief  EDCAN Send write packet function
  * 		Write registers of another device
  *
  * @param  DestinationID: Packet Destination ID
  * @param	RegAddr: First register address in sequence
  * @param	*data: 	pointer to data array to be send
  * @param	len: 	length of data (1..8)
  */
void EDCAN_SendPacketWrite(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len){
	EDCAN_TxFrame_t tx_frame;
	EDCAN_ExtId_u ExtID = {0};
	//CAN_TxHeaderTypeDef tx_header;
	//uint32_t tx_mailbox;

	ExtID.f.DestinationID = DestinationID;
    ExtID.f.SourceID = ED_OwnID;
    ExtID.f.RegisterAddress = RegAddr;
    ExtID.f.PacketType = ED_WRITE;

    tx_frame.tx_header.ExtId = ExtID.raw;

    tx_frame.tx_header.RTR = CAN_RTR_DATA;
    tx_frame.tx_header.IDE = CAN_ID_EXT;
    tx_frame.tx_header.DLC = len;

    memcpy(&tx_frame.data, data, len);

    //Добавление пакета в буфер
	EDCAN_TxBufferAdd(&tx_frame);

	//Также, попытаемся сразу перенести пакет в CAN (если там есть свободное место)
	//Если свободного места нету, то пакет перенесется в CAN позже по прерыванию освобождения буфера
	EDCAN_ExchangeTxBuffer();


}

/**
 * @brief  Send large data by splitting it into smaller packets
 *
 * @param  DestinationID: Packet Destination ID
 * @param RegAddr: First register address in sequence
 * @param *data:  pointer to data array to be send
 * @param len:  length of data to be sent
 */
void EDCAN_SendPacketWriteLong(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint16_t len) {
    uint16_t remainingBytes = len;
    uint16_t currentRegAddr = RegAddr;
    const uint8_t *currentDataPtr = data;

    while (remainingBytes > 0) {
        uint8_t packetSize = (remainingBytes > 8) ? 8 : remainingBytes;
        EDCAN_SendPacketWrite(DestinationID, currentRegAddr, currentDataPtr, packetSize);

        remainingBytes -= packetSize;
        currentRegAddr += packetSize; // Assuming the register address increments by the number of bytes sent
        currentDataPtr += packetSize;
    }
}

/**
  * @brief  EDCAN Send read packet function
  * 		Send registers value of this device
  *
  * @param  DestinationID: Packet Destination ID
  * @param	RegAddr: First register address in sequence
  * @param	*data: 	pointer to data array to be send
  * @param	len: 	length of data (1..8)
  */
void EDCAN_SendPacketRead(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len){
	EDCAN_TxFrame_t tx_frame;
	EDCAN_ExtId_u ExtID = {0};
	//CAN_TxHeaderTypeDef tx_header;
	//uint32_t tx_mailbox;

	ExtID.f.DestinationID = DestinationID;
    ExtID.f.SourceID = ED_OwnID;
    ExtID.f.RegisterAddress = RegAddr;
    ExtID.f.PacketType = ED_READ;

    tx_frame.tx_header.ExtId = ExtID.raw;

    tx_frame.tx_header.RTR = CAN_RTR_DATA;
    tx_frame.tx_header.IDE = CAN_ID_EXT;
    tx_frame.tx_header.DLC = len;

    memcpy(&tx_frame.data, data, len);

    //Добавление пакета в буфер
	EDCAN_TxBufferAdd(&tx_frame);

	//Также, попытаемся сразу перенести пакет в CAN (если там есть свободное место)
	//Если свободного места нету, то пакет перенесется в CAN позже по прерыванию освобождения буфера
	EDCAN_ExchangeTxBuffer();

}

/**
  * @brief  EDCAN Send read request packet function
  * 		Request another device to send its register values
  *
  * @param  DestinationID: Packet Destination ID
  * @param	RegAddr: First register address in sequence
  * @param	len: 	length of data (1..8)
  */
void EDCAN_SendPacketReadRequest(uint8_t DestinationID, uint16_t RegAddr, uint8_t len){
	EDCAN_TxFrame_t tx_frame;
	EDCAN_ExtId_u ExtID = {0};
	//CAN_TxHeaderTypeDef tx_header;
	//uint32_t tx_mailbox;

	ExtID.f.DestinationID = DestinationID;
    ExtID.f.SourceID = ED_OwnID;
    ExtID.f.RegisterAddress = RegAddr;
    ExtID.f.PacketType = ED_READREQ;

    tx_frame.tx_header.ExtId = ExtID.raw;

    tx_frame.tx_header.RTR = CAN_RTR_DATA;
    tx_frame.tx_header.IDE = CAN_ID_EXT;
    tx_frame.tx_header.DLC = 1;
	tx_frame.data[0] = len;

    //Добавление пакета в буфер
	EDCAN_TxBufferAdd(&tx_frame);

	//Также, попытаемся сразу перенести пакет в CAN (если там есть свободное место)
	//Если свободного места нету, то пакет перенесется в CAN позже по прерыванию освобождения буфера
	EDCAN_ExchangeTxBuffer();

}

/**
  * @brief  EDCAN loop function
  * 		Функция для управления буферами, должна быть в while(1)
  *
  */
void EDCAN_Loop(){
	  // Без этого при любой ошибке CAN (в т.ч. bus-off, например от помехи при
	  // программировании через ST-Link) шина EDCAN на этом узле замолкала
	  // навсегда - HAL_CAN_ErrorCallback только взводил can_error, а восстановить
	  // периферию было некому.
	  if(can_error){
		  CAN_ReInit();
		  can_error=0;
	  }
	  //exchange TX buffer
		  if(EDCAN_getTxBufferElementCount()>0){
			  EDCAN_ExchangeTxBuffer();
		  }
	  //every 1s alive packet
	  if ((HAL_GetTick() - lastalivepackettime) > 1000){
		  lastalivepackettime = HAL_GetTick();
		  EDCAN_SendAlivePacket();
	  }
	  //exchange RX buffer (avoid starving RX when TX is heavily loaded)
		  if((EDCAN_getRxBufferElementCount()>0)&&(EDCAN_getTxBufferElementCount()<(BUFFER_SIZE*3/4))){
			  EDCAN_ExchangeRxBuffer();
		  }
}

void EDCAN_SendAlivePacket(){
	  uint8_t data[1];
	  uint8_t DestinationID = 0x00;
	  data[0] = EDCAN_GetOwnRegisterValue(EDCAN_REG_SYS_STATUS);
	  EDCAN_SendPacketRead(DestinationID, EDCAN_REG_SYS_STATUS, data, 1);
}
