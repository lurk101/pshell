#pragma once

#include <stdint.h>

#include "io.h"

// === UART DEFINES ====
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1080)

// === LED pin used to show transfer activity ===
// === Set to 0 if you don't want to use it   ===
#define YMODEM_LED_ACT 0
#define YMODEM_LED_ACT_ON 1 // pin level for LED ON

// ==== Y-MODEM defines ====
#define PACKET_SEQNO_INDEX (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER (3)
#define PACKET_TRAILER (2)
#define PACKET_OVERHEAD (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE (128)
#define PACKET_1K_SIZE (1024)

#define FILE_SIZE_LENGTH (16)

#define SOH (0x01)   /* start of 128-byte data packet */
#define STX (0x02)   /* start of 1024-byte data packet */
#define EOT (0x04)   /* end of transmission */
#define ACK (0x06)   /* acknowledge */
#define NAK (0x15)   /* negative acknowledge */
#define CA (0x18)    /* two of these in succession aborts transfer */
#define CRC16 (0x43) /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1 (0x41) /* 'A' == 0x41, abort by user */
#define ABORT2 (0x61) /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT (1000)
#define MAX_ERRORS (45)

#define YM_MAX_FILESIZE (10 * 1024 * 1024)

int Ymodem_Receive(lfs_file_t* ffd, unsigned int maxsize, char* getname);
int Ymodem_Transmit(char* sendFileName, unsigned int sizeFile, lfs_file_t* ffd);
