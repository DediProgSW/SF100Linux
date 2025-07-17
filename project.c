#include "project.h"
#include "IntelHexFile.h"
#include "MotorolaFile.h"
#include "SerialFlash.h"
#include "board.h"
#include "dpcmd.h"
#include "usbdriver.h"
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include <pthread.h>
#define min(a, b) (a > b ? b : a)

unsigned char* pBufferForLastReadData[16] = { NULL };
unsigned char* pBufferforLoadedFile = NULL;
unsigned long g_ulFileSize = 0;
unsigned int g_uiFileChecksum = 0;
unsigned char g_BatchIndex = 2;
unsigned int CompleteCnt = 0;
unsigned short g_usFileBlockCount = 0;
// bool m_bOperationResult[16];
bool bAuto[16] = { false };
struct BadBlockTable g_BBT[16];
extern unsigned int g_ucFill;
extern int m_boEnReadQuadIO;
extern int m_boEnWriteQuadIO;
extern volatile bool g_bIsSF600[16];
extern volatile bool g_bIsSF700[16];
extern volatile bool g_bIsSF600PG2[16]; 
extern char g_board_type[8];
extern int g_firmversion;
extern char* g_parameter_vcc;
extern char* g_parameter_fw;
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
extern CHIP_INFO g_ChipInfo;
extern int g_StartupMode;
extern int g_CurrentSeriase;
extern struct CAddressRange DownloadAddrRange;
extern struct CAddressRange UploadAddrRange;
extern struct CAddressRange LockAddrrange;
extern bool g_is_operation_on_going;
extern bool g_is_operation_successful[16];
extern unsigned int g_Vcc;
extern unsigned int g_Vpp;
extern unsigned int g_uiAddr;
extern size_t g_uiLen;
extern bool g_bEnableVpp;
extern unsigned int g_uiDevNum;
extern char strTypeName[1024];
extern bool g_bIsNANDFlash;
extern bool g_bSpareAreaUseFile;
extern struct CNANDContext g_NANDContext;
extern bool g_bNandInternalECC;
extern bool g_iNandBadBlockManagement;
extern int g_iNandBlockIndex;
extern int g_iNandBlockCount;
extern char* g_parameter_file_offset;
#if 0
extern int FlashIdentifier(CHIP_INFO* Chip_Info, int search_all, int Index);
#else
extern int FlashIdentifier(CHIP_INFO* Chip_Info, int search_all, int Index);
#endif

pthread_mutex_t g_count_mutex;
#define FIRMWARE_VERSION(x, y, z) ((x << 16) | (y << 8) | z)

enum FILE_FORMAT {
    BIN,
    HEX,
    S19,
};

static unsigned int crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

void TurnONVpp(int Index)
{
    if (g_bEnableVpp == true)
        dediprog_set_vpp_voltage(g_ChipInfo.VppSupport, Index);
}

void TurnOFFVpp(int Index)
{
    if (g_bEnableVpp == true)
        dediprog_set_vpp_voltage(0, Index);
}

void TurnONVcc(int Index)
{
    dediprog_set_spi_voltage(g_Vcc, Index);
}

void TurnOFFVcc(int Index)
{
    dediprog_set_spi_voltage(0, Index);
}

unsigned int CRC32(unsigned char* v, unsigned long size)
{
    unsigned int dwCrc32 = 0xFFFFFFFF;
    unsigned long i;
    for (i = 0; i < size; i++)
        dwCrc32 = ((dwCrc32) >> 8) ^ crc32_tab[(v[i]) ^ ((dwCrc32)&0x000000FF)];
    // checksum += v[i];

    dwCrc32 = ~dwCrc32;
    return dwCrc32; // & 0xFFFF;
}

int ReadBINFileForNAND(const char* filename)
{
	FILE* pFile;//, *pFileFroWrite;
	DWORD lSize, file_size;
	size_t result;
	size_t read_size = 0, temp_size;
	unsigned char *pTemp;
	unsigned char ucTemp[g_NANDContext.realPageSize];
	DWORD temp = 0;
	unsigned short tepmCount = 0;
	temp =strtol((char*)g_parameter_file_offset, NULL, 10);
	
	pFile = fopen(filename, "rb");
	//pFileFroWrite = fopen("read.bin", "wb");
	if (pFile == NULL) {
		printf("Open %s failed\n", filename);
		fputs("File error\n", stderr);
		return 0;
	}
	g_usFileBlockCount = 0;
	// obtain file size:
	fseek(pFile, 0, SEEK_END);
	file_size = ftell(pFile) - temp;
	lSize = g_NANDContext.realChipSize;
	tepmCount = (g_NANDContext.realChipSize/g_NANDContext.realBlockSize)+(((g_NANDContext.realChipSize%g_NANDContext.realBlockSize)==0)? 0:1);
	if(file_size > lSize)
	{
		printf("%s file size is bigger than chip size\n", filename);
		fclose(pFile);
		return false;
	}
	if(temp == 0)
		rewind(pFile);
	else
		fseek(pFile, temp, SEEK_SET);	
	// allocate memory to contain the whole file:
	if (pBufferforLoadedFile != NULL)
		free(pBufferforLoadedFile);
	
	pBufferforLoadedFile = (unsigned char*)malloc(lSize);
	
	if (pBufferforLoadedFile == NULL) {
		fputs("Memory error\n", stderr);
		fclose(pFile);
		return 0;
	}

	pTemp = pBufferforLoadedFile;

	memset(pBufferforLoadedFile, 0xFF, lSize);

	if(g_bSpareAreaUseFile == true)
	{
		read_size = g_NANDContext.realPageSize;
		g_usFileBlockCount = (file_size/g_NANDContext.realBlockSize)+(((file_size%g_NANDContext.realBlockSize)==0)? 0:1);
	}
	else 
	{
		read_size = g_ChipInfo.PageSizeInByte;
		g_usFileBlockCount = (file_size/g_ChipInfo.BlockSizeInByte)+(((file_size%g_ChipInfo.BlockSizeInByte)==0)? 0:1);		
	}

	if(g_iNandBlockCount != 0)
	{
		if(g_iNandBlockCount > tepmCount )
			g_usFileBlockCount = tepmCount;
		else
			g_usFileBlockCount = g_iNandBlockCount;
	}

	g_ulFileSize = file_size;
	
	do
	{
		memset(ucTemp, 0xFF, sizeof(ucTemp));
		temp_size = min(read_size,file_size);
		result = fread(ucTemp, 1, temp_size, pFile);
		if(result != temp_size)
			break;
		
		memcpy(pTemp, ucTemp, temp_size);
		pTemp += (g_NANDContext.realPageSize);
		file_size -= temp_size;
	}while(file_size>0);

	
	DownloadAddrRange.start = (g_iNandBlockIndex * g_NANDContext.realBlockSize);
	DownloadAddrRange.end = (DownloadAddrRange.start + (g_usFileBlockCount * g_NANDContext.realBlockSize));
	DownloadAddrRange.length = DownloadAddrRange.end-DownloadAddrRange.start;
	//fwrite(pBufferforLoadedFile , 1, lSize, pFileFroWrite);
	
	fclose(pFile);
	//fclose(pFileFroWrite);
	return 1;

}

int ReadBINFile(const char* filename, unsigned char* buf, unsigned long* size)
{
    FILE* pFile;
    long lSize;
    size_t result;

    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        printf("Open %s failed\n", filename);
        fputs("File error\n", stderr);
        return 0;
    }

    // obtain file size:
    fseek(pFile, 0, SEEK_END);
    lSize = ftell(pFile);

    rewind(pFile);
    // allocate memory to contain the whole file:
    if (pBufferforLoadedFile != NULL)
        free(pBufferforLoadedFile);

    pBufferforLoadedFile = (unsigned char*)malloc(lSize);

    if (pBufferforLoadedFile == NULL) {
        fputs("Memory error\n", stderr);
        return 0;
    }

    // copy the file into the buffer:
    result = fread(pBufferforLoadedFile, 1, lSize, pFile);
    if (result != lSize) {
        fputs("Reading error\n", stderr);
        return 0;
    }

    /* the whole file is now loaded in the memory buffer. */

    // terminate
    g_ulFileSize = lSize;

    fclose(pFile);
    return 1;
}

int WriteBINFile(const char* filename, unsigned char* buf, unsigned long size)
{
    unsigned long numbytes;
    FILE* image;

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
        printf("File %s could not be written completely.\n", filename);
        return 0;
    }
    return 1;
}

int GetFileFormatFromExt(const char* csPath)
{
    size_t length = strlen(csPath);
    char fmt[5] = { 0 };
    size_t i = length;
    size_t j = 0;

    while (csPath[i] != '.') {
        if (i == 0)
            return BIN;
        i--;
    }

    if (length - i > 5)
        return BIN;

    for (j = i; j < length; j++) {
        fmt[j - i] = toupper(csPath[j]);
    }

    if (strcmp(fmt, ".HEX") == 0)
        return HEX;
    else if (strcmp(fmt, ".S19") == 0 || strcmp(fmt, ".MOT") == 0)
        return S19;
    else
        return BIN;
}

bool ReadFile(const char* csPath, unsigned char* buffer, unsigned long* FileSize, unsigned char PaddingByte)
{
	if(g_bIsNANDFlash == true)
		return ReadBINFileForNAND(csPath);
    switch (GetFileFormatFromExt(csPath)) {
    case HEX:
        return HexFileToBin(csPath, buffer, FileSize, PaddingByte);
    case S19:
        return S19FileToBin(csPath, buffer, FileSize, PaddingByte);
    default:
        return ReadBINFile(csPath, buffer, FileSize);
    }
}

// write file
bool WriteFile(const char* csPath, unsigned char* buffer, unsigned int FileSize)
{
    switch (GetFileFormatFromExt(csPath)) {
    case HEX:
        return BinToHexFile(csPath, buffer, FileSize);
    case S19:
        return BinToS19File(csPath, buffer, FileSize);
    default:
        return WriteBINFile(csPath, buffer, FileSize);
    }
}

bool LoadFile(char* filename)
{
    bool result = true;
    unsigned long size;
    result &= ReadFile(filename, pBufferforLoadedFile, &size, g_ucFill);
    g_uiFileChecksum = CRC32(pBufferforLoadedFile, g_ulFileSize);
    return result;
}

bool IdentifyChipBeforeOperation(int Index)
{
    //printf("\n===>project.c ---- bool IdentifyChipBeforeOperation(%d)\n",Index);
    bool result = false;
    CHIP_INFO binfo;
    binfo.UniqueID = 0;
    int Found = 0;

    if (strstr(g_ChipInfo.Class, SUPPORT_FREESCALE_MCF) != NULL || strstr(g_ChipInfo.Class, SUPPORT_SILICONBLUE_iCE65) != NULL)
        return true;

    Found = FlashIdentifier(&binfo, 0, Index);

    if (Found && (binfo.UniqueID == g_ChipInfo.UniqueID || binfo.UniqueID == g_ChipInfo.JedecDeviceID))
        result = true;
    return result;
}

void PrepareProgramParameters(int Index)
{
    size_t len = 0;
    size_t addrStart = 0;

    addrStart = g_uiAddr;
    len = g_uiLen;

    size_t addrLeng = (0 == len) ? g_ulFileSize : len;
    DownloadAddrRange.start = addrStart;
    DownloadAddrRange.end = addrStart + addrLeng;
    DownloadAddrRange.length = addrLeng;
}

bool ValidateProgramParameters(int Index)
{
    PrepareProgramParameters(Index);

    /// special treatment for AT45DBxxxD
    if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) != NULL) {
        size_t pagesize = 264;
        size_t mask = (1 << 9) - 1;
        return (mask & DownloadAddrRange.start) <= (mask & pagesize);
    }

    /// if file size exceeds memory size, it's an error
    /// if user-specified length exceeds, just ignore
    if (DownloadAddrRange.start > g_ChipInfo.ChipSizeInByte - 1)
        return false;

    size_t size = DownloadAddrRange.length;
    if (size > g_ulFileSize && g_ucFill == 0xFF)
        return false;

    if (DownloadAddrRange.end > g_ChipInfo.ChipSizeInByte)
        return false;
    return true;
}

bool ProgramChip(int Index)
{  
    bool need_padding = (g_ucFill != 0xFF);
    unsigned char* vc;
    struct CAddressRange real_addr[16];

    real_addr[Index].start = (DownloadAddrRange.start & (~(0x200 - 1)));
    real_addr[Index].end = ((DownloadAddrRange.end + (0x200 - 1)) & (~(0x200 - 1)));
    real_addr[Index].length = real_addr[Index].end - real_addr[Index].start;

    vc = (unsigned char*)malloc(real_addr[Index].length);
    memset(vc, g_ucFill, real_addr[Index].length);

    if (need_padding == true) {
        memcpy(vc + (DownloadAddrRange.start & 0x1FF), pBufferforLoadedFile, min(g_ulFileSize, DownloadAddrRange.length));
    } else {
        memcpy(vc + (DownloadAddrRange.start & 0x1FF), pBufferforLoadedFile, DownloadAddrRange.length);
    }

    bool result = SerialFlash_rangeProgram(&real_addr[Index], vc, Index); 
    return result;
}

bool ProgramNANDChip(int Index)
{
	bool bIsBadBlock=false;
	DWORD usFileOffset = 0;
	bool result = true;
	//threadScanBB(Index);
	//printf("\nProgramming, please wait ...\n");
	TurnONVcc(Index);
	if(!SPINAND_ProtectBlock(false,Index))
		return false;
	if(!SPINAND_EnableInternalECC(g_bNandInternalECC,Index))
		return false;

	for(unsigned short i=0; i<g_usFileBlockCount; i++)
	{
		if(g_iNandBadBlockManagement == 0)
		{
			bIsBadBlock = false;
			for(unsigned short j=0; j<g_BBT[Index].cnt; j++)
			{
				if((i+g_iNandBlockIndex)==g_BBT[Index].bbt[j])
				{
					bIsBadBlock = true;
					break;
				}
			}
			if(bIsBadBlock == true)
				continue;
		}
		//printf("File offset=0x%08lX, ",usFileOffset);
		result &= SPINAND_BlockProgram((g_iNandBlockIndex+i)*g_NANDContext.realBlockSize, pBufferforLoadedFile+usFileOffset, mcode_Program, mcode_ProgramCode_4Adr, Index);
		usFileOffset += g_NANDContext.realBlockSize;
	}
	return result;
}

bool ReadChip(const struct CAddressRange range, int Index)
{ 
    bool result = true;
    unsigned char* vc;
    size_t addrStart = range.start;
    //    size_t addrLeng = range.length;
    struct CAddressRange addr;
    addr.start = range.start & (~(0x200 - 1));
    addr.end = (range.end + (0x200 - 1)) & (~(0x200 - 1));
    addr.length = addr.end - addr.start;

    vc = (unsigned char*)malloc(addr.length);

    result = SerialFlash_rangeRead(&addr, vc, Index);

    if (result) {
        if (pBufferForLastReadData[Index] != NULL) {
            free(pBufferForLastReadData[Index]);
        }
        unsigned int offset = (addrStart & 0x1FF);
        pBufferForLastReadData[Index] = (unsigned char*)malloc(range.length);
        memcpy(pBufferForLastReadData[Index], vc + offset, range.length);
        UploadAddrRange = range;
    }

    if (vc != NULL)
        free(vc);

    return result;
}

bool threadBlankCheck(int Index)
{
    bool result = false;
    struct CAddressRange Addr;
    Addr.start = 0;
    Addr.end = g_ChipInfo.ChipSizeInByte;

    SetIOMode(false, Index);

	if(g_bIsNANDFlash == true)
	{
		result = SPINAND_RangeBlankCheck(&Addr,Index);
		goto exit;
	}

    result = SerialFlash_rangeBlankCheck(&Addr, Index);

//    m_bOperationResult[Index] = result ? 1 : RES_BLANK;
#if 0

	if( !bAuto[Index] ) //not batch
	{ 
  
	    	CompleteCnt++;   
		/*if( CompleteCnt==get_usb_dev_cnt())
		{ 
                        g_is_operation_on_going = false;
			//SerialFlash_ClearCancelOperationFlag();
		}*/

	}
	else //batch
	{     
	   // SerialFlash_ClearCancelOperationFlag();
 
	}
#endif
exit:
    g_is_operation_successful[Index] = result;
    return result;
}

bool threadEraseWholeChip(int Index)
{
    bool result = false;

    //	power::CAutoVccPower autopowerVcc(m_usb, m_context.power.vcc,Index);
    //	power::CAutoVppPower autopowerVpp(m_usb, SupportedVpp(),Index);
	if(g_bIsNANDFlash == true)
	{
		//threadScanBB(Index);
		result = SPINAND_chipErase(Index);
		goto exit;
	}
	
    if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL || strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL)
        result = SerialFlash_DieErase(Index);
    else
        result = SerialFlash_chipErase(Index);

        // Log(result ? L"A whole chip erased" : L"Error: Failed to erase a whole chip");

        // m_bOperationResult[Index] = result ? 1 : RES_ERASE;
#if 0
	if( !bAuto[Index] ) //not batch
	{ 
  
	    	CompleteCnt++;   
		 if( CompleteCnt==get_usb_dev_cnt())
		{ 
			g_is_operation_on_going=false;
 
			SerialFlash_ClearCancelOperationFlag();
		}
	}
	else
	{ 
		SerialFlash_ClearCancelOperationFlag();
	}
#endif
exit:
    g_is_operation_successful[Index] = result;
    return result;
}

bool threadReadRangeChip(struct CAddressRange range, int Index)
{
    bool result = true;
    struct CAddressRange AddrRound;

    SetIOMode(false, Index);

    AddrRound.start = (range.start & (~(0x1FF)));
    AddrRound.end = ((range.end + 0x1FF) & (~(0x1FF)));
    AddrRound.length = AddrRound.end - AddrRound.start;

    unsigned char* pBuffer = malloc(AddrRound.length);

    if (pBufferForLastReadData[Index] == NULL)
        pBufferForLastReadData[Index] = malloc(range.end - range.start);
    else {
        free(pBufferForLastReadData[Index]);
        pBufferForLastReadData[Index] = malloc(range.end - range.start);
    }
    if (pBufferForLastReadData[Index] == NULL) {
        printf("allocate memory fail.\n");
        free(pBuffer);
        return false;
    }

	if(g_bIsNANDFlash == true)
		result = SPINAND_RangeRead(&AddrRound, pBuffer, Index);
	else
	    result = SerialFlash_rangeRead(&AddrRound, pBuffer, Index);

    if (result) {
        UploadAddrRange.start = range.start;
        UploadAddrRange.end = range.end;
        UploadAddrRange.length = range.end - range.start;
        memcpy(pBufferForLastReadData[Index], pBuffer + (UploadAddrRange.start & 0x1FF), UploadAddrRange.length);
    }

    free(pBuffer);

    return result;
}

bool threadReadChip(int Index)
{
    bool result = false;
    struct CAddressRange Addr;
    Addr.start = 0;
    Addr.end = g_ChipInfo.ChipSizeInByte;
	if(g_bIsNANDFlash == true)
		Addr.end = g_NANDContext.realChipSize;	

    SetIOMode(false, Index);

    if (pBufferForLastReadData[Index] == NULL)
        pBufferForLastReadData[Index] = (unsigned char*)malloc(Addr.end);
    else {
        free(pBufferForLastReadData[Index]);
        pBufferForLastReadData[Index] = (unsigned char*)malloc(Addr.end);
    }
    if (pBufferForLastReadData[Index] == NULL) {
        printf("allocate memory fail.\n");
        return false;
    }

	if(g_bIsNANDFlash == true)
		result = SPINAND_RangeRead(&Addr, pBufferForLastReadData[Index], Index);
	else
	    result = SerialFlash_rangeRead(&Addr, pBufferForLastReadData[Index], Index);

    if (result) {
        UploadAddrRange.start = 0;
        UploadAddrRange.end = Addr.end;
        UploadAddrRange.length = Addr.end;
    }

    g_is_operation_successful[Index] = result;

    return result;
}
bool threadConfiguredReadChip(int Index)
{
    bool result = true;
    struct CAddressRange addr;
    addr.start = DownloadAddrRange.start;
    addr.length = DownloadAddrRange.end - DownloadAddrRange.start;

    if (0 == addr.length && g_ChipInfo.ChipSizeInByte > addr.start)
        addr.length = g_ChipInfo.ChipSizeInByte - addr.start;

    addr.end = addr.length + addr.start;
    if (addr.start >= g_ChipInfo.ChipSizeInByte) {
        result = false;
    } else if (0 == addr.start && (0 == addr.length)) {
        result = threadReadChip(Index);
    } else {
        result = threadReadRangeChip(addr, Index);
    }

    g_is_operation_successful[Index] = result;
    //    g_is_operation_on_going  = false;
    return result;
}

bool threadProgram(int Index)
{ 
    int pthread_mutex_init(pthread_mutex_t * restrict mutex, const pthread_mutexattr_t* restricattr);

    bool result = true;

    SetIOMode(true, Index);

    pthread_mutex_lock(&g_count_mutex);
    if (g_ulFileSize == 0) {
        result = false;
    }

    if (result && (!ValidateProgramParameters(Index))) {
        result = false;
    }
    pthread_mutex_unlock(&g_count_mutex);

    int pthread_mutex_destroy(pthread_mutex_t * mutex);

	if(result && g_bIsNANDFlash == true)
	{
		result = ProgramNANDChip(Index);
		goto exit;
	}

    if (result && ProgramChip(Index)) {
        result = true;
    } else {
        result = false;
    } 
 exit:
    g_is_operation_successful[Index] = result;
    return result;
}

bool threadCompareFileAndChip(int Index)
{
    //printf("\n===>project.c ---- bool threadCompareFileAndChip(%d)\n",Index);
    bool result = true;

    SetIOMode(false, Index);

    if (g_ulFileSize == 0)
        result = false;

    if (result && (!ValidateProgramParameters(Index)))
        result = false;
    // refresh vcc before verify
    TurnOFFVcc(Index);
    Sleep(100);
    TurnONVcc(Index);
    Sleep(100);

    if (IdentifyChipBeforeOperation(Index) == false)
        result = false;

    if (result) {
		if(g_bIsNANDFlash == true){
			
			DownloadAddrRange.start = (g_iNandBlockIndex * g_NANDContext.realBlockSize);
			DownloadAddrRange.end = (DownloadAddrRange.start + (g_usFileBlockCount * g_NANDContext.realBlockSize));
			DownloadAddrRange.length = DownloadAddrRange.end-DownloadAddrRange.start;
			//printf("ReadBINFileForNAND(), DownloadAddrRange.start=%ld, DownloadAddrRange.end=%ld, DownloadAddrRange.length=%ld\n", DownloadAddrRange.start, DownloadAddrRange.end,DownloadAddrRange.length);
			result = SPINAND_RangeVerify(&DownloadAddrRange,Index);
		}
		else{
	        ReadChip(DownloadAddrRange, Index);
        	size_t offset = min(DownloadAddrRange.length, g_ulFileSize); 
    	    unsigned int crcFile = CRC32(pBufferforLoadedFile, offset);
	        unsigned int crcChip = CRC32(pBufferForLastReadData[Index], offset);
 
        	result = (crcChip == crcFile); 
		}
    }
 
    g_is_operation_successful[Index] = result;

    return result;
}

bool threadScanBB(int Index)
{ 
	bool result = true;
	if (strstr(g_ChipInfo.ICType, "SPI_NAND") == NULL)
		return result;
		
	printf("\nScanning blocks...\n\n");
	g_BBT[Index].cnt=0;
	SetIOMode(false, Index);
	SetSPIClockDefault(Index);
	SPINAND_ScanBadBlock(g_BBT[Index].bbt,&g_BBT[Index].cnt,Index);
	if(g_BBT[Index].cnt > 0)
	{
		if(g_BBT[Index].cnt==1)
			printf("There is 1 bad block: %d\n", g_BBT[Index].bbt[0]);
		else
		{
			printf("There are %d bad blocks: ", g_BBT[Index].cnt);
			for(unsigned short i=0; i<g_BBT[Index].cnt; i++)
			{
				if((i%8) == 0)
					printf("\n");
				printf("%d, ",g_BBT[Index].bbt[i]);
			}
			printf("\n");
		}
		
	}
	SetSPIClock(Index);
    return result;
}

size_t GenerateDiff(uintptr_t* Addr, unsigned char* in1, unsigned long long size1, unsigned char* in2, unsigned long long size2, uintptr_t baseAddr, size_t step)
{
    size_t upper = min(size1, size2);
    size_t realAddr;
    uintptr_t i, j = 0;

    for (i = 0; i < upper; ++i) {
        if (in1[i] != in2[i]) {
            realAddr = i + baseAddr;
            Addr[j++] = (realAddr & (~(step - 1)));

            i += ((step - 1) | realAddr) - realAddr;
            if (i >= upper)
                break;
        }
    }
    return j;
}

size_t Condense(uintptr_t* out, unsigned char* vc, uintptr_t* addrs, size_t addrSize, uintptr_t baseAddr, size_t step)
{
    //    size_t* itr, itr_end;
    size_t i, j, outSize = 0;
    //    itr_end=addrs+addrSize;

    for (j = 0; j < addrSize; j++) {
        for (i = addrs[j] - baseAddr; i < (addrs[j] - baseAddr + step); i++) {
            if (vc[i] != 0xFF) {
                out[outSize] = addrs[j];
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
    struct CAddressRange down_with_lock_range;
    down_with_lock_range.start = DownloadAddrRange.start;
    down_with_lock_range.end = DownloadAddrRange.end;
    down_with_lock_range.length = down_with_lock_range.end - down_with_lock_range.start;
    DownloadAddrRange.length = down_with_lock_range.length;

    if (LockAddrrange.length > 0) {
        if (LockAddrrange.start < DownloadAddrRange.start)
            down_with_lock_range.start = LockAddrrange.start;
        if ((LockAddrrange.start + LockAddrrange.length) > DownloadAddrRange.end)
            down_with_lock_range.end = LockAddrrange.start + LockAddrrange.length;
    }
 
    struct CAddressRange effectiveRange; //(addr_round.SectionRound(down_with_lock_range));
    effectiveRange.start = down_with_lock_range.start & (~(g_ChipInfo.MaxErasableSegmentInByte - 1));
    effectiveRange.end = (down_with_lock_range.end + (g_ChipInfo.MaxErasableSegmentInByte - 1)) & (~(g_ChipInfo.MaxErasableSegmentInByte - 1));
    effectiveRange.length = effectiveRange.end - effectiveRange.start;
 	
     
    if (!threadReadRangeChip(effectiveRange, Index))
        return false;

    unsigned int offsetOfRealStartAddrOffset = 0;
    unsigned char* vc;
    unsigned char* vc2;

    vc = (unsigned char*)malloc(effectiveRange.length);
    vc2 = (unsigned char*)malloc(effectiveRange.length);

    uintptr_t* addrs = (size_t*)malloc(min(DownloadAddrRange.length, g_ulFileSize));
    size_t Leng = 0;
 
    memcpy(vc, pBufferForLastReadData[Index], effectiveRange.length); //memory data

    if (LockAddrrange.length > 0) { 
       offsetOfRealStartAddrOffset = LockAddrrange.start - effectiveRange.start;  
        memcpy(pBufferforLoadedFile + offsetOfRealStartAddrOffset, pBufferForLastReadData[Index] + offsetOfRealStartAddrOffset, LockAddrrange.length);
        Leng = GenerateDiff(addrs, vc, DownloadAddrRange.length, pBufferforLoadedFile, g_ulFileSize, DownloadAddrRange.start, g_ChipInfo.MaxErasableSegmentInByte);
    } else { 
        offsetOfRealStartAddrOffset = DownloadAddrRange.start - effectiveRange.start; 
        
Leng = GenerateDiff(addrs, vc + offsetOfRealStartAddrOffset, DownloadAddrRange.length, pBufferforLoadedFile, g_ulFileSize, DownloadAddrRange.start, g_ChipInfo.MaxErasableSegmentInByte);
    }
   
    if (Leng == 0) // speed optimisation 
        return true;
    else {
        uintptr_t* condensed_addr = (size_t*)malloc(min(DownloadAddrRange.length, g_ulFileSize));
        size_t condensed_size;
        condensed_size = Condense(condensed_addr, vc, addrs, Leng, effectiveRange.start, g_ChipInfo.MaxErasableSegmentInByte);
   
        if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Mxx_Large) != NULL)  
	    SerialFlash_batchErase_W25Mxx_Large(condensed_addr, condensed_size, Index);
        else  
	    SerialFlash_batchErase(condensed_addr, condensed_size, Index); 

        if (strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx) != NULL) {
            TurnOFFVcc(Index);
            Sleep(100);
            TurnONVcc(Index);
        }
 
        memcpy(vc + offsetOfRealStartAddrOffset, pBufferforLoadedFile, DownloadAddrRange.length);
 
        size_t i = 0;
        for (i = 0; i < Leng; i++) {
            size_t idx_in_vc = addrs[i] - effectiveRange.start;
            struct CAddressRange addr_range;
            addr_range.start = addrs[i];
            addr_range.end = addrs[i] + g_ChipInfo.MaxErasableSegmentInByte;
            addr_range.length = addr_range.end - addr_range.start; 
 
            if (SerialFlash_rangeProgram(&addr_range, vc + idx_in_vc, Index) == 0) { 
                free(vc);
                free(addrs);
                free(condensed_addr);
                return false;
            }
        }
        free(vc);
        free(vc2);
        free(addrs);
        free(condensed_addr);
        return true;
    }
    free(vc);
    free(vc2);
    free(addrs);
    return true;
}

bool RangeUpdateThruSectorErase(int Index)
{
    return BlazeUpdate(Index);
}

bool RangeUpdateThruChipErase(int Index)
{ 
    unsigned char* vc = (unsigned char*)malloc(g_ChipInfo.ChipSizeInByte);
    unsigned int i = 0;
    bool boIsBlank = true;
    struct CAddressRange addr;
    DownloadAddrRange.length = DownloadAddrRange.end - DownloadAddrRange.start;
    SetIOMode(false, Index);

    if (!threadReadChip(Index))
        return false;

    memcpy(vc, pBufferForLastReadData, g_ChipInfo.ChipSizeInByte);

    for (i = 0; i < g_ChipInfo.ChipSizeInByte; i++) {
        if (vc[i] != 0xFF) {
            boIsBlank = false;
            break;
        }
    }
    if (boIsBlank == false) {
        if (threadEraseWholeChip(Index) == false)
            return false;

        if (strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx) != NULL || strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxD) != NULL) {
            TurnOFFVcc(Index);
            Sleep(100);
            TurnONVcc(Index);
        }
    }

    memset(vc + DownloadAddrRange.start, g_ucFill, min(DownloadAddrRange.length, g_ChipInfo.ChipSizeInByte));
    memcpy(vc + DownloadAddrRange.start, pBufferforLoadedFile, min(g_ulFileSize, DownloadAddrRange.length));

    SetIOMode(true, Index);
    addr.start = 0;
    addr.end = g_ChipInfo.ChipSizeInByte;
    addr.length = g_ChipInfo.ChipSizeInByte;

 
    return SerialFlash_rangeProgram(&addr, vc, Index);
}

bool RangeUpdate(int Index)
{
    bool fast_load = (g_ChipInfo.ChipSizeInByte > (1 << 16));

    return ((mcode_SegmentErase != 0) && fast_load)
        ? RangeUpdateThruSectorErase(Index)
        : RangeUpdateThruChipErase(Index);
}
bool ReplaceChipContentThruChipErase(int Index)
{
    if (!threadBlankCheck(Index)) // not blank
    {
        if (!threadEraseWholeChip(Index))
            return false;
    }
    return threadProgram(Index);
}

bool threadPredefinedNandBatchSequences(int Index)
{  
	bool result = true; 

	if(pBufferforLoadedFile == NULL || g_usFileBlockCount == 0 || g_ulFileSize == 0) result = false;
	if(result)
	{
		if(!threadScanBB(Index))
			result=false;
	}
	if( result )
	{ 
		switch(g_BatchIndex)
		{ 
			case 1:
				if(!threadBlankCheck(Index))//not blank
				{   
					if(!threadEraseWholeChip(Index))
						result=false; 
				}
				if(result == true)
					result=threadScanBB(Index);
				if(result == true)
					result = threadProgram( Index);
				break; 
			case 2: //special auto
				break;
			default:
				result = false;
				break;
		}
	}
	return result;
} 

bool threadPredefinedBatchSequences(int Index)
{ 
    bool result = true;

    if (g_ulFileSize == 0)
        result = false; 

    if (result && (!ValidateProgramParameters(Index)))
        result = false; 
 
    if (result) {
        switch (g_BatchIndex) {
        case 1: //-z 
            result = ReplaceChipContentThruChipErase(Index);
            break;
        case 2:
        default: 
            result = RangeUpdate(Index); 
            break;
        }
    }
    return result;
}

bool threadSpecialEraseWholeChip(int SiteIndex)
{ 
	bool result = true;
	if(SiteIndex == -1 || SiteIndex >= 128)
		return false;
	
	if(bAuto[SiteIndex] ==false)
		result = SPINand_SpecialErase(0, g_NANDContext.realChipSize/g_NANDContext.realBlockSize, SiteIndex);
	else
		result = SPINand_SpecialErase(g_iNandBlockIndex, g_iNandBlockCount, SiteIndex);				

	g_is_operation_successful[SiteIndex] = result;
    return result;
}

void threadRun(void* Type)
{ 
    THREAD_STRUCT* thread_data = (THREAD_STRUCT*)Type;
    OPERATION_TYPE opType = thread_data->type;
    int Index = thread_data->USBIndex;
    g_is_operation_successful[Index] = true;
    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);
    bool is_SF700_greater_than_4_0_0 = (g_firmversion > 0x40000);

    int dwUID = ReadUID(Index);
    if (g_uiAddr == 0 && g_uiLen == 0) {
		if(is_SF700_Or_SF600PG2(Index)){
	    	if (g_bIsSF700[Index] == true)
                printf("\nDevice %d (SF7%05d):", Index + 1, dwUID);
		    else if (g_bIsSF600PG2[Index] == true) 
                printf("\nDevice %d (S6B%05d):", Index + 1, dwUID);
		}
        else {
        	if ((dwUID / 600000) == 0)
            	printf("\nDevice %d (DP%06d):", Index + 1, dwUID);
	        else
    	        printf("\nDevice %d (SF%06d):", Index + 1, dwUID);
        }
    }

    if (opType == UPDATE_FIRMWARE) {
        g_is_operation_successful[Index] = UpdateFirmware(g_parameter_fw, Index);
        g_is_operation_on_going = false;
        free(Type);
        return;
    }
    if (opType == AUTO_UPDATE_FIRMWARE) {
        free(Type);
        return;
    }

    if (1) // is_good())
    { 
        TurnONVcc(Index);
        if (is_greater_than_5_0_0 || is_SF700_greater_than_4_0_0) {
            SetLEDOnOff(SITE_BUSY, Index);
        }

        if (IdentifyChipBeforeOperation(Index) == false) {
            printf("Warning: Failed to detect flash (%d).\n", Index);
            g_is_operation_successful[Index] = false;

        } else {
            switch (opType) // operations
            {
            case BLANKCHECK_WHOLE_CHIP:
                threadBlankCheck(Index);
                break;

            case ERASE_WHOLE_CHIP:
                threadEraseWholeChip(Index);
                break;

            case PROGRAM_CHIP:
                TurnONVpp(Index);
                threadProgram(Index);
                TurnOFFVpp(Index);
                break;

            case READ_WHOLE_CHIP:
                //                   m threadReadChip(Index);
                break;

            case READ_ANY_BY_PREFERENCE_CONFIGURATION:
                threadConfiguredReadChip(Index);
                break;

            case VERIFY_CONTENT:
                threadCompareFileAndChip(Index);
                break;

            case AUTO:
                TurnONVpp(Index);
                bAuto[Index] = true;
				if(g_bIsNANDFlash)
					threadPredefinedNandBatchSequences(Index);
				else
	                threadPredefinedBatchSequences(Index);
                TurnOFFVpp(Index);
                break;
				
			case NAND_SCAN_BB:
				TurnONVpp(Index);
				threadScanBB(Index);
				break;

			case NAND_SPECIAL_ERASE:
				threadSpecialEraseWholeChip(Index);
            default:
                break;
            }
        }
    }

    SetLEDOnOff(g_is_operation_successful[Index] ? SITE_OK : SITE_ERROR, Index);

    TurnOFFVcc(Index);
    free(Type);
    CompleteCnt++;
    if (g_uiDevNum != 0)
        g_is_operation_on_going = false;
    else if (CompleteCnt == get_usb_dev_cnt())
        g_is_operation_on_going = false;
}

void Run(OPERATION_TYPE type, int DevIndex)
{
    int dev_cnt = get_usb_dev_cnt();
    pthread_t id;
    THREAD_STRUCT* thread_data[16];
    CompleteCnt = 0;

    g_is_operation_on_going = true;

    if (DevIndex == 0) // mutiple
    {
        for (int i = 0; i < dev_cnt; i++) {
            thread_data[i] = (THREAD_STRUCT*)malloc(sizeof(THREAD_STRUCT));
            thread_data[i]->type = type;
            bAuto[i] = false;
            thread_data[i]->USBIndex = i;
            pthread_create(&id, NULL, (void*)threadRun, (void*)thread_data[i]);
        }
    } else if (DevIndex <= dev_cnt) {
        thread_data[DevIndex - 1] = (THREAD_STRUCT*)malloc(sizeof(THREAD_STRUCT));
        thread_data[DevIndex - 1]->type = type;
        bAuto[DevIndex - 1] = false;
        thread_data[DevIndex - 1]->USBIndex = DevIndex - 1; // 0;
        pthread_create(
            &id, NULL, (void*)threadRun, (void*)thread_data[DevIndex - 1]);
    } else
        printf("Error: Did not find the programmer.\n");
}

void SetIOMode(bool isProg, int Index)
{
    size_t IOValue = 0;
    m_boEnReadQuadIO = 0;
    m_boEnWriteQuadIO = 0;

    if ((g_bIsSF600[Index] == false) && (is_SF700_Or_SF600PG2(Index) == false))
        return;

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
	} else {
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

bool is_BoardVersionGreaterThan_5_0_0(int Index)
{
    if (g_firmversion < FIRMWARE_VERSION(5, 0, 0)) { 
        return false;
    }
    return true;
}

bool is_SF100nBoardVersionGreaterThan_5_5_0(int Index)
{ 
    if ((g_firmversion >= FIRMWARE_VERSION(5, 5, 0)) && strstr(g_board_type, "SF100") != NULL) 
        return true;
  
   return false;
}

bool is_SF600nBoardVersionGreaterThan_6_9_0(int Index)
{ 
    if (strstr(g_board_type, "SF600") != NULL) {
        if ((g_firmversion > FIRMWARE_VERSION(7, 0, 1)) || (g_firmversion == FIRMWARE_VERSION(6, 9, 0))) { 
            return true;
        }
    }
    return false;
}

bool is_SF100nBoardVersionGreaterThan_5_2_0(int Index)
{
    if ((g_firmversion >= FIRMWARE_VERSION(5, 2, 0)) && strstr(g_board_type, "SF100") != NULL)
        return true;
    return false;
}

bool is_SF600nBoardVersionGreaterThan_7_0_1n6_7_0(int Index)
{
    if (strstr(g_board_type, "SF600") != NULL) {
        if ((g_firmversion > FIRMWARE_VERSION(7, 0, 1)) || (g_firmversion == FIRMWARE_VERSION(6, 7, 0)))
            return true;
    }
    return false;
}

bool is_SF700_Or_SF600PG2(int Index)
{
    if (strstr(g_board_type, "SF700") != NULL) {
        return true;
    } 
    else if (strstr(g_board_type, "SF600PG2") != NULL) {
        return true;
    }

    return false;
} 
CHIP_INFO GetFirstDetectionMatch(char* TypeName, int Index)
{
    CHIP_INFO binfo;
    binfo.UniqueID = 0;
   // unsigned int g_Vcc_temp = 0;

    int Found = 0;
    int i = 0;
    int Loop = 3;
    if (strcmp(g_parameter_vcc, "NO") != 0) //g_parameter_vcc!=NO
    {
        Loop = 1;
    }

//    if (strlen(TypeName) != 0)
//        g_Vcc_temp = g_Vcc;

    for (i = 0; i < Loop; i++) {
        if (Found == 1) {
            if (strcmp(g_parameter_vcc, "NO") == 0)
            {
            	switch(binfo.VoltageInMv)
            	{
					case 1800:
						g_Vcc = vcc1_8V;
						break;
					case 2500:
						g_Vcc = vcc2_5V;
						break;
					case 3300:
						g_Vcc = vcc3_5V;
						break;
            	}
                
            }
            break;
        }
        if (Loop != 1)
            g_Vcc = vcc1_8V - i;

        TurnONVcc(Index);
        if (Is_usbworking(Index)) {
            if ((g_bIsSF600[Index] == true) || is_SF700_Or_SF600PG2(Index)) {
                int startmode;

                if (g_StartupMode == STARTUP_APPLI_SF_2)
                    SetTargetFlash(STARTUP_APPLI_SF_2, Index);
                else
                    SetTargetFlash(STARTUP_APPLI_SF_1, Index);

                startmode = g_StartupMode;
                Found = FlashIdentifier(&binfo, 0, Index);
                if (Found == 0) {
                    TurnOFFVcc(Index);
                    SetTargetFlash(STARTUP_APPLI_SF_SKT, Index);
                    g_CurrentSeriase = Seriase_25;
                    TurnONVcc(Index);
                    Found = FlashIdentifier(&binfo, 0, Index);

                    if (Found == 0) {
                        TurnOFFVcc(Index);
                        g_CurrentSeriase = Seriase_45;
                        TurnONVcc(Index);
                        Found = FlashIdentifier(&binfo, 0, Index);
                        if (Found == 0)
                            g_StartupMode = startmode;
                    }
                }
            } else {
                Found = FlashIdentifier(&binfo, 0, Index);
            }
        }
        TurnOFFVcc(Index);
    }
    if (Found == 0) {
        binfo.UniqueID = 0;
        binfo.TypeName[0] = '\0';
    }

    return binfo; //*TypeName;
}
 
// fail in case of 1) USB failure, 2) unrecognised ID.
void InitLED(int Index)
{
    bool is_greater_than_5_0_0 = is_BoardVersionGreaterThan_5_0_0(Index);
    bool is_SF700_greater_than_4_0_0 = (g_firmversion > 0x40000);

    if (is_greater_than_5_0_0 || is_SF700_greater_than_4_0_0)
        SetLEDOnOff(SITE_NORMAL, Index);
}

void SetProgReadCommand(int Index)
{
    static const unsigned int AT26DF041 = 0x1F4400;
    static const unsigned int AT26DF004 = 0x1F0400;
    //    static const unsigned int AT26DF081A = 0x1F4501
    mcode_WREN = WREN;
    mcode_RDSR = RDSR;
    mcode_WRSR = WRSR;
	mcode_WRDI = 0x04;
    mcode_ChipErase = CHIP_ERASE;
	mcode_SegmentErase = SE;

	if(strstr(g_ChipInfo.Class,"_Large") == NULL)
	{
    	mcode_Program = PAGE_PROGRAM;
	    mcode_Read = BULK_FAST_READ;
	    mcode_ProgramCode_4Adr = 0xFF;
    	mcode_ReadCode = 0xFF;
	}
	else
	{
		mcode_Program = PP_4ADR_256BYTE;
		mcode_Read = BULK_4BYTE_FAST_READ;
		mcode_ProgramCode_4Adr = (g_ChipInfo.ProgramCmd & 0x000000FF);// single program cmd
		mcode_ReadCode = (g_ChipInfo.ReadCmd & 0x000000FF); // single read cmd
	}

    if (strcmp(g_ChipInfo.Class, SUPPORT_SST_25xFxx) == 0) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read = BULK_NORM_READ;

        if (strstr(g_ChipInfo.TypeName, "B") != NULL || strstr(g_ChipInfo.TypeName, "W") != NULL)
            mcode_Program = AAI_2_BYTE;
        else
            mcode_Program = AAI_1_BYTE;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SST_25xFxxA) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = AAI_1_BYTE;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SST_25xFxxB) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = AAI_2_BYTE;
        mcode_SegmentErase = 0xD8;
        mcode_WRDI = WRDI;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SST_25xFxxC) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0x60;
        mcode_Read = BULK_FAST_READ;
        mcode_Program = PAGE_PROGRAM;
        mcode_SegmentErase = 0xD8;
        mcode_WREN = WREN;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT25FSxxx) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = PP_128BYTE;
        mcode_SegmentErase = 0xD8;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT25Fxxx) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = PP_128BYTE;
        mcode_SegmentErase = 0x52;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_AT26xxx) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_FAST_READ;
        mcode_SegmentErase = 0xD8;
        if (AT26DF041 == g_ChipInfo.UniqueID) {
            mcode_Program = PP_AT26DF041;
        } else if (AT26DF004 == g_ChipInfo.UniqueID) {
            mcode_Program = AAI_1_BYTE;
        } else {
            mcode_Program = PAGE_PROGRAM;
        }
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ESMT_F25Lxx) != NULL) {
        const unsigned int F25L04UA = 0x8C8C8C;
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_FAST_READ;
        mcode_SegmentErase = 0xD8;
        if (F25L04UA == g_ChipInfo.UniqueID)
            mcode_Program = AAI_1_BYTE;
        else
            mcode_Program = AAI_2_BYTE; // PAGE_PROGRAM;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_Alverstone) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_NORM_READ;
        mcode_Program = MODE_NUMONYX_PCM;
        mcode_SegmentErase = 0xD8;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_EON_EN25QHxx_Large) != NULL || strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_4BYTE_FAST_READ;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_4BYTE_FAST_READ;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x12;
        mcode_ReadCode = 0x0C;
    }else if (strstr(g_ChipInfo.Class, SUPPORT_SPANSION_S25FLxxS_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        if(strstr(g_ChipInfo.TypeName, "S25FS256T") != NULL)
            mcode_Read = BULK_NORM_READ;
        else
	    	mcode_Read = BULK_4BYTE_FAST_READ;
        if(g_bIsSF600[Index] == true || g_bIsSF700[Index] == true || g_bIsSF600PG2[Index] == true)
            mcode_Program = PP_PROGRAM_ANYSIZE_PAGESIZE;
        else
            mcode_Program = PP_4ADR_256BYTE;
        mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x12;
	if (strstr(g_ChipInfo.TypeName, "S25HL01GT") != NULL
		|| strstr(g_ChipInfo.TypeName, "S25HL512T") != NULL
		|| strstr(g_ChipInfo.TypeName, "S25HL256T") != NULL
		|| strstr(g_ChipInfo.TypeName, "S25FS256T") != NULL
		|| strstr(g_ChipInfo.TypeName, "S35HL256T") != NULL)
		mcode_ReadCode = 0x0B;	
	else
	    mcode_ReadCode = 0x0C;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_2Die) != NULL ) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0xC4;
        mcode_Program = PP_4ADDR_256BYTE_MICROM;
        if (strstr(g_ChipInfo.TypeName, "N25Q512") != NULL)
            mcode_SegmentErase = 0xD4;
        else
            mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        if (strstr(g_board_type, "SF100") != NULL) // is sf100
        {
            mcode_ReadCode = 0x03;
            mcode_Read = BULK_NORM_READ;
        } else {
            mcode_ReadCode = 0x0B;
            mcode_Read = BULK_4BYTE_FAST_READ_MICRON;
        }

    }  else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large_4Die) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = 0xC4;
        mcode_Program = PAGE_PROGRAM;
        mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x03;
        mcode_Read = BULK_NORM_READ;
        
    } else if (strstr(g_ChipInfo.Class, SUPPORT_NUMONYX_N25Qxxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_4BYTE_FAST_READ_MICRON;
        mcode_Program = PP_4ADDR_256BYTE_MICROM;
        if (strstr(g_ChipInfo.TypeName, "N25Q512") != NULL)
            mcode_SegmentErase = 0xD4;
        else
            mcode_SegmentErase = 0xD8;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;

    } else if (strstr(g_ChipInfo.Class, SUPPORT_MACRONIX_MX25Lxxx_PP32) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Read = BULK_FAST_READ;
        mcode_Program = PP_32BYTE;
        mcode_SegmentErase = 0xD8;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Pxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_Read = BULK_4BYTE_FAST_READ;
        mcode_SegmentErase = SE;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0C; 
    }  else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Mxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
		if (strstr(g_board_type, "SF100") != NULL) // is sf100
            mcode_Program = PP_4ADDR_256BYTE_12;
    	else
            mcode_Program = PP_4ADR_256BYTE;
        mcode_Read = BULK_4BYTE_FAST_READ_MICRON;
        mcode_SegmentErase = 0xDC;
        mcode_ProgramCode_4Adr = 0x12;
        mcode_ReadCode = 0x0C; 
    }else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Pxx) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Program = PAGE_PROGRAM;
        mcode_Read = BULK_FAST_READ;
        mcode_SegmentErase = SE;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B; 
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ST_M25Pxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_Read = BULK_4BYTE_FAST_READ;
        mcode_SegmentErase = SE;
        if (strstr(g_ChipInfo.TypeName, "GD25LB256E") != NULL)
            mcode_ProgramCode_4Adr = 0x12;
        else
            mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0C;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_WINBOND_W25Qxx_Large) != NULL) {
        mcode_RDSR = RDSR;
        mcode_WRSR = WRSR;
        mcode_ChipErase = CHIP_ERASE;
        mcode_Program = PP_4ADR_256BYTE;
        mcode_Read = BULK_4BYTE_FAST_READ;
        mcode_SegmentErase = SE;
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxB) != NULL) {
        mcode_RDSR = 0xD7;
        mcode_WRSR = 0;
        mcode_Read = BULK_AT45xx_READ;
        mcode_Program = PAGE_WRITE;
        mcode_SegmentErase = 0;
        CHIP_INFO mem_id;
        SetPageSize(&mem_id, 0);
        g_ChipInfo.ChipSizeInByte = GetChipSize();
        g_ChipInfo.PageSizeInByte = GetPageSize();
    } else if (strstr(g_ChipInfo.Class, SUPPORT_ATMEL_45DBxxxD) != NULL) {
        mcode_RDSR = 0xD7;
        mcode_WRSR = 0;
        mcode_Read = BULK_AT45xx_READ;
        mcode_Program = PAGE_WRITE;
        mcode_SegmentErase = 0;
        CHIP_INFO mem_id;
        SetPageSize(&mem_id, 0);
        g_ChipInfo.ChipSizeInByte = GetChipSize();
        g_ChipInfo.PageSizeInByte = GetPageSize();
    }
	else if(strstr(g_ChipInfo.Class, SUPPORT_GIGADEVICE_GD5F1GQ4xCx) != NULL){ 			
		mcode_Program = PAGE_PROGRAM; 
		mcode_Read = BULK_NORM_READ; 
        mcode_ProgramCode_4Adr = 0x02;
        mcode_ReadCode = 0x0B;
	} 
}

bool ProjectInitWithID(CHIP_INFO chipinfo, int Index) // by designated ID
{
    DownloadAddrRange.start = 0;
    DownloadAddrRange.end = g_ChipInfo.ChipSizeInByte;
    InitLED(Index);
    //    SetTargetFlash(g_StartupMode,Index); //for SF600 Freescale issue
    SetProgReadCommand(Index);
    if (strcmp(g_parameter_vcc, "NO") == 0) {
        switch (g_ChipInfo.VoltageInMv) {
        case 1800:
            g_Vcc = vcc1_8V;
            break;
        case 2500:
            g_Vcc = vcc2_5V;
        case 3300:
        default:
            g_Vcc = vcc3_5V;
            break;
        }
    }

    return true;
}

bool ProjectInit(int Index) // by designated ID
{
    g_ChipInfo = GetFirstDetectionMatch(strTypeName, Index);
    if (g_ChipInfo.UniqueID == 0) {
        return false;
    }
    return ProjectInitWithID(g_ChipInfo, Index);
}

#if 0
 bool analysisPartitionTable(bool SpareUseFile, char *strTblPath)
 {
 	wstring wstr;
 	Context::CFileContext::NAND_PTN_TBL ptn_tbl;
 	
 	wstr.assign(strTblPath.begin(), strTblPath.end());
 	m_context.file.nand_PartitionTablePath = wstr;
 	
 	boost::filesystem::wpath p(wstr);
 	wstring fmt(p.extension());
 	boost::to_upper(fmt);
 
 	if(fmt == L".CSV")
 		ptn_tbl = Context::CFileContext::PTN_CSV;
 	else if(fmt == L".MBN")
 		ptn_tbl = Context::CFileContext::PTN_MBN;
 	if(fmt == L".DEF")
 		ptn_tbl = Context::CFileContext::PTN_DEF;
 	if(fmt == L".DPMBN")
 		ptn_tbl = Context::CFileContext::PTN_DPMBN;
 	
 		
 	switch(ptn_tbl)
 	{
 		case Context::CFileContext::PTN_CSV://excel	 
 		{
 			CFile file;
 			CStringArray saData;
 			int nLen=0;
 			CString sFileName, str;
 			sFileName = m_context.file.nand_PartitionTablePath.c_str(); 
 			CStdioFile readFile; 
 			CString strLine; 
 			vector<CString> vecstrLine;
 			CFileException fileException; 
 			if(readFile.Open(sFileName, CFile::modeRead|CFile::shareDenyNone, &fileException)) 
 			{ 
 				m_context.file.nand_BlockIndex.resize(0);
 				m_context.file.nand_EndBlock.resize(0);
 				m_context.file.nand_BlockCount.resize(0);
 				m_context.file.nand_MaxBlockCount.resize(0);
 				m_context.file.nand_SpareAreaUseFile.resize(0);
 				
 						 
 				while (readFile.ReadString(strLine))
 				{
 					vecstrLine.push_back(strLine);
 					CString token;
 					int position =0; 
 	
 					m_context.file.nand_BlockIndex.push_back(_ttoi(vecstrLine[nLen].Tokenize(_T(";"),position))); 
 					m_context.file.nand_EndBlock.push_back(_ttoi(vecstrLine[nLen].Tokenize(_T(";"),position)));
 					m_context.file.nand_BlockCount.push_back(_ttoi(vecstrLine[nLen].Tokenize(_T(";"),position))); 
 					m_context.file.nand_MaxBlockCount.push_back(m_context.file.nand_BlockCount.back());
 					m_context.file.nand_SpareAreaUseFile.push_back(SpareUseFile);
 					nLen++; 
 				} 
 				m_context.file.nand_ptnTable_ImageCount=nLen;
 			}
 			else
 			{
 				return false;
 			}	  
 			readFile.Close(); 
 		}
 		break;
 		case Context::CFileContext::PTN_DEF://def
 		{
 			std::vector<DWORD> buffer;
 			bool bWrongFormat=false;
 			std::ifstream in(m_context.file.nand_PartitionTablePath.c_str(), std::ios::in|std::ios::binary) ;
 				in.seekg(0, in.end); 
 			unsigned int FileLen = in.tellg(); 
 			if((FileLen%16)!=0) 
 				return false; 
 	
 			char * buf = new char [FileLen];
 			in.seekg(0, in.beg);
 			in.read(buf,FileLen);
 		 
 			buffer.resize((FileLen)/sizeof(DWORD));
 			memcpy((DWORD*)&buffer[0],&buf[0],FileLen);  
 	 
 			delete [] buf;
 			in.close();    
 	
 			if((buffer[0]==0x554f5247)&&(buffer[1]==0x45442050)&&(buffer[2]==0x454e4946)&&(buffer[3]==0x00000032))//group define2
 			{ 
 				buffer.erase(buffer.begin(),buffer.begin()+4);
 				m_context.file.nand_BlockIndex.resize(0);
 				m_context.file.nand_EndBlock.resize(0);
 				m_context.file.nand_BlockCount.resize(0);
 				m_context.file.nand_MaxBlockCount.resize(0);
 				m_context.file.nand_SpareAreaUseFile.resize(0);
 				for(unsigned int i=0;i<((buffer.size()-4)/4);i++)
 				{ 
 					if(buffer[i*4+1]!=0xffffffff) 
 						m_context.file.nand_BlockIndex.push_back(buffer[i*4+1]); 
 					else 
 						return false;
 					if(buffer[i*4+2]!=0xffffffff)
 						m_context.file.nand_EndBlock.push_back(buffer[i*4+2]);
 					else
 						return false;
 					if(buffer[i*4+3]!=0xffffffff)
 					{
 						m_context.file.nand_BlockCount.push_back(buffer[i*4+3]);
 						m_context.file.nand_MaxBlockCount.push_back(m_context.file.nand_BlockCount.back());
 					}
 					else
 						return false; 
 					m_context.file.nand_ptnTable_ImageCount=(buffer.size()-4)/4;
 					m_context.file.nand_SpareAreaUseFile.push_back(SpareUseFile);
 				}
 				if(((buffer[buffer.size()-3])!=0xffffffff)
 						&&((buffer[buffer.size()-2])!=0xffffffff)
 						&&((buffer[buffer.size()-1])!=0xffffffff))
 				{
 					m_context.file.nand_BlockIndex.push_back(buffer[buffer.size()-3]); 
 					m_context.file.nand_EndBlock.push_back(buffer[buffer.size()-2]);
 					m_context.file.nand_BlockCount.push_back(buffer[buffer.size()-1]);
 					m_context.file.nand_MaxBlockCount.push_back(m_context.file.nand_BlockCount.back());
 					m_context.file.nand_ptnTable_ImageCount+=1;
 					m_context.file.nand_SpareAreaUseFile.push_back(SpareUseFile);
 				} 
 			}
 		}
 		break;
 		case Context::CFileContext::PTN_MBN://mbn
 		case Context::CFileContext::PTN_DPMBN://dpmbn
 		{
 			std::vector<DWORD> buffer;
 			std::ifstream in(m_context.file.nand_PartitionTablePath.c_str(), std::ios::in|std::ios::binary) ;
 					in.seekg(0, in.end); 
 			unsigned int FileLen = in.tellg(); 
 			if((FileLen%16)!=0) 
 				return false; 
 	
 			char * buf = new char [FileLen];
 			in.seekg(0, in.beg);
 			in.read(buf,FileLen);
 			buffer.resize(FileLen/sizeof(DWORD));
 			memcpy((DWORD*)&buffer[0],&buf[0],FileLen);  
 			delete [] buf;
 			in.close();   
 					
 			m_context.file.nand_BlockIndex.resize(0);
 			m_context.file.nand_EndBlock.resize(0);
 			m_context.file.nand_BlockCount.resize(0);
 			m_context.file.nand_MaxBlockCount.resize(0);
 			m_context.file.nand_FileOffset.clear();
 			m_context.file.nand_SpareAreaUseFile.resize(0);
 			
 			if(!((buffer[buffer.size()-1]==0xffffffff)
 				||(buffer[buffer.size()-2]==0xffffffff)
 				||(buffer[buffer.size()-3]==0xffffffff)))
 				return false; 
 			unsigned int i=0;
 			for( i=0;i<(buffer.size()-4)/4;i++)
 			{ 
 				if(buffer[i*4] == 0xffffffff &&  buffer[i*4+1] == 0xffffffff && buffer[i*4+2] == 0xffffffff && buffer[i*4+3] == 0xffffffff)
 					break;
 				if(buffer[i*4]!=0xffffffff) 
 					m_context.file.nand_BlockIndex.push_back(buffer[i*4]); 
 				else 
 					return false;
 				
 				if(buffer[i*4+1]!=0xffffffff)
 					m_context.file.nand_EndBlock.push_back(buffer[i*4+1]);
 				else
 					return false;
 				if(buffer[i*4+2]!=0xffffffff)
 				{
 					m_context.file.nand_BlockCount.push_back(buffer[i*4+2]);
 					m_context.file.nand_MaxBlockCount.push_back(m_context.file.nand_BlockCount.back());
 				}
 				else
 					return false; 
 
 				if(ptn_tbl == Context::CFileContext::PTN_DPMBN)
 					m_context.file.nand_FileOffset.push_back(
 					i==0? 0:m_context.file.nand_BlockIndex[i]-m_context.file.nand_BlockIndex[0]);
 				m_context.file.nand_SpareAreaUseFile.push_back(SpareUseFile);
 			}
 			m_context.file.nand_ptnTable_ImageCount=i;//(buffer.size()-4)/4; 
 		}
 		break; 
 	} 
 	get_context().file.nand_UsePartitionFile = true;
 	m_context.file.nand_FileOverBBcnt.resize(m_context.file.nand_ptnTable_ImageCount);
 	m_context.file.nand_FileOverBBcnt_BC.resize(m_context.file.nand_ptnTable_ImageCount);
 	return true;
 }
#endif
