#pragma once

#ifndef SERIALFLASHS
#define SERIALFLASHS

#include "Macro.h"
#include <inttypes.h>

#define SerialFlash_FALSE -1
#define SerialFlash_TRUE 1

enum //list of all chip-specific instruction, for ST serial flash
{
    WREN = 0x06, // Write Enable
    WRDI = 0x04, // Write Disable
    RDIDJ = 0x9F, //RDIDJ      // Read Jedec ID , except 80
    RDSR = 0x05, // Read Status Register
    WRSR = 0x01, // Write Status Register
    READ = 0x03, // Byte Read
    FREAD = 0x0B, // Fast Read
    PP = 0x02, // Page Program
    SE = 0xD8, // Sector Erase
    CHIP_ERASE = 0xC7, //CHIP_ERASE        // Bulk (or Chip) Erase
    DP = 0xB9, // Deep Power Down
    RDP = 0xAB, //RES        // Release Deep Power Down
    RES = 0xAB, //RES        // RDP and read signature
    RDSCUR = 0x2B,
    GBULK = 0x98,
    EN4B = 0xB7,
    EXIT4B = 0xE9,
};

int SerialFlash_doWRSR(unsigned char cSR, int Index);

int SerialFlash_doRDSR(unsigned char* cSR, int Index);

void S70FSxxx_Large_waitForWEL(bool die1, int Index);
int S70FSxxx_Large_doRDSR1V(bool die1, unsigned char* cSR, int Index);
int S70FSxxx_Large_doRDCR2V(bool die1, unsigned char* cSR, int Index);
void SerialFlash_waitForWEL(int Index);
bool S70FSxxx_Large_waitForWIP(bool die1, int Index);
bool SerialFlash_waitForWIP(int Index);

int SerialFlash_doWREN(int Index);

int SerialFlash_doWRDI(int Index);

int SerialFlash_protectBlock(int bProtect, int Index);

int SerialFlash_EnableQuadIO(int bEnable, int boRW, int Index);

int SerialFlash_Enable4ByteAddrMode(int bEnable, int Index);

int SerialFlash_rangeBlankCheck(struct CAddressRange* Range, int Index);

int SerialFlash_rangeProgram(struct CAddressRange* AddrRange, unsigned char* vData, int Index); 
int SerialFlash_rangeRead(struct CAddressRange* AddrRange, unsigned char* vData, int Index);

int SerialFlash_DoPolling(int Index);

int SerialFlash_is_good();
int SerialFlash_batchErase_W25Mxx_Large(uintptr_t* vAddrs, size_t AddrSize, int Index);
int SerialFlash_batchErase(uintptr_t* vAddrs, size_t AddrSize, int Index);

int SerialFlash_rangeErase(unsigned char cmd, size_t sectionSize, struct CAddressRange* AddrRange, int Index);

bool SerialFlash_chipErase(int Index);
int SerialFlash_DieErase(int Index);

int SerialFlash_bulkPipeProgram(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index);

int SerialFlash_bulkPipeProgram_twoDie(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index); 

int SerialFlash_bulkPipeRead(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index);

int SerialFlash_bulkPipeRead_Micron_4die(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index);

int SerialFlash_bulkPipeRead_twoDie(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index);

int SerialFlash_bulkPipeProgram_Micron_4Die(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index);
bool SerialFlash_doSelectDie(unsigned char dieNum,int Index);
void SerialFlash_SetCancelOperationFlag();

void SerialFlash_ClearCancelOperationFlag();

int SerialFlash_readSR(unsigned char* cSR, int Index);

int SerialFlash_writeSR(unsigned char cSR, int Index);

int SerialFlash_is_protectbits_set(int Index);
bool SST25xFxxA_protectBlock(int bProtect, int Index);
bool SST25xFxx_protectBlock(int bProtect, int Index);
bool AT25FSxxx_protectBlock(int bProtect, int Index);
bool CEN25QHxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index);
bool CN25Qxxx_LargeRDFSR(unsigned char* cSR, int Index);
bool CN25Qxxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index);
bool CN25Qxxx_MutipleDIe_LargeWREAR(unsigned char cSR, int Index);
bool CN25Qxxx_MutipleDIe_LargeRDEAR(unsigned char* cSR, int Index);
bool CN25Qxxx_Large_doRDVCR(unsigned char* ucVCR, int Index);
bool CN25Qxxx_Large_doWRVCR(unsigned char ucVCR, int Index);
bool CN25Qxxx_Large_doRDENVCR(unsigned char* ucENVCR, int Index);
bool CN25Qxxx_Large_doWRENVCR(unsigned char ucENVCR, int Index);
bool CS25FLxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index);
bool CN25Qxxx_Large_4Die_WREAR(unsigned char cSR,int Index);
bool CN25Qxxx_Large_4Die_RDEAR(unsigned char* cEAR,int Index);
size_t GetChipSize(void);
size_t GetPageSize(void);
bool SerialFlash_StartofOperation(int Index);
bool SerialFlash_EndofOperation(int Index);
bool DownloadICInfoForNand(DWORD dwInfoLen, int USBIndex);//SF700
bool Nand_ReadICInfo(SELECT_SPI_NAND_INFO *nandInfo, unsigned short *BBT, unsigned short *BBT_Cnt, int USBIndex);
bool SPINAND_ScanBadBlock( unsigned short *BBT, unsigned short *BBT_Cnt, int USBIndex);
bool SF_Nand_HSBSet( int Index);
bool SPINAND_chipErase(int USBIndex);
bool SPINAND_ProtectBlock(bool bProtect,int USBIndex);
void DownloadICInfo(bool isErase, int USBIndex);
bool SPINAND_RangeBlankCheck(const struct CAddressRange* Range,int USBIndex);
bool SPINAND_RangeProgram(const struct CAddressRange* Range,int USBIndex);
bool SPINAND_BlockProgram(const DWORD dwAddr, const unsigned char *pData, unsigned char modeWrite,unsigned char WriteCom,int USBIndex);
bool SPINAND_EnableInternalECC(bool is_ENECC,int USBIndex);
bool Nand_CaculateErrorBit(unsigned char *pData, unsigned char *pFile, unsigned int ptr_size);
bool SPINAND_RangeVerify( const struct CAddressRange* AddrRange,int USBIndex);
bool Nand_bulkPipeRead(const struct CAddressRange* AddrRange, unsigned char* pBuff,unsigned char modeRead,unsigned char ReadCom,int USBIndex);
bool SPINAND_RangeRead(const struct CAddressRange* Range,unsigned char *pBuff, int USBIndex);
bool SPINand_SpecialErase(unsigned int BlockIndex, unsigned int BlockCnt, int USBIndex);







#endif //SERIALFLASHS
