#pragma once

#include "main.h"
//Library settings
#include <stdbool.h>
#include <edcan_config.h>

#define EDCAN_PRINTF_BUFFER_SIZE 128
#define EDCAN_ENABLE_INTERNAL_PRINTF 0

#if EDCAN_ENABLE_INTERNAL_PRINTF
#include <stdio.h>
#define EDCAN_DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define EDCAN_DBG_PRINTF(...) ((void)0)
#endif

/* Critical sections (can be overridden from project config if needed) */
#ifndef EDCAN_CRITICAL_ENTER
#define EDCAN_CRITICAL_ENTER() __disable_irq()
#endif
#ifndef EDCAN_CRITICAL_EXIT
#define EDCAN_CRITICAL_EXIT() __enable_irq()
#endif

typedef enum{
	LOG_EMERG = 0,
	LOG_ALERT = 1,
	LOG_CRIT = 2,
	LOG_ERR = 3,
	LOG_WARN = 4,
	LOG_NOTICE = 5,
	LOG_INFO = 6,
	LOG_DEBUG = 7,

}EDCAN_LogLevel_t;

typedef struct __attribute__((packed)) {
	uint8_t status;
	uint8_t device_id;
	uint8_t fwver_major;
	uint8_t fwver_minor;
	uint8_t fwver_patch;
	uint8_t board_type;
	uint8_t station_type;
	uint8_t rsvd;
	uint32_t serial_number;
	uint32_t uptime;
}EDCAN_DeviceInfo_t;

// Версия устройства
#define VERSION_OFFSET (0x1E4)

typedef struct __attribute__((packed)) {
	uint32_t serialNumber;      // Байты 0-3: серийный номер станции (little-endian)
	uint8_t stationType;        // Байт 4: тип станции
	uint8_t boardVersion;       // Байт 5: версия платы
	uint8_t addrEdcan;          // Байт 6: адрес EDCAN
	uint8_t reserved[57];       // Байты 7-63: зарезервированы
} InfoBlock_t;

typedef struct __attribute__((packed)) {
	uint8_t  DestinationID;     // (0..7 биты)   - Destination ID (FF - широковещательный)
	uint8_t  SourceID;          // (8..15 биты)  - Source ID
	uint16_t RegisterAddress:11;// (16..26 биты) - адрес регистра (11 бит)
	uint8_t  PacketType:2;      // (27..28 биты) - тип пакета
								//    0x0 Write
								//    0x1 Read request
								//    0x2 Read
								//    0x3 Log

	uint8_t notused:3;          // (29..31 биты) - неиспользованные
}EDCAN_frameId_t;
typedef union {
	uint32_t raw;
	EDCAN_frameId_t f;
} EDCAN_ExtId_u;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(EDCAN_frameId_t) == 4, "EDCAN_frameId_t must be 4 bytes");
_Static_assert(sizeof(EDCAN_ExtId_u) == 4, "EDCAN_ExtId_u must be 4 bytes");
#endif

typedef struct{
	CAN_TxHeaderTypeDef tx_header;
	uint8_t data[8];
}EDCAN_TxFrame_t;

typedef struct{
	EDCAN_frameId_t ExtID;
	uint8_t data[8];
	uint8_t DLC;
}EDCAN_RxFrame_t;

#define ED_WRITE   0
#define ED_READREQ 1
#define ED_READ    2
#define ED_LOG     3 //lowest priority

/* edcan.c */
void EDCAN_Loop();
void EDCAN_FilterInit();
void EDCAN_Init(uint8_t _OwnID);
void EDCAN_SetSecondID(uint8_t _SecondID);
void CAN_ReInit();
/* Vector base helpers for OpenBLT integration. */
void EDCAN_VectorBaseConfigF0(void);
void EDCAN_VectorBaseConfigF1(void);
void EDCAN_ReadHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t *data, uint8_t len);
void EDCAN_SendPacketReadRequest(uint8_t DestinationID, uint16_t RegAddr, uint8_t len);

void EDCAN_SendPacketWrite(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len);
void EDCAN_SendPacketWriteLong(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint16_t len);
void EDCAN_SendPacketRead(uint8_t DestinationID, uint16_t RegAddr, const uint8_t *data, uint8_t len);

void EDCAN_SendAlivePacket();

/* edcan_handler.c */
extern EDCAN_DeviceInfo_t EDCAN_DeviceInfo;
void EDCAN_WriteHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t *data, uint8_t len);
void EDCAN_WriteSystemRegister(uint16_t addr, uint8_t value);

void EDCAN_ReadRequestHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t len);
uint8_t EDCAN_GetOwnRegisterValue (uint16_t addr);
uint8_t EDCAN_GetSystemRegisterValue(uint16_t addr);


/* edcan_handler_user.c */
extern void EDCAN_ReadHandler(uint8_t SourceID, uint8_t DestinationID, uint16_t Addr, uint8_t *data, uint8_t len);
void EDCAN_WriteUserRegister(uint16_t addr, uint8_t value);
uint8_t EDCAN_GetUserRegisterValue(uint16_t addr);


/* edcan_buffer.c */
void EDCAN_ExchangeTxBuffer();
void EDCAN_TxBufferAdd(EDCAN_TxFrame_t *frame);
bool EDCAN_TxBufferGet(EDCAN_TxFrame_t *frame);
uint16_t EDCAN_getTxBufferElementCount() ;
bool EDCAN_TxBufferPeekFirst(EDCAN_TxFrame_t *frame);
bool EDCAN_TxBufferRemoveFirst();
void EDCAN_RxBufferAdd(EDCAN_RxFrame_t *frame) ;
bool EDCAN_RxBufferGet(EDCAN_RxFrame_t *frame);
uint16_t EDCAN_getRxBufferElementCount();
bool EDCAN_RxBufferPeekLast(EDCAN_RxFrame_t *frame);
bool EDCAN_RxBufferRemoveLast();
void EDCAN_ExchangeRxBuffer();

void EDCAN_Log(const char *data, uint16_t len);
void EDCAN_printf(EDCAN_LogLevel_t loglevel, const char *format, ...);

