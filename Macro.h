#pragma once

#ifndef _MACRO_H
#define _MACRO_H
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
// new defined macros
//programmer info RQ
#define PROGINFO_REQUEST                0x08
#define SET_VCC                         0x09                ///< set VCC
#define SET_VPP                         0x03                ///< set VPP
#define SET_CS				0x14
#define SET_IOMODE				0x15
#define SET_SPICLK			0x61
#define SET_HOLD	0x1D
#define SET_SA    0x0A

//first field of RQ
#define OUT_REQUEST                     0x42
#define IN_REQUEST                      0xC2

//second field of RQ in case of bulk transfer
//#define TRANSCEIVE                      0x01
//#define DTC_READ                        0x20
//#define WRITE                           0x30
//#define READ_EEPROM                     0x05
//#define WRITE_EEPROM                    0x06
// values of Request Field of a setup packet
typedef struct FirmwareInfo
{
	char Programmer[20];
	char Version[10];
	char FPGAVersion[10];
	unsigned int dwSignature;
	unsigned int  Rev[3];
	unsigned int  FirstIndex;
	unsigned int  FirstSize;
	unsigned int  SecondIndex;
	unsigned int  SecondSize;
}FW_INFO;

typedef enum
{
    TRANSCEIVE                      = 0x01,

    DTC_READ                        = 0x20,
    WRITE                           = 0x30,

    ATMEL45_WRITE                   = 0x31,

    READ_EEPROM                     = 0x05,
    WRITE_EEPROM                    = 0x06,
	GET_BUTTON_STATUS				= 0x11,
}USB_CMD;

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
}CHIP_INFO;
//third field of RQ
#define CTRL_TIMEOUT                    3000                ///< time out for control pipe or for firmwire

// fourth field of RQ
#define RESULT_IN                       0x01                ///< result in or not
#define NO_RESULT_IN                    0x00
#define GET_REGISTER                    0x01                ///< get register value or not
#define NO_REGISTER                     0x00
#define RFU                             0x00

// bulk write mode
#define PAGE_PROGRAM                    0x01                ///< pp via bulk pipes
#define PAGE_WRITE                      0x02                ///< pw via bulk pipes
#define AAI_1_BYTE                      0x03
#define AAI_2_BYTE                      0x04
#define PP_128BYTE                      0x05
#define PP_AT26DF041                    0x06                ///<  PP AT26DF041
#define PP_SB_FPGA                      0x07                ///<  Silicon Blue FPGA
#define MODE_NUMONYX_PCM                0x08                ///<  Mode_Numonyx_pcm
#define PP_4ADR_256BYTE			0x09
#define PP_32BYTE			0x0A
#define PP_4ADDR_256BYTE_12 0x0B
#define PP_4ADDR_256BYTE_MICROM 0x0C
#define PP_4ADDR_256BYTE_S70FS01GS 0x0D

// bulk read mode
#define BULK_NORM_READ                  0x01                ///< read via bulk pipes
#define BULK_FAST_READ                  0x02                ///< fast read via bulk pipes
#define BULK_AT45xx_READ                  0x03                ///< fast read via bulk pipes
#define BULK_4BYTE_FAST_READ		0x04		///< For size is bigger than 128Mb
#define BULK_4BYTE_FAST_READ_MICRON 0x05 

//for flash card
#define POLLING                         0x02                ///< polling
#define SET_VPP                         0x03                ///< set vpp
#define VPP_OFF                         0x00                ///< vpp value
#define VPP_9V                          0x01
#define VPP_12V                         0x02

#define SET_TARGET_FLASH                0x04            ///< set target FLASH_TRAY
#define APPLICATION_MEMORY_1            0x00            ///< application memory chip 1
#define FLASH_CARD                      0x01            ///< flash card
#define APPLICATION_MEMORY_2            0x02            ///< application memory chip 2

//for io & LED
#define SET_IO                          0x07                ///< request
//IO number
#define IO_1                            0x01
#define IO_2                            0x02
#define IO_3                            0x04
#define IO_4                            0x08
//led number
#define LED_RED                         0x01
#define LED_GREEN                       0x02
#define LED_ORANGE                      0x04

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
	 #define SUPPORT_NANTRONICS_N25Sxx          "N25Sxx"
#endif

#ifdef SUPPORT_ATO
	#define SUPPORT_ATO_ATO25Qxx	"ATO25Qxx"
#endif

#ifdef SUPPORT_ST
    #define SUPPORT_ST_M25PExx          "M25PExx"
    #define SUPPORT_ST_M25Pxx           "M25Pxx"
    #define SUPPORT_ST_M45PExx          "M45PExx"
    #define SUPPORT_NUMONYX_Alverstone  "Alverstone"
	#define SUPPORT_NUMONYX_N25Qxxx_Large  "N25Qxxx_Large"
	#define SUPPORT_NUMONYX_N25Qxxx_Large_2Die  "N25Qxxx_Large_2Die"
	#define SUPPORT_NUMONYX_N25Qxxx_Large_4Die  "N25Qxxx_Large_4Die"
#endif

#ifdef SUPPORT_SST
    #define SUPPORT_SST_25xFxxA         "25xFxxA"
    #define SUPPORT_SST_25xFxxB         "25xFxxB"
    #define SUPPORT_SST_25xFxxC 	     "25xFxxC"
    #define SUPPORT_SST_25xFxx          "25xFxx"
		#define SUPPORT_SST_26xFxxC     "26VFxxC"
#endif

#ifdef SUPPORT_WINBOND
    #define SUPPORT_WINBOND_W25Bxx      "W25Bxx"
    #define SUPPORT_WINBOND_W25Pxx      "W25Pxx"
    #define SUPPORT_WINBOND_W25Pxx_Large      "W25Pxx_Large"
    #define SUPPORT_WINBOND_W25Xxx      "W25Xxx"
#endif

#ifdef SUPPORT_PMC
    #define SUPPORT_PMC_PM25LVxxx       "PM25LVxxx"
     #define SUPPORT_PMC_PM25Wxxx       "PM25Wxxx"
#endif

#ifdef SUPPORT_SPANSION
    #define SUPPORT_SPANSION_S25FLxx    "S25FLxxx"
	#define SUPPORT_SPANSION_S25FLxx_Large    "S25FLxxx_Large"
	#define SUPPORT_SPANSION_S70FSxx_Large    "S70FSxxx_Large"
#endif

#ifdef SUPPORT_MACRONIX
    #define SUPPORT_MACRONIX_MX25Lxxx  "MX25Lxxx"
     #define SUPPORT_MACRONIX_MX25Lxxx_Large  "MX25Lxxx_Large"
    #define SUPPORT_MACRONIX_MX25Lxxx_PP32  "MX25Lxxx_PP32"
#endif

#ifdef SUPPORT_EON
    #define SUPPORT_EON_EN25Xxx          "EN25Xxx"
	#define SUPPORT_EON_EN25QHxx_Large "EN25QHxx_Large"
#endif

#ifdef SUPPORT_ATMEL
    #define SUPPORT_ATMEL_AT26xxx       "AT26xxx"
    #define SUPPORT_ATMEL_AT25Fxxx      "AT25Fxxx"
    #define SUPPORT_ATMEL_AT25FSxxx     "AT25FSxxx"
    #define SUPPORT_ATMEL_45DBxxxD      "AT45DBxxxD"
    #define SUPPORT_ATMEL_45DBxxxB      "AT45DBxxxB"
#endif

#ifdef SUPPORT_AMIC
    #define SUPPORT_AMIC_A25Lxxx        "A25Lxxx"
	#define SUPPORT_AMIC_A25LQxxx        "A25LQxxx"
#endif

#ifdef SUPPORT_ESMT
    #define SUPPORT_ESMT_F25Lxx         "F25Lxx"
#endif

#ifdef SUPPORT_INTEL
    #define SUPPORT_INTEL_S33           "S33"
#endif

#ifdef SUPPORT_FREESCALE
    #define SUPPORT_FREESCALE_MCF           "MCF"
#endif

#ifdef SUPPORT_SANYO
    #define SUPPORT_SANYO_LE25FWxxx     "LE25FWxxx"
#endif

#ifdef SUPPORT_TSI
    #define SUPPORT_TSI_TS25Lxx         "TS25Lxx"
    #define SUPPORT_TSI_TS25Lxx_0A         "TS25Lxx_0A"
#endif

#ifdef SUPPORT_SILICONBLUE
    #define SUPPORT_SILICONBLUE_iCE65   "iCE65"
#endif

#ifdef SUPPORT_FIDELIX
	#define SUPPORT_FIDELIX_FM25Qxx  "FM25Qxx"
#endif

#ifdef SUPPORT_FUDAN
	#define SUPPORT_FUDAN_FM25Fxx  "FM25Fxx"
#endif


// for usb
#define USB_TIMEOUT                     800000                ///< time out value for usb EP2

// for SR reads
#define MAX_TRIALS                      0x80000

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

//#define size_t unsigned int
struct CAddressRange
{
    size_t start;
    size_t end;
    size_t length;
};

struct memory_id
{
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


enum
{
	SITE_NORMAL=0,
	SITE_BUSY,
	SITE_ERROR,
	SITE_OK,
};

enum
{
    Seriase_45 = 0x08,
    Seriase_25 = 0x00,
};

typedef enum
{
    vccPOWEROFF = 0x00,

    vcc3_5V     = 0x10,
    vcc2_5V     = 0x11,
    vcc1_8V     = 0x12,

    vccdo_nothing = 0xFF,

} VCC_VALUE;

    static unsigned int  crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
#endif    //_MACRO_H
