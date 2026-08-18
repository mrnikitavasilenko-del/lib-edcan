/*
 * edcan_handler.c
 *
 * В этом файле расположены обработчики приходящих пакетов, а также функция чтения
 *
 *  Created on: Jul 5, 2024
 *      Author: colorbass
 */

#include <edcan.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

EDCAN_DeviceInfo_t EDCAN_DeviceInfo;

#define EDCAN_REG_DEVICE_INFO 0x00

/* Optional RAM flag to request bootloader stay-open after reset.
 * Keep it configurable to avoid hardfaults on MCUs with smaller SRAM.
 */

#define EDCAN_BOOTLOADER_MAGIC_VALUE    (0xEDB00780u)

extern void NVIC_SystemReset(void);

/* System Registers */


/**
  * @brief  Handler for incoming Write packet
  * 		Another device wants to write registers of this device
  *
  * @param  SourceID: Packet Source ID
  * 		DestinationID: Packet Destination ID
  * 		Addr: First register address in sequence
  * 		*data: pointer for data array
  * 		len: length of data (1..255)
  */
void EDCAN_WriteHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t *data, uint8_t len){
	//Получили пакет Write (запрошенное значение регистров)
	for (uint16_t AddrOffset = 0; AddrOffset < len; AddrOffset++){ //по очереди перебираем все полученные регистры через Handler
		if((Addr+AddrOffset)>=256){
			EDCAN_WriteUserRegister(Addr+AddrOffset, data[AddrOffset]);
		}else{
			EDCAN_WriteSystemRegister(Addr+AddrOffset, data[AddrOffset]);
		}
	}
}

void EDCAN_EnterBootloader(void){
	*(volatile uint32_t *)(uintptr_t)EDCAN_BOOTLOADER_MAGIC_ADDR = (uint32_t)EDCAN_BOOTLOADER_MAGIC_VALUE;
	NVIC_SystemReset();
}

void EDCAN_WriteSystemRegister(uint16_t addr, uint8_t value){

	static uint8_t backdoorStage = 0;

	switch(addr){
	case 0x00: //EDCAN_REG_SYS_STATUS
		if(value == 0x10){
			if(EDCAN_DeviceInfo.status==0)EDCAN_DeviceInfo.status = 0x10;
		}
		break;

	case 0x80: //EDCAN_BOOTLOADER_BACKDOOR_REG0
		backdoorStage = value;
		break;

	case 0x81: //EDCAN_BOOTLOADER_BACKDOOR_REG1
		if ((backdoorStage == 0xFF) && (value == 0x00)) EDCAN_EnterBootloader();
		break;
	default:
		break;
	}

}

/**
  * @brief  Handler to get System register values (0..255)
  *
  * @param  addr: register address
  * @retval register value (uint8_t)
  */
uint8_t EDCAN_GetSystemRegisterValue(uint16_t addr){
	switch (addr){

	/* регистры 0..255 используются для Системных регистров*/

	case EDCAN_REG_DEVICE_INFO ... (EDCAN_REG_DEVICE_INFO+sizeof(EDCAN_DeviceInfo_t) - 1):
		if(addr == 0x0B) EDCAN_DeviceInfo.uptime = HAL_GetTick();
		return ((uint8_t*)&EDCAN_DeviceInfo)[addr - EDCAN_REG_DEVICE_INFO];

		default:
			return 0x00;
		}
}

/**
  * @brief  Handler to get own register values
  *
  * @param  addr: register address
  * @retval register value (uint8_t)
  */
uint8_t EDCAN_GetOwnRegisterValue (uint16_t addr){

	if(addr<256){
		return EDCAN_GetSystemRegisterValue(addr); // 0..255
	}else {
		return EDCAN_GetUserRegisterValue(addr); // 256..2047
	}
}

/**
  * @brief  Handler for incoming Read Request packet
  * 		Another device wants to read registers of this device
  *
  * @param  SourceID: Packet Source ID
  * 		DestinationID: Packet Destination ID
  * 		Addr: First register address in sequence
  * 		*data: pointer for data array
  * 		len: length of data (1..255)
  */
void EDCAN_ReadRequestHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t len){
	//Получили пакет Read (запрошенное значение регистров)
	uint8_t TxData[8];
	uint16_t AddrOffset = Addr;
//	printf("Received packet: Read Request\n");
//	printf("Source ID = %d\n", SourceID);
//	printf("Destination ID = %d\n", DestinationID);
//	printf("Address = %d\n", Addr);
//	printf("Len = %d\n", len);
//	printf("\n");

	while (len>0){ //по очереди перебираем все полученные регистры через Handler
		if(len>=8){ //если количество регистров больше 8, отправляем 8 и разбиваем на несколько пакетов
			for(uint8_t n = 0; n < 8; n++){
				TxData[n] = EDCAN_GetOwnRegisterValue(n+AddrOffset);
				//printf ("register[%d] = %d\n", n+AddrOffset, TxData[n]);
			}
			EDCAN_SendPacketRead(SourceID, AddrOffset, TxData, 8); /* отправляем ответный пакет со значениями собственных регистров */
			//printf ("sent%d, %d\n", AddrOffset, len);
			AddrOffset +=8;
			len -=8;

		}else{
			for(uint8_t n = 0; n < len; n++){
				TxData[n] = EDCAN_GetOwnRegisterValue(n+AddrOffset);
				//printf ("register[%d] = %d\n", n+AddrOffset, TxData[n]);
			}
			EDCAN_SendPacketRead(SourceID, AddrOffset, TxData, len);  /* отправляем ответный пакет со значениями собственных регистров */
			//printf ("sent%d, %d\n", AddrOffset, len);

			AddrOffset +=len;
			len = 0;
		}
	}
//	printf("\n");
}




