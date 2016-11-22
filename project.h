#pragma once
#ifndef _PROJECT_H
#define _PROJECT_H
#include "Macro.h"
#include "dpcmd.h"

#define RES_PROG 0x10
#define RES_VERIFY 0x20
#define RES_BLANK 0x30
#define RES_ERASE 0x40
#define RES_SEND 0x50
#define RES_READ 0x60
#define RES_IDCHECK 0x70
#define RES_EZPORT 0x80

typedef struct thread_data
{
    OPERATION_TYPE type;
    int USBIndex;
}THREAD_STRUCT;
    enum
   {
       GUI_USERS = 1,
       CONSOLE_USERS = 2,
       DLL_USERS = 3
   } ;
int is_BoardVersionGreaterThan_5_0_0(int Index);
int is_SF100nBoardVersionGreaterThan_5_2_0(int Index);
int is_SF100nBoardVersionGreaterThan_5_5_0(int Index);
int is_SF600nBoardVersionGreaterThan_6_9_0(int Index);
int is_SF600nBoardVersionGreaterThan_7_0_1n6_7_0(int Index);
int GetFileFormatFromExt(const char* csPath);
CHIP_INFO GetFirstDetectionMatch(int Index);
void SetIOMode(bool isProg,int Index);
bool ReadFile(const char* csPath, unsigned char* buffer,unsigned long* FileSize,unsigned char PaddingByte);
bool WriteFile(const char* csPath, unsigned char* buffer,unsigned int FileSize);
void InitLED(int Index);
bool ProjectInitWithID(CHIP_INFO chipinfo,int Index); // by designated ID
bool ProjectInit(int Index); // by designated ID
void SetProgReadCommand(void);
void threadRun(void* Type);
void Run(OPERATION_TYPE type);
bool threadBlankCheck(int Index);
bool threadEraseWholeChip(int Index);
bool threadReadRangeChip(struct CAddressRange range,int Index);
bool threadConfiguredReadChip(int Index);
bool threadCompareFileAndChip(int Index);
bool threadReadChip(int Index);
int ReadBINFile(const char *filename,unsigned char *buf, unsigned long* size);
int WriteBINFile(const char *filename,unsigned char *buf, unsigned long size);
bool LoadFile(char* filename);
unsigned int CRC32(unsigned char* v, unsigned long size);
void TurnONVpp(void);
void TurnONVcc(int Index);
void TurnOFFVpp(void);
void TurnOFFVcc(int Idex);
bool ProgramChip(int Index);
void PrepareProgramParameters(int Index);
bool ValidateProgramParameters(int Index);
bool IdentifyChipBeforeOperation(int Index);

#endif    //_PROJECT_H
