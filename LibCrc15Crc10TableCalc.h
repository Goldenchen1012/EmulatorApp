/*
******************************************************************************
* @file     LibCrc15Crc10TableCalc.h
* @author   Golden Chen
* @brief    

******************************************************************************
* @attention
*
* COPYRIGHT(c) 2025 FW Team</center>
******************************************************************************
*/
#ifndef __LIB_CRC15_CRC10_TABLE_CALC_H__
#define	__LIB_CRC15_CRC10_TABLE_CALC_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Includes -----------------------------------------------------------------*/
/* Global define ------------------------------------------------------------*/
/* Global typedef -----------------------------------------------------------*/
/* Global macro -------------------------------------------------------------*/
/* Global function prototypes -----------------------------------------------*/
uint16_t Pec15_Calc(uint8_t len, uint8_t *data);
uint16_t pec10_calc( bool blSrXCmd, int nLength, uint8_t *pDataBuf);

#ifdef __cplusplus
}
#endif

#endif
