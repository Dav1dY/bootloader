/*
 * boot.c
 *
 *  Created on: Mar 20, 2024
 *      Author: jack
 */

#include "boot.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include "usb_device.h"
#include "usbd_core.h"

extern CRC_HandleTypeDef hcrc;
extern USBD_HandleTypeDef hUsbDeviceFS;

void bootFunction(void){
	printf("Bootloader running\r\n");

	app_info_t *app1 = (const app_info_t*)(INFO_BASE);
	app_info_t *app2 = (const app_info_t*)(INFO_BASE+sizeof(app_info_t));

	if(app2->version > app1->version && ((*(uint32_t*)(BACKUP_BASE+4)) & 0xFF000000) == 0x08000000 && (*(uint32_t*)(BACKUP_BASE)) == 0x20020000){
		printf("found update\r\n");
//		//crc check
//		uint32_t crc_value = HAL_CRC_Calculate(&hcrc, (uint32_t*)BACKUP_BASE, (app2->size)/4);
//		if(app2->crc != crc_value){
//			printf("BACKUP crc fail.\r\n");
//			return;
//		}

		uint32_t *app_address = (uint32_t*)APP_BASE;
		uint32_t *backup_address = (uint32_t*)BACKUP_BASE;
		uint8_t prog_success = HAL_OK;
		//erase
		EraseFlash(FLASH_SECTOR_4, 3);
		printf("flash erased\r\n");
		//update

		HAL_FLASH_Unlock();
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
				FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
		__disable_irq();
		for(int i=0; i<(app2->size)/4; i++){
			if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)app_address, *backup_address)==HAL_OK){
				if(*app_address!=*backup_address){
					prog_success = HAL_ERROR;
					break;
				}
				app_address++;
				backup_address++;
			}
			else{
				prog_success = HAL_ERROR;
				break;
			}
		}
		__enable_irq();
		HAL_FLASH_Lock();
		if(prog_success == HAL_OK){
			printf("program success\r\n");
		}
		else{
			printf("program failed\r\n");
			return;
		}
		HAL_Delay(1000);

		uint32_t App_Address = APP_BASE;
		uint32_t App_Size = app2->size;
		uint32_t app_crc = 0;
		for(uint32_t i=0; i<App_Size; i+= CRC_CHUNK_SIZE){
			uint32_t chunk_len = (i+CRC_CHUNK_SIZE>App_Size)?App_Size-i:CRC_CHUNK_SIZE;
			app_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)(App_Address+i), chunk_len);
		}
		printf("crc done, value=%d\r\n", (int)app_crc);

		if(func_write_app_info()!=HAL_OK){
			return;
		}

		HAL_Delay(2000);
		NVIC_SystemReset();
	}
	else if(((*(uint32_t*)(APP_BASE+4)) & 0xFF000000) == 0x08000000 && (*(uint32_t*)(APP_BASE)) == 0x20020000){
		printf("start loading application\r\n");
		//crc check
//		uint32_t crc_value = HAL_CRC_Calculatem(&hcrc, (uint32_t*)APP_BASE, (app1->size)/4);
//		if(app1->crc != crc_value){
//			printf("APP crc fail.\r\n");
//			return;
//		}
		//jump to app;
		uint32_t app_msp = *(__IO uint32_t*)(APP_BASE);
		uint32_t JumpAddress = *(__IO uint32_t*)(APP_BASE+4);
		pFunction JumpToApplication = (pFunction)JumpAddress;
		HAL_CRC_DeInit(&hcrc);
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_15);
		USBD_DeInit(&hUsbDeviceFS);
		HAL_SuspendTick();
		HAL_DeInit();
		__disable_irq();
		__set_MSP(app_msp);
		JumpToApplication();
	}
	printf("no image found\r\n");
}

HAL_StatusTypeDef EraseFlash(uint32_t start_sector, uint32_t sector_number){
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SectorError = 0;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	EraseInitStruct.Sector = start_sector;//FLASH_SECTOR_4
	EraseInitStruct.NbSectors = sector_number;//3

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
			FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
	__disable_irq();
	if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError)!=HAL_OK){
		__enable_irq();
		HAL_FLASH_Lock();
		return HAL_ERROR;
	}
	__enable_irq();
	HAL_FLASH_Lock();
	return HAL_OK;
}

HAL_StatusTypeDef func_write_app_info(void){
	app_info_t* info_backup = (app_info_t*)(INFO_BASE+sizeof(app_info_t));
	printf("backup crc=%d, size=%d, version=%d\r\n", (int)info_backup->crc, (int)info_backup->size, (int)info_backup->version);
	app_info_t* info_app = (app_info_t*)(INFO_BASE);
	printf("app crc=%d, size=%d, version=%d\r\n", (int)info_app->crc, (int)info_app->size, (int)info_app->version);

	app_info_t info1;
	info1.version = info_backup->version;
	info1.size = info_backup->size;
	info1.crc = info_backup->crc;

	if(EraseFlash(FLASH_SECTOR_3, 1)!=HAL_OK){
		printf("erase INFO failed\r\n");
	}
	printf("erase INFO success\r\n");

	app_info_t* info_backup1 = (app_info_t*)(INFO_BASE+sizeof(app_info_t));
	printf("backup crc=%d, size=%d, version=%d\r\n", (int)info_backup1->crc, (int)info_backup1->size, (int)info_backup1->version);
	app_info_t* info_app1 = (app_info_t*)(INFO_BASE);
	printf("app crc=%d, size=%d, version=%d\r\n", (int)info_app1->crc, (int)info_app1->size, (int)info_app1->version);

	// write info
	uint32_t* data_ptr = (uint32_t*)&info1;
	printf("start write to flash\r\n");
	uint8_t prog_success = 0;
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
			FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
	__disable_irq();
	for(uint32_t i = 0; i<sizeof(app_info_t)/sizeof(uint32_t); i++){
		uint32_t data = data_ptr[i];
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, INFO_BASE+i*4, data)!=HAL_OK){
			prog_success = 1;
			break;
		}
	}
	for(uint32_t i = 0; i<sizeof(app_info_t)/sizeof(uint32_t); i++){
		uint32_t data = data_ptr[i];
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, INFO_BASE+sizeof(app_info_t)+i*4, data)!=HAL_OK){
			prog_success = 1;
			break;
		}
	}
	__enable_irq();
	HAL_FLASH_Lock();
	if(prog_success != 0){
		printf("program to INFO failed\r\n");
		return HAL_ERROR;
	}

	printf("write to flash done\r\n");
	app_info_t* info11 = (app_info_t*)(INFO_BASE);
	printf("new crc=%d, size=%d, version=%d\r\n", (int)info11->crc, (int)info11->size, (int)info11->version);
	return HAL_OK;
}
