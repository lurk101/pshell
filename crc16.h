#ifndef _CRC16_H_
#define _CRC16_H_

#include <stdint.h>

uint16_t crc16_ccitt(const void* buf, int len);

#endif /* _CRC16_H_ */
