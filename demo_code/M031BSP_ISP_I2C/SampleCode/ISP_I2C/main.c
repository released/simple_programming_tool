/***************************************************************************//**
 * @file     main.c
 * @brief    ISP tool main function
 * @version  0x34
 * @date     14, June, 2017
 *
 * @note
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2017-2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "targetdev.h"

#define APP_START_ADDR       FMC_APROM_BASE
#define APP_SRAM_SIZE        0x00004000UL
#define APP_MIN_SIZE         0x00000200UL

static uint32_t App_Crc32(uint32_t u32Start, uint32_t u32Size)
{
    uint32_t u32Addr;
    uint32_t u32Crc;
    uint32_t u32Data;
    uint8_t i;
    uint8_t b;
    uint8_t u8Byte;

    u32Crc = 0xFFFFFFFFUL;
    for (u32Addr = u32Start; u32Addr < (u32Start + u32Size); u32Addr += 4UL)
    {
        FMC_Read_User(u32Addr, &u32Data);
        for (i = 0U; i < 4U; i++)
        {
            u8Byte = (uint8_t)(u32Data >> (i * 8U));
            u32Crc ^= u8Byte;
            for (b = 0U; b < 8U; b++)
            {
                if ((u32Crc & 1UL) != 0UL)
                {
                    u32Crc = (u32Crc >> 1U) ^ 0xEDB88320UL;
                }
                else
                {
                    u32Crc >>= 1U;
                }
            }
        }
    }

    return ~u32Crc;
}

uint8_t App_IsValid(void)
{
    uint32_t u32AppSize;
    uint32_t u32AppEnd;
    uint32_t u32ChecksumAddr;
    uint32_t u32Sp;
    uint32_t u32Rv;
    uint32_t u32StoredCrc;
    uint32_t u32CalcCrc;

    u32AppSize = g_dataFlashAddr;
    if (u32AppSize == 0UL)
    {
        u32AppSize = g_apromSize;
    }
    if ((u32AppSize < APP_MIN_SIZE) || ((u32AppSize & 3UL) != 0UL))
    {
        return 0U;
    }

    u32AppEnd = APP_START_ADDR + u32AppSize;
    u32ChecksumAddr = u32AppEnd - 4UL;
    FMC_Read_User(APP_START_ADDR, &u32Sp);
    FMC_Read_User(APP_START_ADDR + 4UL, &u32Rv);
    FMC_Read_User(u32ChecksumAddr, &u32StoredCrc);

    if ((u32Sp < SRAM_BASE) || (u32Sp > (SRAM_BASE + APP_SRAM_SIZE)) || ((u32Sp & 3UL) != 0UL))
    {
        return 0U;
    }

    if ((u32Rv < APP_START_ADDR) || (u32Rv >= u32AppEnd) || ((u32Rv & 1UL) == 0UL))
    {
        return 0U;
    }

    if ((u32StoredCrc == 0xFFFFFFFFUL) || (u32StoredCrc == 0x00000000UL))
    {
        return 0U;
    }

    u32CalcCrc = App_Crc32(APP_START_ADDR, u32AppSize - 4UL);
    if (u32CalcCrc != u32StoredCrc)
    {
        return 0U;
    }

    return 1U;
}

void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Unlock protected registers */
    SYS_UnlockReg();
    /* Enable HIRC clock (Internal RC 48MHz) */
    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;

    /* Wait for HIRC clock ready */
    while ((CLK->STATUS & CLK_STATUS_HIRCSTB_Msk) != CLK_STATUS_HIRCSTB_Msk);

    /* Select HCLK clock source as HIRC and HCLK source divider as 1 */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_HIRC;
    CLK->CLKDIV0 = (CLK->CLKDIV0 & (~CLK_CLKDIV0_HCLKDIV_Msk)) | CLK_CLKDIV0_HCLK(1);
    /* Enable I2C controller */
    CLK->APBCLK0 |= CLK_APBCLK0_I2C0CKEN_Msk;
    /* Update System Core Clock */
    SystemCoreClock = __HIRC;
    CyclesPerUs = (SystemCoreClock + 500000) / 1000000;
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set I2C0 multi-function pins */
    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB4MFP_Msk | SYS_GPB_MFPL_PB5MFP_Msk)) |
                    (SYS_GPB_MFPL_PB4MFP_I2C0_SDA | SYS_GPB_MFPL_PB5MFP_I2C0_SCL);
#ifdef ReadyPin
    PB->MODE = (PB->MODE & ~(GPIO_MODE_MODE0_Msk << (6 << 1))) | (GPIO_MODE_OUTPUT << (6 << 1));
    ReadyPin = 1;
#endif
    /* Lock protected registers */
    SYS_LockReg();
}

int main(void)
{
    uint32_t cmd_buff[16];

    SYS_Init();

    /* Checking if flash page size matches with target chip's */
    if( (GET_CHIP_SERIES_NUM == CHIP_SERIES_NUM_I) || (GET_CHIP_SERIES_NUM == CHIP_SERIES_NUM_G) )
    {
        if(FMC_FLASH_PAGE_SIZE != 2048)
        {
            /* FMC_FLASH_PAGE_SIZE is different from target device */
            /* Please enable the compiler option PAGE_SIZE_2048 in fmc.h */
            while(SYS->PDID);
        }
    }
    else
    {
        if(FMC_FLASH_PAGE_SIZE != 512)
        {
            /* FMC_FLASH_PAGE_SIZE is different from target device */
            /* Please disable the compiler option PAGE_SIZE_2048 in fmc.h */
            while(SYS->PDID);
        }
    }

    CLK->AHBCLK |= CLK_AHBCLK_ISPCKEN_Msk;

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable FMC ISP function. Before using FMC function, it should unlock system register first. */
    FMC->ISPCTL |= (FMC_ISPCTL_ISPEN_Msk | FMC_ISPCTL_APUEN_Msk);

    g_apromSize = GetApromSize();
    GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);
    I2C_Init();
    SysTick->LOAD = 300000 * CyclesPerUs;
    SysTick->VAL   = (0x00);
    SysTick->CTRL = SysTick->CTRL | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;//using cpu clock

    while (1)
    {
        if (bI2cDataReady == 1)
        {
            goto _ISP;
        }

        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            goto _APROM;
        }
    }

_ISP:

    while (1)
    {
        if (bI2cDataReady == 1)
        {
            /* Disable I2C IRQ until ParseCmd() is finished to prevent returning incomplete data prematurely */
            NVIC_DisableIRQ(I2C0_IRQn);        	
            memcpy(cmd_buff, i2c_rcvbuf, 64);
            bI2cDataReady = 0;
            ParseCmd((unsigned char *)cmd_buff, 64);
            bISPDataReady = 1;
            NVIC_EnableIRQ(I2C0_IRQn);            
#ifdef ReadyPin
            ReadyPin = 0;
#endif
        }
    }

_APROM:
    if (App_IsValid() == 0U)
    {
        SysTick->CTRL = 0;
        goto _ISP;
    }
    SYS->RSTSTS = (SYS_RSTSTS_PORF_Msk | SYS_RSTSTS_PINRF_Msk);
    FMC->ISPCTL &= ~(FMC_ISPCTL_ISPEN_Msk | FMC_ISPCTL_BS_Msk);
    SCB->AIRCR = (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ);

    /* Trap the CPU */
    while (1);
}
