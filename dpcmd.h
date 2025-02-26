#pragma once

#ifndef _DPCMD_H
#define _DPCMD_H

#include <stdbool.h>

enum ErrorCode {
    EXCODE_PASS,
    EXCODE_FAIL_USB,
    EXCODE_FAIL_ERASE,
    EXCODE_FAIL_PROG,
    EXCODE_FAIL_VERIFY,
    EXCODE_FAIL_LOADFILEWITHVERIFY,
    EXCODE_FAIL_READ,
    EXCODE_FAIL_BLANK, // 7
    EXCODE_FAIL_BATCH,
    EXCODE_FAIL_CHKSUM,
    EXCODE_FAIL_IDENTIFY,
    EXCODE_FAIL_UPDATE_FW,
    EXCODE_FAIL_OTHERS,
};

int Sequence();
void cli_classic_usage(bool IsShowExample);
bool InitProject(void);
void CloseProject(void);
bool DetectChip(void);
void SetVpp(int Index);
void SetSPIIOMode(int Index);
void SetSPIClock(int Index);
void SetVcc(int Index);
int SetVppVoltage(int vol,int Index);
int do_loadFile(void);
int do_loadFileWithVerify(void);
void BlinkProgrammer(void);
void ListSFSerialID(void);
void do_BlankCheck(void);
void do_Erase(void);
void do_Program(void);
void do_Read(void);
void do_DisplayOrSave(void);
void SaveProgContextChanges(void);
void do_Auto(void);
void do_Verify(void);
void do_ReadSR(int Index);
void do_RawInstructions(int Index);
void do_RawInstructinos_2(int outDLen, char* para, int Index);
void RawInstructions(int Index);
bool BlankCheck(void);
bool Erase(void);
bool Program(void);
bool Read(void);
bool Auto(void);
bool Verify(void);
bool LoadFileWithVerify(void);
bool CalChecksum(void);
int Handler();
bool ListTypes(void);
bool CheckProgrammerInfo(void);
void GetLogPath(char* path);
bool Wait(const char* strOK, const char* strFail);
void ExitProgram(void);
int FirmwareUpdate();
void sin_handler(int sig);
void FillNANDContext(void);
bool ScanBB(void);
void do_ScanBB(void);
void do_NANDSpecialErase(void);
bool SpecialErase(void);


#endif
