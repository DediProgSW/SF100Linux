
///     CFlashCommand Implementations
#include "FlashCommand.h"
#include "Macro.h"
#include "project.h"
#include "usbdriver.h"
#define SerialFlash_FALSE -1
#define SerialFlash_TRUE 1

extern bool Is_NewUSBCommand(int Index);

int FlashCommand_TransceiveOut(unsigned char* v, int len, int has_result_in, int Index)
{
    CNTRPIPE_RQ rq;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = ((has_result_in == 1) ? RESULT_IN : NO_RESULT_IN);
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = ((has_result_in == 1) ? RESULT_IN : NO_RESULT_IN);
    }
    rq.Length = len;
#if 0
    printf("len = %d",len);
    for (i=0 ; i<len; i++) {
        printf("\nv[%d] = 0x%x\n", i, v[i]);
    }
#endif
    return OutCtrlRequest(&rq, v, len, Index);
}

int FlashCommand_TransceiveIn(unsigned char* v, int len, int Index)
{
    CNTRPIPE_RQ rq; 
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) { 
        rq.Value = 0x01;
        rq.Index = NO_REGISTER;
    } else { 
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = len;

    if (InCtrlRequest(&rq, v, (unsigned long)len, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;
    return FlashCommand_TRUE;
}

int FlashCommand_SendCommand_OutOnlyInstruction(unsigned char* v, int len, int Index)
{
    return FlashCommand_TransceiveOut(v, len, NO_RESULT_IN, Index);
}

int FlashCommand_SendCommand_OutInstructionWithCS(unsigned char* v, int len, int Index)
{
    CNTRPIPE_RQ rq;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 0x02;
        rq.Index = 0;
    } else {
        rq.Value = RFU;
        rq.Index = 0x02;
    }
    rq.Length = len;

    return OutCtrlRequest(&rq, v, len, Index);
}

int FlashCommand_SendCommand_OneOutOneIn(unsigned char* vOut, int out_len, unsigned char* vIn, int in_len, int Index)
{
    if (FlashCommand_TransceiveOut(vOut, out_len, RESULT_IN, Index) == SerialFlash_FALSE) {
        return SerialFlash_FALSE;
    }
    if (FlashCommand_TransceiveIn(vIn, in_len, Index) == SerialFlash_FALSE) {
        return SerialFlash_FALSE;
    }
    return FlashCommand_TRUE;
}

int FlashCommand_SendCommand_SetupPacketForBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, unsigned int PageSize, unsigned int AddressMode, int Index)
{ 
    unsigned char vInstruction[15];
    CNTRPIPE_RQ rq;
    // length in terms of 256/128 bytes
    size_t divider;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = WRITE;
    switch (modeWrite) {
    case MODE_NUMONYX_PCM: // 512 bytes
    case PP_32BYTE:
        divider = 9;
        break;
    case PP_128BYTE: // 128 bytes
        divider = 7;
        break;
    case PP_PROGRAM_ANYSIZE_PAGESIZE:
        if(PageSize == 0x200)
            divider = 9;
        else if(PageSize == 0x400)
            divider = 10;
        else 
            divider = 8;
    default: // 256 bytes
        divider = 8;
        break;
    }

    size_t pageNum = (AddrRange->end - AddrRange->start) >> divider;

    vInstruction[0] = (unsigned char)(pageNum & 0xff); // lowest byte of length : page number
    vInstruction[1] = (unsigned char)((pageNum >> 8) & 0xff); // highest byte of length: page number
    vInstruction[2] = (unsigned char)((pageNum >> 16) & 0xff); // reserved
    vInstruction[3] = modeWrite; // PAGE_PROGRAM, PAGE_WRITE, AAI_1_BYTE, AAI_2_BYTE, PP_128BYTE, PP_AT26DF041
    vInstruction[4] = WriteCom;

    if (Is_NewUSBCommand(Index)) { 
        vInstruction[5] = 0;
        vInstruction[6] = (AddrRange->start & 0xff);
        vInstruction[7] = ((AddrRange->start >> 8) & 0xff);
        vInstruction[8] = ((AddrRange->start >> 16) & 0xff);
        vInstruction[9] = ((AddrRange->start >> 24) & 0xff);
        vInstruction[10] = (PageSize & 0xff);
        vInstruction[11] = ((PageSize >> 8) & 0xff);
        vInstruction[12] = ((PageSize >> 16) & 0xff);
        vInstruction[13] = ((PageSize >> 24) & 0xff);
        vInstruction[14] = (AddressMode & 0xff);
        rq.Value = 0;
        rq.Index = 0;
        rq.Length = (unsigned long)(15);
    } else {
        rq.Value = (unsigned short)(AddrRange->start & 0xffff); // 16 bits LSB
        rq.Index = (unsigned short)((AddrRange->start >> 16) & 0xffff); // 16 bits MSB
        rq.Length = (unsigned long)(5);
    }
    // send rq via control pipe
    return OutCtrlRequest(&rq, vInstruction, rq.Length, Index);
}

int FlashCommand_SendCommand_SetupPacketForAT45DBBulkWrite(struct CAddressRange* AddrRange, unsigned char modeWrite, unsigned char WriteCom, int Index)
{
    /*  modeWrite:
      1: page-size = 256
      2: page-size = 264
      3: page-size = 512
      4: page-size = 528
      5: page-size = 1024
      6: page-size = 1056
  */
    size_t pageSize[7] = { 0, 256, 264, 512, 528, 1024, 1056 };

    size_t pageNum = ((AddrRange->end - AddrRange->start) + pageSize[modeWrite] - 1) / pageSize[modeWrite];
    //    printf("modeWrite = %hhu\n",modeWrite);
    //    printf("pageNum   = %lu\n",pageNum);

    unsigned char vInstruction[10];
    vInstruction[0] = (unsigned char)(pageNum & 0xff); // lowest byte of length : page number
    vInstruction[1] = (unsigned char)((pageNum >> 8) & 0xff); // highest byte of length: page number
    vInstruction[2] = (unsigned char)((pageNum >> 16) & 0xff); // reserved
    vInstruction[3] = modeWrite;
    vInstruction[4] = WriteCom;
    CNTRPIPE_RQ rq;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = ATMEL45_WRITE;

    if (Is_NewUSBCommand(Index)) {
        vInstruction[5] = 0;
        vInstruction[6] = (AddrRange->start & 0xff);
        vInstruction[7] = ((AddrRange->start >> 8) & 0xff);
        vInstruction[8] = ((AddrRange->start >> 16) & 0xff);
        vInstruction[9] = ((AddrRange->start >> 24) & 0xff);
        rq.Value = 0;
        rq.Index = 0;
        rq.Length = (unsigned long)(10);
    } else {
        rq.Value = (unsigned short)(AddrRange->start & 0xffff); // 16 bits LSB
        rq.Index = (unsigned short)((AddrRange->start >> 16) & 0xffff); // 16 bits MSB
        rq.Length = (unsigned long)(5);
    }

    // send rq via control pipe
    return OutCtrlRequest(&rq, vInstruction, rq.Length, Index);
}

int FlashCommand_SendCommand_SetupPacketForBulkRead(struct CAddressRange* AddrRange, unsigned char modeRead, unsigned char ReadCom, unsigned int AddrLen, unsigned int DummyLen, int Index)
{
    unsigned char vInstruction[12];
    CNTRPIPE_RQ rq;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = DTC_READ;

    // length in terms of 256 bytes
    size_t pageNum = AddrRange->length >> 9;


    vInstruction[0] = (unsigned char)(pageNum & 0xff); // lowest byte of length : page number
    vInstruction[1] = (unsigned char)((pageNum >> 8) & 0xff); // highest byte of length: page number
    vInstruction[2] = (unsigned char)((pageNum >> 16) & 0xff); // reserved
    vInstruction[3] = modeRead; // BULK_NORM_READ, BULK_FAST_READ
    vInstruction[4] = ReadCom;

    if (Is_NewUSBCommand(Index)) { 
        vInstruction[5] = 0xFF;
        vInstruction[6] = (AddrRange->start & 0xff);
        vInstruction[7] = ((AddrRange->start >> 8) & 0xff);
        vInstruction[8] = ((AddrRange->start >> 16) & 0xff);
        vInstruction[9] = ((AddrRange->start >> 24) & 0xff);
        vInstruction[10] = (AddrLen & 0xff);
        vInstruction[11] = (DummyLen & 0xff);
        rq.Value = 0;
        rq.Index = 0;
        rq.Length = (unsigned long)(12);
    } else {
        rq.Value = (unsigned short)(AddrRange->start & 0xffff); // 16 bits LSB
        rq.Index = (unsigned short)((AddrRange->start >> 16) & 0xffff); // 16 bits MSB
        rq.Length = (unsigned long)(5);
    }

    // send rq via control pipe
    return OutCtrlRequest(&rq, vInstruction, rq.Length, Index);
}
