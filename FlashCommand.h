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

int FlashCommand_SendCommand_SetupPacketForBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, int Index);

int FlashCommand_SendCommand_SetupPacketForAT45DBBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, int Index);

int FlashCommand_SendCommand_SetupPacketForBulkRead(struct CAddressRange* AddrRange, unsigned char modeRead, unsigned char ReadCom, int Index);

#endif //FLASHCOMMANDS
