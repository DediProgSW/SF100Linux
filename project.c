#include "Macro.h"
#include "project.h"
#include "IntelHexFile.h"
#include "MotorolaFile.h"
#include "dpcmd.h"
#include "SerialFlash.h"
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#define min(a,b) (a>b? b:a)

unsigned char* pBufferForLastReadData=NULL;
unsigned char* pBufferforLoadedFile=NULL;
unsigned long g_ulFileSize=0;
unsigned int g_uiFileChecksum=0;
unsigned char g_BatchIndex=2;
extern unsigned int g_ucFill;
extern m_boEnReadQuadIO;
extern m_boEnWriteQuadIO;
extern volatile bool g_bIsSF600;
extern char g_board_type[8];
extern int g_firmversion;
extern char* g_parameter_vcc;

//extern bool S19FileToBin(const char* filePath, unsigned char* vData,unsigned long* FileSize, unsigned char PaddingByte);
//extern bool HexFileToBin(const char* filePath, unsigned char* vOutData,unsigned long* FileSize,unsigned char PaddingByte);
extern unsigned char mcode_WRSR;
extern unsigned char mcode_WRDI;
extern unsigned char mcode_RDSR;
extern unsigned char mcode_WREN;
extern unsigned char mcode_SegmentErase;
extern unsigned char mcode_ChipErase;
extern unsigned char mcode_Program;
extern unsigned char mcode_ProgramCode_4Adr;
extern unsigned char mcode_Read;
extern unsigned char mcode_ReadCode;
extern CHIP_INFO Chip_Info;
extern int g_StartupMode;
extern int g_CurrentSeriase;
extern struct CAddressRange DownloadAddrRange;
extern struct CAddressRange UploadAddrRange;
extern bool g_is_operation_on_going;
extern bool g_is_operation_successful;
extern unsigned int g_Vcc;
extern unsigned int g_Vpp;
extern unsigned int g_uiAddr;
extern unsigned int g_uiLen;
extern bool g_bEnableVpp;
#define FIRMWARE_VERSION(x,y,z) ((x<<16) | (y<<8) | z)

enum FILE_FORMAT{
        BIN ,
        HEX,
        S19,
    } ;

void Sleep(unsigned int mSec)
{
    usleep(mSec*1000);
}

void TurnONVpp(void)
{
    if(g_bEnableVpp==true)
        dediprog_set_vpp_voltage(Chip_Info.VppSupport);
}

void TurnOFFVpp(void)
{
    if(g_bEnableVpp==true)
        dediprog_set_vpp_voltage(0);
}

void TurnONVcc(int Index)
{
//	printf("g_Vcc=%x\n",g_Vcc);
	dediprog_set_spi_voltage(g_Vcc,Index);
}

void TurnOFFVcc(int Index)
{
	dediprog_set_spi_voltage(0,Index);
}


unsigned int CRC32(unsigned char* v, unsigned long size)
{
    unsigned int checksum = 0;
    unsigned long i;
    for(i=0;i<size;i++)
        checksum += v[i];

    return checksum;// & 0xFFFF;

}

int ReadBINFile(const char *filename,unsigned char *buf, unsigned long* size)
{
    FILE * pFile;
    long lSize;
    size_t result;

    pFile = fopen( filename , "rb" );
    if (pFile==NULL)
    {
        printf("Open %s failed\n",filename);
        fputs ("File error\n",stderr);
        return 0;
    }

    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    if(pBufferforLoadedFile!=NULL)
        free(pBufferforLoadedFile);

    pBufferforLoadedFile = (unsigned char*) malloc (sizeof(char)*lSize);

    if (pBufferforLoadedFile == NULL)
    {
        fputs ("Memory error\n",stderr);
        return 0;
    }

    // copy the file into the buffer:
    result = fread (pBufferforLoadedFile,1,lSize,pFile);
    if (result != lSize)
    {
        fputs ("Reading error\n",stderr);
        return 0;
    }

    /* the whole file is now loaded in the memory buffer. */

    // terminate
    g_ulFileSize=lSize;
    fclose (pFile);
    return 1;
}

int WriteBINFile(const char *filename,unsigned char *buf, unsigned long size)
{
	unsigned long numbytes;
	FILE *image;

	if (!filename) {
		printf("No filename specified.\n");
		return 0;
	}
	if ((image = fopen(filename, "wb")) == NULL) {
		perror(filename);
		return 0;
	}

	numbytes = fwrite(buf, 1, size, image);
	fclose(image);
	if (numbytes != size) {
		printf("File %s could not be written completely.\n",
			 filename);
		return 0;
	}
	return 1;
}


int GetFileFormatFromExt(const char* csPath)
{
    size_t length=strlen(csPath);
    char fmt[5]={0};
    size_t i=length;
    size_t j=0;

    while(csPath[i] != '.')
    {
        i--;
    }

    for(j=i;j<length; j++)
    {
        fmt[j-i]=toupper(csPath[j]);
    }

    if(strcmp(fmt,".HEX")==0)
        return HEX ;
    else if(strcmp(fmt,".S19")==0 || strcmp(fmt,".MOT")==0)
        return S19 ;
    else
        return BIN ;
}

bool ReadFile(const char* csPath, unsigned char* buffer,unsigned long* FileSize,unsigned char PaddingByte)
{
    switch(GetFileFormatFromExt(csPath))
    {
        case HEX :
            return HexFileToBin(csPath, buffer,FileSize,PaddingByte) ;
        case S19 :
            return S19FileToBin(csPath, buffer,FileSize,PaddingByte) ;
        default :
            return ReadBINFile(csPath, buffer,FileSize) ;
    }
}

// write file
bool WriteFile(const char* csPath, unsigned char* buffer,unsigned int FileSize)
{
    switch(GetFileFormatFromExt(csPath))
    {
        case HEX :
            return BinToHexFile( csPath,buffer,FileSize) ;
        case S19 :
            return BinToS19File(csPath,buffer, FileSize) ;
        default :
            return WriteBINFile(csPath,buffer, FileSize) ;
    }
}

bool  LoadFile(char* filename)
{
    bool result=true;
    unsigned long size;
    result &= ReadFile(filename,pBufferforLoadedFile, &size, g_ucFill);
    g_uiFileChecksum=CRC32(pBufferforLoadedFile,g_ulFileSize);
    return result;
}

bool IdentifyChipBeforeOperation(int Index)
{
    bool result = false;
    CHIP_INFO binfo;
    binfo.UniqueID=0;
    int Found=0;

    if(strstr(Chip_Info.Class, SUPPORT_FREESCALE_MCF) != NULL ||
        strstr(Chip_Info.Class, SUPPORT_SILICONBLUE_iCE65) != NULL)
        return true;

    Found=FlashIdentifier(&binfo,0);

	if( binfo.UniqueID == Chip_Info.UniqueID ||  binfo.UniqueID == Chip_Info.JedecDeviceID)
		result = true;

    return result;
}

void PrepareProgramParameters(int Index)
{
    size_t len = 0;
    size_t addrStart = 0;

    addrStart =  g_uiAddr;
    len = g_uiLen;

    size_t addrLeng = (0 == len) ? g_ulFileSize: len;
    DownloadAddrRange.start=addrStart;
    DownloadAddrRange.end=addrStart +  addrLeng;
    DownloadAddrRange.length=addrLeng;

}

bool ValidateProgramParameters(int Index)
{
    PrepareProgramParameters(Index);

    /// special treatment for AT45DBxxxD
    if (strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL)
    {
        size_t pagesize=264;
        size_t mask= (1<<9)-1;
        return (mask  &  DownloadAddrRange.start) <=  (mask &  pagesize);
    }

    /// if file size exceeds memory size, it's an error
    /// if user-specified length exceeds, just ignore
    if(DownloadAddrRange.start > Chip_Info.ChipSizeInByte - 1)
        return false;

    size_t size = DownloadAddrRange.length;
    if(size > g_ulFileSize && g_ucFill==0xFF)
        return false;

    if(DownloadAddrRange.end > Chip_Info.ChipSizeInByte)
        return false;

    return true;
}

bool ProgramChip(int Index)
{
    bool need_padding = (g_ucFill != 0xFF);
    unsigned char* vc;
    struct CAddressRange real_addr;
    real_addr.start=(DownloadAddrRange.start &(~(0x200 - 1)));
    real_addr.end=((DownloadAddrRange.end + (0x200 - 1)) & (~(0x200 - 1)));
    real_addr.length=real_addr.end-real_addr.start;

    vc=(unsigned char*)malloc(real_addr.length);
    memset(vc,g_ucFill,real_addr.length);

    if(need_padding==true)
    {
        memcpy(vc+(DownloadAddrRange.start & 0x1FF),pBufferforLoadedFile,min(g_ulFileSize,DownloadAddrRange.length));
    }
    else
    {
        memcpy(vc+(DownloadAddrRange.start & 0x1FF),pBufferforLoadedFile,DownloadAddrRange.length);
    }

    bool result= SerialFlash_rangeProgram(&real_addr, vc,Index);

    return result;

}

bool ReadChip(const struct CAddressRange range,int Index)
{
    bool result = true;
    unsigned char* vc;
    size_t addrStart = range.start;
    size_t addrLeng = range.length;
    struct CAddressRange addr;
    addr.start=	range.start &(~(0x200 - 1));
    addr.end=(range.end + (0x200 - 1)) & (~(0x200 - 1));
    addr.length=addr.end-addr.start;
    vc=(unsigned char*)malloc(addr.length);

    result = SerialFlash_rangeRead(&addr, vc, Index);

    if(result)
    {
        if(pBufferForLastReadData != NULL)
            free(pBufferForLastReadData);

        unsigned int offset = (addrStart & 0x1FF);
        pBufferForLastReadData=(unsigned char*)malloc(range.length);
        memcpy(pBufferForLastReadData,vc+offset,range.length);
        UploadAddrRange=range;
    }

    if(vc != NULL)
        free (vc);

	return result;

}

bool threadBlankCheck(int Index)
{
    bool result = false;
//    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);
    struct CAddressRange Addr;
    Addr.start=0;
    Addr.end=Chip_Info.ChipSizeInByte;

    //(L"Blank Checking ...");

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(SITE_BUSY,Index);

    SetIOMode(false,Index);


    result = SerialFlash_rangeBlankCheck(&Addr,Index) ;

//	m_bOperationResult[Index]=result? 1:RES_BLANK;
#if 0
	if( !bAuto[Index] )
	{

		m_context.runtime.elapsed_time_of_last_operation = timer.elapsed() ;
		Log(wformat(L"Operation completed. \n%1% seconds elapsed.")%m_context.runtime.elapsed_time_of_last_operation);

		CompleteCnt++;

		if( CompleteCnt==GetDevNum() )
		{
			m_context.runtime.is_operation_on_going  = false;
			m_chip[Index]->ClearCancelOperationFlag();
		}

		if(is_greater_than_5_0_0)
			m_board->SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);
	}
#endif
//    SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);
//    g_is_operation_on_going  = false;
    g_is_operation_successful = result;
    return result;
}

bool threadEraseWholeChip(int Index)
{
	bool result = false;
//	bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);

//	if(is_greater_than_5_0_0)
//		SetLEDOnOff(SITE_BUSY,Index);

//	Log(L"Erasing a whole chip ...");

//	power::CAutoVccPower autopowerVcc(m_usb, m_context.power.vcc,Index);
//	power::CAutoVppPower autopowerVpp(m_usb, SupportedVpp(),Index);
	result = SerialFlash_chipErase(Index);

//	Log(result ? L"A whole chip erased" : L"Error: Failed to erase a whole chip");

//	m_bOperationResult[Index]=result? 1:RES_ERASE;
#if 0
	if( !bAuto[Index] )
	{
		m_context.runtime.is_operation_successful = result;
		m_context.runtime.elapsed_time_of_last_operation = timer.elapsed() ;
		Log(wformat(L"Operation completed. \n%1% seconds elapsed.")%m_context.runtime.elapsed_time_of_last_operation);

		CompleteCnt++;

		if( CompleteCnt==GetDevNum() )
		{
			m_context.runtime.is_operation_on_going  = false;
			m_chip[Index]->ClearCancelOperationFlag();
		}

		if(is_greater_than_5_0_0)
			m_board->SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);
	}
#endif
//    SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);
   g_is_operation_successful = result;
//    g_is_operation_on_going  = false;
	return result;
}

bool threadReadRangeChip(struct CAddressRange range,int Index)
{
    bool result = true;
//    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);
    struct CAddressRange AddrRound;

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(SITE_BUSY,Index);

    SetIOMode(false,Index);

    AddrRound.start= (range.start &(~(0x1FF)));
    AddrRound.end=((range.end +0x1FF) & (~(0x1FF)));
    AddrRound.length=AddrRound.end-AddrRound.start;
    unsigned char *pBuffer = malloc(AddrRound.length);

    if(pBufferForLastReadData==NULL)
        pBufferForLastReadData=malloc(range.end-range.start);
    else
    {
        free(pBufferForLastReadData);
        pBufferForLastReadData=malloc(range.end-range.start);
    }
    if(pBufferForLastReadData==NULL)
    {
        printf("allocate memory fail.\n");
        free(pBuffer);
        return false;
    }

    result = SerialFlash_rangeRead(&AddrRound,  pBuffer, Index);
    if(result)
    {
        UploadAddrRange.start = range.start;
        UploadAddrRange.end= range.end;
        UploadAddrRange.length = range.end-range.start;
        memcpy(pBufferForLastReadData,pBuffer+ (UploadAddrRange.start & 0x1FF),UploadAddrRange.length);
    }
    free(pBuffer);

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);

    return result;

}

bool threadReadChip(int Index)
{
    bool result = false;
    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);
    struct CAddressRange Addr;
    Addr.start=0;
    Addr.end=Chip_Info.ChipSizeInByte;

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(SITE_BUSY,Index);

    SetIOMode(false,Index);

    if(pBufferForLastReadData==NULL)
            pBufferForLastReadData=(unsigned char*)malloc(Chip_Info.ChipSizeInByte);
    else
    {
        free(pBufferForLastReadData);
        pBufferForLastReadData=(unsigned char*)malloc(Chip_Info.ChipSizeInByte);
    }
    if(pBufferForLastReadData==NULL)
    {
        printf("allocate memory fail.\n");
        return false;
    }

    result =  SerialFlash_rangeRead(&Addr, pBufferForLastReadData,Index);

    if(result)
    {
        UploadAddrRange.start = 0;
        UploadAddrRange.end=Chip_Info.ChipSizeInByte;
        UploadAddrRange.length=Chip_Info.ChipSizeInByte;
    }

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(result?SITE_OK:SITE_ERROR,Index);

    g_is_operation_successful = result;

    return result;
}
bool threadConfiguredReadChip(int Index)
{
    bool result = true;
    struct CAddressRange addr;
    addr.start= DownloadAddrRange.start;
    addr.length= DownloadAddrRange.end-DownloadAddrRange.start;


    if(0 == addr.length && Chip_Info.ChipSizeInByte > addr.start)
        addr.length = Chip_Info.ChipSizeInByte - addr.start;

    addr.end=addr.length+addr.start;
    if(addr.start >= Chip_Info.ChipSizeInByte)
    {
        result = false;
    }
    else if(0 == addr.start && (0 == addr.length))
    {
        result = threadReadChip(Index);
    }
    else
    {
        result = threadReadRangeChip(addr,Index);
    }

    g_is_operation_successful = result;
//    g_is_operation_on_going  = false;
    return result;
}

bool threadProgram(int Index)
{
    bool result = true;
    SetIOMode(true,Index);


    if(g_ulFileSize== 0)
    {
        result = false;
    }

    if( result && (!ValidateProgramParameters(Index)) )
    {
        result = false;
    }

    if( result && ProgramChip(Index))
    {
        result = true;
    }
    else
    {
        result = false;
    }

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);
    g_is_operation_successful = result;
    return result;
}

bool threadCompareFileAndChip(int Index)
{
    bool result = true;
//    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(SITE_BUSY,Index);

    SetIOMode(false,Index);

    if(g_ulFileSize==0)
        result = false;

    if( result && (!ValidateProgramParameters(Index)) )
        result = false;

    if( result )
    {
        ReadChip(DownloadAddrRange,Index);

        size_t offset = min(DownloadAddrRange.length,g_ulFileSize);
        unsigned int crcFile = CRC32(pBufferforLoadedFile,offset);
        unsigned int crcChip = CRC32(pBufferForLastReadData,offset);
        unsigned int i=0;
        #if 0
        for(i=0; i<10; i++)
        {
            if(pBufferforLoadedFile[i] != pBufferForLastReadData[i])
                printf("Data deferent at %X, pBufferforLoadedFile[i]=%x,pBufferForLastReadData[i]=%x\r\n",i,pBufferforLoadedFile[i],pBufferForLastReadData[i]);
        }
        #endif

        result = (crcChip == crcFile);
    }

//    if(is_greater_than_5_0_0)
//        SetLEDOnOff(result? SITE_OK:SITE_ERROR,Index);

    g_is_operation_successful = result;
    return result;
}

size_t GenerateDiff(size_t* Addr,unsigned char* in1, unsigned long long size1, unsigned char* in2, unsigned long long size2, size_t baseAddr, size_t step)
{
    size_t upper = min(size1, size2);
    size_t realAddr,i,j=0;

    for(i = 0; i < upper; ++i)
    {
        if(in1[i] != in2[i])
        {
            realAddr = i + baseAddr;
            Addr[j++]=(realAddr & (~(step - 1)));

            i += ((step - 1)|realAddr) - realAddr;
            if(i >= upper) break;
        }
    }
    return j;
}

size_t Condense(size_t* out,unsigned char* vc, size_t* addrs, size_t addrSize, size_t baseAddr, size_t step)
{
    size_t* itr, itr_end;
    size_t i,j,outSize=0;
    itr_end=addrs+addrSize;

    for(j=0; j<addrSize; j++)
    {
        for(i=addrs[j] - baseAddr; i<(addrs[j] - baseAddr+step); i++)
        {
            if(vc[i] != 0xFF)
            {
                out[j]=addrs[j];
                outSize++;
                break;
            }
        }
    }
    return outSize;
}

extern void SetPageSize(CHIP_INFO* mem, int USBIndex);

bool BlazeUpdate(int Index)
{
    struct CAddressRange addr_round;//(Chip_Info.MaxErasableSegmentInByte);

//    if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
//        SetPageSize(&Chip_Info,Index);

    // dealwith lock phase 1
    struct CAddressRange down_with_lock_range;//(m_context.runtime.range_download);
    down_with_lock_range.start=DownloadAddrRange.start;
    down_with_lock_range.end=DownloadAddrRange.end;
    DownloadAddrRange.length=DownloadAddrRange.end-DownloadAddrRange.start;
    down_with_lock_range.length=down_with_lock_range.end-down_with_lock_range.start;

    struct CAddressRange effectiveRange;//(addr_round.SectionRound(down_with_lock_range));
    effectiveRange.start=down_with_lock_range.start &(~(Chip_Info.MaxErasableSegmentInByte - 1)) ;
    effectiveRange.end=(down_with_lock_range.end + (Chip_Info.MaxErasableSegmentInByte - 1)) & (~(Chip_Info.MaxErasableSegmentInByte - 1));
    effectiveRange.length=effectiveRange.end-effectiveRange.start;

    if(!threadReadRangeChip(effectiveRange,Index)) return false;

    unsigned int offsetOfRealStartAddrOffset = DownloadAddrRange.start - effectiveRange.start;

    unsigned char* vc;
    vc=(unsigned char*)malloc(effectiveRange.length);
    memcpy(vc,pBufferForLastReadData,effectiveRange.length);
    size_t* addrs=(size_t*)malloc(min(DownloadAddrRange.length,g_ulFileSize));
    size_t Leng=0;
    Leng=GenerateDiff(addrs,vc+offsetOfRealStartAddrOffset,DownloadAddrRange.length,pBufferforLoadedFile,g_ulFileSize,DownloadAddrRange.start,Chip_Info.MaxErasableSegmentInByte);

//    printf("Leng=%X\r\n",Leng);
    if(Leng==0)  // speed optimisation
    {
        return true;
    }
    else
    {
        size_t* condensed_addr=(size_t*)malloc(min(DownloadAddrRange.length,g_ulFileSize));
        size_t condensed_size;
        condensed_size=Condense(condensed_addr,vc, addrs, Leng, effectiveRange.start,Chip_Info.MaxErasableSegmentInByte);
//        printf("condensed_size=%d\r\n",condensed_size);
        SerialFlash_batchErase(condensed_addr,condensed_size,Index);

        if(strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx) != NULL)
        {
            TurnOFFVcc(Index);
            Sleep(100);
            TurnONVcc(Index);
        }

        memcpy(vc+offsetOfRealStartAddrOffset,pBufferforLoadedFile,DownloadAddrRange.length);
        size_t i = 0;
        for(i=0; i<Leng; i++)
        {
            size_t idx_in_vc = addrs[i] - effectiveRange.start;
            struct CAddressRange addr_range;
            addr_range.start=addrs[i];
            addr_range.end=addrs[i]+Chip_Info.MaxErasableSegmentInByte;
            addr_range.length=addr_range.end-addr_range.start;

            if(SerialFlash_rangeProgram(&addr_range,vc+idx_in_vc,Index)==0)
            {
                free(vc);
                free(addrs);
                free(condensed_addr);
                return false;
            }
        }
        free(vc);
        free(addrs);
        free(condensed_addr);
        return true;
    }
    free(vc);
    free(addrs);
    return true;
}

bool RangeUpdateThruSectorErase(int Index)
{
    return BlazeUpdate(Index);
}


bool RangeUpdateThruChipErase(int Index)
{
//    printf("RangeUpdateThruChipErase!\n");
    unsigned char* vc=(unsigned char*)malloc(Chip_Info.ChipSizeInByte);
    unsigned int i=0;
    bool boIsBlank=true;
    struct CAddressRange addr;
    DownloadAddrRange.length=DownloadAddrRange.end-DownloadAddrRange.start;
    SetIOMode(false, Index);

    if(!threadReadChip(Index)) return false;

    memcpy(vc,pBufferForLastReadData,Chip_Info.ChipSizeInByte);

    for(i=0; i<Chip_Info.ChipSizeInByte; i++)
    {
        if(vc[i]!= 0xFF)
        {
            boIsBlank=false;
            break;
        }
    }
    if(boIsBlank==false)
    {
        if(threadEraseWholeChip(Index)==false) return false;

        if(strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
        {
            TurnOFFVcc(Index);
            Sleep(100);
            TurnONVcc(Index);
        }
    }

    memset(vc+DownloadAddrRange.start,g_ucFill,min(DownloadAddrRange.length,Chip_Info.ChipSizeInByte));
    memcpy(vc+DownloadAddrRange.start,pBufferforLoadedFile,min(g_ulFileSize,DownloadAddrRange.length));

    SetIOMode(true, Index);
    addr.start=0;
    addr.end=Chip_Info.ChipSizeInByte;
    addr.length=Chip_Info.ChipSizeInByte;

    return SerialFlash_rangeProgram(&addr, vc,Index);
}

bool RangeUpdate(int Index)
{
    bool fast_load = (Chip_Info.ChipSizeInByte > (1<<16));

    return ((mcode_SegmentErase!=0) && fast_load)
        ? RangeUpdateThruSectorErase(Index)
        : RangeUpdateThruChipErase(Index);
}
bool ReplaceChipContentThruChipErase(int Index)
{
	if(!threadBlankCheck(Index))//not blank
	{
		if(!threadEraseWholeChip(Index))
			return false;
	}

    return threadProgram(Index);
}

bool threadPredefinedBatchSequences(int Index)
{
    bool result = true;

    if(g_ulFileSize==0) result = false;

    size_t option=2;
    bool bVerifyAfterCompletion;
       //  07.11.2009
    bool bIdentifyBeforeOperation;

    if( result && (!ValidateProgramParameters(Index)) ) result = false;
#if 0
    if(strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx)!= NULL
		||strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL
		||strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_PP32)!=NULL)
		Sleep(10);
#endif

    if( result )
    {
    	switch(g_BatchIndex)
    	{
    		case 1:
				result=ReplaceChipContentThruChipErase(Index);
				break;
			case 2:
			default:
				result = RangeUpdate(Index);
				break;
    	}
    }

	return result;
}

void threadRun(void* Type)
{
    THREAD_STRUCT* thread_data=(THREAD_STRUCT*)Type;
    OPERATION_TYPE opType=thread_data->type;
    int Index=thread_data->USBIndex;
    g_is_operation_successful = true;

    g_is_operation_on_going = true;

    if( opType==UPDATE_FIRMWARE )
    {
        return;
    }
    if(opType==AUTO_UPDATE_FIRMWARE)
    {
        return;
    }

    if(1)//is_good())
    {
        TurnONVcc(Index);
        bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);

        if(is_greater_than_5_0_0)
            SetLEDOnOff(SITE_BUSY,Index);

        if(IdentifyChipBeforeOperation(Index)==false)
        {
        		printf("Warning: Failed to detect flash.\r\n");
            g_is_operation_on_going = false;
            g_is_operation_successful = false;
            SetLEDOnOff(g_is_operation_successful? SITE_OK:SITE_ERROR,Index);
			TurnOFFVcc(Index);

            return;
        }
        // operations
        switch(opType)
        {
            case BLANKCHECK_WHOLE_CHIP:
                threadBlankCheck(Index);
                break;

            case ERASE_WHOLE_CHIP:
                threadEraseWholeChip(Index);
                break;

            case PROGRAM_CHIP:
                TurnONVpp();
                threadProgram(Index);
                break;

            case READ_WHOLE_CHIP:
//                    m threadReadChip(Index);
                break;

            case READ_ANY_BY_PREFERENCE_CONFIGURATION:
                threadConfiguredReadChip(Index);
                break;

            case VERIFY_CONTENT:
                threadCompareFileAndChip(Index);
                break;

            case AUTO:
                TurnONVpp();
                threadPredefinedBatchSequences(Index) ;
                break;

            default:
                break;
        }
    }
    TurnOFFVcc(Index);
    TurnOFFVpp();
    SetLEDOnOff(g_is_operation_successful? SITE_OK:SITE_ERROR,Index);
    g_is_operation_on_going  = false;
}


void Run(OPERATION_TYPE type)
{
    pthread_t id;
    int i,ret;
    g_is_operation_on_going = true;
    THREAD_STRUCT thread_data;
    thread_data.type=type;
    thread_data.USBIndex=0;
    ret=pthread_create(&id,NULL,(void *) threadRun,(void*)&thread_data);

}

void SetIOMode(bool isProg,int Index)
{
    size_t IOValue=0;
    m_boEnReadQuadIO=0;
    m_boEnWriteQuadIO=0;

    if(g_bIsSF600==false) return;

    SetIOModeToSF600(IOValue, Index);
    return;
//    if(strlen(Chip_Info.ProgramIOMethod)==0)
//    {
//        SetIOModeToSF600(IOValue, Index);
//        return;
//    }

#if 0
	if(isProg)
	{
		switch(m_context.chip.IO_Mode)
		{
			case Context::CChipContext::IO_Single:
			default:
				if(out[1].find(L"SW")!=-1)
					IOValue=0;
				break;
			case Context::CChipContext::IO_Dual1:
			case Context::CChipContext::IO_Dual2:
				if((out[0]==L"SPDD" || out[0]==L"SPQD") && out[1].find(L"DW")!=-1)
					IOValue = 1;
				else if((out[0]==L"DPDD" || out[0]==L"QPQD") && out[1].find(L"DW")!=-1)
					IOValue = 2;
				break;
			case Context::CChipContext::IO_Quad1:
			case Context::CChipContext::IO_Quad2:
				if(out[0]==L"SPDD" && out[1].find(L"DW")!=-1)
					IOValue = 1;
				else if(out[0]==L"DPDD" && out[1].find(L"DW")!=-1)
					IOValue = 2;
				else if(out[0]==L"SPQD" && out[1].find(L"QW")!=-1)
					IOValue = 3;
				else if(out[0]==L"QPQD" && out[1].find(L"QW")!=-1)
					IOValue = 4;
				break;
		}
	}
	else
	{
	switch(m_context.chip.IO_Mode)
	{
		case Context::CChipContext::IO_Single:
		default:
			break;
		case Context::CChipContext::IO_Dual1:
		case Context::CChipContext::IO_Dual2:
				if(out[0]==L"SPDD" || out[0]==L"SPQD")
				IOValue = 1;
				else if(out[0]==L"DPDD" || out[0]==L"QPQD")
				IOValue = 2;
			break;
		case Context::CChipContext::IO_Quad1:
		case Context::CChipContext::IO_Quad2:
				if(out[0]==L"SPDD")
				IOValue = 1;
				else if(out[0]==L"DPDD")
				IOValue = 2;
				else if(out[0]==L"SPQD")
				IOValue = 3;
				else if(out[0]==L"QPQD")
				IOValue = 4;
			break;
	}
	}

    if((m_context.StartupMode[Index]==Context::STARTUP_APPLI_SF_SKT) && (IOValue>=3))
    {
        IOValue-=2;
        Log(L"Warning: Socket mode can not support Quad IO.");
    }

	switch(IOValue)
	{
		case 0:
			Log(L"Single IO is set.");
			break;
		case 1:
		case 2:
			Log(L"Dual IO is set.");
			break;
		case 3:
		case 4:
			Log(L"Quad IO is set.");
			if( m_chip[Index] )
			{
				m_chip[Index]->m_boEnReadQuadIO=(out[1].find(L'R')!=-1);//true;
				m_chip[Index]->m_boEnWriteQuadIO=(out[1].find(L'W')!=-1);
			}
			break;
	}

    m_board->SetIOMode(IOValue, Index);
    SetProgramReadCom(IOValue,Index);
#endif
}

int is_BoardVersionGreaterThan_5_0_0(int Inde)
{
    if(g_firmversion < FIRMWARE_VERSION(5, 0, 0))
        return false;
    return true;
}

int is_SF100nBoardVersionGreaterThan_5_5_0(int Inde)
{
	if((g_firmversion >= FIRMWARE_VERSION(5, 5, 0)) && strstr(g_board_type,"SF100") != NULL)
        return true;
    return false;
}

int is_SF600nBoardVersionGreaterThan_6_9_0(int Inde)
{
	if(strstr(g_board_type,"SF600") != NULL)
	{
		if((g_firmversion > FIRMWARE_VERSION(7, 0, 1)) || (g_firmversion == FIRMWARE_VERSION(6, 9, 0)))
			return true;
	}
    return false;
}

CHIP_INFO GetFirstDetectionMatch(int Index)
{
	CHIP_INFO binfo;
	binfo.UniqueID=0;
	int Found=0;
	int i=0;
	int Loop=3;
	if(strcmp(g_parameter_vcc,"NO") != 0)
		Loop=1;

	for(i=0; i<Loop; i++)
	{
		if(Found==1) break;
		if(Loop==1)
			g_Vcc=vcc3_5V;
		else
			g_Vcc=vcc1_8V-i;

		TurnONVcc(Index);
		if(Is_usbworking())
		{
			if(g_bIsSF600==true)
			{
				int startmode;

				if(g_StartupMode==STARTUP_APPLI_SF_2)
					SetTargetFlash(STARTUP_APPLI_SF_2,Index);
				else
					SetTargetFlash(STARTUP_APPLI_SF_1,Index);

				startmode=g_StartupMode;
				Found=FlashIdentifier(&binfo,0);
				if(Found==0)
				{
					TurnOFFVcc(Index);
					SetTargetFlash(STARTUP_APPLI_SF_SKT,Index);
					g_CurrentSeriase=Seriase_25;
					TurnONVcc(Index);
					Found=FlashIdentifier(&binfo,0);
					if(Found==0)
					{
						TurnOFFVcc(Index);
						g_CurrentSeriase=Seriase_45;
						TurnONVcc(Index);
						Found=FlashIdentifier(&binfo,0);
						if(Found==0)
							g_StartupMode=startmode;
					}
				}
			}
			else
			{
				Found=FlashIdentifier(&binfo,0);
			}
		}
		TurnOFFVcc(Index);
	}
	if(Found==0)
	{
		binfo.UniqueID=0;
		binfo.TypeName[0]='\0';
	}
	return binfo;
}

// fail in case of 1) USB failure, 2) unrecognised ID.
void InitLED(int Index)
{
    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);

    if(is_greater_than_5_0_0)
        SetLEDOnOff(SITE_NORMAL,Index);
}

void SetProgReadCommand(void)
{
    static const unsigned int AT26DF041 = 0x1F4400;
    static const unsigned int AT26DF004 = 0x1F0400;
    static const unsigned int AT26DF081A = 0x1F4501;
    mcode_WREN = WREN;
    mcode_RDSR = RDSR;
    mcode_WRSR = WRSR;
    mcode_ChipErase = CHIP_ERASE;
    mcode_Program = PAGE_PROGRAM;
    mcode_Read    = BULK_FAST_READ;
    mcode_SegmentErase= SE;
    mcode_WRDI=0x04;
    mcode_ProgramCode_4Adr=0xFF;
    mcode_ReadCode=0xFF;

    if(strcmp(Chip_Info.Class,SUPPORT_SST_25xFxx)==0)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read  = BULK_NORM_READ;

        if(strstr(Chip_Info.TypeName,"B") != NULL ||strstr(Chip_Info.TypeName,"W") != NULL)
            mcode_Program = AAI_2_BYTE ;
        else
            mcode_Program = AAI_1_BYTE ;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_SST_25xFxxA)!=NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = AAI_1_BYTE ;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_SST_25xFxxB)!=NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read      = BULK_NORM_READ;
        mcode_Program   = AAI_2_BYTE ;
        mcode_SegmentErase  = 0xD8;
        mcode_WRDI = WRDI;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_SST_25xFxxC) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read      = BULK_FAST_READ;
        mcode_Program   = PAGE_PROGRAM ;
        mcode_SegmentErase  = 0xD8;
        mcode_WREN = WREN;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_AT25FSxxx) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_NORM_READ;
        mcode_Program   = PP_128BYTE ;
        mcode_SegmentErase  = 0xD8;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_AT25Fxxx) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_NORM_READ;
        mcode_Program   = PP_128BYTE ;
        mcode_SegmentErase  = 0x52;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_AT26xxx) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_FAST_READ;
        mcode_SegmentErase  = 0xD8;
        if(AT26DF041 == Chip_Info.UniqueID)
        {
            mcode_Program = PP_AT26DF041;
        }
        else if(AT26DF004 == Chip_Info.UniqueID)
        {
            mcode_Program = AAI_1_BYTE;
        }
        else
        {
            mcode_Program = PAGE_PROGRAM;
        }
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ESMT_F25Lxx) != NULL)
    {
        const unsigned int F25L04UA=0x8C8C8C;
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_FAST_READ;
        mcode_SegmentErase  = 0xD8;
        if(F25L04UA == Chip_Info.UniqueID)
            mcode_Program = AAI_1_BYTE;
        else
            mcode_Program = AAI_2_BYTE;//PAGE_PROGRAM;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_NUMONYX_Alverstone) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_NORM_READ;
        mcode_Program   = MODE_NUMONYX_PCM ;
        mcode_SegmentErase  = 0xD8;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_EON_EN25QHxx_Large) != NULL
            || strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_4BYTE_FAST_READ;
        mcode_Program   = PP_4ADR_256BYTE ;
        mcode_SegmentErase  = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_SPANSION_S25FLxx_Large) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_4BYTE_FAST_READ;
        mcode_Program   = PP_4ADR_256BYTE ;
        mcode_SegmentErase  = 0xD8;
        mcode_ProgramCode_4Adr = 0x12;
        mcode_ReadCode = 0x0C;
        //printf("Read Code=%X\r\n",mcode_ReadCode);
    }
    else if(strstr(Chip_Info.Class,SUPPORT_NUMONYX_N25Qxxx_Large) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_4BYTE_FAST_READ_MICRON;
        mcode_Program   = PP_4ADDR_256BYTE_MICROM ;
        if(strstr(Chip_Info.TypeName,"N25Q512") != NULL)
            mcode_SegmentErase  = 0xD4;
        else
            mcode_SegmentErase  = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_MACRONIX_MX25Lxxx_PP32) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read      = BULK_FAST_READ;
        mcode_Program   = PP_32BYTE ;
        mcode_SegmentErase  = 0xD8;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_WINBOND_W25Pxx_Large) != NULL)
    {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_Read    = BULK_4BYTE_FAST_READ;
        mcode_SegmentErase= SE;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0C;
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL)
    {
        mcode_RDSR = 0xD7;
        mcode_WRSR = 0;
        mcode_Read      = BULK_AT45xx_READ;
        mcode_Program   = PAGE_WRITE;
        mcode_SegmentErase  = 0;
        CHIP_INFO mem_id;
        SetPageSize(&mem_id, 0);
        Chip_Info.ChipSizeInByte=GetChipSize();
        Chip_Info.PageSizeInByte=GetPageSize();
    }
    else if(strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL)
    {
        mcode_RDSR = 0xD7;
        mcode_WRSR = 0;
        mcode_Read      = BULK_AT45xx_READ;
        mcode_Program   = PAGE_WRITE;
        mcode_SegmentErase  = 0;
        CHIP_INFO mem_id;
        SetPageSize(&mem_id, 0);
        Chip_Info.ChipSizeInByte=GetChipSize();
        Chip_Info.PageSizeInByte=GetPageSize();
    }
//	printf("Erase Code=%X\r\n",mcode_ChipErase);
//    printf("Read Code=%X\r\n",mcode_ReadCode);
//    printf("mcode_ProgramCode_4Adr=%X\r\n",mcode_ProgramCode_4Adr);
}

bool ProjectInitWithID(CHIP_INFO chipinfo,int Index) // by designated ID
{
    DownloadAddrRange.start=0;
    DownloadAddrRange.end=Chip_Info.ChipSizeInByte;
    InitLED(Index);
//    SetTargetFlash(g_StartupMode,Index); //for SF600 Freescale issue
    SetProgReadCommand();
	if(strcmp(g_parameter_vcc,"NO") == 0)
	{
		switch(Chip_Info.VoltageInMv)
		{
			case 1800:
				g_Vcc=vcc1_8V;
				break;
			case 2500:
				g_Vcc=vcc2_5V;
			case 3300:
			default:
				g_Vcc=vcc3_5V;
				break;
		}
	}

    return true;
}

bool ProjectInit(int Index) // by designated ID
{
    Chip_Info=GetFirstDetectionMatch(Index);
    if(Chip_Info.UniqueID==0)
        return false;
    return ProjectInitWithID(Chip_Info,Index);
}
