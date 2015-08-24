
#pragma once

#ifndef _BOARD_H
#define _BOARD_H

void QueryBoard(int Index);
unsigned char GetFPGAVersion(int Index);
bool SetIO(unsigned char ioState, int Index);
bool SetTargetFlash(unsigned char StartupMode,int Index);
bool SetLEDProgBoard(size_t Clolor,int Index);
bool SetGreenLEDOn(bool boOn,int Index);
bool SetOrangeLEDOn(bool boOn, int Index);
bool SetRedLEDOn(bool boOn, int Index);
bool SetLEDOnOff(size_t Color,int Index);
bool SetCS(size_t value,int Index);
bool SetIOModeToSF600(size_t value,int Index);
bool BlinkProgBoard(bool boIsV5,int Index);
bool ReadOnBoardFlash(unsigned char* Data,bool ReadUID,int Index);
bool LeaveSF600Standalone(bool Enable,int USBIndex);
bool SetSPIClockValue(unsigned short v,int Index);
unsigned int ReadUID(int Index);

#endif

