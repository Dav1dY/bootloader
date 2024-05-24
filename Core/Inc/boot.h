/*
 * boot.h
 *
 *  Created on: Mar 20, 2024
 *      Author: jack
 */

#include "stm32f4xx_hal.h"

#ifndef INC_BOOT_H_
#define INC_BOOT_H_

#define INFO_BASE 0x800C000
#define APP_BASE 0x8010000
#define BACKUP_BASE 0x8060000
#define CRC_CHUNK_SIZE 1024

typedef void (*pFunction)(void);

typedef struct __attribute__((packed)){
	uint32_t version;
	uint32_t size;
	uint32_t crc;
} app_info_t;

void bootFunction(void);
HAL_StatusTypeDef EraseFlash(uint32_t start_sector, uint32_t sector_number);
HAL_StatusTypeDef func_write_app_info(void);

#endif /* INC_BOOT_H_ */
