#pragma once

#ifndef FLASHCOMMANDS
#define FLASHCOMMANDS

#include "Macro.h"

#define FlashCommand_TRUE 1
#define FlashCommand_FALSE 0

int FlashCommand_TransceiveOut(unsigned char* v, int len, int has_result_in, int Index);

int FlashCommand_TransceiveIn(unsigned char* v, int len, int Index);

int FlashCommand_SendCommand_OutOnlyInstruction(unsigned char* v, int len, int Index);

int FlashCommand_SendCommand_OutInstructionWithCS(unsigned char* v, int len, int Index);

int FlashCommand_SendCommand_OneOutOneIn(unsigned char* vOut, int out_len, unsigned char* vIn, int in_len, int Index);

int FlashCommand_SendCommand_SetupPacketForBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, unsigned int PageSize, unsigned int AddressMode, int Index);

int FlashCommand_SendCommand_SetupPacketForAT45DBBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, int Index);

int FlashCommand_SendCommand_SetupPacketForBulkRead(struct CAddressRange* AddrRange, unsigned char modeRead, unsigned char ReadCom, unsigned int AddrLen, unsigned int DummyLen, int Index);
int FlashCommand_SendCommand_SetupPacketForBulkReadNAND(unsigned int dwAddr, unsigned int PageNum, unsigned char modeRead,WORD pageSize, WORD blockPages, unsigned char ReadCom,unsigned char AddrLen, unsigned char ReadDummyLen,unsigned char nCA, int USBIndex);
bool FlashCommand_SendCommand_SetupPacketForBulkWriteNAND(unsigned int dwAddr, size_t dwLength,unsigned char modeWrite,WORD pageSize, WORD blockPages, unsigned char WriteCom,unsigned char AddrLen, unsigned char WriteDummyLen,unsigned char nCA, int USBIndex);


#endif //FLASHCOMMANDS
