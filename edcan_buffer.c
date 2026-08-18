/*
 * edcan_buffer.c
 * 	Буферы исходящих и входящих пакетов
 *  Created on: Jul 16, 2024
 *      Author: colorbass
 */


#include <edcan.h>
#include "edcan_config.h"
#include <stdint.h>
#include <stdbool.h>

#include <string.h>


// Определение структуры кольцевого буфера для Tx и Rx
typedef struct {
	EDCAN_TxFrame_t buffer[BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    int16_t count;
    //uint8_t state;
    uint8_t busy;
} TxCircularBuffer_t;

typedef struct {
	EDCAN_RxFrame_t buffer[BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    int16_t count;
    uint8_t busy;
} RxCircularBuffer_t;

// Инициализация глобальных буферов
TxCircularBuffer_t txBuffer = { .head = 0, .tail = 0, .count = 0, .busy = 0 };
RxCircularBuffer_t rxBuffer = { .head = 0, .tail = 0, .count = 0, .busy = 0 };


// Добавление элемента в буфер
void EDCAN_TxBufferAdd(EDCAN_TxFrame_t *frame) {
	EDCAN_CRITICAL_ENTER();

    memcpy(&txBuffer.buffer[txBuffer.head], frame, sizeof(EDCAN_TxFrame_t));
    txBuffer.head = (txBuffer.head + 1) % BUFFER_SIZE;

    if (txBuffer.count == BUFFER_SIZE) {
        txBuffer.tail = (txBuffer.tail + 1) % BUFFER_SIZE; // Перезапись старых данных
    } else {
        txBuffer.count++;
    }

	EDCAN_CRITICAL_EXIT();
}

//Количество элементов в буфере
uint16_t EDCAN_getTxBufferElementCount() {
	EDCAN_CRITICAL_ENTER();
    uint16_t c = (uint16_t)txBuffer.count;
	EDCAN_CRITICAL_EXIT();
    return c;
}

// функция для получения первого элемента без удаления его из буфера
bool EDCAN_TxBufferPeekFirst(EDCAN_TxFrame_t *frame) {
	bool ok = false;
	EDCAN_CRITICAL_ENTER();
    if (txBuffer.count > 0) {
        memcpy(frame, &txBuffer.buffer[txBuffer.tail], sizeof(EDCAN_TxFrame_t));
        ok = true;
    }
	EDCAN_CRITICAL_EXIT();
    return ok;

}

// функция для удаления первого элемента из буфера
bool EDCAN_TxBufferRemoveFirst() {
	bool ok = false;
	EDCAN_CRITICAL_ENTER();
    if (txBuffer.count > 0) {
        txBuffer.tail = (txBuffer.tail + 1) % BUFFER_SIZE;
        txBuffer.count--;
        ok = true;
    }
	EDCAN_CRITICAL_EXIT();
    return ok;
}

//Функция для передачи данных из буфера в mailbox CAN шины
void EDCAN_ExchangeTxBuffer(){
	EDCAN_TxFrame_t TxFrame;
	uint32_t tx_mailbox;
	HAL_StatusTypeDef CAN_result;

	//Если есть свободные Mailbox
	if(HAL_CAN_GetTxMailboxesFreeLevel(&ED_CAN_INSTANCE) > 0){

		//Забираем первый элемент атомарно, без удержания IRQ во время HAL_CAN_AddTxMessage
		EDCAN_CRITICAL_ENTER();
		if (txBuffer.busy || (txBuffer.count <= 0)) {
			EDCAN_CRITICAL_EXIT();
			return;
		}
		txBuffer.busy = 1;
		memcpy(&TxFrame, &txBuffer.buffer[txBuffer.tail], sizeof(EDCAN_TxFrame_t));
		EDCAN_CRITICAL_EXIT();

			/* отправка сообщения */
			CAN_result = HAL_CAN_AddTxMessage(&ED_CAN_INSTANCE, &TxFrame.tx_header, TxFrame.data, &tx_mailbox);

		EDCAN_CRITICAL_ENTER();
		/* если отправка удалась, удаляем элемент из буфера */
			if(CAN_result == HAL_OK) {
			txBuffer.tail = (txBuffer.tail + 1) % BUFFER_SIZE;
			txBuffer.count--;
		}
		txBuffer.busy = 0;
		EDCAN_CRITICAL_EXIT();

		if(CAN_result == HAL_ERROR) {
				/* если ошибка, обработка ошибки */
				if(ED_CAN_INSTANCE.ErrorCode & HAL_CAN_ERROR_NOT_INITIALIZED) {
					CAN_ReInit(); //CAN не инициализирован, переинициализация
				EDCAN_DBG_PRINTF("CAN Reinit\n");
				}
			EDCAN_DBG_PRINTF("CAN.ErrorCode = %d\n",(int)ED_CAN_INSTANCE.ErrorCode);
				ED_CAN_INSTANCE.ErrorCode = 0; //Clear errors
		}

	}
}

// Функции работы с Rx буфером
void EDCAN_RxBufferAdd(EDCAN_RxFrame_t *frame) {
	// Исполнение из прерывания

    memcpy(&rxBuffer.buffer[rxBuffer.head], frame, sizeof(EDCAN_RxFrame_t));
    rxBuffer.head = (rxBuffer.head + 1) % BUFFER_SIZE;

    if (rxBuffer.count == BUFFER_SIZE) {
        rxBuffer.tail = (rxBuffer.tail + 1) % BUFFER_SIZE; // Перезапись старых данных
    } else {
        rxBuffer.count++;
    }

}

//Извлечь и удалить первый элемент буфера
bool EDCAN_RxBufferGet(EDCAN_RxFrame_t *frame) {
	//LOCKED function
	EDCAN_CRITICAL_ENTER();
    if (rxBuffer.count > 0) {
        memcpy(frame, &rxBuffer.buffer[rxBuffer.tail], sizeof(EDCAN_RxFrame_t));
    	rxBuffer.tail = (rxBuffer.tail + 1) % BUFFER_SIZE;
        rxBuffer.count--;
        EDCAN_CRITICAL_EXIT();
        return true;
    } else {
        // Буфер пуст, можно добавить обработку ошибки
    	EDCAN_CRITICAL_EXIT();
        return false;
    }

}

//Количество элементов в буфере
uint16_t EDCAN_getRxBufferElementCount() {
	EDCAN_CRITICAL_ENTER();
    uint16_t c = (uint16_t)rxBuffer.count;
	EDCAN_CRITICAL_EXIT();
    return c;
}

//Функция для обработки входящих пакетов из буфера
void EDCAN_ExchangeRxBuffer(){
	EDCAN_RxFrame_t Rxframe;

	if (EDCAN_RxBufferGet(&Rxframe)){

		if(Rxframe.ExtID.PacketType == ED_WRITE){
			EDCAN_WriteHandler(Rxframe.ExtID.SourceID, Rxframe.ExtID.DestinationID, Rxframe.ExtID.RegisterAddress, Rxframe.data, Rxframe.DLC);
		}

		if(Rxframe.ExtID.PacketType == ED_READREQ){
			EDCAN_ReadRequestHandler(Rxframe.ExtID.SourceID, Rxframe.ExtID.DestinationID, Rxframe.ExtID.RegisterAddress, Rxframe.data[0]);
		}

		if(Rxframe.ExtID.PacketType == ED_READ){
			EDCAN_ReadHandler(Rxframe.ExtID.SourceID, Rxframe.ExtID.DestinationID, Rxframe.ExtID.RegisterAddress, Rxframe.data, Rxframe.DLC);
		}
	}

}
