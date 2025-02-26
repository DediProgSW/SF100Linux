#include "SerialFlash.h"
#include "ChipInfoDb.h"
#include "FlashCommand.h"
#include "project.h"
#include "dpcmd.h"
#include "usbdriver.h"
#include "board.h"
#include <stdlib.h>
#include <sys/stat.h>

extern int m_isCanceled;
extern int m_bProtectAfterWritenErase;
extern int m_boEnReadQuadIO;
extern int m_boEnWriteQuadIO;
extern bool g_bSpareAreaUseFile;
extern bool g_bNandForceErase;
extern CHIP_INFO g_ChipInfo;
extern struct CNANDContext g_NANDContext;
extern bool g_bNandInternalECC;
extern int g_iNandBadBlockManagement;
extern struct BadBlockTable g_BBT[16];
extern volatile bool g_bIsSF600[16];
extern volatile bool g_bIsSF700[16];
extern volatile bool g_bIsSF600PG2[16]; 
extern unsigned char *pBufferforLoadedFile;
extern bool Is_NewUSBCommand(int Index);
extern unsigned int CRC32(unsigned char* v, unsigned long size);

unsigned char mcode_WRSR = 0x01;
unsigned char mcode_WRDI = 0x04;
unsigned char mcode_RDSR = 0x05;
unsigned char mcode_WREN = 0x06;
unsigned char mcode_SegmentErase = SE;
unsigned char mcode_ChipErase = CHIP_ERASE;
unsigned char mcode_Program = PAGE_PROGRAM;
unsigned char mcode_ProgramCode_4Adr = 0x02;
unsigned char mcode_ProgramCode_4Adr_12 = 0x12;
unsigned char mcode_Read = BULK_FAST_READ;
unsigned char mcode_ReadCode = 0x0B;
unsigned char mcode_ReadCode_0C = 0x0C;
unsigned int g_AT45_PageSize = 0;
unsigned int g_AT45_PageSizeMask = 0;
size_t AT45ChipSize = 0;
size_t AT45PageSize = 0;

bool AT45doRDSR(unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; //size 1

    // first control packet
    vInstruction = 0xD7;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = (unsigned long)1;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = (unsigned long)1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

bool AT45WaitForWIP(int USBIndex)
{
    unsigned char cSR;
    size_t i = g_ChipInfo.Timeout * 100;
    if (i == 0)
        i = 0x10000;
    // wait until WIP = 0
    do {
        AT45doRDSR(&cSR, USBIndex);
        Sleep(10);
    } while ((!(cSR & 0x80)) && (i-- > 0)); // poll bit 7

    if (i <= 0)
        return false;

    return true;
}

unsigned char getWriteMode(int USBIndex)
{
    unsigned char cMode;
    unsigned char cSR;
    AT45doRDSR(&cSR, USBIndex);
    bool powerOfTwo = (1 == (cSR & 0x1));

    //    typedef enum
    enum {
        AT45DB011D = 0x1F22,
        AT45DB021D = 0x1F23,
        AT45DB041D = 0x1F24,
        AT45DB081D = 0x1F25,
        AT45DB161D = 0x1F26,
        AT45DB321D = 0x1F27,
        AT45DB642D = 0x1F28,
    };

    switch (g_ChipInfo.UniqueID) {
    case AT45DB011D:
    case AT45DB021D:
    case AT45DB041D:
    case AT45DB081D:
        cMode = powerOfTwo ? 1 : 2; //256 : 264;
        break;
    case AT45DB161D:
    case AT45DB321D:
        cMode = powerOfTwo ? 3 : 4; //512 : 528;
        break;
    case AT45DB642D:
        cMode = powerOfTwo ? 5 : 6; //1024 : 1056;
        break;
    default:
        cMode = powerOfTwo ? 1 : 2; //256 : 264;
    }
    return cMode;
}

void SetPageSize(CHIP_INFO* mem, int USBIndex)
{
    unsigned char writeMode;
    writeMode = getWriteMode(USBIndex);
    mcode_Program = writeMode;
    size_t pageSize[7] = { 0, 256, 264, 512, 528, 1024, 1056 };
    //    size_t pageSizeMask[7] = {0, (1<<8)-1, (1<<9)-1, (1<<9)-1, (1<<10)-1, (1<<10)-1, (1<<11)-1};
    mem->PageSizeInByte = pageSize[writeMode];

    if (!(writeMode & 0x1)) // for AT45DB:0x1F2200 - 0x1F2800
        mem->ChipSizeInByte = g_ChipInfo.ChipSizeInByte / 256 * 8 + g_ChipInfo.ChipSizeInByte;
    else
        mem->ChipSizeInByte = g_ChipInfo.ChipSizeInByte;

    AT45ChipSize = mem->ChipSizeInByte;
    AT45PageSize = mem->PageSizeInByte;
}

size_t GetChipSize(void)
{
    return AT45ChipSize;
}

size_t GetPageSize(void)
{
    return AT45PageSize;
}

bool AT45xxx_protectBlock(int bProtect, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[4]; //size 1

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = (unsigned long)4;
    // protect block ,set BP1 BP0 to 1
    if (bProtect) {
        vInstruction[0] = 0x3D;
        vInstruction[1] = 0x2A;
        vInstruction[2] = 0x7F;
        vInstruction[3] = 0xA9;
    } else {
        vInstruction[0] = 0x3D;
        vInstruction[1] = 0x2A;
        vInstruction[2] = 0x7F;
        vInstruction[3] = 0x9A;
    }

    if (OutCtrlRequest(&rq, vInstruction, 4, Index) == SerialFlash_FALSE)
        return false;
    return true;
}

bool AT45rangeProgram(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{
    CHIP_INFO mem_id;

    SetPageSize(&mem_id, Index);
    modeWrite = getWriteMode(Index);
    size_t packageNum = (AddrRange->end - AddrRange->start + mem_id.PageSizeInByte - 1) / mem_id.PageSizeInByte;
    size_t pageSize[2] = { 264, 256 };
    unsigned char idx = modeWrite & 0x1;
    size_t itrCnt = packageNum * mem_id.PageSizeInByte / pageSize[idx];
    unsigned char* itr_begin;
    size_t i;
    //    AT45xxx_protectBlock(false,Index);

    FlashCommand_SendCommand_SetupPacketForAT45DBBulkWrite(AddrRange, modeWrite, WriteCom, Index);

    itr_begin = vData;

    for (i = 0; i < itrCnt; ++i) {
        bool b = BulkPipeWrite(itr_begin + (i * pageSize[idx]), pageSize[idx], USB_TIMEOUT, Index);
        if ((!b) || m_isCanceled) {
            return false;
        }
    }
    //    Sleep(10) ;
    return true;
}

int AT45rangSectorErase(size_t sectionSize, struct CAddressRange AddrRange, int USBIndex)
{
    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[5] = { 0x7C, 0x50, 0x50, 0x50, 0x50 };

    // instrcution format
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(USBIndex)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 4;

    size_t sectorNum = (AddrRange.end - AddrRange.start + sectionSize - 1) / sectionSize;
    size_t i;

    for (i = 0; i < sectorNum; ++i) {
        size_t addr = (AddrRange.start + i * sectionSize);
        vInstruction[1] = (unsigned char)((addr >> 16) & 0xff); //MSB
        vInstruction[2] = (unsigned char)((addr >> 8) & 0xff); //M
        vInstruction[3] = (unsigned char)(addr & 0xff); //LSB

        int b = OutCtrlRequest(&rq, vInstruction, 4, USBIndex);
        if ((b == SerialFlash_FALSE) || m_isCanceled)
            return false;

        if (AT45WaitForWIP(USBIndex) == false)
            return false;
    }

    return true;
}

bool AT45batchErase(size_t* vAddrs, size_t AddrSize, int USBIndex)
{
    CHIP_INFO mem_id;
    int i;
    SetPageSize(&mem_id, USBIndex);
    if (strcmp(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) == 0) {
        mem_id.PageSizeInByte = g_ChipInfo.PageSizeInByte;
        mem_id.ChipSizeInByte = g_ChipInfo.ChipSizeInByte;
    }
    mem_id.SectorSizeInByte = g_ChipInfo.SectorSizeInByte;

    struct CAddressRange range;

    for (i = 0; i < AddrSize; i++) {
        switch (mem_id.ChipSizeInByte) {
        case (1 << 17): // 1Mbit
            if (vAddrs[i] < (1 << 11)) {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 11);
                AT45rangSectorErase(1 << 11, range, USBIndex);
            } else {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 15);
                AT45rangSectorErase(1 << 15, range, USBIndex);
            }
            break;
        case (1 << 17) * 264 / 256: // 1Mbit
            if (vAddrs[i] < (1 << 12)) {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 12);
                AT45rangSectorErase(1 << 12, range, USBIndex);
            } else {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 16);
                AT45rangSectorErase(1 << 16, range, USBIndex);
            }
            break;
        case (1 << 18): // 2Mbit
            range.start = 0;
            range.end = 1 << 12;
            AT45rangSectorErase(1 << 11, range, USBIndex);
            range.start = 1 << 16;
            range.end = mem_id.ChipSizeInByte;
            AT45rangSectorErase(1 << 15, range, USBIndex);
            break;
        case (1 << 18) * 264 / 256: // 2Mbit
            range.start = 0;
            range.end = 1 << 13;
            AT45rangSectorErase(1 << 12, range, USBIndex);
            range.start = 1 << 16;
            range.end = mem_id.ChipSizeInByte << 1;
            AT45rangSectorErase(1 << 16, range, USBIndex);
            break;
        case (1 << 19): // 4Mbit
            range.start = 0;
            range.end = 1 << 12;
            AT45rangSectorErase(1 << 11, range, USBIndex);
            range.start = 1 << 16;
            range.end = mem_id.ChipSizeInByte;
            AT45rangSectorErase(1 << 16, range, USBIndex);
            break;
        case (1 << 19) * 264 / 256: // 4Mbit
            range.start = 0;
            range.end = 1 << 13;
            AT45rangSectorErase(1 << 12, range, USBIndex);
            range.start = 1 << 17;
            range.end = mem_id.ChipSizeInByte << 1;
            AT45rangSectorErase(1 << 17, range, USBIndex);
            break;
        case (1 << 20): // 8Mbit
            range.start = 0;
            range.end = 1 << 12;
            AT45rangSectorErase(1 << 11, range, USBIndex);
            range.start = 1 << 16;
            range.end = mem_id.ChipSizeInByte;
            AT45rangSectorErase(1 << 16, range, USBIndex);
            break;
        case (1 << 20) * 264 / 256: // 8Mbit
            range.start = 0;
            range.end = 1 << 13;
            AT45rangSectorErase(1 << 12, range, USBIndex);
            range.start = 1 << 17;
            range.end = mem_id.ChipSizeInByte << 1;
            AT45rangSectorErase(1 << 17, range, USBIndex);
            break;
        case (1 << 21): // 16Mbit
            if (vAddrs[i] < (1 << 13)) {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 12);
                AT45rangSectorErase(1 << 12, range, USBIndex);
            } else {
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 17);
                AT45rangSectorErase(1 << 17, range, USBIndex);
            }
            break;
        case (1 << 21) * 264 / 256: // 16Mbit
            if (vAddrs[i] < (1 << 14)) { 
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 13);
                AT45rangSectorErase(1 << 13, range, USBIndex);
            } else { 
                range.start = vAddrs[i];
                range.end = vAddrs[i] + (1 << 18);
                AT45rangSectorErase(1 << 18, range, USBIndex);
            }
            break;
        case (1 << 22): // 32Mbit
            range.start = 0;
            range.end = 1 << 13;
            AT45rangSectorErase(1 << 12, range, USBIndex);
            range.start = 1 << 18;
            range.end = mem_id.ChipSizeInByte;
            AT45rangSectorErase(1 << 18, range, USBIndex);
            break;
        case (1 << 22) / 256 * 264: // 32Mbit
            range.start = 0;
            range.end = 1 << 14;
            AT45rangSectorErase(1 << 13, range, USBIndex);
            range.start = 1 << 17;
            range.end = mem_id.ChipSizeInByte << 1;
            AT45rangSectorErase(1 << 17, range, USBIndex);
            break;
        case (1 << 23): // 64Mbit
            range.start = 0;
            range.end = 1 << 14;
            AT45rangSectorErase(1 << 13, range, USBIndex);
            range.start = 1 << 18;
            range.end = mem_id.ChipSizeInByte;
            AT45rangSectorErase(1 << 18, range, USBIndex);
            break;
        case (1 << 23) / 256 * 264: // 64Mbit
            range.start = 0;
            range.end = 1 << 15;
            AT45rangSectorErase(1 << 14, range, USBIndex);
            range.start = 1 << 19;
            range.end = mem_id.ChipSizeInByte << 1;
            AT45rangSectorErase(1 << 19, range, USBIndex);
            break;
        default:
            // CSerialFlash::chipErase();
            break;
        };
    }
    return true;
}

bool AT45chipErase(unsigned int Addr, unsigned int Length, int USBIndex)
{
    unsigned char com[4] = { 0xC7, 0x94, 0x80, 0x9A };
    int b = FlashCommand_SendCommand_OutOnlyInstruction(com, 4, USBIndex);
    if (b == SerialFlash_FALSE)
        return false;

    if (AT45WaitForWIP(USBIndex) == false)
        return false;

    return true;

    CHIP_INFO mem_id;
    SetPageSize(&mem_id, USBIndex);
    if (strcmp(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) == 0) {
        mem_id.PageSizeInByte = g_ChipInfo.PageSizeInByte;
        mem_id.ChipSizeInByte = g_ChipInfo.ChipSizeInByte;
    }
    mem_id.SectorSizeInByte = g_ChipInfo.SectorSizeInByte;
    //    AT45xxx_protectBlock(false,USBIndex);

    struct CAddressRange range;
    switch (mem_id.ChipSizeInByte) {
    case (1 << 17): // 1Mbit
        range.start = 0;
        range.end = 1 << 12;
        AT45rangSectorErase(1 << 11, range, USBIndex);
        range.start = 1 << 15;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 15, range, USBIndex);
        break;
    case (1 << 17) * 264 / 256: // 1Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 16;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 16, range, USBIndex);
        break;
    case (1 << 18): // 2Mbit
        range.start = 0;
        range.end = 1 << 12;
        AT45rangSectorErase(1 << 11, range, USBIndex);
        range.start = 1 << 16;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 15, range, USBIndex);
        break;
    case (1 << 18) * 264 / 256: // 2Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 16;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 16, range, USBIndex);
        break;
    case (1 << 19): // 4Mbit
        range.start = 0;
        range.end = 1 << 12;
        AT45rangSectorErase(1 << 11, range, USBIndex);
        range.start = 1 << 16;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 16, range, USBIndex);
        break;
    case (1 << 19) * 264 / 256: // 4Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 17;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 17, range, USBIndex);
        break;
    case (1 << 20): // 8Mbit
        range.start = 0;
        range.end = 1 << 12;
        AT45rangSectorErase(1 << 11, range, USBIndex);
        range.start = 1 << 16;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 16, range, USBIndex);
        break;
    case (1 << 20) * 264 / 256: // 8Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 17;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 17, range, USBIndex);
        break;
    case (1 << 21): // 16Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 17;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 17, range, USBIndex);
        break;
    case (1 << 21) * 264 / 256: // 16Mbit
        range.start = 0;
        range.end = 1 << 14;
        AT45rangSectorErase(1 << 13, range, USBIndex);
        range.start = 1 << 18;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 18, range, USBIndex);
        break;
    case (1 << 22): // 32Mbit
        range.start = 0;
        range.end = 1 << 13;
        AT45rangSectorErase(1 << 12, range, USBIndex);
        range.start = 1 << 18;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 18, range, USBIndex);
        break;
    case (1 << 22) / 256 * 264: // 32Mbit
        range.start = 0;
        range.end = 1 << 14;
        AT45rangSectorErase(1 << 13, range, USBIndex);
        range.start = 1 << 17;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 17, range, USBIndex);
        break;
    case (1 << 23): // 64Mbit
        range.start = 0;
        range.end = 1 << 14;
        AT45rangSectorErase(1 << 13, range, USBIndex);
        range.start = 1 << 18;
        range.end = mem_id.ChipSizeInByte;
        AT45rangSectorErase(1 << 18, range, USBIndex);
        break;
    case (1 << 23) / 256 * 264: // 64Mbit
        range.start = 0;
        range.end = 1 << 15;
        AT45rangSectorErase(1 << 14, range, USBIndex);
        range.start = 1 << 19;
        range.end = mem_id.ChipSizeInByte << 1;
        AT45rangSectorErase(1 << 19, range, USBIndex);
        break;
    default:
        // CSerialFlash::chipErase();
        break;
    };
    return true;
}

// write status register , just 1 bytes
// WRSR only is effects on SRWD BP2 BP1 BP0 when WEL = 1
int SerialFlash_doWRSR(unsigned char cSR, int Index)
{
    //simon	if(! m_usb.is_open())
    //Simon		return false ;

    // wait until WIP = 0
    SerialFlash_waitForWIP(Index);

    // wait until WEL = 1
    SerialFlash_waitForWEL(Index);

    // send request
    unsigned char vInstruction[2];

    vInstruction[0] = mcode_WRSR;
    vInstruction[1] = cSR;

    return FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 2, Index);
}

int SerialFlash_doRDSR(unsigned char* cSR, int Index)
{
    // read status
    //Simon	if(! m_usb.is_open() )
    //Simon		return false ;

    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; //size 1

    // first control packet
    vInstruction = mcode_RDSR;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;
    return SerialFlash_TRUE;
}

int S70FSxxx_Large_doRDSR1V(bool die1, unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[7] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x65;
    vInstruction[1] = ((0x800000 >> 24) | (die1 ? 0 : (0x4000000 >> 24)));
    vInstruction[2] = (unsigned char)((0x800000 >> 16) & 0xff);
    vInstruction[3] = (unsigned char)((0x800000 >> 8) & 0xff);
    vInstruction[4] = (unsigned char)((0x800000) & 0xff);
    vInstruction[5] = 0;
    vInstruction[6] = 0;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 7; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 7, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

int S70FSxxx_Large_doRDCR2V(bool die1, unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[7] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x65;
    vInstruction[1] = ((0x800003 >> 24) | (die1 ? 0 : (0x4000000 >> 24)));
    vInstruction[2] = (unsigned char)((0x800003 >> 16) & 0xff);
    vInstruction[3] = (unsigned char)((0x800003 >> 8) & 0xff);
    vInstruction[4] = (unsigned char)((0x800003) & 0xff);
    vInstruction[5] = 0;
    vInstruction[6] = 0;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 7; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 7, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

int MX25Lxxx_Large_doRDCR(unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[1] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x15;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

int MX25Lxxx_Large_doRDSR(unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[1] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x05;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

int MX25Lxxx_Large_doWRCR(unsigned char cCR, int Index)
{
    unsigned char cSR;

    CNTRPIPE_RQ rq;
    unsigned char vInstruction[1] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x05;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    cSR = vBuffer;

    // wait until WIP = 0
    SerialFlash_waitForWIP(Index);

    // wait until WEL = 1
    SerialFlash_waitForWEL(Index);
    // send request
    unsigned char vInstruction2[3];

    vInstruction2[0] = 0x01;
    vInstruction2[1] = cSR;
    vInstruction2[2] = cCR;

    return FlashCommand_SendCommand_OutOnlyInstruction(vInstruction2, 3, Index);
}
//
/**
 * \brief
 * wait for write enable with a timeout.
 *
 * Unless the memory is malfunctioning, it should return
 * before the timer counts down to 0 from 0xFFFFFFFF.
 *
 * \remarks
 *  SR bitmap:
 *   7       6   5   4   3   2   1   0
 *   SRWD    0   0   BP2 BP1 BP0 WEL WIP
 *
 */
void SerialFlash_waitForWEL(int Index)
{
    unsigned char cSR;
    size_t i = 10;
    // wait until WIP = 0 and WEL = 1
    do {
        SerialFlash_doWREN(Index);

        // read SR to check WEL until WEL = 1
        SerialFlash_doRDSR(&cSR, Index);
    } while (((cSR & 0x02) == 0) && (i-- > 0));
}

void S70FSxxx_Large_waitForWEL(bool die1, int Index)
{
    unsigned char cSR = 0;
    size_t i = 10;

    do { // wait until WIP = 0 and WEL = 1
        SerialFlash_doWREN(Index);

        // read SR to check WEL until WEL = 1
        S70FSxxx_Large_doRDSR1V(die1, &cSR, Index);
    } while (((cSR & 0x02) == 0) && (i-- > 0));
}

bool S25FSxxS_Large_doRDCF3V(unsigned char *cSR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[6] = { 0 }; //size 1

    // first control packet
    vInstruction[0] = 0x65;
    vInstruction[1] = (unsigned char)((0x00800004 >> 24) & 0xff);
    vInstruction[2] = (unsigned char)((0x00800004 >> 16) & 0xff);
    vInstruction[3] = (unsigned char)((0x00800004 >> 8) & 0xff);
    vInstruction[4] = (unsigned char)((0x00800004) & 0xff);
    vInstruction[5] = 0;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 6; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, vInstruction, 6, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

/**
 * \brief
 * wait Write In Process with a timeout.
 *
 * Unless the memory is malfunctioning, it should return
 * before the timer counts down to 0 from 0xFFFFFFFF.
 *
 * \remarks
 *  SR bitmap:
 *   7       6   5   4   3   2   1   0
 *   SRWD    0   0   BP2 BP1 BP0 WEL WIP
 *
 */
bool S70FSxxx_Large_waitForWIP(bool die1, int Index)
{
    unsigned char cSR;
    size_t i = g_ChipInfo.Timeout * 100;
    if (i == 0)
        i = MAX_TRIALS;
    // wait until WIP = 0
    do {
        S70FSxxx_Large_doRDSR1V(die1, &cSR, Index);
        Sleep(5);
    } while ((cSR & 0x01) && (i-- > 0));
    if (i <= 0)
        return false;
    return true;
}

bool SerialFlash_waitForWIP(int Index)
{
    unsigned char cSR;
    size_t i = g_ChipInfo.Timeout * 100;
    if (i == 0)
        i = MAX_TRIALS;
    // wait until WIP = 0
    do {
        SerialFlash_doRDSR(&cSR, Index);
        Sleep(5);
    } while ((cSR & 0x01) && (i-- > 0) && (cSR != 0xFF));
    if (i <= 0)
        return false;
    return true;
}

int SerialFlash_doWREN(int Index)
{
    unsigned char v = mcode_WREN;
    return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, Index);
}

int SerialFlash_doWRDI(int Index)
{
    unsigned char v = mcode_WRDI;
    return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, Index);
}

bool SST25xFxx_protectBlock(int bProtect, int Index)
{
    bool result = false;
    unsigned char tmpSRVal;
    unsigned char dstSRVal;

    SerialFlash_waitForWIP(Index);
    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00;
    // protect block ,set BP1 BP0 to 1
    if (bProtect) {
        dstSRVal |= 0x8C; // 8C : 1000 1100
    }

    unsigned char vInstruction[2];
    vInstruction[0] = 0x50; //Write enable
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 1, Index);

    int numOfRetry = 1000;
    vInstruction[0] = 0x01; //Write register
    vInstruction[1] = dstSRVal;
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 2, Index);

    result = SerialFlash_doRDSR(&tmpSRVal, Index);
    while ((tmpSRVal & 0x01) && numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal, Index);

        if (!result)
            return false;

        numOfRetry--;
    };

    return ((tmpSRVal ^ dstSRVal) & 0x0C) ? false : true;
}

bool SST25xFxxA_protectBlock(int bProtect, int Index)
{
    bool result = false;
    unsigned char tmpSRVal;
    unsigned char dstSRVal;

    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00;
    // protect block ,set BP1 BP0 to 1
    if (bProtect) {
        dstSRVal |= 0x0; // 8C : 1000 1100
    }

    int numOfRetry = 1000;
    result = SerialFlash_doWRSR(dstSRVal, Index);
    result = SerialFlash_doRDSR(&tmpSRVal, Index);
    while ((tmpSRVal & 0x01) && numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal, Index);

        if (!result)
            return false;

        numOfRetry--;
    };

    return ((tmpSRVal ^ dstSRVal) & 0x0C) ? false : true;
}

bool AT25FSxxx_protectBlock(int bProtect, int Index)
{
    bool result = false;
    unsigned char tmpSRVal;
    unsigned char dstSRVal;

    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00;
    // protect block ,set BP1 BP0 to 1
    if (bProtect) {
        dstSRVal |= 0xFF; // 8C : 1000 1100
    }

    int numOfRetry = 1000;
    result = SerialFlash_doWRSR(dstSRVal, Index);
    result = SerialFlash_doRDSR(&tmpSRVal, Index);
    while ((tmpSRVal & 0x01) && numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal, Index);

        if (!result)
            return false;

        numOfRetry--;
    };

    return ((tmpSRVal ^ dstSRVal) & 0x0C) ? false : true;
}

bool AT26Fxxx_protectBlock(int bProtect, int Index)
{
    const unsigned int AT26DF041 = 0x1F4400;
    const unsigned int AT26DF004 = 0x1F0400;
    const unsigned int AT26DF081A = 0x1F4501;
    enum {
        PROTECTSECTOR = 0x36, // Write Enable
        UNPROTECTSECTOR = 0x39, // Write Disable
        READPROTECTIONREGISTER = 0x3C, // Write Disable
    };
    if (AT26DF041 == g_ChipInfo.UniqueID)
        return true; // feature is not supported on this chip

    bool result = false;

    int numOfRetry = 1000;

    result = SerialFlash_doWRSR(0, Index);

    unsigned char tmpSRVal;
    do {
        result = SerialFlash_doRDSR(&tmpSRVal, Index);
        numOfRetry--;

    } while ((tmpSRVal & 0x01) && numOfRetry > 0 && result);

    if (tmpSRVal & 0x80)
        return false; ///< enable SPRL

    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[4] = { 0 };

    vInstruction[0] = bProtect ? PROTECTSECTOR : UNPROTECTSECTOR;

    size_t iUniformSectorSize = 0x10000; // always regarded as 64K
    size_t cnt = g_ChipInfo.ChipSizeInByte / iUniformSectorSize;
    size_t i;
    for (i = 0; i < cnt; ++i) {
        SerialFlash_doWREN(Index);

        vInstruction[1] = (unsigned char)i;

        rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
        rq.Direction = VENDOR_DIRECTION_OUT;
        rq.Request = TRANSCEIVE;
        if (Is_NewUSBCommand(Index)) {
            rq.Value = RESULT_IN;
            rq.Index = RFU;
        } else {
            rq.Value = RFU;
            rq.Index = RESULT_IN;
        }
        rq.Length = 4;

        if (OutCtrlRequest(&rq, vInstruction, 4, Index) == SerialFlash_FALSE)
            return false;
    }

    if (AT26DF081A == g_ChipInfo.UniqueID || AT26DF004 == g_ChipInfo.UniqueID) // 8K each for the last 64K
    {
        vInstruction[1] = (unsigned char)(cnt - 1);
        for (i = 1; i < 8; ++i) {
            SerialFlash_doWREN(Index);

            vInstruction[2] = (unsigned char)(i << 5);

            rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
            rq.Direction = VENDOR_DIRECTION_OUT;
            rq.Request = TRANSCEIVE;
            if (Is_NewUSBCommand(Index)) {
                rq.Value = RESULT_IN;
                rq.Index = RFU;
            } else {
                rq.Value = RFU;
                rq.Index = RESULT_IN;
            }
            rq.Length = 4;

            if (OutCtrlRequest(&rq, vInstruction, 4, Index) == SerialFlash_FALSE)
                return false;
        }
    }

    return true;
}

bool CMX25LxxxdoRDSCUR(unsigned char* cSR, int Index)
{
    // read status
    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction = RDSCUR; //size 1

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = RFU;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

bool CS25FLxxx_LargedoUnlockDYB(unsigned int cSR, int Index)
{
    // wait until WIP = 0
    if (SerialFlash_waitForWIP(Index) == false)
        return false;

    unsigned char vInstruction[15];

    int i;
    unsigned int topend, bottomstart, end;

    if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL) //256
    {
        topend = 0x20000;
        bottomstart = 0x1fe0000;
        end = 0x2000000;
    } else {
        topend = 0x20000;
        bottomstart = 0xfe0000;
        end = 0x1000000;
    }

    for (i = 0; i < topend; i = i + 0x1000) //top 4k sectors
    {
        SerialFlash_waitForWEL(Index);
        vInstruction[0] = 0xE1; //write DYB
        vInstruction[1] = (unsigned char)(i >> 24);
        vInstruction[2] = (unsigned char)(i >> 16);
        vInstruction[3] = (unsigned char)(i >> 8);
        vInstruction[4] = (unsigned char)(i);
        vInstruction[5] = 0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 6, Index);
        SerialFlash_waitForWIP(Index);
    }

    for (i = topend; i < bottomstart; i = i + 0x10000) //64k sectors
    {
        SerialFlash_waitForWEL(Index);
        vInstruction[0] = 0xE1; //write DYB
        vInstruction[1] = (unsigned char)(i >> 24);
        vInstruction[2] = (unsigned char)(i >> 16);
        vInstruction[3] = (unsigned char)(i >> 8);
        vInstruction[4] = (unsigned char)(i);
        vInstruction[5] = 0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 6, Index);
        SerialFlash_waitForWIP(Index);
    }

    for (i = bottomstart; i < end; i = i + 0x1000) //bottom 4k sectors
    {
        SerialFlash_waitForWEL(Index);
        vInstruction[0] = 0xE1; //write DYB
        vInstruction[1] = (unsigned char)(i >> 24);
        vInstruction[2] = (unsigned char)(i >> 16);
        vInstruction[3] = (unsigned char)(i >> 8);
        vInstruction[4] = (unsigned char)(i);
        vInstruction[5] = 0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 6, Index);
        SerialFlash_waitForWIP(Index);
    }
    return true;
}

int SerialFlash_protectBlock(int bProtect, int Index)
{
    if (strcmp(g_ChipInfo.Class, SUPPORT_SST_25xFxx) == 0 || strstr(g_ChipInfo.Class, SUPPORT_SST_25xFxxB) != NULL) // || strstr(g_ChipInfo.Class,SUPPORT_SST_25xFxxC)!=NULL)
        return SST25xFxx_protectBlock(bProtect, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_SST_25xFxxA) != NULL)
        return SST25xFxxA_protectBlock(bProtect, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT25FSxxx) != NULL || strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT25Fxxx) != NULL)
        return AT25FSxxx_protectBlock(bProtect, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT26xxx) != NULL)
        return AT26Fxxx_protectBlock(bProtect, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx) != NULL || strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL) {
        unsigned char tmpSRVal;
        bool result;
        result = CMX25LxxxdoRDSCUR(&tmpSRVal, Index);
        if (result == true && (tmpSRVal & 0x80) && g_ChipInfo.MXIC_WPmode == true) {
            if (bProtect != false)
                return true;
            SerialFlash_doWREN(Index);

            unsigned char v = GBULK;
            return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, Index);
        }
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx) != NULL || strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL) {
        if (bProtect == false && strstr(g_ChipInfo.TypeName, "Secure") != NULL) {
            CS25FLxxx_LargedoUnlockDYB(0, Index);
        }
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SST_26xFxxC) != NULL) {
        unsigned char v = 0x98;
        SerialFlash_waitForWEL(Index);
        FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, Index);
    }
    if (SerialFlash_is_protectbits_set(Index) == bProtect)
        return 1;

    bool result = false;
    unsigned char tmpSRVal;
    unsigned char dstSRVal;
    //int numOfRetry = 3 ;
    // un-protect block ,set BP2 BP1 BP2 to 0
    dstSRVal = 0x00;
    // protect block ,set BP2 BP1 BP2 to 1
    if (bProtect) {
        dstSRVal += 0x9C; // 9C : 9001 1100
    }

    int numOfRetry = 1000;
    result = SerialFlash_doWRSR(dstSRVal, Index);
    result = SerialFlash_doRDSR(&tmpSRVal, Index);
    while ((tmpSRVal & 0x01) && numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal, Index);

        if (!result)
            return false;

        numOfRetry--;
    };

    return ((tmpSRVal ^ dstSRVal) & 0x0C) ? 0 : 1;
}

int SerialFlash_EnableQuadIO(int bEnable, int boRW, int Index)
{
    m_boEnWriteQuadIO = (bEnable & boRW); //Simon: ported done???
    m_boEnReadQuadIO = (bEnable & boRW); //Simon: ported done???
    return SerialFlash_TRUE;
}

bool CEN25QHxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index)
{
    //    SerialFlash_doWREN(Index);
    if (Enable4Byte) {
        unsigned char v = EN4B;
        if (FlashCommand_TransceiveOut(&v, 1, false, Index) == SerialFlash_FALSE)
            return false;
    } else {
        unsigned char v = EXIT4B;
        if (FlashCommand_TransceiveOut(&v, 1, false, Index) == SerialFlash_FALSE)
            return false;
    }
    return true;
}

bool CN25Qxxx_LargeRDFSR(unsigned char* cSR, int Index)
{
    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction = 0x70; //size 1

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;
    return SerialFlash_TRUE;
}

bool CN25Qxxx_Large_4Die_WREAR(unsigned char cSR,int Index)
{ 
    SerialFlash_waitForWIP(Index);
    SerialFlash_waitForWEL(Index);

    unsigned char vInstruction[2];
    vInstruction[0] = 0xC5 ;
    vInstruction[1] = cSR&0xFF; 

    FlashCommand_TransceiveOut(vInstruction, 2, false, Index);

    SerialFlash_waitForWIP(Index);
    return SerialFlash_TRUE;
}

bool CN25Qxxx_Large_4Die_RDEAR(unsigned char* cEAR, int Index)
{
	 
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; 
    vInstruction = 0xC8;
  
   
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;
         
    *cEAR = vBuffer; 
    return SerialFlash_TRUE;
}

bool CN25Qxxx_Large_doWRVCR(unsigned char ucVCR, int Index)
{
    unsigned char vInstruction[2]; //size 1

    SerialFlash_waitForWIP(Index);
    SerialFlash_waitForWEL(Index);
    vInstruction[0] = 0x81;
    vInstruction[1] = ucVCR & 0xFF;

    FlashCommand_TransceiveOut(vInstruction, 2, false, Index);

    SerialFlash_waitForWIP(Index); 
    return true;
}

bool CN25Qxxx_Large_doRDENVCR(unsigned char* ucENVCR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; //size 1
    vInstruction = 0x65;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;
    *ucENVCR = vBuffer;
    return SerialFlash_TRUE;
}

bool CN25Qxxx_Large_doWRENVCR(unsigned char ucENVCR, int Index)
{
    unsigned char vInstruction[2]; //size 1

    SerialFlash_waitForWIP(Index);
    SerialFlash_waitForWEL(Index);
    vInstruction[0] = 0x61;
    vInstruction[1] = ucENVCR & 0xFF;

    FlashCommand_TransceiveOut(vInstruction, 2, false, Index); 
    return true;
}

bool CN25Qxxx_Large_doRDVCR(unsigned char* ucVCR, int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; //size 1
    vInstruction = 0x85;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;
    *ucVCR = vBuffer;
    return SerialFlash_TRUE;
}
bool CN25Qxxx_MutipleDIe_LargeRDEAR(unsigned char* cSR, int Index)
{ // read status
    //Simon	if(! m_usb.is_open() )
    //Simon		return false ;

    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction; //size 1

    // first control packet
    vInstruction = 0xc8;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1; //(unsigned long) 1 ;

    if (OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    // second control packet : fetch data
    unsigned char vBuffer; //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 1;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = 1; //(unsigned long) 1;

    if (InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    *cSR = vBuffer;
    return SerialFlash_TRUE;
}

bool CN25Qxxx_MutipleDIe_LargeWREAR(unsigned char cSR, int Index)
{
    unsigned char v[2];
    v[0] = 0xc5;
    v[1] = cSR;
    FlashCommand_TransceiveOut(v, 2, false, Index);
    return true;
}

bool CS25FLxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index)
{
    if ((strstr(g_ChipInfo.TypeName, "S25FL512Sxxxxxx1x") != NULL) || (strstr(g_ChipInfo.TypeName, "S25FL512Sxxxxxx1x(Secure)") != NULL)) {
        SerialFlash_waitForWEL(Index);
        if (Enable4Byte) {
            unsigned char v[2];
            v[0] = 0x17;
            v[1] = 0x80;
            FlashCommand_TransceiveOut(v, 2, false, Index);
        } else {
            unsigned char v[2];
            v[0] = 0x17;
            v[1] = 0x00;
            FlashCommand_TransceiveOut(v, 2, false, Index);
        }
    } else {
        if (Enable4Byte) {
            unsigned char v = EN4B;
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            return true;
        } else {
            unsigned char v = EXIT4B;
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            return true;
        }
    }
    return true;
}
bool Universal_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index)
{  
    if (Enable4Byte) {
        unsigned char v = EN4B;
        FlashCommand_TransceiveOut(&v, 1, false, Index);
        return true;
    } else {
        unsigned char v = EXIT4B;
        FlashCommand_TransceiveOut(&v, 1, false, Index);
        return true;
    }
    return true;
}
bool CN25Qxxx_LargeEnable4ByteAddrMode(bool Enable4Byte, int Index)
{
#if 0
    if(Enable4Byte)
    {
        WORD cSR;
        RDVCR(cSR, Index);
        cSR &= 0xFFFE;
        WRVCR(cSR, Index);
    }
    return true;
#else
    if (Enable4Byte) {
        unsigned char v = EN4B;
        int numOfRetry = 5;
        unsigned char re;
        do {
            SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            CN25Qxxx_LargeRDFSR(&re, Index);
        } while ((re & 0x01) == 0 && numOfRetry-- > 0);
        return true;
    } else {
        unsigned char re;
        //        CN25Qxxx_LargeRDFSR(&re,Index);
        unsigned char v = EXIT4B;
        int numOfRetry = 5;

        do {
            SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            CN25Qxxx_LargeRDFSR(&re, Index);
        } while ((re & 0x01) == 1 && numOfRetry-- > 0);
        return true;
    }
#endif
}

int S70FSxxx_Large_Enable4ByteAddrMode(int Enable4Byte, int Index)
{
    if (Enable4Byte) {
        unsigned char v = EN4B;
        int numOfRetry = 5;
        unsigned char re;
        do {
            //  SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            S70FSxxx_Large_doRDCR2V(false, &re, Index);
        } while ((re & 0x80) == 0 && numOfRetry-- > 0);
        do {
            //  SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            S70FSxxx_Large_doRDCR2V(true, &re, Index);

        } while ((re & 0x80) == 0 && numOfRetry-- > 0);
        return true;
    } else {
        unsigned char re;
        unsigned char v = EXIT4B;
        int numOfRetry = 5;

        do {
            // SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            S70FSxxx_Large_doRDCR2V(false, &re, Index);
        } while ((re & 0x80) == 0 && numOfRetry-- > 0);

        do {
            // SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v, 1, false, Index);
            Sleep(100);
            S70FSxxx_Large_doRDCR2V(true, &re, Index);
        } while ((re & 0x80) == 0 && numOfRetry-- > 0);
        return true;
    }
}
//Simon: unused ???
int SerialFlash_Enable4ByteAddrMode(int bEnable, int Index)
{ 
    if (strstr(g_ChipInfo.Class, SUPPORT_EON_EN25QHxx_Large) != NULL || strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL || strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Pxx_Large) != NULL || strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Qxx_Large) != NULL)
        return CEN25QHxx_LargeEnable4ByteAddrMode(bEnable, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S70FSxx_Large) != NULL)
        return S70FSxxx_Large_Enable4ByteAddrMode(bEnable, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large) != NULL)
        return CN25Qxxx_LargeEnable4ByteAddrMode(bEnable, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL)
        return CS25FLxx_LargeEnable4ByteAddrMode(bEnable, Index);
    else if(g_ChipInfo.ChipSizeInByte > 0x1000000)
        return Universal_LargeEnable4ByteAddrMode(bEnable, Index);
       
    return SerialFlash_TRUE;
}

/**
 * \brief
 * read data, check if 0xFF
 *
 * \returns
 * true, if blank; false otherwise
 */
int SerialFlash_rangeBlankCheck(struct CAddressRange* Range, int Index)
{
    unsigned char* vBuffer;
    unsigned int i;
    Range->length = Range->end - Range->start;

    if (Range->length <= 0)
        return SerialFlash_FALSE;
    vBuffer = malloc(Range->length);
    if (vBuffer != 0) {
        if (SerialFlash_rangeRead(Range, vBuffer, Index) == SerialFlash_FALSE) {
            free(vBuffer);
            return false;
        }
        for (i = 0; i < Range->length; i++) {
            if (vBuffer[i] != 0xFF) { 
                free(vBuffer);
                return false;
            }
        }
        free(vBuffer);
        return true;
    } else {
        free(vBuffer);
        return SerialFlash_FALSE;
    }
}

/**
 * \brief
 * Write brief comment for Download here.
 *
 * it first check the data size and then delegates the actual operation to
 * BulkPipeWrite()
 *
 * \param AddrRange
 * the flash memory starting(inclusive) and end address(exlusive)
 *
 * \param vData
 * data to be written into the flash memory.
 *
 * \returns
 * true, if successfull
 * false, if data size larger than memory size, or operation fails
 */
int SerialFlash_rangeProgram(struct CAddressRange* AddrRange, unsigned char* vData, int Index)
{ 
    if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45rangeProgram(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr, Index);
    else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL) {
        if ((g_bIsSF600[Index] == true) || (g_bIsSF700[Index] == true) || (g_bIsSF600PG2[Index] == true))
            return SerialFlash_bulkPipeProgram(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr, Index);
        else
            return SerialFlash_bulkPipeProgram(AddrRange, vData, PP_4ADDR_256BYTE_12, mcode_ProgramCode_4Adr, Index);
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxxL_Large) != NULL) {
        return SerialFlash_bulkPipeProgram(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr_12, Index);
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S70FSxx_Large) != NULL) {
        if ((g_bIsSF600[Index] == true) || (g_bIsSF700[Index] == true)|| (g_bIsSF600PG2[Index] == true)) {
            return SerialFlash_bulkPipeProgram(AddrRange, vData, PP_4ADDR_256BYTE_S70FS01GS, mcode_ProgramCode_4Adr_12, Index);
        } else
            return false;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Mxx_Large) != NULL) { 
          return SerialFlash_bulkPipeProgram_twoDie(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr, Index); 
    }  else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL) { 
          return SerialFlash_bulkPipeProgram_Micron_4Die(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr, Index); 
    } else { 
        return SerialFlash_bulkPipeProgram(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr, Index);
    }
}

int SerialFlash_rangeRead(struct CAddressRange* AddrRange, unsigned char* vData, int Index)
{ 
    if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL) {
        if ((g_bIsSF600[Index] == true) || (g_bIsSF700[Index] == true)|| (g_bIsSF600PG2[Index] == true))
            return SerialFlash_bulkPipeRead(AddrRange, vData, BULK_4BYTE_FAST_READ, mcode_ReadCode, Index);
        else
            return SerialFlash_bulkPipeRead(AddrRange, vData, BULK_4BYTE_FAST_READ_MICRON, mcode_ReadCode, Index);
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxxL_Large) != NULL) {
        return SerialFlash_bulkPipeRead(AddrRange, vData, mcode_Read, mcode_ReadCode, Index);
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S70FSxx_Large) != NULL) {
        if ((g_bIsSF600[Index] == true) || (g_bIsSF700[Index] == true)|| (g_bIsSF600PG2[Index] == true))
            return SerialFlash_bulkPipeRead(AddrRange, vData, BULK_4BYTE_FAST_READ, mcode_ReadCode_0C, Index);
        else
            return false;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Mxx_Large) != NULL) {  
            return SerialFlash_bulkPipeRead_twoDie(AddrRange, vData, (unsigned char)mcode_Read, (unsigned char)mcode_ReadCode_0C, Index);
    } else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL) {  
            return SerialFlash_bulkPipeRead_Micron_4die(AddrRange, vData, (unsigned char)mcode_Read, (unsigned char)mcode_ReadCode_0C, Index);
    } else
        return SerialFlash_bulkPipeRead(AddrRange, vData, (unsigned char)mcode_Read, (unsigned char)mcode_ReadCode, Index);
};

/**
 * \brief
 * poll the status register after Erase operation.
 *
 * \returns
 * To be defined
 *
 * (not fully implemented or used now)
 *
 * \remarks
 * A polling command can be used to poll the status register after Erase
 * operation
 *
 */
int SerialFlash_DoPolling(int Index)
{
    CNTRPIPE_RQ rq;
    unsigned char vDataPack[4];

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = POLLING;
    rq.Value = (CTRL_TIMEOUT >> 16) & 0xffff;
    rq.Index = CTRL_TIMEOUT & 0xffff;
    rq.Length = (unsigned long)(4);

    if (OutCtrlRequest(&rq, vDataPack, 4, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;

    return SerialFlash_TRUE;
}

/**
 * \brief
 * retrieves object wellness flag
 *
 * before calling any other method, be sure to call is_good() to check the
 * wellness which indicates whether the instance is constructed successfully or
 * not
 *
 * \returns
 * true only if all objects of chip info, chip itself, USB  are correctly
 * constructed false otherwise
 *
 */
int SerialFlash_is_good()
{
    //Simon	return ( m_info.is_good() && m_usb.is_open() ) ;
    return SerialFlash_TRUE;
}
int SerialFlash_batchErase_W25Mxx_Large(uintptr_t* vAddrs, size_t AddrSize, int Index)
{  
    if (!SerialFlash_StartofOperation(Index))
        return false;
 
    if (0 == mcode_SegmentErase)
        return 1; //  chipErase code not initialised or not supported, please check chip class ctor.SerialFlash_waitForWEL

    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    // send request
    CNTRPIPE_RQ rq;
    size_t i;
    size_t addrTemp;
    unsigned char vInstruction[5]; //(5, mcode_SegmentErase) ;
    vInstruction[0] = mcode_SegmentErase;
 
 
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = NO_RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = NO_RESULT_IN;
    }
    rq.Length = 5;
    for (i = 0; i < AddrSize; i++) {
        SerialFlash_waitForWEL(Index);
	if (vAddrs[i]>=0x2000000) { 
	   SerialFlash_doSelectDie(0x01,Index);
	   addrTemp = vAddrs[i] - 0x2000000;
        }
	else {
	   SerialFlash_doSelectDie(0x00,Index);
	   addrTemp = vAddrs[i];
        }
        //if (Chip_Info.ChipSizeInByte > 0x1000000) {
            // MSB~ LSB (31...0)
            vInstruction[1] = (unsigned char)((addrTemp >> 24) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((addrTemp >> 16) & 0xff); //M
            vInstruction[3] = (unsigned char)((addrTemp >> 8) & 0xff); //M
            vInstruction[4] = (unsigned char)(addrTemp & 0xff); //LSB
            rq.Length = 5;
       /* } else {
            // MSB~ LSB (23...0)
            vInstruction[1] = (unsigned char)((vAddrs[i] >> 16) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((vAddrs[i] >> 8) & 0xff); //M
            vInstruction[3] = (unsigned char)(vAddrs[i] & 0xff); //LSB
            rq.Length = 4;
        }*/
        OutCtrlRequest(&rq, vInstruction, rq.Length, Index);

        SerialFlash_waitForWIP(Index);
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);
    if (!SerialFlash_EndofOperation(Index))
        return false;
    return true;
}
int SerialFlash_batchErase(uintptr_t* vAddrs, size_t AddrSize, int Index)
{
    if (!SerialFlash_StartofOperation(Index))
        return false;
 
    if (0 == mcode_SegmentErase)
        return 1; //  chipErase code not initialised or not supported, please check chip class ctor.SerialFlash_waitForWEL

    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    // send request
    CNTRPIPE_RQ rq;
    size_t i;
    unsigned char vInstruction[5]; //(5, mcode_SegmentErase) ;
    vInstruction[0] = mcode_SegmentErase;

 
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = NO_RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = NO_RESULT_IN;
    }
    rq.Length = 5;
    for (i = 0; i < AddrSize; i++) {
        SerialFlash_waitForWEL(Index);
        if (g_ChipInfo.ChipSizeInByte > 0x1000000) {
            // MSB~ LSB (31...0)
            vInstruction[1] = (unsigned char)((vAddrs[i] >> 24) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((vAddrs[i] >> 16) & 0xff); //M
            vInstruction[3] = (unsigned char)((vAddrs[i] >> 8) & 0xff); //M
            vInstruction[4] = (unsigned char)(vAddrs[i] & 0xff); //LSB
            rq.Length = 5;
        } else {
            // MSB~ LSB (23...0)
            vInstruction[1] = (unsigned char)((vAddrs[i] >> 16) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((vAddrs[i] >> 8) & 0xff); //M
            vInstruction[3] = (unsigned char)(vAddrs[i] & 0xff); //LSB
            rq.Length = 4;
        }
        OutCtrlRequest(&rq, vInstruction, rq.Length, Index);

        SerialFlash_waitForWIP(Index);
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);
    if (!SerialFlash_EndofOperation(Index))
        return false;
    return true;
}

int SerialFlash_rangeErase(unsigned char cmd, size_t sectionSize, struct CAddressRange* AddrRange, int Index)
{
    if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45rangSectorErase(sectionSize, *AddrRange, Index);

    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    // send request
    CNTRPIPE_RQ rq;
    unsigned char vInstruction[5]; 
    vInstruction[0] = cmd;

    // instrcution format
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = RFU;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = (unsigned long)(1);

    size_t sectorNum = (AddrRange->end - AddrRange->start + sectionSize - 1) / sectionSize;
    size_t i;
    for (i = 0; i < sectorNum; ++i) {
        SerialFlash_waitForWEL(Index);

        // MSB~ LSB (23...0)
        size_t addr = (AddrRange->start + i * sectionSize);

        if (g_ChipInfo.ChipSizeInByte > 0x1000000) {
            // MSB~ LSB (31...0)
            vInstruction[1] = (unsigned char)((addr >> 24) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((addr >> 16) & 0xff); //M
            vInstruction[3] = (unsigned char)((addr >> 8) & 0xff); //M
            vInstruction[4] = (unsigned char)(addr & 0xff); //LSB
            rq.Length = (unsigned long)(5);
        } else {
            // MSB~ LSB (23...0)
            vInstruction[1] = (unsigned char)((addr >> 16) & 0xff); //MSB
            vInstruction[2] = (unsigned char)((addr >> 8) & 0xff); //M
            vInstruction[3] = (unsigned char)(addr & 0xff); //LSB
            rq.Length = (unsigned long)(4);
        }

        int b = OutCtrlRequest(&rq, vInstruction, rq.Length, Index);
        if ((b == SerialFlash_FALSE) || m_isCanceled)
            return false;

        SerialFlash_waitForWIP(Index);
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);

    return true;
}

bool S70FSxxx_Large_chipErase(unsigned int Addr, unsigned int Length, int USBIndex)
{ 

    // wait until WIP = 0
    S70FSxxx_Large_waitForWIP(true, USBIndex);
    // wait until WEL = 1
    S70FSxxx_Large_waitForWEL(true, USBIndex);
    unsigned char vInstruction[5] = { 0 };
    vInstruction[0] = 0xFE;
    vInstruction[1] = 0x00;
    vInstruction[2] = 0x00;
    vInstruction[3] = 0x00;
    vInstruction[4] = 0x00;
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 5, USBIndex);

    S70FSxxx_Large_waitForWIP(true, USBIndex); //check die 1

    S70FSxxx_Large_Enable4ByteAddrMode(false, USBIndex);
    S70FSxxx_Large_waitForWIP(false, USBIndex);

    // wait until WEL = 1
    S70FSxxx_Large_waitForWEL(false, USBIndex);
    vInstruction[0] = 0xFE;
    vInstruction[1] = 0x04;
    vInstruction[2] = 0x00;
    vInstruction[3] = 0x00;
    vInstruction[4] = 0x00;
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 5, USBIndex);

    S70FSxxx_Large_waitForWIP(false, USBIndex); //check die 2
    return true;
}

bool W25Mxx_Large_chipErase(unsigned int Addr, unsigned int Length, int USBIndex)
{
    SerialFlash_doSelectDie(0x00,USBIndex);
    
    SerialFlash_waitForWEL(USBIndex);
    unsigned char v = mcode_ChipErase;
    FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, USBIndex);
    SerialFlash_waitForWIP(USBIndex); 
    if (!SerialFlash_EndofOperation(USBIndex))
        return false;

    SerialFlash_doSelectDie(0x01,USBIndex);
    
    SerialFlash_waitForWEL(USBIndex);

    FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, USBIndex);
    SerialFlash_waitForWIP(USBIndex); 
    if (!SerialFlash_EndofOperation(USBIndex))
        return false;

    return true;
}

/// chip erase
bool SerialFlash_chipErase(int Index)
{
    if (!SerialFlash_StartofOperation(Index))
        return false;
    if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45chipErase(0, g_ChipInfo.ChipSizeInByte, Index);
    if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S70FSxx_Large) != NULL)
        return S70FSxxx_Large_chipErase(0, g_ChipInfo.ChipSizeInByte, Index);
    if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Mxx_Large) != NULL)
        return W25Mxx_Large_chipErase(0, g_ChipInfo.ChipSizeInByte, Index);

    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;

    SerialFlash_waitForWEL(Index);
    unsigned char v = mcode_ChipErase;
    FlashCommand_SendCommand_OutOnlyInstruction(&v, 1, Index);
    SerialFlash_waitForWIP(Index);
    // if( SerialFlash_protectBlock(m_bProtectAfterWritenErase,Index) ==
    // SerialFlash_FALSE) return false ;
    if (!SerialFlash_EndofOperation(Index))
        return false;
    return true;
}

/// die erase
int SerialFlash_DieErase(int Index)
{
    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(true, Index); 

    // send request
    unsigned char vInstruction[5];
    int numOfRetry = 5;
    unsigned char re;
    vInstruction[0] = mcode_ChipErase; 

    size_t dieNum = ((strstr(g_ChipInfo.Class, "2Die") != NULL) ? 2 : 4);
    size_t die_size = g_ChipInfo.ChipSizeInByte / dieNum;
    size_t i;
    for (i = 0; i < dieNum; i++) {
        SerialFlash_waitForWEL(Index);

        // MSB~ LSB (23...0)
        size_t addr = (i * die_size);
        // MSB~ LSB (31...0)
        vInstruction[1] = (unsigned char)((addr >> 24) & 0xff); //MSB
        vInstruction[2] = (unsigned char)((addr >> 16) & 0xff); //M
        vInstruction[3] = (unsigned char)((addr >> 8) & 0xff); //M
        vInstruction[4] = (unsigned char)(addr & 0xff); //LSB

        int b = FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 5, Index);
        if ((b == SerialFlash_FALSE) || m_isCanceled)
            return false;

        SerialFlash_waitForWIP(Index);
        numOfRetry = 5;
        do {
            CN25Qxxx_LargeRDFSR(&re, Index);
            Sleep(2);
        } while ((re & 0x01) == 0 && numOfRetry-- > 0);
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);
    return true;
}

int SerialFlash_bulkPipeProgram(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{  
    size_t i, j, divider;
    unsigned char* itr_begin;

    itr_begin = vData;
    switch (modeWrite) {
    //transfer how many data each time
    case MODE_NUMONYX_PCM:
    case PP_32BYTE:
        divider = 9;
        break;
    case PP_128BYTE:
        divider = 7;
        break;
    case PP_PROGRAM_ANYSIZE_PAGESIZE:
        if(g_ChipInfo.PageSizeInByte == 0x200)
            divider = 9;
        else if(g_ChipInfo.PageSizeInByte == 0x400)
            divider = 10;
        else 
            divider = 8;
        break;
    default:
        divider = 8;
        break;
    }

    if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxxS_Large) != NULL)
    {
        unsigned char cSR;
        S25FSxxS_Large_doRDCF3V(&cSR,  Index);
        if((cSR & 0x10) == 0x10)
            divider = 9;
        else
            divider = 8;
    }
	
    if (!SerialFlash_StartofOperation(Index))
        return false;
    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;
	
    if (modeWrite != PP_SB_FPGA)
        SerialFlash_waitForWEL(Index);
	
    SerialFlash_Enable4ByteAddrMode(true, Index);
    if (SerialFlash_EnableQuadIO(true, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE)
        return false;
	

    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000))   
    {
        struct CAddressRange down_range;
        struct CAddressRange range_temp;
        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

        down_range.start = AddrRange->start;
        down_range.end = AddrRange->end;
        size_t packageNum;
        size_t loop = (range_temp.end - range_temp.start) / 0x1000000; 

        for (j = 0; j < loop; j++) {
            if (j == (loop - 1))
                down_range.end = AddrRange->end;
            else
                down_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                down_range.start = AddrRange->start;
            else
                down_range.start = (AddrRange->start & 0xFF000000) + (0x1000000 * j);

            down_range.length = down_range.end - down_range.start;
            packageNum = down_range.length >> divider; 
            FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range, modeWrite, WriteCom, g_ChipInfo.PageSizeInByte, g_ChipInfo.AddrWidth, Index);
            for (i = 0; i < packageNum; ++i) {
                BulkPipeWrite((unsigned char*)(itr_begin + (i << divider)), 1 << divider, USB_TIMEOUT, Index);
                if (m_isCanceled)
                    return false;
            }
            itr_begin = itr_begin + (packageNum << divider);
        }
    } else {
        size_t packageNum = (AddrRange->end - AddrRange->start) >> divider;
        FlashCommand_SendCommand_SetupPacketForBulkWrite(AddrRange, modeWrite, WriteCom, 1 << divider, g_ChipInfo.AddrWidth, Index);
        for (i = 0; i < packageNum; ++i) { 
            BulkPipeWrite((unsigned char*)((itr_begin + (i << divider))), 1 << divider, USB_TIMEOUT, Index);
            if (m_isCanceled)
                return false;
        }
    }

    if (mcode_Program == AAI_2_BYTE)
        SerialFlash_doWRDI(Index);

    if (SerialFlash_protectBlock(m_bProtectAfterWritenErase, Index) == SerialFlash_FALSE)
        return false;
    if (SerialFlash_EnableQuadIO(false, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    if (!SerialFlash_EndofOperation(Index))
        return false;
    return true;
}

int SerialFlash_bulkPipeProgram_Micron_4Die(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{    
    size_t i, j, divider;
    unsigned char* itr_begin;

    if (!SerialFlash_StartofOperation(Index)) 
        return false; 
   if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE) 
        return false;  
    if (SerialFlash_EnableQuadIO(true, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE) 
       return false;

    itr_begin = vData; 
    divider = 8;

    unsigned char timeout = 3;
    unsigned char re = 0;
    unsigned char EAR = (AddrRange->start & 0xFF000000)>>24;
    unsigned char preEAR = 0;
	
    do{ 
    	CN25Qxxx_Large_4Die_WREAR(0,Index);
    	Sleep(1);
    	CN25Qxxx_Large_4Die_RDEAR(&re,Index);    
    	timeout--;
    }while((re!=0)&&(timeout!=0));
   if(timeout == 0) 
    	return false;

    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000))   
    { 
        struct CAddressRange down_range;
        struct CAddressRange range_temp;
        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

        down_range.start = AddrRange->start;
        down_range.end = AddrRange->end;
        size_t packageNum;
        size_t loop = (range_temp.end - range_temp.start) / 0x1000000; 

        for (j = 0; j < loop; j++) {
            if (j == (loop - 1))
                down_range.end = AddrRange->end;
            else
                down_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                down_range.start = AddrRange->start;
            else
                down_range.start = (AddrRange->start & 0xFF000000) + (0x1000000 * j);

 
	 
            timeout = 3;
	    re = 0;
	    EAR = (down_range.start & 0xFF000000)>>24;
	    if((EAR != preEAR)&&(EAR!=0)) {
		    do{  
			CN25Qxxx_Large_4Die_WREAR(EAR,Index); 
			Sleep(1);
			CN25Qxxx_Large_4Die_RDEAR(&re,Index);    
			timeout--;
		    }while(((re&EAR)!=EAR)&&(timeout!=0));
		    if(timeout == 0)
		    	return false;
		    preEAR = EAR; 
	    }
	    	 
	down_range.end=down_range.end-(0x1000000*EAR);
	down_range.start=down_range.start-(0x1000000*EAR); 

        down_range.length = down_range.end - down_range.start;
        packageNum = down_range.length >> divider; 
         
        FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range, modeWrite, WriteCom, g_ChipInfo.PageSizeInByte, g_ChipInfo.AddrWidth, Index);
        for (i = 0; i < packageNum; ++i) {
       	    BulkPipeWrite((unsigned char*)(itr_begin + (i << divider)), 1 << divider, USB_TIMEOUT, Index);
            if (m_isCanceled)
                return false;
            }
            itr_begin = itr_begin + (packageNum << divider);
        }
    } else { 
        struct CAddressRange down_range;
    	divider = 8;
    
            timeout = 3;
	    re = 0;
	    EAR = (AddrRange->start & 0xFF000000)>>24;
	    if((EAR != preEAR)&&(EAR!=0)) {
		    do{ 
			CN25Qxxx_Large_4Die_WREAR(EAR,Index);
			Sleep(1);
			CN25Qxxx_Large_4Die_RDEAR(&re,Index);    
			timeout--;
		   	preEAR = EAR; 
		    }while(((re&EAR)!=EAR)&&(timeout!=0));
		    if(timeout == 0) 
		    	return false; 
	    }

	down_range.end=AddrRange->end-(0x1000000*EAR);
	down_range.start=AddrRange->start-(0x1000000*EAR); 

        size_t packageNum = (down_range.end - down_range.start) >> divider;
        FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range, modeWrite, WriteCom, g_ChipInfo.PageSizeInByte, g_ChipInfo.AddrWidth, Index);
        for (i = 0; i < packageNum; ++i) { 
            BulkPipeWrite((unsigned char*)((itr_begin + (i << divider))), 1 << divider, USB_TIMEOUT, Index);
            if (m_isCanceled)
                return false;
        }
    }
 

    if (SerialFlash_protectBlock(m_bProtectAfterWritenErase, Index) == SerialFlash_FALSE)  
        return false; 
   if (SerialFlash_EnableQuadIO(false, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE)  
      return false;  
    if (!SerialFlash_EndofOperation(Index))  
       return false;   
    return true;
} 
bool SerialFlash_doSelectDie(unsigned char dieNum,int Index)
{ 
    if(SerialFlash_waitForWIP(Index)==false) 
	return false; 

    SerialFlash_waitForWEL(Index);
 
    unsigned char vInstruction[2];

    vInstruction[0] = 0xC2;
    vInstruction[1] = dieNum; 

    return FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 2, Index);
}

int SerialFlash_bulkPipeProgram_twoDie(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{   
    size_t i, j, divider;
    unsigned char* itr_begin;

    if (!SerialFlash_StartofOperation(Index))
        return false;
    if (SerialFlash_protectBlock(false, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    if (modeWrite != PP_SB_FPGA)
        SerialFlash_waitForWEL(Index);
 

    if (SerialFlash_EnableQuadIO(true, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE)
        return false;
 
    itr_begin = vData; 
    divider = 8;  

    struct CAddressRange down_range;
    struct CAddressRange range_temp;
    struct CAddressRange down_range_die2;
    
    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000))   
    { 
        struct CAddressRange range_temp_die2;

        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

        down_range.start = AddrRange->start;
        down_range.end = AddrRange->end;
        size_t packageNum;
        size_t loop = (range_temp.end - range_temp.start) / 0x1000000; 

        for (j = 0; j < loop; j++) 
	{
            if (j == (loop - 1))
                down_range.end = AddrRange->end;
            else
                down_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                down_range.start = AddrRange->start;
            else
                down_range.start = (AddrRange->start & 0xFF000000) + (0x1000000 * j);

	    range_temp_die2.start = down_range.start;
	    range_temp_die2.end = down_range.end;
	    if(down_range.start>=0x2000000)
	    { 
   		SerialFlash_doSelectDie(1,Index); 
		// wait until WIP = 0
		if(SerialFlash_waitForWIP(Index)==false) 
		    return false; 
		SerialFlash_waitForWEL(Index);
 
		down_range_die2.start = range_temp_die2.start-0x2000000;
		down_range_die2.end = range_temp_die2.end-0x2000000;	  
	    } 
   	    else
	    { 
	    	SerialFlash_doSelectDie(0,Index);  
		down_range_die2.start = down_range.start;
		down_range_die2.end = down_range.end;	  
	    }

            down_range.length = down_range.end - down_range.start;
            packageNum = down_range.length >> divider; 
  
            FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range_die2, modeWrite, WriteCom, g_ChipInfo.PageSizeInByte,g_ChipInfo.AddrWidth, Index);
            for (i = 0; i < packageNum; ++i) {
                BulkPipeWrite((unsigned char*)(itr_begin + (i << divider)), 1 << divider, USB_TIMEOUT, Index);
                if (m_isCanceled)
                    return false;
            }
            itr_begin = itr_begin + (packageNum << divider); 
        }
    } else {  
		//struct CAddressRange down_range_die2; 
	if(AddrRange->start>=0x2000000)
	{  
	    SerialFlash_doSelectDie(1,Index);  
	    down_range_die2.start = AddrRange->start-0x2000000;
	    down_range_die2.end = AddrRange->end-0x2000000;
	     
    	} 
	else
	{ 
  	    SerialFlash_doSelectDie(0,Index);    
		down_range_die2.start = AddrRange->start;
		down_range_die2.end = AddrRange->end;	  
	}

        size_t packageNum = (AddrRange->end - AddrRange->start) >> divider;
        FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range_die2, modeWrite, WriteCom, g_ChipInfo.PageSizeInByte, g_ChipInfo.AddrWidth, Index);
        for (i = 0; i < packageNum; ++i) { 
            BulkPipeWrite((unsigned char*)((itr_begin + (i << divider))), 1 << divider, USB_TIMEOUT, Index);
            if (m_isCanceled)
                return false;
        }
    }

    if (mcode_Program == AAI_2_BYTE)
        SerialFlash_doWRDI(Index);

    if (SerialFlash_protectBlock(m_bProtectAfterWritenErase, Index) == SerialFlash_FALSE)
        return false;
    if (SerialFlash_EnableQuadIO(false, m_boEnWriteQuadIO, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    if (!SerialFlash_EndofOperation(Index))
        return false;
    return true;
}

int SerialFlash_bulkPipeRead(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index)
{ 
    size_t i, j, loop, pageNum, BufferLocation = 0;
    int ret = 0;
    if (!SerialFlash_StartofOperation(Index))
        return false;
    if (!(strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL && strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL && ((g_bIsSF600[Index] == true) || (g_bIsSF700[Index] == true)|| (g_bIsSF600PG2[Index] == true))))
        SerialFlash_Enable4ByteAddrMode(true, Index);

    if (SerialFlash_EnableQuadIO(true, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;

    AddrRange->length = AddrRange->end - AddrRange->start;
    if (AddrRange->length <= 0)
        return false;
 
    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000)) //(AddrRange.end>0x1000000 && AddrRange.start<0x1000000)
    { 
        struct CAddressRange read_range;
        struct CAddressRange range_temp;
        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

        read_range.start = AddrRange->start;
        read_range.end = AddrRange->end;
        loop = (range_temp.end - range_temp.start) / 0x1000000;

        for (j = 0; j < loop; j++) {
            if (((g_bIsSF600[Index] == false) && (g_bIsSF700[Index] == false)&& (g_bIsSF600PG2[Index] == false)) && (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL || strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL)) // for sf100
            {
                unsigned char re = 0;
                int numOfRetry = 5;
                do {
                    TurnOFFVcc(Index);
                    Sleep(100);
                    TurnONVcc(Index);
                    SerialFlash_waitForWEL(Index);
                    unsigned char v[2] = { 0xc5, j };
                    FlashCommand_TransceiveOut(v, 2, false, Index);
                    Sleep(100);
                    CN25Qxxx_MutipleDIe_LargeRDEAR(&re, Index);
                } while (((re & j) != j) && numOfRetry-- > 0);
                if (numOfRetry == 0)
                    return false;
            }

            if (j == (loop - 1))
                read_range.end = AddrRange->end;
            else
                read_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                read_range.start = AddrRange->start;
            else
                read_range.start = ((AddrRange->start & 0xFF000000) + (0x1000000 * j));

            read_range.length = read_range.end - read_range.start;

            pageNum = read_range.length >> 9; 
            FlashCommand_SendCommand_SetupPacketForBulkRead(&read_range, modeRead, ReadCom, g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
            for (i = 0; i < pageNum; ++i) {
                ret = BulkPipeRead(vData + (BufferLocation + i) * (1<<9), USB_TIMEOUT, Index); 
                if ((ret != (1<<9)) || m_isCanceled)
                    return 0;
                //memcpy(vData + (BufferLocation+i)*512, v, 512);
            }
            BufferLocation += pageNum;
        }
    } else {
        unsigned char EAR = (AddrRange->start * 0x1000000) >> 24;
        if (((g_bIsSF600[Index] == false) && (g_bIsSF700[Index] == false) && (g_bIsSF600PG2[Index] == false)) && (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL || strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL)) {
            unsigned char re = 0;
            int numOfRetry = 5;
            do {
                TurnOFFVcc(Index);
                Sleep(100);
                TurnONVcc(Index);
                SerialFlash_waitForWEL(Index);
                unsigned char v[2] = { 0xc5, EAR };
                FlashCommand_TransceiveOut(v, 2, false, Index);
                Sleep(100);
                CN25Qxxx_MutipleDIe_LargeRDEAR(&re, Index);
            } while (((re & EAR) != EAR) && numOfRetry-- > 0);
            if (numOfRetry == 0) {
                return false;
            }
        } 
        pageNum = AddrRange->length >> 9;
        FlashCommand_SendCommand_SetupPacketForBulkRead(AddrRange, modeRead, ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
        for (i = 0; i < pageNum; ++i) {
            ret = BulkPipeRead(vData + i * ret, USB_TIMEOUT, Index);
            if ((ret != 512) || m_isCanceled) { 
                return false;
            }
            //memcpy(vData + i*ret, v, ret);
        }
    }
    if (SerialFlash_EnableQuadIO(false, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    if (!SerialFlash_EndofOperation(Index))
        return false;

    return true;
}

int SerialFlash_bulkPipeRead_Micron_4die(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index)
{ 
    size_t i, j, loop, pageNum, BufferLocation = 0;
    int ret = 0;
    if (!SerialFlash_StartofOperation(Index))
        return false; 
    if (SerialFlash_EnableQuadIO(true, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;

    AddrRange->length = AddrRange->end - AddrRange->start;
    if (AddrRange->length <= 0)
        return false;
 
    unsigned char EAR = 0;
    unsigned char preEAR = 0;
    unsigned char timeout = 3;
    unsigned char re = 0;
    do{
	 CN25Qxxx_Large_4Die_WREAR(0,Index);
	 Sleep(1);
	 CN25Qxxx_Large_4Die_RDEAR(&re,Index);  
	 timeout--;  
    }while((re != 0)&&(timeout!=0));
    if(timeout == 0)
	 return false;
   
    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000)) //(AddrRange.end>0x1000000 && AddrRange.start<0x1000000)
    { 
        struct CAddressRange read_range;
        struct CAddressRange range_temp;
        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

        read_range.start = AddrRange->start;
        read_range.end = AddrRange->end;
        loop = (range_temp.end - range_temp.start) / 0x1000000;

        for (j = 0; j < loop; j++) {
               

            if (j == (loop - 1))
                read_range.end = AddrRange->end;
            else
                read_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                read_range.start = AddrRange->start;
            else
                read_range.start = ((AddrRange->start & 0xFF000000) + (0x1000000 * j));
	    timeout = 3;
	    re = 0;
	    EAR = (read_range.start & 0xFF000000)>>24;
	    if((EAR != preEAR)&&(EAR)) {
		do{
			CN25Qxxx_Large_4Die_WREAR(EAR,Index);
			Sleep(1);
			CN25Qxxx_Large_4Die_RDEAR(&re,Index);    
			timeout--;
		 }while(((re&EAR)!=EAR)&&(timeout!=0));
	         if(timeout == 0)
	       	    return false;
		preEAR = EAR;
	    }
	    	
	    read_range.end=read_range.end-(0x1000000*EAR);
	    read_range.start=read_range.start-(0x1000000*EAR); 

            read_range.length = read_range.end - read_range.start;

            pageNum = read_range.length >> 9; 
            FlashCommand_SendCommand_SetupPacketForBulkRead(&read_range, modeRead, ReadCom, g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
            for (i = 0; i < pageNum; ++i) {
                ret = BulkPipeRead(vData + (BufferLocation + i) * (1<<9), USB_TIMEOUT, Index); 
                if ((ret != (1<<9)) || m_isCanceled)
                    return 0;
                //memcpy(vData + (BufferLocation+i)*512, v, 512);
            }
            BufferLocation += pageNum;
        }
    } else {
     
    
        struct CAddressRange read_range;
	     
        timeout = 3;
        re = 0;
	EAR = (read_range.start & 0xFF000000)>>24;
	if((EAR != preEAR)&&(EAR)) {
	    do{
	    	CN25Qxxx_Large_4Die_WREAR(EAR,Index);
		Sleep(1);
		CN25Qxxx_Large_4Die_RDEAR(&re,Index);    
		timeout--;
	    }while(((re&EAR)!=EAR)&&(timeout!=0));
	    if(timeout == 0)
	        return false;
	    preEAR = EAR;
	}
	    	
          
	read_range.end=AddrRange->end-(0x1000000*EAR);
	read_range.start=AddrRange->start-(0x1000000*EAR); 
        pageNum = read_range.length >> 9;
        FlashCommand_SendCommand_SetupPacketForBulkRead(&read_range, modeRead, ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
        for (i = 0; i < pageNum; ++i) {
            ret = BulkPipeRead(vData + i * ret, USB_TIMEOUT, Index);
            if ((ret != 512) || m_isCanceled) { 
                return false;
            }
            //memcpy(vData + i*ret, v, ret);
        }
    }
    if (SerialFlash_EnableQuadIO(false, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    if (!SerialFlash_EndofOperation(Index))
        return false;

    return true;
}

int SerialFlash_bulkPipeRead_twoDie(struct CAddressRange* AddrRange, unsigned char* vData, unsigned char modeRead, unsigned char ReadCom, int Index)
{  
    size_t i, j, loop, pageNum, BufferLocation = 0;
    int ret = 0;
    if (!SerialFlash_StartofOperation(Index))
        return false;     
    if (SerialFlash_EnableQuadIO(true, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;

    AddrRange->length = AddrRange->end - AddrRange->start;
    if (AddrRange->length <= 0)
        return false;
 
    if ((AddrRange->end / 0x1000000) > (AddrRange->start / 0x1000000)) //(AddrRange.end>0x1000000 && AddrRange.start<0x1000000)
    { 
   	 SerialFlash_Enable4ByteAddrMode(true, Index);
        struct CAddressRange read_range;
        struct CAddressRange range_temp;

	struct CAddressRange range_die2;
	struct CAddressRange range_die2_temp;

        range_temp.start = AddrRange->start & 0xFF000000;
        range_temp.end = AddrRange->end + ((AddrRange->end % 0x1000000) ? (0x1000000 - (AddrRange->end % 0x1000000)) : 0);

 
        loop = (range_temp.end - range_temp.start) / 0x1000000;

        for (j = 0; j < loop; j++) { 
            if (j == (loop - 1))
                read_range.end = AddrRange->end;
            else
                read_range.end = (AddrRange->start & 0xFF000000) + (0x1000000 * (j + 1));

            if (j == 0)
                read_range.start = AddrRange->start;
            else
                read_range.start = ((AddrRange->start & 0xFF000000) + (0x1000000 * j));

            read_range.length = read_range.end - read_range.start;

	    range_die2_temp.start=read_range.start;
	    range_die2_temp.end=read_range.end;

	    if(read_range.start>=0x2000000)
	    {
  		SerialFlash_doSelectDie(1,Index);  

		range_die2.start=range_die2_temp.start-0x2000000;
		range_die2.end=range_die2_temp.end-0x2000000;	 

		read_range.start=range_die2.start;
		read_range.end=range_die2.end;
	    } 
	    else
	    {  
		SerialFlash_doSelectDie(0,Index); 
	     } 
            pageNum = read_range.length >> 9;
	        FlashCommand_SendCommand_SetupPacketForBulkRead(&read_range, modeRead, ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
            for (i = 0; i < pageNum; ++i) {
                ret = BulkPipeRead(vData + (BufferLocation + i) * 512, USB_TIMEOUT, Index);
                if ((ret != 512) || m_isCanceled)
                    return 0;
                //memcpy(vData + (BufferLocation+i)*512, v, 512);
            }
            BufferLocation += pageNum;
        }
    } else { 
	struct CAddressRange range_die2; 
	
	range_die2.length = AddrRange->end - AddrRange->start;
	if(AddrRange->start>=0x2000000)
	{  
	    SerialFlash_doSelectDie(1,Index);  
	    range_die2.start=AddrRange->start-0x2000000;
	    range_die2.end=AddrRange->end-0x2000000;	 
	} 
	else
	{  
	    SerialFlash_doSelectDie(0,Index);  
	    range_die2.start=AddrRange->start;
	    range_die2.end=AddrRange->end; 
	}
 
        pageNum = range_die2.length >> 9; 
        
        FlashCommand_SendCommand_SetupPacketForBulkRead(&range_die2, modeRead, ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen, Index);
        for (i = 0; i < pageNum; ++i) 
 	{
            ret = BulkPipeRead(vData + i * ret, USB_TIMEOUT, Index);
            if ((ret != 512) || m_isCanceled) 
	    { 
                return false;
            }
            //memcpy(vData + i*ret, v, ret);
        }
    }
    if (SerialFlash_EnableQuadIO(false, m_boEnReadQuadIO, Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    if (!SerialFlash_EndofOperation(Index))
        return false;

    return true;
}

void SerialFlash_SetCancelOperationFlag()
{
    m_isCanceled = SerialFlash_TRUE;
}

void SerialFlash_ClearCancelOperationFlag()
{
    m_isCanceled = SerialFlash_FALSE;
}

int SerialFlash_readSR(unsigned char* cSR, int Index)
{
    return SerialFlash_doRDSR(cSR, Index);
}

int SerialFlash_writeSR(unsigned char cSR, int Index)
{
    SerialFlash_doWREN(Index);
    return SerialFlash_doWRSR(cSR, Index);
}

int SerialFlash_is_protectbits_set(int Index)
{
    unsigned char sr;
    SerialFlash_doRDSR(&sr, Index);

    return ((0 != (sr & 0x9C)) ? 1 : 0);
}
bool SerialFlash_StartofOperation(int Index)
{
    if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL || strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL || strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large) != NULL) {
        if (CN25Qxxx_Large_doWRVCR(0xFB, Index) == false)
            return false;
        if (CN25Qxxx_Large_doWRENVCR(0xFF, Index) == false)
            return false;

        return true;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ST_M25Pxx) != NULL) {
        if (strstr(g_ChipInfo.TypeName, "N25Q064") != NULL) {
            if (CN25Qxxx_Large_doWRVCR(0xFB, Index) == false)
                return false;
            if (CN25Qxxx_Large_doWRENVCR(0xDF, Index) == false)
                return false;
        }
        if (strstr(g_ChipInfo.TypeName, "N25Q128") != NULL) {
            if (CN25Qxxx_Large_doWRVCR(0xFB, Index) == false)
                return false;
            if (CN25Qxxx_Large_doWRENVCR(0xF7, Index) == false)
                return false;
        }
    }
    if (strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL) {
        if ((strstr(g_ChipInfo.TypeName, "MX66U1G45GXDJ54") != NULL) || (strstr(g_ChipInfo.TypeName, "MX66U2G45GXRI54") != NULL)) {
            MX25Lxxx_Large_doWRCR(0x70, Index);
        }
    }

	if((strstr(g_ChipInfo.TypeName,"TC58CVG1S3HRAIG" ) != NULL) ||(strstr(g_ChipInfo.TypeName,"TC58CVG1S3HRAIJ") != NULL)
		||(strstr(g_ChipInfo.TypeName,"TC58CVG0S3HRAIJ") != NULL))
	{
		if(!SF_Nand_HSBSet(Index))
			return false;
	}  

    return true;
}

bool SerialFlash_EndofOperation(int Index)
{
    return true;
}

void crc16l(WORD *crc, unsigned char rawdata) //
{ 
		unsigned int i;
        for(i=0x80; i!=0; i>>=1)
        {
            if((*crc&0x8000)!=0)
                {*crc<<=1; *crc^=0x1021;}
            else
                *crc<<=1;
            if((rawdata&i)!=0)
                *crc^=0x1021;
        }

}

void CalculateCrc(WORD *crc, unsigned char* start, unsigned int Length)
{
    unsigned int i;

    for (i=0; i<Length; i++)
    {
        crc16l(crc, start[i]); 
    }

} 

void DownloadICInfo(bool isErase, int USBIndex)
{
	SELECT_SPI_NAND_INFO nandInfo;
	unsigned char buf[sizeof(nandInfo)]={0};
	nandInfo.Hearder=0xA1A1; 
	nandInfo.Version=0x01; 
	nandInfo.Type=0x01; 

	nandInfo.STsize=sizeof(nandInfo); 
	nandInfo.ChipID=(((g_ChipInfo.JedecDeviceID&0xFF0000)>>16)|((g_ChipInfo.JedecDeviceID&0xFF00))|((g_ChipInfo.JedecDeviceID&0xFF)<<16)); 
	nandInfo.ChipIDMask=g_ChipInfo.ChipIDMask; 
	nandInfo.RealPagesize=g_ChipInfo.PageSizeInByte; //when scan bb, the size not include spare area size 
	nandInfo.PagesPerBlock=g_ChipInfo.BlockSizeInByte/g_ChipInfo.PageSizeInByte; 
	nandInfo.Blocks=g_ChipInfo.ChipSizeInByte/g_ChipInfo.BlockSizeInByte; 
	nandInfo.BlockCount=0;
	nandInfo.BlockIndex=0;

	nandInfo.EN_spareArea= g_bSpareAreaUseFile;
	nandInfo.MA_pagesize=g_ChipInfo.PageSizeInByte; 
	nandInfo.BeCmd=g_NANDContext.EraseCmd.ChipErase;  
	nandInfo.RdCmd=g_NANDContext.ReadCmd.SingleRead; 
	nandInfo.WrCmd=g_NANDContext.ProgramCmd.SingleProgram;//m_serial.ProgramCode; 
	nandInfo.AddrLen=g_ChipInfo.AddrWidth; 
	nandInfo.nCA_Rd=g_ChipInfo.nCA_Rd; 
	nandInfo.nCA_Wr=g_ChipInfo.nCA_Wr; 
	nandInfo.Dummy=g_ChipInfo.ReadDummyLen; 
	nandInfo.MaxERR_bits=g_ChipInfo.DefaultErrorBits; 
	nandInfo.BBMark=g_ChipInfo.BBMark;//800H is reserved for initial bad block mark 
	nandInfo.BB_BYTE_TYPE=0x11; //~0xFF  
 	nandInfo.BBM_TYPE=0x00;  
	nandInfo.BBK_TOTAL=0;  
	nandInfo.UNprotect_type=1;//m_serial.is_EnableUnprotect;  
	nandInfo.En_internalECC= 0;//(g_bNandInternalECC)?1:0;
	nandInfo.EraseStartBlock=0;//1:default enable   
	nandInfo.EraseEndBlock=(g_NANDContext.realChipSize/g_NANDContext.realBlockSize);
	nandInfo.EraseStatus=0; 
	nandInfo.saveSTA=0;  
	nandInfo.rfu0=0;
	nandInfo.perPageSizeForErrBit=g_ChipInfo.DefaultDataUnitSize;	
	if(strstr(g_ChipInfo.TypeName, "GD5F4GM5UExxG") != NULL)
		nandInfo.SPA_pagesize=g_ChipInfo.SpareSizeInByte>>16;//g_ChipInfo.SpareSizeInBytem_context->chip.realSpareAreaSize;
	else
		nandInfo.SPA_pagesize=g_NANDContext.realSpareAreaSize;

	nandInfo.VFMODE= ((1<<3)|(g_ChipInfo.DefaultErrorBits<<8));
									//[3]==1:new: [15:8]Max Error Bit allowed
									//[2:0]==MODE	0:DS 1:DSDSDS..DS 2:DDDD...DSSSS...S  3:DDD...D 4: for internal ECC skip
	nandInfo.VFCOFGL=nandInfo.SPA_pagesize;    
											   //for mode 0,1,2,3:
											   //[31:16]: Data Unit size;
											   //[15:0]: Ecc Unit size;
	nandInfo.VFCOFGH=nandInfo.MA_pagesize;	   
											   //for mode 4:
											   //[31:16]: User Data mask
											   //[15:0]: Internal ECC address mask
	nandInfo.staPVpagesize=nandInfo.SPA_pagesize;
	nandInfo.staPVpages=nandInfo.MA_pagesize;
	nandInfo.staPVsize=nandInfo.staPVpagesize*nandInfo.staPVpages;

	if(isErase == true){
		if(g_bNandForceErase ==true) 
			nandInfo.EraseMode=1; //force erase   
		else 
			nandInfo.EraseMode=2;//skip BB   
			
		nandInfo.StartBB=0;////set 1, start BB scan
		nandInfo.saveSTA=0;
	}else{
		nandInfo.EraseMode=0; //do not erase, just scan bad block
		nandInfo.StartBB=1; 
		nandInfo.saveSTA=0;  // no standalone
	}
	if(strstr(g_ChipInfo.Class,"W25N512GWxIN")) 
		nandInfo.ReadSatus_mode=2;//2:w25N512G
	else
		nandInfo.ReadSatus_mode=1;//1:other 
	
	for(int i=0;i<34;i++)
		nandInfo.rfu[i]=0;

	nandInfo.ReadSatus_mode=1;//1:other 
	memcpy(buf, &nandInfo.Hearder,sizeof(nandInfo));
    WORD crc=0xffff;
	CalculateCrc(&crc,buf,sizeof(nandInfo)-2);
	nandInfo.crc=crc; 
	//	
	memset(buf,0,sizeof(nandInfo));
	memcpy(buf, &nandInfo.Hearder,sizeof(nandInfo)); 
	
	if(!DownloadICInfoForNand(0x200,USBIndex))
		return;
	
	BulkPipeWrite(buf,sizeof(buf),USB_TIMEOUT,USBIndex);

	#if 0
	printf("		nandInfo.Hearder=0xA1A1;\n ");
	printf("		nandInfo.Version=0x01; \n ");
	printf("		nandInfo.Type=0x01;   \n");
	printf("		nandInfo.STsize=%d;   \n",nandInfo.STsize);
	printf("		nandInfo.ChipID=0x%x\n",nandInfo.ChipID);
	printf("		nandInfo.ChipIDMask=0x%lx\n",g_ChipInfo.ChipIDMask);
	printf("		nandInfo.RealPagesize=0x%x\n",nandInfo.RealPagesize);
	printf("		nandInfo.PagesPerBlock=0x%x\n",nandInfo.PagesPerBlock);
	printf("		nandInfo.Blocks=0x%x\n",nandInfo.Blocks);
	printf("		nandInfo.EN_spareArea=0x%x\n",nandInfo.EN_spareArea);
	printf("		nandInfo.SPA_pagesize=0x%x\n",nandInfo.SPA_pagesize);
	printf("		nandInfo.MA_pagesize=0x%x\n",nandInfo.MA_pagesize);
	printf("		nandInfo.BeCmd=0x%x\n",nandInfo.BeCmd); 
	printf("		nandInfo.RdCmd=0x%x\n",nandInfo.RdCmd); 
	printf("		nandInfo.WrCmd=0x%x\n",nandInfo.WrCmd); 
	printf("		nandInfo.AddrLen=0x%x\n",nandInfo.AddrLen); 
	printf("		nandInfo.nCA_Rd=0x%x\n",nandInfo.nCA_Rd);	
	printf("		nandInfo.nCA_Wr=0x%x\n",nandInfo.nCA_Wr);	
	printf("		nandInfo.Dummy=0x%x\n",nandInfo.Dummy); 
	printf("		nandInfo.MaxERR_bits=0x%x\n",nandInfo.MaxERR_bits);
	printf("		nandInfo.BBMark=0x%x\n",nandInfo.BBMark);	
	printf("		nandInfo.BB_BYTE_TYPE=0x%x\n",nandInfo.BB_BYTE_TYPE);	
	printf("		nandInfo.BBM_TYPE=0x%x\n",nandInfo.BBM_TYPE);
	printf("		nandInfo.BBK_TOTAL=0x%x\n",nandInfo.BBK_TOTAL);
	printf("		nandInfo.StartBB=0x%x\n",nandInfo.StartBB);
	printf("		nandInfo.UNprotect_type=0x%x\n",nandInfo.UNprotect_type);	
	printf("		nandInfo.En_internalECC=0x%x\n",nandInfo.En_internalECC);
	printf("		nandInfo.EraseStartBlock=0x%x\n",nandInfo.EraseStartBlock);	
	printf("		nandInfo.EraseEndBlock=0x%x\n",nandInfo.EraseEndBlock);	
	printf("		nandInfo.EraseMode=0x%x\n",nandInfo.EraseMode);
	printf("		nandInfo.EraseStatus=0x%x\n",nandInfo.EraseStatus); 
	printf("		nandInfo.saveSTA=0x%x\n",nandInfo.saveSTA);
	printf("		nandInfo.VFMODE=0x%x\n",nandInfo.VFMODE);
	printf("		nandInfo.VFCOFGL=0x%x\n",nandInfo.VFCOFGL);	
	printf("		nandInfo.VFCOFGH=0x%x\n",nandInfo.VFCOFGH);
	printf("		nandInfo.staPVpagesize=0x%x\n",nandInfo.staPVpagesize);	
	printf("		nandInfo.staPVpages=0x%x\n",nandInfo.staPVpages);	
	printf("		nandInfo.staPVsize=0x%x\n",nandInfo.staPVsize);
	printf("		nandInfo.perPageSizeForErrBit=0x%x\n",nandInfo.perPageSizeForErrBit);	
	printf("		nandInfo.ReadSatus_mode=0x%x\n",nandInfo.ReadSatus_mode);	
	printf("		nandInfo.crc=0x%x\n",nandInfo.crc);
	#endif
}

bool SPINAND_ScanBadBlock( unsigned short *BBT, unsigned short *BBT_Cnt, int USBIndex)
{    
	 if (!SerialFlash_StartofOperation(USBIndex))
	 	return false;
	 #if 1
	SELECT_SPI_NAND_INFO nandInfo;
	DownloadICInfo( false, USBIndex);
	#else
	SELECT_SPI_NAND_INFO nandInfo;
	nandInfo.Hearder=0xA1A1; 
	nandInfo.Version=0x01; 
	nandInfo.Type=0x01; 

	nandInfo.STsize=sizeof(nandInfo); 
	nandInfo.ChipID=(((g_ChipInfo.JedecDeviceID&0xFF0000)>>16)|((g_ChipInfo.JedecDeviceID&0xFF00))|((g_ChipInfo.JedecDeviceID&0xFF)<<16)); 
	nandInfo.ChipIDMask=g_ChipInfo.ChipIDMask; 

	nandInfo.RealPagesize=g_ChipInfo.PageSizeInByte; //when scan bb, the size not include spare area size 
	
	nandInfo.PagesPerBlock=g_ChipInfo.BlockSizeInByte/g_ChipInfo.PageSizeInByte; 
	nandInfo.Blocks=g_ChipInfo.ChipSizeInByte/g_ChipInfo.BlockSizeInByte; 
	#if 0
	printf("		nandInfo.Hearder=0xA1A1;\n ");
	printf("		nandInfo.Version=0x01; \n ");
	printf("		nandInfo.Type=0x01;   \n");
	printf("		nandInfo.STsize=%d;   \n",nandInfo.STsize);
	printf("		nandInfo.ChipID=0x%x\n",nandInfo.ChipID);
	printf("		nandInfo.ChipIDMask=0x%lx\n",g_ChipInfo.ChipIDMask);
	printf("		nandInfo.RealPagesize=0x%x\n",nandInfo.RealPagesize);
	printf("		nandInfo.PagesPerBlock=0x%x\n",nandInfo.PagesPerBlock);
	printf("		nandInfo.Blocks=0x%x\n",nandInfo.Blocks);
	#endif
	nandInfo.BlockCount=0;
	nandInfo.BlockIndex=0;

	nandInfo.EN_spareArea= g_bSpareAreaUseFile;
	if(strstr(g_ChipInfo.TypeName, "GD5F4GM5UExxG") != NULL)
		nandInfo.SPA_pagesize=g_ChipInfo.SpareSizeInByte>>16;//g_ChipInfo.SpareSizeInBytem_context->chip.realSpareAreaSize;
	else
		nandInfo.SPA_pagesize=g_NANDContext.realSpareAreaSize;
	nandInfo.MA_pagesize=g_ChipInfo.PageSizeInByte; 
	#if 0
	printf("		nandInfo.EN_spareArea=0x%x\n",nandInfo.EN_spareArea);
	printf("		nandInfo.SPA_pagesize=0x%x\n",nandInfo.SPA_pagesize);
	printf("		nandInfo.MA_pagesize=0x%x\n",nandInfo.MA_pagesize);
	#endif
	nandInfo.BeCmd=g_NANDContext.EraseCmd.ChipErase;  
	nandInfo.RdCmd=g_NANDContext.ReadCmd.SingleRead; 
	nandInfo.WrCmd=g_NANDContext.ProgramCmd.SingleProgram;//m_serial.ProgramCode; 
	nandInfo.AddrLen=g_ChipInfo.AddrWidth; 
	nandInfo.nCA_Rd=g_ChipInfo.nCA_Rd; 
	nandInfo.nCA_Wr=g_ChipInfo.nCA_Wr; 
	nandInfo.Dummy=g_ChipInfo.ReadDummyLen; 
	nandInfo.MaxERR_bits=g_ChipInfo.DefaultErrorBits; 
	#if 0
	printf("		nandInfo.BeCmd=0x%x\n",nandInfo.BeCmd);	
	printf("		nandInfo.RdCmd=0x%x\n",nandInfo.RdCmd);	
	printf("		nandInfo.WrCmd=0x%x\n",nandInfo.WrCmd);	
	printf("		nandInfo.AddrLen=0x%x\n",nandInfo.AddrLen);	
	printf("		nandInfo.nCA_Rd=0x%x\n",nandInfo.nCA_Rd);	
	printf("		nandInfo.nCA_Wr=0x%x\n",nandInfo.nCA_Wr);	
	printf("		nandInfo.Dummy=0x%x\n",nandInfo.Dummy);	
	printf("		nandInfo.MaxERR_bits=0x%x\n",nandInfo.MaxERR_bits);
	#endif
	nandInfo.BBMark=g_ChipInfo.BBMark;//800H is reserved for initial bad block mark 
	nandInfo.BB_BYTE_TYPE=0x11; //~0xFF  
 	nandInfo.BBM_TYPE=0x00;  
	nandInfo.BBK_TOTAL=0;  
	#if 0
	printf("		nandInfo.BBMark=0x%x\n",nandInfo.BBMark);	
	printf("		nandInfo.BB_BYTE_TYPE=0x%x\n",nandInfo.BB_BYTE_TYPE);	
	printf("		nandInfo.BBM_TYPE=0x%x\n",nandInfo.BBM_TYPE);
	printf("		nandInfo.BBK_TOTAL=0x%x\n",nandInfo.BBK_TOTAL);
	#endif
	nandInfo.StartBB=1; 
	nandInfo.UNprotect_type=1;//m_serial.is_EnableUnprotect;  
	nandInfo.En_internalECC= 0;//(g_bNandInternalECC)?1:0;
	nandInfo.EraseStartBlock=0;//1:default enable   
	nandInfo.EraseEndBlock=(g_NANDContext.realChipSize/g_NANDContext.realBlockSize);
	nandInfo.EraseMode=0; //do not erase
	nandInfo.EraseStatus=0; 
	nandInfo.saveSTA=0;  
	#if 0
	printf("		nandInfo.StartBB=0x%x\n",nandInfo.StartBB);
	printf("		nandInfo.UNprotect_type=0x%x\n",nandInfo.UNprotect_type);	
	printf("		nandInfo.En_internalECC=0x%x\n",nandInfo.En_internalECC);
	printf("		nandInfo.EraseStartBlock=0x%x\n",nandInfo.EraseStartBlock);	
	printf("		nandInfo.EraseEndBlock=0x%x\n",nandInfo.EraseEndBlock);	
	printf("		nandInfo.EraseMode=0x%x\n",nandInfo.EraseMode);
	printf("		nandInfo.EraseStatus=0x%x\n",nandInfo.EraseStatus); 
	printf("		nandInfo.saveSTA=0x%x\n",nandInfo.saveSTA);
	 #endif
	nandInfo.rfu0=0;
	nandInfo.perPageSizeForErrBit=g_ChipInfo.DefaultDataUnitSize;	
	
	if(strstr(g_ChipInfo.Class,"W25N512GWxIN")) 
		nandInfo.ReadSatus_mode=2;//2:w25N512G
	else
		nandInfo.ReadSatus_mode=1;//1:other 
		#if 0
	nandInfo.VFMODE= ((1<<3)|(g_ChipInfo.DefaultErrorBits<<8));
									//[3]==1:new: [15:8]Max Error Bit allowed
									//[2:0]==MODE	0:DS 1:DSDSDS..DS 2:DDDD...DSSSS...S  3:DDD...D 4: for internal ECC skip
	nandInfo.VFCOFGL=nandInfo.SPA_pagesize;    
											   //for mode 0,1,2,3:
											   //[31:16]: Data Unit size;
											   //[15:0]: Ecc Unit size;
	nandInfo.VFCOFGH=nandInfo.MA_pagesize;	   
											   //for mode 4:
											   //[31:16]: User Data mask
											   //[15:0]: Internal ECC address mask
	nandInfo.staPVpagesize=0xcdcd;//nandInfo.SPA_pagesize;
	nandInfo.staPVpages=0xcdcdcdcd;//nandInfo.MA_pagesize;
	nandInfo.staPVsize=0xcdcdcdcd;//nandInfo.staPVpagesize*nandInfo.staPVpages;
	printf("		nandInfo.VFMODE=0x%x\n",nandInfo.VFMODE);
	printf("		nandInfo.VFCOFGL=0x%x\n",nandInfo.VFCOFGL);	
	printf("		nandInfo.VFCOFGH=0x%x\n",nandInfo.VFCOFGH);
	printf("		nandInfo.staPVpagesize=0x%x\n",nandInfo.staPVpagesize);	
	printf("		nandInfo.staPVpages=0x%x\n",nandInfo.staPVpages);	
	printf("		nandInfo.staPVsize=0x%x\n",nandInfo.staPVsize);
	#endif
	for(int i=0;i<34;i++)
		nandInfo.rfu[i]=0;

	nandInfo.ReadSatus_mode=1;//1:other 
	#if 0
	printf("		nandInfo.perPageSizeForErrBit=0x%x\n",nandInfo.perPageSizeForErrBit);	
	printf("		nandInfo.ReadSatus_mode=0x%x\n",nandInfo.ReadSatus_mode);	
	#endif
	unsigned char buf[sizeof(nandInfo)]={0};
	memcpy(buf, &nandInfo.Hearder,sizeof(nandInfo));
    WORD crc=0xffff;
	CalculateCrc(&crc,buf,sizeof(nandInfo)-2);
	nandInfo.crc=crc; 
	//printf("		nandInfo.crc=0x%x\n",nandInfo.crc);	
	memset(buf,0,sizeof(nandInfo));
	memcpy(buf, &nandInfo.Hearder,sizeof(nandInfo)); 
	//printf("		sizeof(nandInfo)=%ld\n", sizeof(nandInfo));
	#endif
	Sleep(2000);
	Nand_ReadICInfo(&nandInfo,BBT,BBT_Cnt,USBIndex);
	return true;
}

bool DownloadICInfoForNand(DWORD dwInfoLen, int USBIndex)//SF700
{
 	CNTRPIPE_RQ rq ; 

	unsigned char  vInfoLen[4]; //0x200
	dwInfoLen=dwInfoLen>>9;
	vInfoLen[0] = ((unsigned char)(dwInfoLen));
	vInfoLen[1] = ((unsigned char)(dwInfoLen>>8));
	vInfoLen[2] = ((unsigned char)(dwInfoLen>>16));
	vInfoLen[3] = ((unsigned char)(dwInfoLen>>24));
 
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = DL_IC_INFO_NAND ;
	rq.Value = 0 ;       
	rq.Index = 0 ;		 
	rq.Length = 1;
	 
	if(!OutCtrlRequest(&rq, vInfoLen, 1,USBIndex))
		return false;

	return true;
}

bool ReadICInfoForNand(DWORD dwInfoLen, int USBIndex)
{ //SF700
 	CNTRPIPE_RQ rq ; 

	unsigned char vInfoLen[4]; //0x200

	vInfoLen[0] = (dwInfoLen); 
	vInfoLen[1] = (dwInfoLen>>8);
	vInfoLen[2] = (dwInfoLen>>16);
	vInfoLen[3] = (dwInfoLen>>24);
 
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = READ_IC_INFO_NAND ;
	rq.Value = 0 ;      
	rq.Index = 0 ;		 
	rq.Length = 4 ; 
	 
	if(!OutCtrlRequest(&rq, vInfoLen, sizeof(vInfoLen), USBIndex))
		return false;
	return true;
}


bool Nand_ReadICInfo(SELECT_SPI_NAND_INFO *nandInfo, unsigned short *BBT, unsigned short *BBT_Cnt,int USBIndex)
{   
#define min(a, b) (a > b ? b : a)

	unsigned char vData[0x400];   
	//SELECT_SPI_NAND_INFO nandInfo;
	if(!ReadICInfoForNand(0x400,USBIndex))
		return false; 

	for(int i=0;i<2;i++)
	{
		if(!BulkPipeRead(vData+i*0x200, USB_TIMEOUT, USBIndex))
			return false ; 
	}  
	memcpy((unsigned char*)&nandInfo->Hearder,vData, sizeof(SELECT_SPI_NAND_INFO));
	//printf("nandInfo.BBK_TOTAL=%d\n",nandInfo->BBK_TOTAL);
	memcpy(BBT,vData+0x200,min((nandInfo->BBK_TOTAL*2),0x200));
	*BBT_Cnt = nandInfo->BBK_TOTAL;
	//for(unsigned int i=0; i < min((nandInfo.BBK_TOTAL),0x100); i++)
	//	printf("BBT[%d]=%d\n",i, BBT[i]);
	return true;
}

bool Nand_waitForWEL(int USBIndex)
{    
	size_t i = g_ChipInfo.Timeout*100;  
	unsigned char cSR;
	unsigned char vOut[2]; 

	vOut[0] = WREN;
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut,1,USBIndex))
		return false; 
	Sleep(1); 
	 i = g_ChipInfo.Timeout*100;  
	do{  
		vOut[0] = 0x0F;//.GetFeatures;
		vOut[1] = 0xC0;//.push_back(m_context->serialFlash.SRStatus); 
		if(!FlashCommand_SendCommand_OneOutOneIn(vOut,2,&cSR,1,USBIndex)) 
			return false;  
		Sleep(100);
	}while(((cSR & 0x02)== 0) && (i-- > 1)) ; 
 
    return true;
}  
bool Nand_waitForOIP(int USBIndex)
{  
	size_t i = g_ChipInfo.Timeout*100;  
	unsigned char cSR;
	unsigned char vOut[2];
	do{  
		vOut[0] = 0x0F;//.GetFeatures;
		vOut[1] = 0xC0;//.push_back(m_context->serialFlash.SRStatus); 
		if(!FlashCommand_SendCommand_OneOutOneIn(vOut,2,&cSR,1,USBIndex)) 
			return false;  
		Sleep(100);
	}while((cSR & 0x01) && (i-- > 1)) ;

    if((i<=0)||((cSR&0x0C)!=0)) 
		return false;

	Sleep(2);
    return true;
}   

bool Nand_WRSRProtection(unsigned char cSR,int USBIndex)
{ 
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//.push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xA0;//.push_back(m_context->serialFlash.SRProtection); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut,sizeof(vOut),USBIndex)) 
		return false;  
	Sleep(2);
	return true;
}

bool Nand_WRSRFeature1(unsigned char cSR,int USBIndex)
{
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//.push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xB0;//.push_back(m_context->serialFlash.SRFeature1); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut,sizeof(vOut),USBIndex)) 
		return false;  
	Sleep(2);
	return true;
}
bool Nand_WRSRStatus(unsigned char cSR,int USBIndex)
{
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//. push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xC0;//.push_back(m_context->serialFlash.SRStatus); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut,sizeof(vOut), USBIndex)) 
		return false;  
	Sleep(2);
	return true;
}
bool Nand_WRSRFeature2(unsigned char cSR,int USBIndex)
{
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//.push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xD0;//.push_back(m_context->serialFlash.SRFeature2); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut, sizeof(vOut), USBIndex)) 
		return false;  
	Sleep(2);
	return true;
}

bool Nand_RDSRProtection(unsigned char *cSR,int USBIndex)
{    
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xA0;//.push_back(m_context->serialFlash.SRProtection); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut,sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}

bool Nand_RDSRFeature1(unsigned char *cSR,int USBIndex)
{
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xB0;//.push_back(m_context->serialFlash.SRFeature1); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}
bool Nand_RDSRStatus1(unsigned char *cSR,int USBIndex)
{
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xC0;//.push_back(m_context->serialFlash.SRStatus); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}
bool Nand_RDSRFeature2(unsigned char *cSR,int USBIndex)
{
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xD0;//.push_back(m_context->serialFlash.SRFeature2); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}   


bool Nand_Winbond_RDSR1(unsigned char *cSR,int USBIndex)
{    
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xA0;//.push_back(m_context->serialFlash.Winbond_SR1); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}
bool Nand_Winbond_RDSR2(unsigned char *cSR,int USBIndex)
{    
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xB0;//.push_back(m_context->serialFlash.Winbond_SR2); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}
bool Nand_Winbond_RDSR3(unsigned char *cSR,int USBIndex)
{    
	unsigned char vOut[2], vIn ;   
	vOut[0] = 0x0F;//.push_back(m_context->serialFlash.GetFeatures);
	vOut[1] = 0xC0;//.push_back(m_context->serialFlash.Winbond_SR3); 
	if(!FlashCommand_SendCommand_OneOutOneIn(vOut, sizeof(vOut), &vIn, 1, USBIndex)) 
		return false;  
	*cSR=vIn;  
    return true;
}

bool Nand_Winbond_WRSR1(unsigned char cSR,int USBIndex)
{ 
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//.push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xA0;//.push_back(m_context->serialFlash.Winbond_SR1); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut, sizeof(vOut), USBIndex)) 
		return false;  
	return true;
}
bool Nand_Winbond_WRSR2(unsigned char cSR,int USBIndex)
{ 
	if(!Nand_waitForWEL(USBIndex))
		return false;
	unsigned char vOut[3];   
	vOut[0] = 0x1F;//.push_back(m_context->serialFlash.SetFeatures);
	vOut[1] = 0xB0;//.push_back(m_context->serialFlash.Winbond_SR2); 
	vOut[2] = cSR;//.push_back(cSR); 
	if(!FlashCommand_SendCommand_OutOnlyInstruction(vOut, sizeof(vOut), USBIndex)) 
		return false;  
	return true;
}

bool SF_Nand_HSBSet( int Index)
{
	unsigned char cSR=0;
	Nand_RDSRFeature1(&cSR,Index);
	Nand_WRSRFeature1(cSR&0xFD,Index);//HSB
	Nand_RDSRFeature1(&cSR,Index);

	if((cSR&0x02)!=0x00)
		return false;
	return true;
} 

bool SPINAND_ProtectBlock(bool bProtect,int USBIndex)
{
    //Protect  A0H    BRWD/Reserved/BP2/BP1/BP0/INV/CMP/Reserve
	bool result = false ;
    unsigned char  tmpSRVal=0;
    unsigned char dstSRVal =0;
	int numOfRetry = 100 ;

    result = Nand_RDSRProtection(&dstSRVal,USBIndex) ;

    // un-protect block ,set BRWD BP2 BP1 BP0 to 0
    dstSRVal &= (~0xFD) ;

    // protect block ,set BP2 BP1 BP2 to 1
    if(bProtect){
        dstSRVal += 0xFD ;    // B8 : 1011 1000
    }

	if((g_ChipInfo.ProtectBlockMask & 0x02) ||((strstr(g_ChipInfo.TypeName,"S35ML01G3") != NULL) ||(strstr(g_ChipInfo.TypeName,"S35ML02G3")!= NULL) 
		||(strstr(g_ChipInfo.TypeName, "S35ML04G3")!=NULL)))
	{
		dstSRVal |= 0x02;
		result = Nand_WRSRProtection(dstSRVal,USBIndex) ;
	}
	do
	{ // WIP = TRUE;  
		result &= Nand_WRSRProtection(dstSRVal,USBIndex) ;
		result &= Nand_RDSRProtection(&tmpSRVal,USBIndex) ; 
		 
		if(! result) 
			return false; 
		numOfRetry -- ;  
	}while( (tmpSRVal & 0x01) &&  numOfRetry > 0);

  
	if((tmpSRVal ^ dstSRVal)& 0x0C ) 
		return false;

	return true;

}

bool Nand_BUFSet( int USBIndex)
{
	unsigned char cSR=0;
	Nand_Winbond_RDSR2(&cSR,USBIndex);
	cSR&=0xF9;
	Nand_Winbond_WRSR2(cSR|0x08,USBIndex);
	Nand_Winbond_RDSR2(&cSR,USBIndex);
	if((cSR&0x08)!=0x08)
		return false;
	return true;
}

bool Nand_CaculateErrorBit(unsigned char *pData, unsigned char *pFile, unsigned int ptr_size)
{
	unsigned int WholeFileLenth = ptr_size;//vData.size();  
	//unsigned int UnitCount = 0; 
	if(g_bSpareAreaUseFile == true)
	{
		for(unsigned int PageCount=0 ; PageCount< WholeFileLenth/g_NANDContext.realPageSize ; PageCount++)
		{
			unsigned char aPageSizeReadBuf[g_NANDContext.realPageSize];
			unsigned char aPageSizeFileBuf[g_NANDContext.realPageSize];
			memset(aPageSizeReadBuf, 0xFF, g_NANDContext.realPageSize);
			memset(aPageSizeFileBuf, 0xFF, g_NANDContext.realPageSize);
			memcpy(aPageSizeReadBuf, pData+PageCount*g_NANDContext.realPageSize, g_NANDContext.realPageSize);
			memcpy(aPageSizeFileBuf, pFile+PageCount*g_NANDContext.realPageSize, g_NANDContext.realPageSize);
			
			unsigned int DataUnitCount = (g_NANDContext.realPageSize/g_ChipInfo.DefaultDataUnitSize);    

			for(unsigned int i=0;i<DataUnitCount;i++)
			{
				unsigned int errorbit=0;
				unsigned char DataUnitBuf_Read[g_ChipInfo.DefaultDataUnitSize];//,0xFF); 
				unsigned char DataUnitBuf_File[g_ChipInfo.DefaultDataUnitSize];
					
				memcpy(DataUnitBuf_Read, aPageSizeReadBuf+i*g_ChipInfo.DefaultDataUnitSize, g_ChipInfo.DefaultDataUnitSize);
				memcpy(DataUnitBuf_File, aPageSizeFileBuf+i*g_ChipInfo.DefaultDataUnitSize, g_ChipInfo.DefaultDataUnitSize);
						  
				for(unsigned int j=0;j<g_ChipInfo.DefaultDataUnitSize;j++)
				{
					if(DataUnitBuf_Read[j]!=DataUnitBuf_File[j])
					{
						unsigned char uc1=DataUnitBuf_Read[j];
						unsigned char uc2=DataUnitBuf_File[j];
						for(int c=0;c<8;c++)
						{
							if(((uc1>>c)&0x01)!=((uc2>>c)&0x01))
							{
								if(errorbit<g_ChipInfo.DefaultErrorBits)
									errorbit++;
								else
									return false;
							}
						}
					}
				}
			}
		} 
	}
	else //spare area unuse file //caculate Data Unit only
	{  
		for(unsigned int PageCount=0 ; PageCount< WholeFileLenth/g_ChipInfo.PageSizeInByte ; PageCount++)
		{ 
			unsigned char aPageSizeReadBuf[g_ChipInfo.PageSizeInByte];//,0xFF);
			unsigned char aPageSizeFileBuf[g_ChipInfo.PageSizeInByte];//,0xFF);
			memcpy(aPageSizeReadBuf, pData+PageCount*g_ChipInfo.PageSizeInByte, g_ChipInfo.PageSizeInByte);
			memcpy(aPageSizeFileBuf, pFile+PageCount*g_ChipInfo.PageSizeInByte, g_ChipInfo.PageSizeInByte);
				 //All Area
			unsigned int DataUnitCount = (g_ChipInfo.PageSizeInByte/g_ChipInfo.DefaultDataUnitSize);    
			unsigned int realDataUnitSize= (g_ChipInfo.PageSizeInByte/DataUnitCount);

			for(unsigned int i=0;i<DataUnitCount;i++)
			{
				unsigned int errorbit=0;
				unsigned char DataUnitBuf_Read[realDataUnitSize];//,0xFF); 
				unsigned char DataUnitBuf_File[realDataUnitSize];
				memcpy(DataUnitBuf_Read, aPageSizeReadBuf+i*realDataUnitSize, realDataUnitSize);
				memcpy(DataUnitBuf_File, aPageSizeFileBuf+i*realDataUnitSize, realDataUnitSize);
						  
				for(unsigned int j=0;j<realDataUnitSize;j++)
				{
					if(DataUnitBuf_Read[j]!=DataUnitBuf_File[j])
					{
						unsigned char uc1=DataUnitBuf_Read[j];
						unsigned char uc2=DataUnitBuf_File[j];
						for(int c=0;c<8;c++)
						{
							if(((uc1>>c)&0x01)!=((uc2>>c)&0x01))
							{
								if(errorbit<g_ChipInfo.DefaultErrorBits)
									errorbit++;
								else
									return false;
							}
						}
					}
				}
			}
		}
	} 	
	return true;
}

bool MarkNewBadBlock(DWORD dwAddr, int USBIndex)
{
	unsigned char *pBuf = (unsigned char*)malloc(g_NANDContext.realBlockSize);
	memset(pBuf, 0, g_NANDContext.realBlockSize);
	WORD BlockPages =(g_NANDContext.realBlockSize/g_NANDContext.realPageSize);
	FlashCommand_SendCommand_SetupPacketForBulkWriteNAND(dwAddr,g_NANDContext.realBlockSize>>9, mcode_ProgramCode_4Adr,g_NANDContext.realPageSize,
			BlockPages,mcode_Program,g_ChipInfo.AddrWidth, g_ChipInfo.ReadDummyLen,g_ChipInfo.nCA_Wr,USBIndex);
	size_t WriteCounter=g_NANDContext.realBlockSize>>9 ;  
	for(size_t i = 0; i < WriteCounter; ++ i)
	{ 
		BulkPipeWrite(pBuf, 1<<9, USB_TIMEOUT,USBIndex);
		pBuf += (1<<9);
	}
	free(pBuf);
	return true;
}


unsigned int SF_GetLoop(const struct CAddressRange* AddrRange, unsigned int uiUnit)
{
	struct CAddressRange AddrLoop;//(*AddrRange);
	AddrLoop.start = AddrRange->start;
	AddrLoop.end= AddrRange->end;
	AddrLoop.start= AddrRange->start-(AddrRange->start%uiUnit);
	AddrLoop.end= AddrLoop.end + ((AddrLoop.end % uiUnit)? (uiUnit-(AddrLoop.end % uiUnit)):0);
	AddrLoop.length = AddrLoop.end-AddrLoop.start;
	unsigned int loop = AddrLoop.length/uiUnit;
	return loop;
}

bool SPINAND_EnableInternalECC(bool is_ENECC,int USBIndex)
{ 
	unsigned char ucData=0,ucTempData=0;
	// if((g_ChipInfo.TypeName,"MX35LF1G24") != NULL))
	//	 return true;
	if(!g_ChipInfo.ECCEnable)
		return true;
	if(!Nand_RDSRFeature1(&ucData,USBIndex))
		return false;
	ucTempData=ucData;
	 

	if(is_ENECC)
	{	 
		if((ucTempData&0x10) == 0x10)
			return true; 
		if(!Nand_WRSRFeature1(ucData|0x10,USBIndex))
			return false; 
		if(!Nand_RDSRFeature1(&ucTempData,USBIndex))
			return false;
		if(ucTempData==(ucData|0x10))
			return true;
		else 
			return false;
	}
	else
	{   
		ucData&=0xEF;
		 if(!Nand_WRSRFeature1(ucData,USBIndex))
			return false; 
		if(!Nand_RDSRFeature1(&ucTempData,USBIndex))
			return false;
		if((ucTempData&0x10)==0x10)
			return false;  
	}
	return true; 
}

bool SPINAND_DisableContinueRead(int USBIndex)
{ 
	if((strstr(g_ChipInfo.TypeName,"MT29F4G") != NULL)
		||(strstr(g_ChipInfo.TypeName,"F50L4G41XB") != NULL)
		||(strstr(g_ChipInfo.TypeName,"TC58CVG2S0HRAIJ") != NULL))
	{
		unsigned char ucData=0,ucTempData=0;
	 
		if(!Nand_RDSRFeature1(&ucData,USBIndex))
			return false;
		 if(!Nand_WRSRFeature1(ucData&0xFC,USBIndex))
			return false; 
		if(!Nand_RDSRFeature1(&ucTempData,USBIndex))
			return false;
		if(ucTempData!=(ucData&0xFC)) 
			return false;
	}
	else if((strstr(g_ChipInfo.TypeName, "W25N512GWxIT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GWxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GVxxG") != NULL) 
		||(strstr(g_ChipInfo.TypeName,"W25N512GVxxR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01GWxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01GVxxIT") != NULL) 
		||(strstr(g_ChipInfo.TypeName,"W25N01GVxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01JWxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01JWxxIT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N02JWxxIC") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N02JWxxIF") != NULL) 
		||(strstr(g_ChipInfo.TypeName,"W35N01JWxxxG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W35N01JWxxxT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N04KVxxIR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N04KVxxIU") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N02KVxxIR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01KVxxIR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01KVxxIU") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N04KWxxIR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N04KWxxIU") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W35N02JW") != NULL))
	{
		if(!SPINAND_EnableInternalECC(true,USBIndex))
			return false;
		if(!Nand_BUFSet(USBIndex))
			return false;
	}
	return true; 
} 

bool SPINAND_EnableQuadIO(bool bENQuad,bool boRW,int Index)
{  
	if(!boRW)
	{
		return true;
	}
	if((strstr(g_ChipInfo.TypeName,"W25N512GWxIT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GWxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GVxxG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GVxxT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GVxxR") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N512GWxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01GWxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01GVxxIT") != NULL) 
		||(strstr(g_ChipInfo.TypeName,"W25N01GVxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01JWxxIG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N01JWxxIT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N02JWxxIC") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W25N02JWxxIF") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W35N01JWxxxG") != NULL)
		||(strstr(g_ChipInfo.TypeName,"W35N01JWxxxT") != NULL)
		||(strstr(g_ChipInfo.TypeName,"S35ML02G3") != NULL)) 
	{
		
		return true; 
	}
	unsigned char ucData=0,ucTempData=0;
	 
	if(!Nand_RDSRFeature1(&ucData,Index))
		return false;
	if(!bENQuad) //disable quad io
	{ 
		ucData&=0xFE;
		 if(!Nand_WRSRFeature1(ucData,Index))
			return false; 
		if(!Nand_RDSRFeature1(&ucTempData,Index))
			return false;
		if(ucTempData!=ucData)
			return false;
	}
	else
	{ 
		 if(!Nand_WRSRFeature1(ucData|0x01,Index))
			return false; 
		if(!Nand_RDSRFeature1(&ucTempData,Index))
			return false;
		if(ucTempData!=(ucData|0x01))
			return false;
	}
	return true; 
}

bool SPINAND_chipErase(int USBIndex)
{      
	if(! SPINAND_ProtectBlock(false,USBIndex) ) 
        return false ; 
	
	unsigned short vICInfo[0x200];
	unsigned short cnt=0;
	unsigned char ucSR=0;
	SELECT_SPI_NAND_INFO nandInfo;
	SetSPIClockDefault(USBIndex); 
	DownloadICInfo(true,USBIndex);
	SetSPIClock(USBIndex);
	//Sleep(2000);
	Nand_ReadICInfo(&nandInfo,vICInfo,&cnt,USBIndex);
	 
	if(nandInfo.EraseStatus!=0)
	{
		return false ;
	}
	
	if(!Nand_RDSRStatus1(&ucSR,USBIndex))
		return false;
	return true ;
} 


bool Nand_bulkPipeVerify(struct CAddressRange* AddrRange,unsigned char modeRead,unsigned char ReadCom, int USBIndex)
{     
//	if(!&g_ChipInfo)
//		return false;   

	size_t BlockPages=g_NANDContext.realBlockSize/g_NANDContext.realPageSize;
  	unsigned char *pData = NULL;//[0x1000000]; 
  	unsigned char *pFile = pBufferforLoadedFile;
	size_t ReadCounter=0;
	size_t divider=16; 
	
    pData = (unsigned char*)malloc(g_NANDContext.realBlockSize);
	memset(pData, 0xFF, g_NANDContext.realBlockSize);
	DWORD StartBlock = AddrRange->start/g_NANDContext.realBlockSize;	
	DWORD StartPage = AddrRange->start/g_NANDContext.realPageSize; 
	 
	unsigned int dwAddr=0;
	if(BlockPages==256)
		dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x7FFC0 ));   
	else if(BlockPages==128)
		dwAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80 ));  
	else
		dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0)) ;

	struct CAddressRange read_range;//(AddrRange); 
	read_range.start = AddrRange->start;
	read_range.end = AddrRange->end;
	read_range.length = AddrRange->length;

	size_t RealReadSize=read_range.length;

	FlashCommand_SendCommand_SetupPacketForBulkReadNAND(dwAddr,RealReadSize>>9, modeRead,g_NANDContext.realPageSize,BlockPages,ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen,g_ChipInfo.nCA_Rd,USBIndex);

	ReadCounter=(RealReadSize/(1<<divider));  
	if((RealReadSize%(1<<divider))!=0)
		ReadCounter += 1 ;
		
	for(size_t i = 0; i < ReadCounter; i++)
	{   
		BulkPipeReadEx(pData+(i<<divider),min((1<<divider),RealReadSize-(i<<divider)),USB_TIMEOUT, USBIndex);
	}   
		
	if(g_bNandInternalECC == false) //check ECC
	{//CompareBuf
		if(Nand_CaculateErrorBit(pData, pFile+read_range.start , RealReadSize) == false)
		{
			free(pData);
			return false; 
		}
	} 
	else
	{
		DWORD crc_v,crc_buf;
		if(g_bSpareAreaUseFile == false || g_ChipInfo.ECCParityGroup== 0) /*!((g_bSpareAreaUseFile == true) && (g_ChipInfo.ECCParityGroup!=0)))*/
		{   
			for(unsigned int i=1; i<BlockPages+1; i++)
				memset(pData+i*g_ChipInfo.PageSizeInByte+((i-1)*g_NANDContext.realSpareAreaSize), 0xFF, g_NANDContext.realSpareAreaSize);

			crc_v=CRC32(pData,RealReadSize);
			crc_buf=CRC32(pFile+read_range.start, RealReadSize);  
			if(crc_v!=crc_buf) 
			{
				for(int i=0; i<g_NANDContext.realBlockSize; i++)
				{
					//if(pData[i] != pFile[i+read_range.start])
					//	printf("pData[%d]=%d, pFile[%d]=%d\n",i,pData[i],i,pFile[i+read_range.start]);
				}

				free(pData);
				return false;
			}
		}
		else
		{  
		 	unsigned char vcTemp[g_ChipInfo.ECCProtectLength];//,0xff);  
		 	memset(vcTemp, 0xFF, g_ChipInfo.ECCProtectLength);
			if((g_ChipInfo.ECCParityGroup!=0)&&(g_ChipInfo.ECCProtectStartAddr<g_NANDContext.realSpareAreaSize))
			{ 
				for(unsigned int i=0; i<RealReadSize/g_NANDContext.realPageSize;i++)
				{
					for(int k=0;k<g_ChipInfo.ECCParityGroup;k++)
					{
						size_t SPsize=(i+1)*g_ChipInfo.PageSizeInByte+(i*g_NANDContext.realSpareAreaSize)+((g_ChipInfo.ECCProtectStartAddr+k*g_ChipInfo.ECCProtectDis)&0xff);
						memcpy(pData+SPsize,vcTemp, g_ChipInfo.ECCProtectLength);
						memcpy(pFile+SPsize, vcTemp, g_ChipInfo.ECCProtectLength);
					} 
				}  
			}
				 
			crc_v=CRC32(pFile+read_range.start,RealReadSize);
			crc_buf=CRC32(pData,RealReadSize);   
			if(crc_v!=crc_buf) 
			{
				//for(int i=0; i<8; i++)
					//printf("pData[%d]=%d, pFile[%d]=%d\n",pData[i],pFile[i+read_range.start]);
					
				free(pData);
				return false;  
			}
		} 
	}

	free(pData);
	return true;
}   

bool Nand_bulkPipeBlankCheck(const struct CAddressRange* AddrRange, unsigned char modeRead,unsigned char ReadCom,int USBIndex)
{   
	unsigned char *pData= NULL;// = new unsigned char[0x1000000]; 
//	unsigned char BBTscan=0; 
  
  	size_t BlockPages= g_NANDContext.realBlockSize/g_NANDContext.realPageSize;
  
	size_t divider=16; 
	size_t RunUnitSize=0; 
	switch(g_NANDContext.realBlockSize)
	{
		default:
		case 0x44000:
		case 0x22000:
			RunUnitSize = 0x880000; 
			break;
		case 0x42000:
		case 0x21000:
			RunUnitSize=0x840000;
			break;
		case 0x20800:
			RunUnitSize=0x820000;
			break;
		case 0x20400:
			RunUnitSize=0x810000;
			break; 
	}  
	//vector<unsigned char> vData(RunUnitSize,0xFF);     
	pData = (unsigned char* )malloc(RunUnitSize);
	memset(pData, 0xFF, RunUnitSize);
	
	if((AddrRange->start/RunUnitSize)!=(AddrRange->end/RunUnitSize)) 
	{ 
		struct CAddressRange range_temp;//(*AddrRange);
		range_temp.start=AddrRange->start;
		range_temp.end=AddrRange->end;
		range_temp.length=AddrRange->end-AddrRange->start;
		
		//size_t baseAddr=AddrRange->start-(AddrRange->start%RunUnitSize); 
		size_t loop=SF_GetLoop(AddrRange ,RunUnitSize);

        for(size_t j=0; j<loop; j++)
		{ 
            if(j==(loop-1))
                range_temp.end=range_temp.end;
            else
                range_temp.end=AddrRange->start-(AddrRange->start%RunUnitSize)+(RunUnitSize*(j+1));

            if(j==0)
                range_temp.start=AddrRange->start;
            else
                range_temp.start=AddrRange->start-(AddrRange->start%RunUnitSize)+(RunUnitSize*j);
			
			DWORD StartBlock = range_temp.start/g_NANDContext.realBlockSize;	
			DWORD StartPage = range_temp.start/g_NANDContext.realPageSize; 

			range_temp.length = range_temp.end-range_temp.start;
			unsigned int dwAddr=0;
			if(BlockPages==256)
				dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x7FFC0 ));  
			else if(BlockPages==128)
				dwAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80)) ;  
			else
				dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0)) ;
  
			size_t ReadCounter=range_temp.length >> divider ;  
			if((range_temp.length%(1<<divider))!=0)
				ReadCounter += 1 ;  
			
 			FlashCommand_SendCommand_SetupPacketForBulkReadNAND(dwAddr,range_temp.length/512, modeRead,g_NANDContext.realPageSize,BlockPages,ReadCom,g_ChipInfo.AddrWidth, g_ChipInfo.ReadDummyLen, g_ChipInfo.nCA_Rd,USBIndex);
		 	

			for(size_t i = 0; i < ReadCounter; i++)
			{     
				if(!BulkPipeReadEx(&pData[(i<<divider)], min((1<<divider),range_temp.length-(i<<divider)), USB_TIMEOUT, USBIndex))
				{
					free(pData);
					return false ;	    
				}
			}
			for(size_t i=0; i<range_temp.length; i++)
			{
				if(pData[i] != 0xFF)
				{
					free(pData);
					return false ;	    
				}
			}
		}
	}
	else
	{
		DWORD StartBlock = AddrRange->start/g_NANDContext.realBlockSize;	
		DWORD StartPage = AddrRange->start/g_NANDContext.realPageSize; 
		unsigned int dwAddr=0;
		if(BlockPages==256)
			dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x7FFC0)) ;  
		else if(BlockPages==128)
			dwAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80)) ;  
		else
			dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0)) ;

		size_t ReadCounter=AddrRange->length >> divider ;  
		if((AddrRange->length%(1<<divider))!=0)
			ReadCounter += 1 ;
 		FlashCommand_SendCommand_SetupPacketForBulkReadNAND(dwAddr,AddrRange->length/512, modeRead,g_NANDContext.realPageSize,BlockPages,ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen,g_ChipInfo.nCA_Rd,USBIndex);
	 	  

		for(size_t i = 0; i < ReadCounter; i++)
		{     
			if(!BulkPipeReadEx(&pData[(i<<divider)], min((1<<divider),AddrRange->length-(i<<divider)), USB_TIMEOUT,USBIndex))
			{
				free(pData);
				return  false ;	    
			}
		}
		for(size_t i=0; i<AddrRange->length; i++)
		{
			if(pData[i] != 0xFF)
			{
				free(pData);
				return  false ;	    
			}
		}
		
	}

	free(pData);
	return true; 
} 

bool Nand_bulkPipeRead(const struct CAddressRange* AddrRange, unsigned char* pBuff,unsigned char modeRead,unsigned char ReadCom,int USBIndex)
{   
	unsigned char *pData= NULL;// = new unsigned char[0x1000000]; 
	unsigned char *pTemp = pBuff;
//	unsigned char BBTscan=0; 
  
  	size_t BlockPages= g_NANDContext.realBlockSize/g_NANDContext.realPageSize;
  
	size_t divider=16; 
	size_t RunUnitSize=0; 
	switch(g_NANDContext.realBlockSize)
	{
		default:
		case 0x44000:
		case 0x22000:
			RunUnitSize = 0x880000; 
			break;
		case 0x42000:
		case 0x21000:
			RunUnitSize=0x840000;
			break;
		case 0x20800:
			RunUnitSize=0x820000;
			break;
		case 0x20400:
			RunUnitSize=0x810000;
			break; 
	}  
	//vector<unsigned char> vData(RunUnitSize,0xFF);     
	pData = (unsigned char* )malloc(RunUnitSize);
	memset(pData, 0xFF, RunUnitSize);
	
	if((AddrRange->start/RunUnitSize)!=(AddrRange->end/RunUnitSize)) 
	{ 
		struct CAddressRange range_temp;//(*AddrRange);
		range_temp.start=AddrRange->start;
		range_temp.end=AddrRange->end;
		range_temp.length=AddrRange->end-AddrRange->start;
		
		//size_t baseAddr=AddrRange->start-(AddrRange->start%RunUnitSize); 
		size_t loop=SF_GetLoop(AddrRange ,RunUnitSize);

        for(size_t j=0; j<loop; j++)
		{ 
            if(j==(loop-1))
                range_temp.end=range_temp.end;
            else
                range_temp.end=AddrRange->start-(AddrRange->start%RunUnitSize)+(RunUnitSize*(j+1));

            if(j==0)
                range_temp.start=AddrRange->start;
            else
                range_temp.start=AddrRange->start-(AddrRange->start%RunUnitSize)+(RunUnitSize*j);
			
			DWORD StartBlock = range_temp.start/g_NANDContext.realBlockSize;	
			DWORD StartPage = range_temp.start/g_NANDContext.realPageSize; 

			range_temp.length = range_temp.end-range_temp.start;
			unsigned int dwAddr=0;
			if(BlockPages==256)
				dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x7FFC0 ));  
			else if(BlockPages==128)
				dwAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80)) ;  
			else
				dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0)) ;
  
			size_t ReadCounter=range_temp.length >> divider ;  
			if((range_temp.length%(1<<divider))!=0)
				ReadCounter += 1 ;  
			
 			FlashCommand_SendCommand_SetupPacketForBulkReadNAND(dwAddr,range_temp.length/512, modeRead,g_NANDContext.realPageSize,BlockPages,ReadCom,g_ChipInfo.AddrWidth, g_ChipInfo.ReadDummyLen, g_ChipInfo.nCA_Rd,USBIndex);

			for(size_t i = 0; i < ReadCounter; i++)
			{     
				if(!BulkPipeReadEx(&pData[(i<<divider)], min((1<<divider),range_temp.length-(i<<divider)), USB_TIMEOUT, USBIndex))
				{
					free(pData);
					return false ;	    
				}
			}
			memcpy(pTemp, pData, range_temp.length);
			pTemp += range_temp.length;
		}
	}
	else
	{
		DWORD StartBlock = AddrRange->start/g_NANDContext.realBlockSize;	
		DWORD StartPage = AddrRange->start/g_NANDContext.realPageSize; 
		unsigned int dwAddr=0;
		if(BlockPages==256)
			dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x7FFC0)) ;  
		else if(BlockPages==128)
			dwAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80)) ;  
		else
			dwAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0)) ;

		size_t ReadCounter=AddrRange->length >> divider ;  
		if((AddrRange->length%(1<<divider))!=0)
			ReadCounter += 1 ;
 		FlashCommand_SendCommand_SetupPacketForBulkReadNAND(dwAddr,AddrRange->length/512, modeRead,g_NANDContext.realPageSize,BlockPages,ReadCom,g_ChipInfo.AddrWidth,g_ChipInfo.ReadDummyLen,g_ChipInfo.nCA_Rd,USBIndex);
	 	  

		for(size_t i = 0; i < ReadCounter; i++)
		{     
			if(!BulkPipeReadEx(&pData[(i<<divider)], min((1<<divider),AddrRange->length-(i<<divider)), USB_TIMEOUT,USBIndex))
			{
				free(pData);
				return false ;	    
			}
		}

		memcpy(pTemp, pData, AddrRange->length);
	}

	free(pData);
	return true; 
} 

bool SPINAND_RangeVerify( const struct CAddressRange* AddrRange,int USBIndex)
{  
	if(!SPINAND_DisableContinueRead(USBIndex))
		return false;  
//	if(!SPINAND_EnableQuadIO(true,true,USBIndex))
//		return false;   
	size_t loop=SF_GetLoop(AddrRange ,g_NANDContext.realBlockSize);
	//printf("SPINAND_RangeVerify(), AddrRange->start=%ld, AddrRange->end=%ld, loop=%ld\n", AddrRange->start, AddrRange->end,loop);
	bool bIsBadBlock = false;
	bool result = true;
	size_t StartBlock = AddrRange->start/g_NANDContext.realBlockSize;
	struct CAddressRange tempRange;
	for(unsigned short i=StartBlock; i<(loop+StartBlock); i++)
	{
		if(g_iNandBadBlockManagement == 0)
		{
			bIsBadBlock = false;
			for(unsigned short j=0; j<g_BBT[USBIndex].cnt; j++)
			{
				if(i==g_BBT[USBIndex].bbt[j])
				{
					bIsBadBlock = true;
					break;
				}
			}
			if(bIsBadBlock == true)
				continue;
		}

		tempRange.start = (i*g_NANDContext.realBlockSize);
		tempRange.end = ((i+1)*g_NANDContext.realBlockSize);
		tempRange.length = g_NANDContext.realBlockSize;
		result &= Nand_bulkPipeVerify(&tempRange, BULK_QUAD_READ,mcode_ReadCode,USBIndex);
	}
	return result;
}

bool SPINAND_RangeBlankCheck(const struct CAddressRange* Range,int USBIndex)
{ 
	bool result =true; 
	if(!SPINAND_DisableContinueRead(USBIndex))
		return false; 
	if(!SPINAND_EnableInternalECC(g_bNandInternalECC,USBIndex))
		return false;
//	if(!SPINAND_EnableQuadIO(true,true,USBIndex)) 
//		return false;
	if(!Nand_bulkPipeBlankCheck(Range, BULK_QUAD_READ,mcode_ReadCode,USBIndex))
		result = false;
//	if(!SPINAND_EnableQuadIO(false,true,USBIndex)) 
//		result &= false;   
	
	return result ;
}

bool SPINAND_BlockProgram(const DWORD dwAddr,  					  const unsigned char *pData, unsigned char modeWrite,unsigned char WriteCom,int USBIndex)
{  
	bool result=true; 
    size_t divider=9;
	//size_t WriteAddr=0;
	//unsigned char ucSR=0;
 	unsigned char *pTemp=(unsigned char *)pData;
	DWORD StartBlock = dwAddr/g_NANDContext.realBlockSize;	 
	DWORD StartPage = dwAddr/g_NANDContext.realPageSize; 
	WORD BlockPages =(g_NANDContext.realBlockSize/g_NANDContext.realPageSize);
	unsigned int dwProgAddr=0;
	if(BlockPages==128)
		dwProgAddr=((StartPage & 0x7F)|((StartBlock<<7)& 0x7FF80 ));    
	else
		dwProgAddr=((StartPage & 0x3F)|((StartBlock<<6)& 0x3FFC0 ));  

	//printf("dwAddr=0x%08lX, Program addr = 0x%08X\n",dwAddr,dwProgAddr);
	FlashCommand_SendCommand_SetupPacketForBulkWriteNAND(dwProgAddr,g_NANDContext.realBlockSize>>divider, modeWrite,g_NANDContext.realPageSize,
		BlockPages,WriteCom,g_ChipInfo.AddrWidth, g_ChipInfo.ReadDummyLen,g_ChipInfo.nCA_Wr,USBIndex);

	size_t WriteCounter=g_NANDContext.realBlockSize >> divider ;  
	if((g_NANDContext.realBlockSize %(1<<divider))!=0)
		WriteCounter += 1 ;
	for(size_t i = 0; i < WriteCounter; i++)
	{ 
		BulkPipeWrite(pTemp, 1<<divider, USB_TIMEOUT,USBIndex);
		pTemp += (1<<divider);
	}
	#if 0
	if(!Nand_RDSRStatus1(ucSR,USBIndex))
		result = false;
	
	if(result==true && (ucSR & (0x08)) == 0x08) // prog flag is set
	{
		MarkNewBadBlock(m_c,temp_addr, USBIndex);
		temp_addr += m_context->chip.realBlockSize;
		result = false;
		cnt++;
	}
	#endif
		
	return result;
} 

bool SPINAND_RangeRead(const struct CAddressRange* Range,unsigned char *pBuff, int USBIndex)
{
	if(!SPINAND_DisableContinueRead(USBIndex))
		return false;  
	//	if(!SPINAND_EnableQuadIO(true,true,USBIndex)) 
	//		return false;
	if(Nand_bulkPipeRead(Range, pBuff, BULK_QUAD_READ,mcode_ReadCode,USBIndex))
		return false;
	//	if(!SPINAND_EnableQuadIO(false,true,USBIndex)) 
	//		return false;
	return true;
}

bool SPINAND_RangeProgram(const struct CAddressRange* Range,int USBIndex)
{
	return true;
}

bool Nand_SpecialRangeErase(unsigned int BlockIndex, unsigned int BlockCnt,int USBIndex)
{      
	SetSPIClockValue(0x02,USBIndex); 

	unsigned short BBT[0x200];
	unsigned short BBTCnt = 0;
	
	SPINAND_ScanBadBlock(BBT, &BBTCnt, USBIndex);

	if(BBTCnt >= 0x200)
		return false;

	unsigned int TotalBlockCnt = g_NANDContext.realChipSize/g_NANDContext.realBlockSize;
	DWORD PageCnt = g_NANDContext.realBlockSize/g_NANDContext.realPageSize;
	DWORD addr = 0;
	bool skip = false;
	unsigned char ucSR=0;
	unsigned char v[4];// m_context->serialFlash.EraseCmd.ChipErase);
	v[0] = mcode_ChipErase;
	for(size_t i=BlockIndex; i<(BlockIndex+BlockCnt); i++)
	{
		skip = false;
		for(unsigned int j=0; j<BBTCnt; j++)
		{
			if(BBT[j] == i)
			{
				skip = true;
			}
		}
		if(skip == true) continue;
		
		Nand_waitForWEL(USBIndex);
		addr = PageCnt * i;
		v[1] = (unsigned char)((addr >> 16) & 0xff) ;     //MSB
		v[2] = (unsigned char)((addr >> 8) & 0xff) ;      //M
		v[3] = (unsigned char)(addr & 0xff) ;             //LSB
		FlashCommand_SendCommand_OutOnlyInstruction(v, sizeof(v), USBIndex);

		do
		{
			if(!Nand_RDSRStatus1(&ucSR,USBIndex))
				return false;
			if((ucSR & (0x05)) == 0x05) // erase flag and busy flag are set
			{
				MarkNewBadBlock(addr, USBIndex);
				if(BlockCnt < TotalBlockCnt)
					BlockCnt++;
				break;
			}
		}while(ucSR & 0x01);
		
		
	}
	SetSPIClock(USBIndex);
	
	return true ;
} 

bool SPINand_SpecialErase(unsigned int BlockIndex, unsigned int BlockCnt, int USBIndex)
{
	bool result = true;
	if(!SPINAND_DisableContinueRead(USBIndex))
		return false; 
	if(! SPINAND_ProtectBlock(false,USBIndex) ) 
        return false ; 
	result = Nand_SpecialRangeErase(BlockIndex, BlockCnt, USBIndex);
	return result;
}

