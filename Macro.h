#pragma once

#ifndef _MACRO_H
#define _MACRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

// new defined macros
//programmer info RQ
#define PROGINFO_REQUEST 0x08
#define SET_VCC 0x09 ///< set VCC
#define SET_VPP 0x03 ///< set VPP
#define SET_CS 0x14
#define SET_IOMODE 0x15
#define SET_SPICLK 0x61
#define SET_HOLD 0x1D
#define SET_SA 0x0A

//first field of RQ
#define OUT_REQUEST 0x42
#define IN_REQUEST 0xC2

//second field of RQ in case of bulk transfer
//#define TRANSCEIVE                      0x01
//#define DTC_READ                        0x20
//#define WRITE                           0x30
//#define READ_EEPROM                     0x05
//#define WRITE_EEPROM                    0x06
// values of Request Field of a setup packet
typedef struct FirmwareInfo {
    char Programmer[20];
    char Version[10];
    char FPGAVersion[10];
    unsigned int dwSignature;
    unsigned int Rev[3];
    unsigned int FirstIndex;
    unsigned int FirstSize;
    unsigned int SecondIndex;
    unsigned int SecondSize;
} FW_INFO;

typedef enum {
    TRANSCEIVE = 0x01,

    DTC_READ = 0x20,
    WRITE = 0x30,

    ATMEL45_WRITE = 0x31,

    READ_EEPROM = 0x05,
    WRITE_EEPROM = 0x06,
    GET_BUTTON_STATUS = 0x11,
} USB_CMD;

typedef struct ChipInfo {
    char TypeName[100];
    size_t UniqueID;
    char Class[100];
    char Description[256];

    char Manufacturer[100];
    char ManufactureUrl[100];
    char Voltage[20];
    char Clock[20];
    char ProgramIOMethod[20];

    size_t ManufactureID;
    size_t JedecDeviceID;
    size_t AlternativeID;

    size_t ChipSizeInByte;
    size_t SectorSizeInByte;
    size_t PageSizeInByte;
    size_t BlockSizeInByte;

    size_t MaxErasableSegmentInByte;

    size_t AddrWidth;

    bool DualID;
    size_t VppSupport;
    bool MXIC_WPmode;
    size_t IDNumber;
    size_t RDIDCommand;
    size_t Timeout;
    size_t VoltageInMv;
} CHIP_INFO;

//third field of RQ
#define CTRL_TIMEOUT 3000 ///< time out for control pipe or for firmwire

// fourth field of RQ
#define RESULT_IN 0x01 ///< result in or not
#define NO_RESULT_IN 0x00
#define GET_REGISTER 0x01 ///< get register value or not
#define NO_REGISTER 0x00
#define RFU 0x00

// bulk write mode
#define PAGE_PROGRAM 0x01 ///< pp via bulk pipes
#define PAGE_WRITE 0x02 ///< pw via bulk pipes
#define AAI_1_BYTE 0x03
#define AAI_2_BYTE 0x04
#define PP_128BYTE 0x05
#define PP_AT26DF041 0x06 ///<  PP AT26DF041
#define PP_SB_FPGA 0x07 ///<  Silicon Blue FPGA
#define MODE_NUMONYX_PCM 0x08 ///<  Mode_Numonyx_pcm
#define PP_4ADR_256BYTE 0x09
#define PP_32BYTE 0x0A
#define PP_4ADDR_256BYTE_12 0x0B
#define PP_4ADDR_256BYTE_MICROM 0x0C
#define PP_4ADDR_256BYTE_S70FS01GS 0x0D

// bulk read mode
#define BULK_NORM_READ 0x01 ///< read via bulk pipes
#define BULK_FAST_READ 0x02 ///< fast read via bulk pipes
#define BULK_AT45xx_READ 0x03 ///< fast read via bulk pipes
#define BULK_4BYTE_FAST_READ 0x04 ///< For size is bigger than 128Mb
#define BULK_4BYTE_FAST_READ_MICRON 0x05

//for flash card
#define POLLING 0x02 ///< polling
#define SET_VPP 0x03 ///< set vpp
#define VPP_OFF 0x00 ///< vpp value
#define VPP_9V 0x01
#define VPP_12V 0x02

#define SET_TARGET_FLASH 0x04 ///< set target FLASH_TRAY
#define APPLICATION_MEMORY_1 0x00 ///< application memory chip 1
#define FLASH_CARD 0x01 ///< flash card
#define APPLICATION_MEMORY_2 0x02 ///< application memory chip 2

//for io & LED
#define SET_IO 0x07 ///< request
//IO number
#define IO_1 0x01
#define IO_2 0x02
#define IO_3 0x04
#define IO_4 0x08
//led number
#define LED_RED 0x01
#define LED_GREEN 0x02
#define LED_ORANGE 0x04

#define SUPPORT_ST
#define SUPPORT_SST
#define SUPPORT_WINBOND
#define SUPPORT_PMC
#define SUPPORT_SPANSION
#define SUPPORT_MACRONIX
#define SUPPORT_EON
#define SUPPORT_ATMEL
#define SUPPORT_AMIC
#define SUPPORT_ESMT
#define SUPPORT_INTEL
#define SUPPORT_SANYO
#define SUPPORT_TSI
#define SUPPORT_FREESCALE
#define SUPPORT_SILICONBLUE
#define SUPPORT_NANTRONICS
#define SUPPORT_ATO
#define SUPPORT_FIDELIX
#define SUPPORT_FUDAN

// memory support list
#ifdef SUPPORT_NANTRONICS
#define SUPPORT_NANTRONICS_N25Sxx "N25Sxx"
#endif

#ifdef SUPPORT_ATO
#define SUPPORT_ATO_ATO25Qxx "ATO25Qxx"
#endif

#ifdef SUPPORT_ST
#define SUPPORT_ST_M25PExx "M25PExx"
#define SUPPORT_ST_M25Pxx "M25Pxx"
#define SUPPORT_ST_M45PExx "M45PExx"
#define SUPPORT_NUMONYX_Alverstone "Alverstone"
#define SUPPORT_NUMONYX_N25Qxxx_Large "N25Qxxx_Large"
#define SUPPORT_NUMONYX_N25Qxxx_Large_2Die "N25Qxxx_Large_2Die"
#define SUPPORT_NUMONYX_N25Qxxx_Large_4Die "N25Qxxx_Large_4Die"
#endif

#ifdef SUPPORT_SST
#define SUPPORT_SST_25xFxxA "25xFxxA"
#define SUPPORT_SST_25xFxxB "25xFxxB"
#define SUPPORT_SST_25xFxxC "25xFxxC"
#define SUPPORT_SST_25xFxx "25xFxx"
#define SUPPORT_SST_26xFxxC "26VFxxC"
#endif

#ifdef SUPPORT_WINBOND
#define SUPPORT_WINBOND_W25Bxx "W25Bxx"
#define SUPPORT_WINBOND_W25Pxx "W25Pxx"
#define SUPPORT_WINBOND_W25Pxx_Large "W25Pxx_Large"
#define SUPPORT_WINBOND_W25Xxx "W25Xxx"
#endif

#ifdef SUPPORT_PMC
#define SUPPORT_PMC_PM25LVxxx "PM25LVxxx"
#define SUPPORT_PMC_PM25Wxxx "PM25Wxxx"
#endif

#ifdef SUPPORT_SPANSION
#define SUPPORT_SPANSION_S25FLxx "S25FLxxx"
#define SUPPORT_SPANSION_S25FLxx_Large "S25FLxxx_Large"
#define SUPPORT_SPANSION_S70FSxx_Large "S70FSxxx_Large"
#endif

#ifdef SUPPORT_MACRONIX
#define SUPPORT_MACRONIX_MX25Lxxx "MX25Lxxx"
#define SUPPORT_MACRONIX_MX25Lxxx_Large "MX25Lxxx_Large"
#define SUPPORT_MACRONIX_MX25Lxxx_PP32 "MX25Lxxx_PP32"
#endif

#ifdef SUPPORT_EON
#define SUPPORT_EON_EN25Xxx "EN25Xxx"
#define SUPPORT_EON_EN25QHxx_Large "EN25QHxx_Large"
#endif

#ifdef SUPPORT_ATMEL
#define SUPPORT_ATMEL_AT26xxx "AT26xxx"
#define SUPPORT_ATMEL_AT25Fxxx "AT25Fxxx"
#define SUPPORT_ATMEL_AT25FSxxx "AT25FSxxx"
#define SUPPORT_ATMEL_45DBxxxD "AT45DBxxxD"
#define SUPPORT_ATMEL_45DBxxxB "AT45DBxxxB"
#endif

#ifdef SUPPORT_AMIC
#define SUPPORT_AMIC_A25Lxxx "A25Lxxx"
#define SUPPORT_AMIC_A25LQxxx "A25LQxxx"
#endif

#ifdef SUPPORT_ESMT
#define SUPPORT_ESMT_F25Lxx "F25Lxx"
#endif

#ifdef SUPPORT_INTEL
#define SUPPORT_INTEL_S33 "S33"
#endif

#ifdef SUPPORT_FREESCALE
#define SUPPORT_FREESCALE_MCF "MCF"
#endif

#ifdef SUPPORT_SANYO
#define SUPPORT_SANYO_LE25FWxxx "LE25FWxxx"
#endif

#ifdef SUPPORT_TSI
#define SUPPORT_TSI_TS25Lxx "TS25Lxx"
#define SUPPORT_TSI_TS25Lxx_0A "TS25Lxx_0A"
#endif

#ifdef SUPPORT_SILICONBLUE
#define SUPPORT_SILICONBLUE_iCE65 "iCE65"
#endif

#ifdef SUPPORT_FIDELIX
#define SUPPORT_FIDELIX_FM25Qxx "FM25Qxx"
#endif

#ifdef SUPPORT_FUDAN
#define SUPPORT_FUDAN_FM25Fxx "FM25Fxx"
#endif

// for usb
#define USB_TIMEOUT 800000 ///< time out value for usb EP2

// for SR reads
#define MAX_TRIALS 0x80000

#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80
#define BIT8 0x100
#define BIT9 0x200
#define BIT10 0x400
#define BIT11 0x800
#define BIT12 0x1000
#define BIT13 0x2000
#define BIT14 0x4000
#define BIT15 0x8000

#define ERASE BIT0
#define PROGRAM BIT1
#define VERIFY BIT2
#define BLANK BIT3
#define DETECT BIT4
#define BATCH BIT5
#define TYPE BIT6
#define FSUM BIT7
#define CSUM BIT8
#define READ_TO_FILE BIT9
#define BLINK BIT10
#define DEVICE_ID BIT11
#define LIST_TYPE BIT12
#define LOADFILE BIT13

struct CAddressRange {
    size_t start;
    size_t end;
    size_t length;
};

struct memory_id {
    char TypeName[20];
    size_t UniqueID;
    char Class[20];
    char Description[20];

    char Manufacturer[20];
    char ManufactureUrl[20];
    char Voltage[20];
    char Clock[20];
    char ProgramIOMethod[20];

    size_t ManufactureID;
    size_t JedecDeviceID;
    size_t AlternativeID;

    size_t ChipSizeInByte;
    size_t SectorSizeInByte;
    size_t PageSizeInByte;
    size_t BlockSizeInByte;

    size_t MaxErasableSegmentInByte;

    size_t AddrWidth;

    bool DualID;
    size_t VppSupport;
    bool MXIC_WPmode;
    size_t IDNumber;
    size_t RDIDCommand;
    size_t Timeout;
};

enum {
    SITE_NORMAL = 0,
    SITE_BUSY,
    SITE_ERROR,
    SITE_OK,
};

enum {
    Seriase_45 = 0x08,
    Seriase_25 = 0x00,
};

typedef enum {
    vccPOWEROFF = 0x00,

    vcc3_5V = 0x10,
    vcc2_5V = 0x11,
    vcc1_8V = 0x12,

    vccdo_nothing = 0xFF,

} VCC_VALUE;

#endif //_MACRO_H
