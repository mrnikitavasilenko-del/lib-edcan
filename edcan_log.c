#include <edcan_config.h>
#include <edcan.h>
#include "string.h"
#include "can.h"
#include <stdarg.h>
#include <stdio.h>

void EDCAN_SendPacketLog(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len);

void EDCAN_printf(EDCAN_LogLevel_t loglevel, const char *format, ...) {
    char buffer[EDCAN_PRINTF_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    int offset = snprintf(buffer, sizeof(buffer), "%d", loglevel); // Записываем лог-уровень в начало
    vsnprintf(buffer + offset, sizeof(buffer) - offset, format, args); // Записываем основное сообщение с учётом смещения
    va_end(args);

    EDCAN_Log(buffer, strlen(buffer));
}


/**
 * @brief  Send large data by splitting it into smaller packets
 *
 * @param  DestinationID: Packet Destination ID
 * @param RegAddr: First register address in sequence
 * @param *data:  pointer to data array to be send
 * @param len:  length of data to be sent
 */
void EDCAN_Log(const char *data, uint16_t len) {
	uint8_t DestinationID = 0x00;
    uint16_t remainingBytes = len;//strlen(data)+1; //add zero symbol
    uint16_t currentRegAddr = 0x00; //LOG reg addr
    const uint8_t *currentDataPtr = (const uint8_t*)data;

    while (remainingBytes > 0) {
        uint8_t packetSize = (remainingBytes > 8) ? 8 : remainingBytes;
        EDCAN_SendPacketLog(DestinationID, currentRegAddr, currentDataPtr, packetSize);

        remainingBytes -= packetSize;
        //currentRegAddr += packetSize; // Assuming the register address increments by the number of bytes sent
        currentDataPtr += packetSize;
    }
}

/**
  * @brief  EDCAN Send write packet function
  * 		Write registers of another device
  *
  * @param  DestinationID: Packet Destination ID
  * @param	RegAddr: First register address in sequence
  * @param	*data: 	pointer to data array to be send
  * @param	len: 	length of data (1..8)
  */
void EDCAN_SendPacketLog(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len){
	EDCAN_TxFrame_t tx_frame;
	EDCAN_ExtId_u ExtID = {0};
	//CAN_TxHeaderTypeDef tx_header;
	//uint32_t tx_mailbox;

	ExtID.f.DestinationID = DestinationID;
    ExtID.f.SourceID = ED_OwnID;
    ExtID.f.RegisterAddress = RegAddr;
    ExtID.f.PacketType = ED_LOG;

    tx_frame.tx_header.ExtId = ExtID.raw;

    tx_frame.tx_header.RTR = CAN_RTR_DATA;
    tx_frame.tx_header.IDE = CAN_ID_EXT;
    tx_frame.tx_header.DLC = len;

    memcpy(&tx_frame.data, data, len);

	//EDCAN_AddTxMessage(&ED_CAN_INSTANCE, &tx_header, data, &tx_mailbox);

    //Добавление пакета в буфер
	EDCAN_TxBufferAdd(&tx_frame);

	//Также, попытаемся сразу перенести пакет в CAN (если там есть свободное место)
	//Если свободного места нету, то пакет перенесется в CAN позже по прерыванию освобождения буфера
	EDCAN_ExchangeTxBuffer();


}
