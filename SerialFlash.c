#include "Macro.h"
#include "usbdriver.h"
#include "ChipInfoDb.h"
#include "FlashCommand.h"
#include "SerialFlash.h"
#include <sys/stat.h>
#include <stdbool.h>

extern int m_isCanceled;
extern int m_bProtectAfterWritenErase;
extern int m_boEnReadQuadIO;
extern int m_boEnWriteQuadIO;
extern CHIP_INFO Chip_Info;
extern volatile bool g_bIsSF600;
extern void Sleep(unsigned int ms);
extern bool Is_NewUSBCommand(int Index);

unsigned char mcode_WRSR=0x01;
unsigned char mcode_WRDI=0x04;
unsigned char mcode_RDSR=0x05;
unsigned char mcode_WREN=0x06;
unsigned char mcode_SegmentErase=SE;
unsigned char mcode_ChipErase=CHIP_ERASE;
unsigned char mcode_Program=PAGE_PROGRAM;
unsigned char mcode_ProgramCode_4Adr=0x02;
unsigned char mcode_Read=BULK_FAST_READ;
unsigned char mcode_ReadCode=0x0B;
unsigned int g_AT45_PageSize=0;
unsigned int g_AT45_PageSizeMask=0;
size_t AT45ChipSize=0;
size_t AT45PageSize=0;
bool AT45doRDSR(unsigned char* cSR, int Index)
{
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction;    //size 1

    // first control packet
    vInstruction = 0xD7;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = (unsigned long) 1 ;

    if(OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE ;

    // second control packet : fetch data
    unsigned char vBuffer ;        //just read one bytes , in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = 1;
		rq.Index = 0;
	}
	else
	{
	    rq.Value = CTRL_TIMEOUT;
    	rq.Index = NO_REGISTER;
	}
    rq.Length = (unsigned long) 1;

    if(InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE ;

    *cSR = vBuffer;

    return SerialFlash_TRUE;
}

bool AT45WaitForWIP(int USBIndex)
{
    unsigned char cSR ;
    size_t i = Chip_Info.Timeout*100;
    if(i==0)
        i=0x10000;
    // wait until WIP = 0
    do{
        AT45doRDSR(&cSR,USBIndex) ;
        Sleep(10);
    }while((!(cSR & 0x80)) && (i-- > 0)) ;  // poll bit 7
    if(i<=0) return false;

    return true;
}

unsigned char getWriteMode(int USBIndex)
{
    unsigned char cMode;
    unsigned char cSR ;
    AT45doRDSR(&cSR,USBIndex);
    bool powerOfTwo = (1 == (cSR & 0x1));

    enum
        {
            AT45DB011D = 0x1F22,
            AT45DB021D = 0x1F23,
            AT45DB041D = 0x1F24,
            AT45DB081D = 0x1F25,
            AT45DB161D = 0x1F26,
            AT45DB321D = 0x1F27,
            AT45DB642D = 0x1F28,
        };

    switch(Chip_Info.UniqueID)
    {
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
            ;
    }
    return cMode;

}

void SetPageSize(CHIP_INFO* mem, int USBIndex)
{
    unsigned char writeMode;
    writeMode = getWriteMode(USBIndex);
    mcode_Program=writeMode;
    size_t pageSize[7] = {0, 256, 264, 512, 528, 1024, 1056};
//    size_t pageSizeMask[7] = {0, (1<<8)-1, (1<<9)-1, (1<<9)-1, (1<<10)-1, (1<<10)-1, (1<<11)-1};
    mem->PageSizeInByte=pageSize[writeMode];

    if(! (writeMode & 0x1) )            // for AT45DB:0x1F2200 - 0x1F2800
        mem->ChipSizeInByte=Chip_Info.ChipSizeInByte / 256 * 8+Chip_Info.ChipSizeInByte;
    else
        mem->ChipSizeInByte=Chip_Info.ChipSizeInByte ;

    AT45ChipSize=mem->ChipSizeInByte;
    AT45PageSize=mem->PageSizeInByte;
}

size_t GetChipSize(void)
{
    return AT45ChipSize;
}

size_t GetPageSize(void)
{
    return AT45PageSize;
}


bool AT45xxx_protectBlock(int bProtect,int Index)
{
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction[4];    //size 1

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = (unsigned long) 4 ;
    // protect block ,set BP1 BP0 to 1
    if(bProtect)
    {
        vInstruction[0]=0x3D;
        vInstruction[1]=0x2A;
        vInstruction[2]=0x7F;
        vInstruction[3]=0xA9;
    }
    else
    {
        vInstruction[0]=0x3D;
        vInstruction[1]=0x2A;
        vInstruction[2]=0x7F;
        vInstruction[3]=0x9A;
    }

    if(OutCtrlRequest(&rq, vInstruction, 4, Index) == SerialFlash_FALSE)
        return false ;
    return true;
}

bool AT45rangeProgram(struct CAddressRange* AddrRange, unsigned char *vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{
   CHIP_INFO mem_id;

    SetPageSize(&mem_id,Index);
    modeWrite=getWriteMode(Index);
    size_t packageNum = (AddrRange->end - AddrRange->start + mem_id.PageSizeInByte- 1) / mem_id.PageSizeInByte;
    size_t pageSize[2] = { 264, 256};
    unsigned char idx = modeWrite & 0x1;
    size_t itrCnt =  packageNum *  mem_id.PageSizeInByte / pageSize[idx];
    unsigned char* itr_begin;
    size_t i;
//    AT45xxx_protectBlock(false,Index);

    FlashCommand_SendCommand_SetupPacketForAT45DBBulkWrite(AddrRange, modeWrite,WriteCom,Index);

    itr_begin = vData;

    for( i = 0; i < itrCnt; ++ i)
    {
        bool b =  BulkPipeWrite(itr_begin + (i * pageSize[idx]),  pageSize[idx], USB_TIMEOUT,Index);
        if((!b) || m_isCanceled)
        {
            return false ;
        }
    }
//    Sleep(10) ;
    return true ;

}

int AT45rangSectorErase(size_t sectionSize, struct CAddressRange AddrRange,int USBIndex)
{
    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction[5]={0x7C,0x50,0x50,0x50,0x50} ;

    // instrcution format
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(USBIndex))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = 4 ;

    size_t sectorNum = (AddrRange.end - AddrRange.start + sectionSize - 1) /  sectionSize ;
    size_t i;

    for( i = 0; i < sectorNum; ++ i)
    {
        size_t addr = (AddrRange.start + i *  sectionSize) ;
        vInstruction[1] = (unsigned char)((addr >> 16) & 0xff) ;     //MSB
        vInstruction[2] = (unsigned char)((addr >> 8) & 0xff) ;      //M
        vInstruction[3] = (unsigned char)(addr & 0xff) ;             //LSB

        int b = OutCtrlRequest(&rq, vInstruction,4,USBIndex);
        if((b==SerialFlash_FALSE)|| m_isCanceled)  return false ;

        if(AT45WaitForWIP(USBIndex)==false) return false;
    }

    return true ;
}

bool AT45batchErase(size_t* vAddrs,size_t AddrSize,int USBIndex)
{
    CHIP_INFO mem_id;
    int i;
    SetPageSize(&mem_id,USBIndex);
    if(strcmp(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB)==0)
    {
        mem_id.PageSizeInByte=Chip_Info.PageSizeInByte;
        mem_id.ChipSizeInByte=Chip_Info.ChipSizeInByte;
    }
    mem_id.SectorSizeInByte=Chip_Info.SectorSizeInByte;

    struct CAddressRange range;

    for(i=0; i<AddrSize; i++)
    {
        switch(mem_id.ChipSizeInByte)
        {
            case (1<<17):           // 1Mbit
                if(vAddrs[i] < (1<<11))
                {
                    range.start=vAddrs[i];
                    range.end=vAddrs[i]+(1<<11);
                    AT45rangSectorErase(1<<11, range,USBIndex);
                }
                else
                {
                    range.start=vAddrs[i];
                    range.end=vAddrs[i]+(1<<15);
                    AT45rangSectorErase(1<<15, range,USBIndex);
                }
                break;
            case (1<<17)*264/256:   // 1Mbit
                if(vAddrs[i] < (1<<12))
                {
                    range.start=vAddrs[i];
                    range.end=vAddrs[i]+(1<<12);
                    AT45rangSectorErase( 1<<12, range,USBIndex);
                }
                else
                {
                    range.start=vAddrs[i];
                    range.end=vAddrs[i]+(1<<16);
                    AT45rangSectorErase( 1<<16, range,USBIndex);
                }
                break;
        case (1<<18):           // 2Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<15, range,USBIndex);
            break;
        case (1<<18)*264/256:   // 2Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<19):           // 4Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<19)*264/256:   // 4Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<20):           // 8Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<20)*264/256:   // 8Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<21):           // 16Mbit
            if(vAddrs[i] < (1<<13))
            {
                range.start=vAddrs[i];
                range.end=vAddrs[i]+(1<<12);
                AT45rangSectorErase( 1<<12, range,USBIndex);
            }
            else
            {
                range.start=vAddrs[i];
                range.end=vAddrs[i]+(1<<17);
                AT45rangSectorErase( 1<<17, range,USBIndex);
            }
            break;
        case (1<<21)*264/256:   // 16Mbit
            if(vAddrs[i] < (1<<14))
            {
                //printf("Addr=0x%X\r\n",vAddrs[i]);
                range.start=vAddrs[i];
                range.end=vAddrs[i]+(1<<13);
                AT45rangSectorErase( 1<<13, range,USBIndex);
            }
            else
            {
                //printf("Addr=0x%X\r\n",vAddrs[i]);
                range.start=vAddrs[i];
                range.end=vAddrs[i]+(1<<18);
                AT45rangSectorErase( 1<<18, range,USBIndex);
            }
            break;
        case (1<<22):           // 32Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<18;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<18, range,USBIndex);
            break;
        case (1<<22)/256*264:   // 32Mbit
            range.start=0;
            range.end=1<<14;
            AT45rangSectorErase( 1<<13, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<23):           // 64Mbit
            range.start=0;
            range.end=1<<14;
            AT45rangSectorErase( 1<<13, range,USBIndex);
            range.start=1<<18;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<18, range,USBIndex);
            break;
        case (1<<23)/256*264:   // 64Mbit
            range.start=0;
            range.end=1<<15;
            AT45rangSectorErase( 1<<14, range,USBIndex);
            range.start=1<<19;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<19, range,USBIndex);
            break;
        default:
            ; //CSerialFlash::chipErase();
    };
        }
    return true;
}

bool AT45chipErase(unsigned int Addr,unsigned int Length,int USBIndex)
{
    unsigned char com[4]={0xC7,0x94,0x80,0x9A};
    int b = FlashCommand_SendCommand_OutOnlyInstruction(com,4,USBIndex);
    if(b==SerialFlash_FALSE)  return false ;

    if(AT45WaitForWIP(USBIndex)==false) return false;

    return true;

    CHIP_INFO mem_id;
    SetPageSize(&mem_id,USBIndex);
    if(strcmp(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB)==0)
    {
        mem_id.PageSizeInByte=Chip_Info.PageSizeInByte;
        mem_id.ChipSizeInByte=Chip_Info.ChipSizeInByte;
    }
    mem_id.SectorSizeInByte=Chip_Info.SectorSizeInByte;
//    AT45xxx_protectBlock(false,USBIndex);

    struct CAddressRange range;
    switch(mem_id.ChipSizeInByte)
    {
        case (1<<17):           // 1Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase(1<<11, range,USBIndex);
            range.start=1<<15;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase(1<<15, range,USBIndex);
            break;
        case (1<<17)*264/256:   // 1Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<18):           // 2Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<15, range,USBIndex);
            break;
        case (1<<18)*264/256:   // 2Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<19):           // 4Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<19)*264/256:   // 4Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<20):           // 8Mbit
            range.start=0;
            range.end=1<<12;
            AT45rangSectorErase( 1<<11, range,USBIndex);
            range.start=1<<16;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<16, range,USBIndex);
            break;
        case (1<<20)*264/256:   // 8Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<21):           // 16Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<21)*264/256:   // 16Mbit
            range.start=0;
            range.end=1<<14;
            AT45rangSectorErase( 1<<13, range,USBIndex);
            range.start=1<<18;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<18, range,USBIndex);
            break;
        case (1<<22):           // 32Mbit
            range.start=0;
            range.end=1<<13;
            AT45rangSectorErase( 1<<12, range,USBIndex);
            range.start=1<<18;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<18, range,USBIndex);
            break;
        case (1<<22)/256*264:   // 32Mbit
            range.start=0;
            range.end=1<<14;
            AT45rangSectorErase( 1<<13, range,USBIndex);
            range.start=1<<17;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<17, range,USBIndex);
            break;
        case (1<<23):           // 64Mbit
            range.start=0;
            range.end=1<<14;
            AT45rangSectorErase( 1<<13, range,USBIndex);
            range.start=1<<18;
            range.end=mem_id.ChipSizeInByte;
            AT45rangSectorErase( 1<<18, range,USBIndex);
            break;
        case (1<<23)/256*264:   // 64Mbit
            range.start=0;
            range.end=1<<15;
            AT45rangSectorErase( 1<<14, range,USBIndex);
            range.start=1<<19;
            range.end=mem_id.ChipSizeInByte<<1;
            AT45rangSectorErase( 1<<19, range,USBIndex);
            break;
        default:
            ; //CSerialFlash::chipErase();
    };
    return true;
}


// write status register , just 1 bytes
// WRSR only is effects on SRWD BP2 BP1 BP0 when WEL = 1
int SerialFlash_doWRSR(unsigned char cSR,int Index)
{
//simon	if(! m_usb.is_open())
//Simon		return false ;

	// wait until WIP = 0
	SerialFlash_waitForWIP(Index) ;

	// wait until WEL = 1
	SerialFlash_waitForWEL(Index) ;

	// send request
	unsigned char vInstruction[2];

	vInstruction[0] = mcode_WRSR;
	vInstruction[1] = cSR;

	return FlashCommand_SendCommand_OutOnlyInstruction(vInstruction, 2,Index);
}


int SerialFlash_doRDSR(unsigned char *cSR,int Index)
{
	// read status
//Simon	if(! m_usb.is_open() )
//Simon		return false ;

	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vInstruction;    //size 1

	// first control packet
	vInstruction = mcode_RDSR;

	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN ;
		rq.Index = RFU ;
	}
	else
	{
		rq.Value = RFU ;
		rq.Index = RESULT_IN ;
	}
	rq.Length = 1;//(unsigned long) 1 ;

	if(OutCtrlRequest(&rq, &vInstruction, 1, Index) == SerialFlash_FALSE)
		return SerialFlash_FALSE ;

	// second control packet : fetch data
	unsigned char vBuffer ;        //just read one bytes , in fact more bytes are also available
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_IN ;
	rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = 1;
		rq.Index = 0;
	}
	else
	{
		rq.Value = CTRL_TIMEOUT ;
		rq.Index = NO_REGISTER ;
	}
	rq.Length = 1;//(unsigned long) 1;

	if(InCtrlRequest(&rq, &vBuffer, 1, Index) == SerialFlash_FALSE)
		return SerialFlash_FALSE ;

	*cSR = vBuffer;
	return SerialFlash_TRUE;
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
	unsigned char cSR ;
	size_t i = 10;
	// wait until WIP = 0 and WEL = 1
	do{
		SerialFlash_doWREN(Index) ;

		// read SR to check WEL until WEL = 1
		SerialFlash_doRDSR(&cSR,Index) ;
	}while(((cSR & 0x02) == 0)  && (i-- > 0)) ;
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
bool SerialFlash_waitForWIP(int Index)
{
    unsigned char cSR ;
    size_t i = Chip_Info.Timeout*100;
    if(i==0)
        i=MAX_TRIALS;
	// wait until WIP = 0
    do{
        SerialFlash_doRDSR(&cSR,Index) ;
        Sleep(5);
    }while((cSR & 0x01) && (i-- > 0)) ;
    if(i<=0)
        return false;
    return true;
}


int SerialFlash_doWREN(int Index)
{
    unsigned char v=mcode_WREN;;
	return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1,Index);

}


int SerialFlash_doWRDI(int Index)
{
	unsigned char v=mcode_WRDI;
	return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1,Index);

}

bool SST25xFxx_protectBlock(int bProtect,int Index)
{
    bool result = false ;
    unsigned char  tmpSRVal;
    unsigned char dstSRVal ;

    SerialFlash_waitForWIP(Index);
    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00 ;
    // protect block ,set BP1 BP0 to 1
    if(bProtect){
        dstSRVal |= 0x8C ;    // 8C : 1000 1100
    }


    unsigned char vInstruction[2] ;
    vInstruction[0] = 0x50 ; //Write enable
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction,1,Index);

    int numOfRetry = 1000 ;
    vInstruction[0] = 0x01 ; //Write register
    vInstruction[1] =  dstSRVal;
    FlashCommand_SendCommand_OutOnlyInstruction(vInstruction,2,Index);

    result = SerialFlash_doRDSR(&tmpSRVal,Index) ;
    while( (tmpSRVal & 0x01) &&  numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal,Index) ;

        if(! result) return false;

        numOfRetry -- ;

    };

    return ((tmpSRVal ^ dstSRVal)& 0x0C ) ? false:true;
}

bool SST25xFxxA_protectBlock(int bProtect,int Index)
{
    bool result = false ;
    unsigned char  tmpSRVal;
    unsigned char dstSRVal ;

    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00 ;
    // protect block ,set BP1 BP0 to 1
    if(bProtect){
        dstSRVal |= 0x0 ;    // 8C : 1000 1100
    }

    int numOfRetry = 1000 ;
    result = SerialFlash_doWRSR(dstSRVal,Index) ;
    result = SerialFlash_doRDSR(&tmpSRVal,Index) ;
    while( (tmpSRVal & 0x01) &&  numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal,Index) ;

        if(! result) return false;

        numOfRetry -- ;

    };

    return ((tmpSRVal ^ dstSRVal)& 0x0C ) ? false:true;
}

bool AT25FSxxx_protectBlock(int bProtect,int Index)
{
    bool result = false ;
    unsigned char  tmpSRVal;
    unsigned char dstSRVal ;

    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00 ;
    // protect block ,set BP1 BP0 to 1
    if(bProtect){
        dstSRVal |= 0xFF ;    // 8C : 1000 1100
    }

    int numOfRetry = 1000 ;
    result = SerialFlash_doWRSR(dstSRVal,Index) ;
    result = SerialFlash_doRDSR(&tmpSRVal,Index) ;
    while( (tmpSRVal & 0x01) &&  numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal,Index) ;

        if(! result) return false;

        numOfRetry -- ;

    };

    return ((tmpSRVal ^ dstSRVal)& 0x0C ) ? false:true;
}

bool AT26Fxxx_protectBlock(int bProtect,int Index)
{
    const unsigned int AT26DF041 = 0x1F4400;
    const unsigned int AT26DF004 = 0x1F0400;
    const unsigned int AT26DF081A = 0x1F4501;
    enum
    {
        PROTECTSECTOR               = 0x36,                     // Write Enable
        UNPROTECTSECTOR             = 0x39,                     // Write Disable
        READPROTECTIONREGISTER      = 0x3C,                     // Write Disable
    };
    if(AT26DF041 == Chip_Info.UniqueID) return true; // feature is not supported on this chip

    bool result = false ;

    int numOfRetry = 1000 ;

    result = SerialFlash_doWRSR(0,Index) ;

    unsigned char   tmpSRVal;
    do
    {
        result = SerialFlash_doRDSR(&tmpSRVal,Index) ;
        numOfRetry -- ;

    }while( (tmpSRVal & 0x01) &&  numOfRetry > 0 && result);


    if (tmpSRVal & 0x80 ) return false;   ///< enable SPRL


    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction[4] ={0};

    vInstruction[0] = bProtect ? PROTECTSECTOR : UNPROTECTSECTOR ;

    size_t iUniformSectorSize = 0x10000;    // always regarded as 64K
    size_t cnt = Chip_Info.ChipSizeInByte / iUniformSectorSize;
    size_t i;
    for( i = 0; i < cnt; ++i)
    {
        SerialFlash_doWREN(Index);

        vInstruction[1] = (unsigned char)i;

        rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
        rq.Direction = VENDOR_DIRECTION_OUT ;
        rq.Request = TRANSCEIVE ;
		if(Is_NewUSBCommand(Index))
		{
			rq.Value = RESULT_IN;
			rq.Index = RFU;
		}
		else
		{
	    	rq.Value = RFU ;
	    	rq.Index = RESULT_IN ;
		}
        rq.Length = 4;

        if(OutCtrlRequest(&rq, vInstruction,4,Index)==SerialFlash_FALSE)
            return false ;
    }

    if(AT26DF081A == Chip_Info.UniqueID || AT26DF004 == Chip_Info.UniqueID) // 8K each for the last 64K
    {
        vInstruction[1] = (unsigned char)(cnt - 1);
        for( i = 1; i < 8; ++i)
        {
            SerialFlash_doWREN(Index);

            vInstruction[2] = (unsigned char)(i<<5);

            rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
            rq.Direction = VENDOR_DIRECTION_OUT ;
            rq.Request = TRANSCEIVE ;
			if(Is_NewUSBCommand(Index))
			{
				rq.Value = RESULT_IN;
				rq.Index = RFU;
			}
			else
			{
			    rq.Value = RFU ;
		    	rq.Index = RESULT_IN ;
			}
            rq.Length = 4;

            if(OutCtrlRequest(&rq, vInstruction,4,Index) == SerialFlash_FALSE)
                return false ;
        }
    }

    return true;
}

bool CMX25LxxxdoRDSCUR(unsigned char* cSR,int Index)
{
	// read status
	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vInstruction=RDSCUR ;    //size 1

	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
	rq.Length = 1;

	if(OutCtrlRequest(&rq, &vInstruction,1,Index)==SerialFlash_FALSE)
		return SerialFlash_FALSE ;

	// second control packet : fetch data
	unsigned char vBuffer ;        //just read one bytes , in fact more bytes are also available
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_IN ;
	rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = 1;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = CTRL_TIMEOUT;
    	rq.Index = NO_REGISTER;
	}
	rq.Length = 1;

	if(InCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
		return SerialFlash_FALSE ;

	*cSR = vBuffer;

	return SerialFlash_TRUE;
}


bool CS25FLxxx_LargedoUnlockDYB(unsigned int  cSR, int Index)
{
	// wait until WIP = 0
    if(SerialFlash_waitForWIP(Index)==false) return false;

    unsigned char vInstruction[15] ;
    int i;
    unsigned int topend,bottomstart,end;

    if(strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx_Large) != NULL)//256
    {
        topend=0x20000;
        bottomstart=0x1fe0000;
        end=0x2000000;
    }
    else
    {
        topend=0x20000;
        bottomstart=0xfe0000;
        end=0x1000000;
    }

    for( i=0;i<topend;i=i+0x1000) //top 4k sectors
    {
        SerialFlash_waitForWEL(Index) ;
        vInstruction[0]=0xE1; //write DYB
        vInstruction[1]=(unsigned char)(i>>24);
        vInstruction[2]=(unsigned char)(i>>16);
        vInstruction[3]=(unsigned char)(i>>8);
        vInstruction[4]=(unsigned char)(i);
        vInstruction[5]=0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction,6,Index);
        SerialFlash_waitForWIP(Index);
    }

    for( i=topend;i<bottomstart;i=i+0x10000) //64k sectors
    {
        SerialFlash_waitForWEL(Index) ;
        vInstruction[0]=0xE1; //write DYB
        vInstruction[1]=(unsigned char)(i>>24);
        vInstruction[2]=(unsigned char)(i>>16);
        vInstruction[3]=(unsigned char)(i>>8);
        vInstruction[4]=(unsigned char)(i);
        vInstruction[5]=0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction,6,Index);
        SerialFlash_waitForWIP(Index);
    }

    for(i=bottomstart;i<end;i=i+0x1000) //bottom 4k sectors
    {
        SerialFlash_waitForWEL(Index) ;
        vInstruction[0]=0xE1; //write DYB
        vInstruction[1]=(unsigned char)(i>>24);
        vInstruction[2]=(unsigned char)(i>>16);
        vInstruction[3]=(unsigned char)(i>>8);
        vInstruction[4]=(unsigned char)(i);
        vInstruction[5]=0xff;
        FlashCommand_SendCommand_OutOnlyInstruction(vInstruction,6,Index);
        SerialFlash_waitForWIP(Index);
    }
        return true;
}

int SerialFlash_protectBlock(int bProtect,int Index)
{
    if(strcmp(Chip_Info.Class,SUPPORT_SST_25xFxx)==0 || strstr(Chip_Info.Class,SUPPORT_SST_25xFxxB)!=NULL)// || strstr(Chip_Info.Class,SUPPORT_SST_25xFxxC)!=NULL)
        return SST25xFxx_protectBlock( bProtect, Index);
    else if(strstr(Chip_Info.Class,SUPPORT_SST_25xFxxA)!=NULL)
        return SST25xFxxA_protectBlock( bProtect, Index);
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_AT25FSxxx)!=NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_AT25Fxxx)!=NULL)
        return AT25FSxxx_protectBlock( bProtect, Index);
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_AT26xxx)!=NULL)
        return AT26Fxxx_protectBlock( bProtect, Index);
    else if(strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx)!=NULL || strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_Large)!=NULL)
    {
        unsigned char  tmpSRVal;
        bool result;
        result=CMX25LxxxdoRDSCUR(&tmpSRVal,Index);
       if(result==true && (tmpSRVal&0x80) && Chip_Info.MXIC_WPmode==true)
		{
			if(bProtect != false)	return true;
			SerialFlash_doWREN(Index) ;

			unsigned char v=GBULK;
			return FlashCommand_SendCommand_OutOnlyInstruction(&v, 1,  Index);
		}
    }
    else if(strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx) != NULL || strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx_Large) != NULL)
    {
        if(bProtect==false && strstr(Chip_Info.TypeName,"Secure") != NULL)
        {
            CS25FLxxx_LargedoUnlockDYB(0, Index);
        }
    }
	else if(strstr(Chip_Info.Class,SUPPORT_SST_26xFxxC) != NULL)
	{
		unsigned char v=0x98;
		SerialFlash_waitForWEL(Index) ;
		FlashCommand_SendCommand_OutOnlyInstruction(&v,1,Index);
	}
    if(SerialFlash_is_protectbits_set(Index)==bProtect) return 1;

    bool result = false ;
    unsigned char  tmpSRVal;
    unsigned char dstSRVal ;
    //int numOfRetry = 3 ;
    // un-protect block ,set BP2 BP1 BP2 to 0
    dstSRVal = 0x00 ;
    // protect block ,set BP2 BP1 BP2 to 1
    if(bProtect){
        dstSRVal += 0x9C ;    // 9C : 9001 1100
    }

    int numOfRetry = 1000 ;
    result = SerialFlash_doWRSR(dstSRVal,Index) ;
    result = SerialFlash_doRDSR(&tmpSRVal,Index) ;
    while( (tmpSRVal & 0x01) &&  numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = SerialFlash_doRDSR(&tmpSRVal,Index) ;

        if(! result) return false;

        numOfRetry -- ;

    };

    return ((tmpSRVal ^ dstSRVal)& 0x0C ) ? 0:1;
}


int SerialFlash_EnableQuadIO(int bEnable,int boRW,int Index)
{
    m_boEnWriteQuadIO = (bEnable & boRW); //Simon: ported done???
    m_boEnReadQuadIO = (bEnable & boRW); //Simon: ported done???
	return SerialFlash_TRUE;
}

bool  CEN25QHxx_LargeEnable4ByteAddrMode(bool Enable4Byte,int Index)
{
//    SerialFlash_doWREN(Index);
    if(Enable4Byte)
    {
        unsigned char v= EN4B;
        if( FlashCommand_TransceiveOut(&v,1,false,Index)==SerialFlash_FALSE)
            return false;
    }
    else
    {
        unsigned char v= EXIT4B;
        if(FlashCommand_TransceiveOut(&v,1,false,Index)==SerialFlash_FALSE)
            return false;
    }
    return true;
}

bool CN25Qxxx_LargeRDFSR(unsigned char *cSR, int Index)
{
    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction=0x70 ;    //size 1

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = 1;

    if(OutCtrlRequest(&rq, &vInstruction,1,Index)==SerialFlash_FALSE)
        return SerialFlash_FALSE ;

    // second control packet : fetch data
    unsigned char vBuffer;        //just read one byte, in fact more bytes are also available
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = CTRL_TIMEOUT;
	    rq.Index = NO_REGISTER;
	}
    rq.Length = 1;

    if(OutCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
        return SerialFlash_FALSE ;
    *cSR = vBuffer;

    return true ;
}

bool CN25Qxxx_LargeEnable4ByteAddrMode(bool Enable4Byte,int Index)
{
#if 0
    if(Enable4Byte)
    {
        WORD cSR;
        RDNVCR(cSR, Index);
        cSR &= 0xFFFE;
        WRNVCR(cSR, Index);
    }
    return true;
#else
    if(Enable4Byte)
    {
        unsigned char v= EN4B;
         int numOfRetry = 5 ;
        unsigned char re;
        do{
            SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v,1,false,Index);
            Sleep(100);
            CN25Qxxx_LargeRDFSR(&re,Index);
        }while((re & 0x01)==0 && numOfRetry-- > 0);
        return true;
    }
    else
    {
        unsigned char re;
//        CN25Qxxx_LargeRDFSR(&re,Index);
        unsigned char v= EXIT4B;
        int numOfRetry = 5 ;

        do{
           SerialFlash_waitForWEL(Index);
            FlashCommand_TransceiveOut(&v,1,false,Index);
            Sleep(100);
            CN25Qxxx_LargeRDFSR(&re,Index);
        }while((re & 0x01)==1 && numOfRetry-- > 0);
        return true;
    }
#endif
}


//Simon: unused ???
int SerialFlash_Enable4ByteAddrMode(int bEnable,int Index)
{
    if(strstr(Chip_Info.Class,SUPPORT_EON_EN25QHxx_Large) != NULL || strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL
        || strstr(Chip_Info.Class,SUPPORT_WINBOND_W25Pxx_Large) != NULL)
        return CEN25QHxx_LargeEnable4ByteAddrMode(bEnable,Index);

    if(strstr(Chip_Info.Class,SUPPORT_NUMONYX_N25Qxxx_Large) != NULL)
        return CN25Qxxx_LargeEnable4ByteAddrMode(bEnable, Index);

	return SerialFlash_TRUE;
}


/**
 * \brief
 * read data, check if 0xFF
 *
 * \returns
 * true, if blank; false otherwise
 */
int SerialFlash_rangeBlankCheck(struct CAddressRange *Range,int Index)
{
    unsigned char *vBuffer;
    unsigned int i;
    Range->length = Range->end - Range->start;

    if (Range->length <= 0)
        return SerialFlash_FALSE;
    vBuffer = malloc(Range->length);
    if (vBuffer != 0)
    {
        if(SerialFlash_rangeRead(Range, vBuffer,Index) == SerialFlash_FALSE)
        {
            free(vBuffer);
            return false ;
        }
        for(i=0; i<Range->length; i++)
        {
            if(vBuffer[i] != 0xFF)
            {
//                printf("not blank at %X(%d)=%X\r\n",i,i,vBuffer[i]);
                free(vBuffer);
                 return false;
            }
        }
        free(vBuffer);
        return true ;
    }
    else
    {
        free(vBuffer);
        return SerialFlash_FALSE;
    }
}


/**
 * \brief
 * Write brief comment for Download here.
 *
 * it first check the data size and then delegates the actual operation to BulkPipeWrite()
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
int SerialFlash_rangeProgram(struct CAddressRange *AddrRange, unsigned char *vData,int Index)
{
    if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45rangeProgram(AddrRange, vData,mcode_Program, mcode_ProgramCode_4Adr, Index);
    else if(strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx_Large)!=NULL)
    {
        if(g_bIsSF600==true)
            return SerialFlash_bulkPipeProgram(AddrRange, vData, PP_4ADR_256BYTE,mcode_ProgramCode_4Adr,Index);
        else
            return SerialFlash_bulkPipeProgram(AddrRange, vData, PP_4ADDR_256BYTE_12,mcode_ProgramCode_4Adr,Index);
    }
    else
        return SerialFlash_bulkPipeProgram(AddrRange, vData, mcode_Program, mcode_ProgramCode_4Adr,Index);
}

int SerialFlash_rangeRead(struct CAddressRange *AddrRange, unsigned char *vData,int Index)
{
    if(strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx_Large)!=NULL)
    {
        if(g_bIsSF600==true)
            return SerialFlash_bulkPipeRead(AddrRange, vData, BULK_4BYTE_FAST_READ,mcode_ReadCode,Index);
        else
            return SerialFlash_bulkPipeRead(AddrRange, vData, BULK_4BYTE_FAST_READ_MICRON,mcode_ReadCode,Index);
    }
    else
	return SerialFlash_bulkPipeRead(AddrRange, vData, (unsigned char) mcode_Read, (unsigned char) mcode_ReadCode, Index);
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
 * A polling command can be used to poll the status register after Erase operation
 *
 */
int SerialFlash_DoPolling(int Index)
{
	CNTRPIPE_RQ rq ;
	unsigned char vDataPack[4];

	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_IN ;
	rq.Request = POLLING ;
	rq.Value = (CTRL_TIMEOUT >> 16) & 0xffff ;
	rq.Index = CTRL_TIMEOUT & 0xffff ;
	rq.Length = (unsigned long)(4) ;

	if(OutCtrlRequest(&rq, vDataPack, 4,Index) == SerialFlash_FALSE)
		return SerialFlash_FALSE ;

	return SerialFlash_TRUE ;

}


/**
 * \brief
 * retrieves object wellness flag
 *
 * before calling any other method, be sure to call is_good() to check the wellness
 *  which indicates whether the instance is constructed successfully or not
 *
 * \returns
 * true only if all objects of chip info, chip itself, USB  are correctly constructed
 * false otherwise
 *
 */
int SerialFlash_is_good()
{
//Simon	return ( m_info.is_good() && m_usb.is_open() ) ;
    return SerialFlash_TRUE ;
}

int SerialFlash_batchErase(uintptr_t* vAddrs,size_t AddrSize,int Index)
{
//    if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
//        return AT45batchErase(vAddrs, AddrSize,Index);

    if(0 == mcode_SegmentErase) return 1;      //  chipErase code not initialised or not supported, please check chip class ctor.

   if(SerialFlash_protectBlock(false,Index) == SerialFlash_FALSE)
           return false ;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    // send request
    CNTRPIPE_RQ rq ;
    size_t i;
    unsigned char vInstruction[5];//(5, mcode_SegmentErase) ;
    vInstruction[0]=mcode_SegmentErase;

//	printf("mcode_SegmentErase=%x\n",mcode_SegmentErase);
    // instrcution format
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = NO_RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = NO_RESULT_IN ;
	}
    rq.Length = 5;

    for(i=0; i<AddrSize; i++)
    {
        SerialFlash_waitForWEL(Index) ;

        if(Chip_Info.ChipSizeInByte>0x1000000)
        {
            // MSB~ LSB (31...0)
            vInstruction[1]=(unsigned char)((vAddrs[i] >> 24) & 0xff) ;     //MSB
            vInstruction[2] = (unsigned char)((vAddrs[i] >> 16) & 0xff) ;     //M
            vInstruction[3] = (unsigned char)((vAddrs[i] >> 8) & 0xff) ;      //M
            vInstruction[4] = (unsigned char)(vAddrs[i] & 0xff) ;             //LSB
            rq.Length = 5;
        }
        else
        {
            // MSB~ LSB (23...0)
            vInstruction[1] = (unsigned char)((vAddrs[i] >> 16) & 0xff) ;     //MSB
            vInstruction[2] = (unsigned char)((vAddrs[i] >> 8) & 0xff) ;      //M
            vInstruction[3] = (unsigned char)(vAddrs[i] & 0xff) ;             //LSB
            rq.Length = 4;
        }
        OutCtrlRequest(&rq, vInstruction,rq.Length,Index);

        SerialFlash_waitForWIP(Index);
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);
    return true ;
}

int SerialFlash_rangeErase(unsigned char cmd, size_t sectionSize, struct CAddressRange *AddrRange,int Index)
{
    if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45rangSectorErase(sectionSize,  *AddrRange,Index);

    if(SerialFlash_protectBlock(false,Index) == SerialFlash_FALSE)
        return SerialFlash_FALSE ;
    SerialFlash_Enable4ByteAddrMode(true, Index);

    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction[5];
    vInstruction[0] = cmd;

    // instrcution format
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN;
		rq.Index = RFU;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = (unsigned long)(1) ;

    size_t sectorNum = (AddrRange->end - AddrRange->start + sectionSize - 1) /  sectionSize ;
    size_t i;
    for(i = 0; i < sectorNum; ++ i)
    {
        SerialFlash_waitForWEL(Index) ;

        // MSB~ LSB (23...0)
        size_t addr = (AddrRange->start + i *  sectionSize) ;

        if(Chip_Info.ChipSizeInByte>0x1000000)
        {
            // MSB~ LSB (31...0)
            vInstruction[1] = (unsigned char)((addr >> 24) & 0xff) ;     //MSB
            vInstruction[2] = (unsigned char)((addr >> 16) & 0xff) ;     //M
            vInstruction[3] = (unsigned char)((addr >> 8) & 0xff) ;      //M
            vInstruction[4] = (unsigned char)(addr & 0xff) ;             //LSB
            rq.Length = (unsigned long)(5) ;
        }
        else
        {
            // MSB~ LSB (23...0)
            vInstruction[1] = (unsigned char)((addr >> 16) & 0xff) ;     //MSB
            vInstruction[2] = (unsigned char)((addr >> 8) & 0xff) ;      //M
            vInstruction[3] = (unsigned char)(addr & 0xff) ;             //LSB
            rq.Length = (unsigned long)(4);
        }

        int b = OutCtrlRequest(&rq, vInstruction, rq.Length, Index);
        if((b==SerialFlash_FALSE)|| m_isCanceled)  return false ;

        SerialFlash_waitForWIP(Index) ;
    }
    SerialFlash_Enable4ByteAddrMode(false, Index);

    return true ;
}

/// chip erase
int SerialFlash_chipErase(int Index)
{
    if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
        return AT45chipErase(0, Chip_Info.ChipSizeInByte, Index);

    if( SerialFlash_protectBlock(false,Index) == SerialFlash_FALSE)  return false ;
    SerialFlash_waitForWEL(Index) ;
    unsigned char v = mcode_ChipErase;
    FlashCommand_SendCommand_OutOnlyInstruction(&v,1,Index);
    SerialFlash_waitForWIP(Index) ;
    //if( SerialFlash_protectBlock(m_bProtectAfterWritenErase,Index) == SerialFlash_FALSE) return false ;
    return true ;
}


int SerialFlash_bulkPipeProgram(struct CAddressRange *AddrRange, unsigned char *vData, unsigned char modeWrite, unsigned char WriteCom, int Index)
{
    size_t i,j,divider;
    unsigned char *itr_begin;
    if(SerialFlash_protectBlock(false,Index) ==  SerialFlash_FALSE)
        return false ;

    if(modeWrite!= PP_SB_FPGA)
        SerialFlash_waitForWEL(Index) ;

    SerialFlash_Enable4ByteAddrMode(true, Index);
    if(SerialFlash_EnableQuadIO(true,m_boEnWriteQuadIO,Index) == SerialFlash_FALSE)
        return false;

//    printf("WriteMode=%d, WriteCom=%X\r\n", modeWrite,WriteCom);
    itr_begin = vData;
    switch(modeWrite)
    {
        //transfer how many data each time
        case MODE_NUMONYX_PCM:
        case PP_32BYTE:
            divider=9;
            break;
        case PP_128BYTE:
            divider=7;
            break;
        default:
            divider=8;
            break;
    }

    if((AddrRange->end/0x1000000)>(AddrRange->start/0x1000000))//(AddrRange.end>0x1000000 && AddrRange.start<0x1000000) ||(AddrRange.end>0x2000000 && AddrRange.start<0x2000000) ||
    {
        struct CAddressRange down_range;
        struct CAddressRange range_temp;
        range_temp.start= AddrRange->start&0xFF000000;
        range_temp.end= AddrRange->end + ((AddrRange->end % 0x1000000)? (0x1000000-(AddrRange->end % 0x1000000)):0);

        down_range.start = AddrRange->start;
        down_range.end = AddrRange->end;
        size_t packageNum;
        size_t loop=(range_temp.end-range_temp.start)/0x1000000;
//				printf("loop=%d      \r\n",loop);
//				printf("range_temp.end=%x,range_temp.start=%x\n\r",range_temp.end,range_temp.start);

        for(j=0; j<loop; j++)
        {
            if(j==(loop-1))
                down_range.end=AddrRange->end;
            else
                down_range.end=(AddrRange->start&0xFF000000)+(0x1000000*(j+1));

            if(j==0)
                down_range.start=AddrRange->start;
            else
                down_range.start=(AddrRange->start&0xFF000000)+(0x1000000*j);

            down_range.length=down_range.end-down_range.start;
            packageNum = down_range.length >> divider ;
//			printf("packageNum=%d  \r\n",packageNum);
            FlashCommand_SendCommand_SetupPacketForBulkWrite(&down_range, modeWrite,WriteCom,Index);
            for( i = 0; i < packageNum; ++ i)
            {
                BulkPipeWrite((unsigned char *)(itr_begin + (i << divider)), 1<<divider,USB_TIMEOUT,Index);
                if(m_isCanceled) return false;
            }
            itr_begin = itr_begin+(packageNum<< divider);
        }
    }
    else
    {
        size_t packageNum = (AddrRange->end - AddrRange->start) >> divider ;
        FlashCommand_SendCommand_SetupPacketForBulkWrite(AddrRange, modeWrite,WriteCom,Index);
        for(i = 0; i < packageNum; ++ i)
        {
            BulkPipeWrite((unsigned char *)((itr_begin + (i << divider))), 1<<divider,USB_TIMEOUT,Index);
            if(m_isCanceled)
                return false ;
        }
    }

    if(mcode_Program==AAI_2_BYTE)
        SerialFlash_doWRDI(Index);

    if(SerialFlash_protectBlock(m_bProtectAfterWritenErase,Index) == SerialFlash_FALSE)      return false ;
    if(SerialFlash_EnableQuadIO(false,m_boEnWriteQuadIO,Index)== SerialFlash_FALSE) return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);

    return true ;
}

int SerialFlash_bulkPipeRead(struct CAddressRange *AddrRange, unsigned char *vData, unsigned char modeRead,unsigned char ReadCom,int Index)
{
    size_t i,j,loop,pageNum,BufferLocation=0;
    int ret = 0;
    SerialFlash_Enable4ByteAddrMode(true, Index);
    if(SerialFlash_EnableQuadIO(true, m_boEnReadQuadIO,Index)== SerialFlash_FALSE)
        return false;

//    unsigned char v[512];
    AddrRange->length = AddrRange->end - AddrRange->start;
    if (AddrRange->length <= 0) {
        return false;
    }
//    printf("modeRead=%d\r\n",modeRead);

//    printf("AddrRange->end=%x, AddrRange->start=%x\r\n",AddrRange->end,AddrRange->start);
    if((AddrRange->end/0x1000000) > (AddrRange->start/0x1000000))//(AddrRange.end>0x1000000 && AddrRange.start<0x1000000)
    {
        struct CAddressRange read_range;
        struct CAddressRange range_temp;
        range_temp.start= AddrRange->start&0xFF000000;
        range_temp.end= AddrRange->end + ((AddrRange->end % 0x1000000)? (0x1000000-(AddrRange->end % 0x1000000)):0);

        read_range.start=AddrRange->start;
        read_range.end=AddrRange->end;
        loop=(range_temp.end-range_temp.start)/0x1000000;

        for(j=0; j<loop; j++)
        {
            if(j==(loop-1))
                read_range.end=AddrRange->end;
            else
                read_range.end=(AddrRange->start&0xFF000000)+(0x1000000*(j+1));

            if(j==0)
                read_range.start=AddrRange->start;
            else
                read_range.start=((AddrRange->start&0xFF000000)+(0x1000000*j));

            read_range.length=read_range.end-read_range.start;

            pageNum = read_range.length >> 9 ;
            FlashCommand_SendCommand_SetupPacketForBulkRead(&read_range, modeRead,ReadCom,Index);
            for(i = 0; i < pageNum; ++ i)
            {
                ret = BulkPipeRead(vData + (BufferLocation+i)*512, USB_TIMEOUT,Index);
                if((ret!=512) || m_isCanceled) return 0 ;
                //memcpy(vData + (BufferLocation+i)*512, v, 512);
            }
            BufferLocation += pageNum;
        }
    }
    else
    {
        pageNum = AddrRange->length >> 9 ;
        FlashCommand_SendCommand_SetupPacketForBulkRead(AddrRange, modeRead,ReadCom,Index);
        for(i = 0; i < pageNum; ++ i)
        {
            ret = BulkPipeRead(vData + i*ret, USB_TIMEOUT,Index);
            if((ret != 512) || m_isCanceled)
            {
                return false ;
            }
            //memcpy(vData + i*ret, v, ret);
        }
    }
    if(SerialFlash_EnableQuadIO(false,m_boEnReadQuadIO,Index) == SerialFlash_FALSE)
        return false;
    SerialFlash_Enable4ByteAddrMode(false, Index);
	return true ;
}


void SerialFlash_SetCancelOperationFlag()
{
	m_isCanceled = SerialFlash_TRUE;
}

void SerialFlash_ClearCancelOperationFlag()
{
	m_isCanceled = SerialFlash_FALSE;
}

int SerialFlash_readSR(unsigned char *cSR,int Index)
{
	return SerialFlash_doRDSR(cSR,Index);
}

int SerialFlash_writeSR(unsigned char cSR,int Index)
{
	SerialFlash_doWREN(Index);
	return SerialFlash_doWRSR(cSR,Index);
}

int SerialFlash_is_protectbits_set(int Index)
{
	unsigned char sr;
	SerialFlash_doRDSR(&sr,Index) ;

	return ((0 != (sr & 0x9C) )? 1: 0);
}

