#pragma once

#ifndef _BOARD_H
#define _BOARD_H

#include <stdbool.h>
#include <stddef.h>

void SendFFSequence(int Index);
void QueryBoard(int Index);
unsigned char GetFPGAVersion(int Index);
bool SetIO(unsigned char ioState, int Index);
bool SetTargetFlash(unsigned char StartupMode, int Index);
bool SetLEDProgBoard(size_t Clolor, int Index);
bool SetGreenLEDOn(bool boOn, int Index);
bool SetOrangeLEDOn(bool boOn, int Index);
bool SetRedLEDOn(bool boOn, int Index);
bool SetLEDOnOff(size_t Color, int Index);
bool SetCS(size_t value, int Index);
bool SetIOModeToSF600(size_t value, int Index);
bool BlinkProgBoard(bool boIsV5, int Index);
bool LeaveSF600Standalone(bool Enable, int Index);
bool SetSPIClockValue(unsigned short v, int Index);
unsigned int ReadUID(int Index);
bool SetSPIClockDefault(int Index);
bool EraseST7Sectors(bool bSect1, int Index);
bool ProgramSectors(const char* sFilePath, bool bSect1, int Index);
bool UpdateChkSum(int Index);
bool WriteUID(unsigned int dwUID, int Index);
bool WriteManufacturerID(unsigned char ManuID, int Index);
bool EncrypFirmware(unsigned char* vBuffer, unsigned int Size, int Index);
bool UpdateSF600Flash(const char* sFilePath, int Index);
bool WriteSF600UID(unsigned int dwUID, unsigned char ManuID, int Index);
bool UpdateSF600Flash_FPGA(const char* sFilePath, int Index);
bool UpdateSF600Firmware(const char* sFolder, int Index);
bool UpdateFirmware(const char* sFolder, int Index);

#endif
