#include "Macro.h"
#include "usbdriver.h"
#include "board.h"
#define SerialFlash_FALSE   -1
#define SerialFlash_TRUE    1
#define min(a,b) (a>b? b:a)

volatile bool g_bIsSF600=false;
extern char g_board_type[8];
extern int g_firmversion;
extern unsigned int g_IO1Select;
extern unsigned int g_IO4Select;
extern unsigned int g_Vcc;
extern void Sleep(unsigned int ms);
extern bool Is_NewUSBCommand(int Index);
//extern int is_SF100nBoardVersionGreaterThan_5_2_0(int Index);
//extern int is_SF600nBoardVersionGreaterThan_6_9_0(int Index);

void QueryBoard(int Index)
{
//	printf("QueryBoard\r\n");
    return;
    if(!Is_usbworking(Index))
    {
	 printf("Do not find SFxx programmer!!\n");
        return ;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer[16];

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = PROGINFO_REQUEST ;
    rq.Value = RFU ;
    rq.Index = 0x00 ;
    rq.Length = (unsigned long)16;
    memset(vBuffer, '\0', 16);

    if(InCtrlRequest(&rq, vBuffer,16,Index) == SerialFlash_FALSE)
    {
    	printf("send fail\r\n");
        return ;
    }

    memcpy(g_board_type,&vBuffer[0],8);
//    memcpy(g_firmversion,&vBuffer[10],8);
//		printf("g_board_type=%s\r\n",g_board_type);
//    if(strstr(g_board_type,"SF600") != NULL)
//        g_bIsSF600=true;
//    else
//        g_bIsSF600=false;
#if 0
    m_info.fpga_version=GetFPGAVersion(Index);
             vector<unsigned char> vBuffer1(3) ;

		rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
		rq.Direction    = VENDOR_DIRECTION_IN ;
		rq.Request      = 0x7;
		rq.Value        = 0 ;
		rq.Index        = 0xEF00 ;
		rq.Length       = 3;


		if(! m_usb.InCtrlRequest(rq, vBuffer1,Index))
		{
			return ;
		}

            if(m_info.board_type==L"SF600")
            {
                ReadOnBoardFlash(false,Index);
               m_info.dwUID=(DWORD)m_vUID[0]<<16 | (DWORD)m_vUID[1]<<8 | m_vUID[2];
            }
            else
		m_info.dwUID=(DWORD)vBuffer1[0]<<16 | (DWORD)vBuffer1[1]<<8 | vBuffer1[2];
#endif
}

unsigned char GetFPGAVersion(int Index)
{
    if(strstr(g_board_type,"SF600")==NULL) return -1;

    CNTRPIPE_RQ rq ;
    unsigned char vDataPack ;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = 0x1C ;
    rq.Value = RFU ;
    rq.Index = 0 ;
    rq.Length = 1 ;

    if(InCtrlRequest(&rq,&vDataPack,1,Index)==SerialFlash_FALSE){
        return -1 ;
    }

    return vDataPack;
}

bool SetIO(unsigned char ioState, int Index)
{
    CNTRPIPE_RQ rq ;
    unsigned char vDataPack ; // 1 bytes, in fact no needs

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_IO ;

	if(Is_NewUSBCommand(Index))
	{
		rq.Value = (ioState&0x0F)|0x70 ;
    	rq.Index = 0;
	}
	else
	{
	    rq.Value = ioState ;
    	rq.Index = 0x07 ;
	}
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq,&vDataPack,0,Index)==SerialFlash_FALSE){
        return false ;
    }

    return true ;
}

bool SetTargetFlash(unsigned char StartupMode,int Index)
{
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction ;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_TARGET_FLASH;
    rq.Value = StartupMode;
    rq.Index = 0 ;
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq,&vInstruction,0,Index)==SerialFlash_FALSE )
    {
        return false ;
    }

    return true;
}

bool SetLEDProgBoard(size_t Color,int Index)
{
    if(! Is_usbworking(Index))
    {
        return false;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer[16];

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_IO ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = (Color&0xFFF7) | (g_IO1Select<<1)|(g_IO4Select<<3);
		rq.Index=0;
	}
	else
	{
		rq.Value = 0x09 ;
    	rq.Index = Color>>8 ;//LED 0:ON  1:OFF   Bit0:Green  Bit1:Orange  Bit2:Red
	}
	rq.Length = 0 ;

    if(OutCtrlRequest(&rq,vBuffer,0,Index)==SerialFlash_FALSE)
    {
        return false;
    }

    return true;
}
bool SetGreenLEDOn(bool boOn,int Index)
{
    return SetLEDProgBoard(boOn?0x0609:0x0709,Index);
}

bool SetOrangeLEDOn(bool boOn, int Index)
{
    return SetLEDProgBoard(boOn?0x0509:0x0709,Index);
}

bool SetRedLEDOn(bool boOn, int Index)
{
    return SetLEDProgBoard(boOn?0x0309:0x0709,Index);
}

bool SetLEDOnOff(size_t Color,int Index)
{
    bool result=true;
    switch(Color)
    {
        case SITE_ERROR:
            result&=SetRedLEDOn(true,Index);
	    break;
        case SITE_BUSY:
            result&=SetOrangeLEDOn(true,Index);
	    break;
        case SITE_OK:
            result&=SetGreenLEDOn(true,Index);
	    break;
        case SITE_NORMAL:
            result&=SetLEDProgBoard(0x0709,Index);// Turn off LED
	    break;
    }  
    return result;
}

bool SetCS(size_t value,int Index)
{
    if(! Is_usbworking(Index))
    {
        return false;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer ;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_CS;
    rq.Value = value; // 12MHz
    rq.Index = RFU ;
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq, &vBuffer,0,Index)==SerialFlash_FALSE)
    {
        return	false;
    }

    return true;
}

bool SetIOModeToSF600(size_t value,int Index)
{
    if(! Is_usbworking(Index))
    {
        return false;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer ;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_IOMODE;
    rq.Value = value;
    rq.Index = RFU ;
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq, &vBuffer,0,Index)==SerialFlash_FALSE)
    {
        return	false;
    }

    return true;
}

bool BlinkProgBoard(bool boIsV5,int Index)
{
    if(! Is_usbworking(Index) )
    {
        return false;
    }

    SetGreenLEDOn(true,Index);

    Sleep(500);

    SetGreenLEDOn(false,Index);

    return true;
}

bool ReadOnBoardFlash(unsigned char* Data,bool ReadUID,int Index)
{
    CNTRPIPE_RQ rq ;
    unsigned char vBuffer[16] ;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = 0x05 ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = ReadUID;
    	rq.Index =  0;
	}
	else
	{
	   rq.Value = 0x00 ;
    	rq.Index =  ReadUID;
	}
    rq.Length = 16 ;

    if(InCtrlRequest(&rq, vBuffer,16,Index)==SerialFlash_FALSE)
    {
        return false;
    }
    memcpy(Data,vBuffer,16);
}

bool LeaveSF600Standalone(bool Enable,int Index)
{
    if(! Is_usbworking(Index))
    {
        return false;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer ;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_SA;
    rq.Value = Enable;
    rq.Index = RFU ;
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq,&vBuffer,0,Index)==SerialFlash_FALSE)
    {
        return false;
    }

    return true;
}

bool SetSPIClockValue(unsigned short v,int Index)
{
    if(!Is_usbworking(Index) )
        return false;

    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vBuffer;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_SPICLK;
    rq.Value = v ;
    rq.Index = RFU ;
    rq.Length = 0 ;

    if(OutCtrlRequest(&rq, &vBuffer,0,Index)==SerialFlash_FALSE)
        return  false;

    return true;
}

unsigned int ReadUID(int Index)
{
    if(! Is_usbworking(Index) )
    {
        return false;
    }
    unsigned int dwUID=0;

    if(g_bIsSF600==true)
    {
        unsigned char vUID[16];
        ReadOnBoardFlash(vUID,false,Index);
        dwUID=(unsigned int)vUID[0]<<16 | (unsigned int)vUID[1]<<8 | vUID[2];
        return dwUID;
    }

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer[3] ;

    // read
    rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
    rq.Direction    = VENDOR_DIRECTION_IN ;
    rq.Request      = 0x7;
    rq.Value        = 0 ;
    rq.Index        = 0xEF00 ;
    rq.Length       = 3;

    if( InCtrlRequest(&rq, vBuffer,3,Index)==SerialFlash_FALSE)
    {
        return  false;
    }

    dwUID=(unsigned int)vBuffer[0]<<16 | (unsigned int)vBuffer[1]<<8 | vBuffer[2];
    return dwUID;
}

bool SetSPIClockDefault(int Index)
{
	if(!Is_usbworking(Index) )
			return false;
	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vBuffer ;
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = SET_SPICLK;
	rq.Value = 0x02; // 12MHz
	rq.Index = 0 ;
	rq.Length = 0 ;

	if(OutCtrlRequest(&rq, &vBuffer,0,Index)==SerialFlash_FALSE)
	{
		printf("Set SPI clock error\r\n");
		return	false;
	}

	return true;
}

bool SetVpp4IAP(bool bOn,int Index)
{
	if(!Is_usbworking(Index) )
		return false;
	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vBuffer ;

	rq.Function = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction = VENDOR_DIRECTION_IN ;
	rq.Request = bOn ? 0x0 : 0x01 ;
	rq.Value = 0 ;
	rq.Index = 0 ;
	rq.Length = 0x01 ;

	if(OutCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
	{
		return  false;
	}

	Sleep(200);
	return true;
}

bool UnlockRASS(int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vBuffer ;

	rq.Function = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction = VENDOR_DIRECTION_IN ;
	rq.Request = 0x03 ;
	rq.Value = 0 ;
	rq.Index = 0 ;
	rq.Length = 0x01 ;

	if(OutCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
	{
		return  false;
	}
	return true;
}

unsigned char ReadManufacturerID(int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	if(g_bIsSF600==true)
	{
		unsigned char vUID[16];
		ReadOnBoardFlash(vUID,false,Index);
		return vUID[3];
	}

	CNTRPIPE_RQ rq ;
	unsigned char vBuffer;

	// read
	rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction    = VENDOR_DIRECTION_IN ;
	rq.Request      = 0x7;
	rq.Value        = 0 ;
	rq.Index        = 0xEF03;
	rq.Length       = 1;

	if(InCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
	{
		return  false;
	}
	return vBuffer;

}

bool EraseST7Sectors(bool bSect1,int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	// send request
	CNTRPIPE_RQ rq ;
	unsigned char vBuffer ;

	rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction    = VENDOR_DIRECTION_IN ;
	rq.Request  = bSect1 ? 0x04 : 0x05 ;
	rq.Value    = 0 ;
	rq.Index    = 0 ;
	rq.Length   = 0x01 ;

	if(OutCtrlRequest(&rq, &vBuffer,1,Index)==SerialFlash_FALSE)
	{
		return  false;
	}
	return true;
}

bool ProgramSectors(const char* sFilePath, bool bSect1,int Index)
{
	const unsigned int iSect1StartAddr  = 0xE000;
	const unsigned int iSect2StartAddr  = 0x8000;
	const unsigned int iSect1Size       = 0x1000;
	const unsigned int iSect2Size       = 0x6000;

	unsigned char* pBuffer=NULL;
	unsigned char* pTmp=NULL;
	FW_INFO fw_info;
	FILE * pFile;
	size_t lSize;
	unsigned char Data;

	unsigned int iStartAddr = bSect1 ? iSect1StartAddr : iSect2StartAddr;
	unsigned int Size,tmp;
	unsigned int uiIndex=0;

	// prog sectors
	CNTRPIPE_RQ rq ;

	pFile = fopen( sFilePath , "rb" );
	if (pFile==NULL)
	{
		printf("Can not open %s \n",sFilePath);
		return false;
	}

	fseek (pFile , 0 , SEEK_SET);
	lSize=fread ((unsigned char*)&fw_info,1,sizeof(FW_INFO),pFile);
	if(lSize != sizeof(FW_INFO))
		printf("Possible read length error %s\n", sFilePath);


	if(bSect1==true)
	{
		Size=iSect1Size;
		uiIndex=fw_info.FirstIndex;
	}
	else
	{
		Size=iSect2Size;
		uiIndex=fw_info.SecondIndex;
	}

	pBuffer = (unsigned char*)malloc(Size);
	memset(pBuffer,0xFF,Size);
	fseek (pFile , uiIndex, SEEK_SET);
	lSize = fread(pBuffer,1,Size,pFile);
	if(lSize != Size)
		printf("Possible read length error %s\n", sFilePath);

	fclose (pFile);
	pTmp=pBuffer;


	while(Size)
	{
		tmp=min(Size,0x100);
		/// receive page
		rq.Function 		= URB_FUNCTION_VENDOR_OTHER;
		rq.Direction		= VENDOR_DIRECTION_OUT;
		rq.Request			= 0x1;
		rq.Value				= 0;
		rq.Index				= 0;
		rq.Length 			= 0x100;	 /// plage size for ST7 Iap prog

		if(OutCtrlRequest(&rq,pTmp,tmp,Index)==SerialFlash_FALSE)
		{
			free(pBuffer);
			return false;
		}

		/// program page
		rq.Function 		= URB_FUNCTION_VENDOR_OTHER;
		rq.Direction		= VENDOR_DIRECTION_IN;
		rq.Request			= 0x8;
		rq.Value				= 0;
		rq.Index				= iStartAddr & 0xFFFF;	 ///< ConvLongToInt(lngST7address Mod &H10000)
		rq.Length 			= 0x1;
		iStartAddr		 += 0x100;

		if(OutCtrlRequest(&rq,&Data,1,Index	)==SerialFlash_FALSE)
		{
			free(pBuffer);
			return false;
		}
		Size-=tmp;
		pTmp+=tmp;
	}

	free(pBuffer);
	return true;
}

bool UpdateChkSum(int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	CNTRPIPE_RQ rq ;
	unsigned char vBuffer[2] ;

	// send to calculate checksum
	rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction    = VENDOR_DIRECTION_IN ;
	rq.Request      = 0x9;
	rq.Value        = 0 ;
	rq.Index        = 0 ;
	rq.Length       = 2 ;

	if(OutCtrlRequest(&rq, vBuffer,2,Index)==SerialFlash_FALSE)
	{
		return  false;
	}

	return true;
}

bool WriteUID(unsigned int dwUID,int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	if(g_bIsSF600)
		return true;

	CNTRPIPE_RQ rq;
	unsigned char Data;

	// read
	rq.Function 	= URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction	= VENDOR_DIRECTION_IN ;
	rq.Request	= 0x6;
	rq.Value		= (unsigned char)(dwUID>>16) ;
	rq.Index		= 0xEF00 ;
	rq.Length		= 1;

	if(InCtrlRequest(&rq, &Data,1,Index)==SerialFlash_FALSE)
	{
		return	false;
	}

	rq.Index		= 0xEF01 ;
	rq.Value		= (unsigned char)(dwUID>>8) ;

	if(InCtrlRequest(&rq, &Data,1,Index)==SerialFlash_FALSE)
	{
		return	false;
	}

	rq.Index		= 0xEF02 ;
	rq.Value		= (unsigned char)(dwUID) ;

	if(!InCtrlRequest(&rq, &Data,1,Index))
	{
		return	false;
	}

	return true;
}

bool WriteManufacturerID(unsigned char ManuID,int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	if(g_bIsSF600)
		return true;

	CNTRPIPE_RQ rq ;
	unsigned char Data;

	// read
	rq.Function 	= URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction	= VENDOR_DIRECTION_IN ;
	rq.Request	= 0x6;
	rq.Value        = ManuID ;
	rq.Index        = 0xEF03 ;
	rq.Length       = 1;

	if(InCtrlRequest(&rq, &Data,1,Index)==SerialFlash_FALSE)
	{
		return	false;
	}

	return true;
}

bool ReadMemOnST7(unsigned int iAddr,int Index)
{
	if(!Is_usbworking(Index) )
		return false;

	CNTRPIPE_RQ rq ;
	unsigned char vBuffer[2] ;

	// read
	rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
	rq.Direction    = VENDOR_DIRECTION_IN ;
	rq.Request      = 0x7;
	rq.Value        = 0 ;
	rq.Index        = iAddr & 0xFFFF ;
	rq.Length       = 2;

	if(OutCtrlRequest(&rq, vBuffer,2,Index)==SerialFlash_FALSE)
	{
		return  false;
	}

	return true;
}

bool UpdateSF600Firmware(const char* sFolder,int Index)
{
	bool boResult=true;
	unsigned char vUID[16];
	unsigned int dwUID;

	ReadOnBoardFlash(vUID,false,Index);
	dwUID=(unsigned int)vUID[0]<<16 | (unsigned int)vUID[1]<<8 | vUID[2];
	boResult &= UpdateSF600Flash(sFolder,Index);
	Sleep(200);
	boResult &= UpdateSF600Flash_FPGA(sFolder,Index);
	Sleep(1000);
	WriteSF600UID(dwUID,vUID[3],Index);
	return boResult;
}

bool WriteSF600UID(unsigned int dwUID, unsigned char ManuID,int Index)
{
	CNTRPIPE_RQ rq ;
	unsigned char vBuffer[16] ;

	// first control packet
	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
	rq.Direction = VENDOR_DIRECTION_OUT ;
	rq.Request = WRITE_EEPROM ;
	rq.Value = 0;
	rq.Index = 0 ;
	rq.Length = 16;

	vBuffer[0]=(unsigned char)(dwUID>>16) ;
	vBuffer[1]=(unsigned char)(dwUID>>8) ;
	vBuffer[2]=(unsigned char)(dwUID) ;
	vBuffer[3]=ManuID ;

	if(!OutCtrlRequest(&rq, vBuffer,16,Index))
		return false ;

	return true ;
}

void EncrypFirmware(unsigned char* vBuffer,unsigned int Size,int Index)
{
	unsigned char vUID[16];
	unsigned int i=0;
	ReadOnBoardFlash(vUID,true,Index);
	for(i=0; i<16; i++)
		vBuffer[i]=vBuffer[i]^vUID[i];

	for(; i<Size; i++)
		vBuffer[i]=vBuffer[i]^vBuffer[i-16];
}

bool UpdateSF600Flash(const char* sFilePath,int Index)
{
	CNTRPIPE_RQ rq ;
	unsigned char* pBuffer;
	int pagenum=0;
	unsigned int dwsize=0;
	FW_INFO fw_info;
	FILE * pFile;
	size_t lSize;
	int i=0;

	pFile = fopen( sFilePath , "rb" );
	if (pFile==NULL)
	{
		printf("Can not open %s \n",sFilePath);
		return false;
	}

	fseek (pFile , 0 , SEEK_SET);
	lSize = fread ((unsigned char*)&fw_info,1,sizeof(FW_INFO),pFile);
	if(lSize != sizeof(FW_INFO))
		printf("Possible read length error %s\n", sFilePath);

	pBuffer = (unsigned char*) malloc(fw_info.FirstSize);
	fseek (pFile , fw_info.FirstIndex, SEEK_SET);
	lSize = fread (pBuffer,1,fw_info.FirstSize,pFile);
	if(lSize != fw_info.FirstSize)
		printf("Possible read length error %s\n", sFilePath);

	fclose(pFile);

	EncrypFirmware(pBuffer,fw_info.FirstSize,Index);

	if(Is_NewUSBCommand(Index))
	{//for win8.1
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = 0x1A;
		rq.Value = 0;
		rq.Index = 0;
		rq.Length = 6 ;

		//evy 6.0.4.25 for win 8.1 with new firmware(7.0.2 up)
		unsigned char Package[6];
		Package[0]=pBuffer[0];
		Package[1]=pBuffer[1];
		Package[2]=(fw_info.FirstSize & 0xff);
		Package[3]=((fw_info.FirstSize >>8) & 0xff);
		Package[4]=((fw_info.FirstSize >> 16) & 0xff);
		Package[5]=((fw_info.FirstSize >> 24) & 0xff);

		if(!OutCtrlRequest(&rq, Package,6,Index))
		{
			free(pBuffer);
			return false ;
		}
	}
	else
	{
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = 0x1A ;
		rq.Value = (unsigned short)(fw_info.FirstSize & 0xffff) ;
		rq.Index = (unsigned short)((fw_info.FirstSize >> 16) & 0xffff) ;
		rq.Length = 0 ;


		if(!OutCtrlRequest(&rq, pBuffer,0,Index) )
		{
			free(pBuffer);
			return false ;
		}
	}

	pagenum=(fw_info.FirstSize>>9)+((fw_info.FirstSize%(1<<9))? 1:0);
	dwsize=fw_info.FirstSize;

	for( i=0; i<pagenum ; i++)
	{
		BulkPipeWrite((pBuffer+(i<<9)),min(512,dwsize),USB_TIMEOUT,Index);
		dwsize-=512;
	}

	free(pBuffer);
	return true;
}

bool UpdateSF600Flash_FPGA(const char* sFilePath,int Index)
{
	CNTRPIPE_RQ rq ;
	unsigned char* pBuffer;
	int pagenum=0;
	unsigned int dwsize=0;
	FW_INFO fw_info;
	FILE * pFile;
	size_t lSize;
	int i=0;

	pFile = fopen( sFilePath , "rb" );
	if (pFile==NULL)
	{
		printf("Can not open %s \n",sFilePath);
		return false;
	}

	fseek (pFile , 0 , SEEK_SET);
	lSize = fread ((unsigned char*)&fw_info,1,sizeof(FW_INFO),pFile);
	if(lSize != sizeof(FW_INFO))
		printf("Possible read length error %s\n", sFilePath);

	pBuffer = (unsigned char*) malloc(fw_info.SecondSize);
	fseek (pFile , fw_info.SecondIndex, SEEK_SET);
	lSize = fread (pBuffer,1,fw_info.SecondSize,pFile);
	if(lSize != fw_info.SecondSize)
		printf("Possible read length error %s\n", sFilePath);

	fclose(pFile);

	EncrypFirmware(pBuffer,fw_info.SecondSize,Index);

	if(Is_NewUSBCommand(Index))
	{//for win8.1
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = 0x1B;
		rq.Value = 0;//static_cast<unsigned short>(vBuffer.size() & 0xffff) ;
		rq.Index = 0;//static_cast<unsigned short>((vBuffer.size() >> 16) & 0xffff) ;
		rq.Length = 4 ;

			//evy 6.0.4.25 for win 8.1 with new firmware(7.0.2 up)
		unsigned char Package[4];
		Package[0]=(fw_info.SecondSize & 0xff);
		Package[1]=((fw_info.SecondSize>>8) & 0xff);
		Package[2]=((fw_info.SecondSize >> 16) & 0xff);
		Package[3]=((fw_info.SecondSize >> 24) & 0xff);

		if(!OutCtrlRequest(&rq, Package,4,Index) )
		{
			printf("Error: 1\r\n");
			free(pBuffer);
			return false ;
		}
	}
	else
	{
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = 0x1B;
		rq.Value = (unsigned short)(fw_info.SecondSize & 0xffff) ;
		rq.Index = (unsigned short)((fw_info.SecondSize >> 16) & 0xffff) ;
		rq.Length = 0 ;

		if(!OutCtrlRequest(&rq, pBuffer,0,Index) )
		{
			printf("Error: 2\r\n");
			free(pBuffer);
			return false ;
		}
	}

	pagenum=(fw_info.SecondSize>>9)+((fw_info.SecondSize%(1<<9))? 1:0);
	dwsize=fw_info.SecondSize;

	for( i=0; i<pagenum ; i++)
	{
		BulkPipeWrite(pBuffer+(i<<9),min(512,dwsize),USB_TIMEOUT,Index);
		dwsize-=512;
	}
	return true;
}

bool UpdateFirmware(const char* sFolder,int Index)
{
	bool bResult = true;
	unsigned int UID=0;
	unsigned char ManID=0;
	// read status
	if(g_bIsSF600==true)
		return UpdateSF600Firmware(sFolder,Index);

	dediprog_set_spi_voltage(g_Vcc,Index);

	if(g_firmversion > 0x040107) // 4.1.7
		bResult &= SetSPIClockDefault(Index);

	bResult &= SetVpp4IAP(true,Index);
	bResult &= UnlockRASS(Index);

	UID = ReadUID(Index);
	ManID = ReadManufacturerID(Index);

	// erase & program sector 1
	bResult &= EraseST7Sectors(true,Index);
	bResult &= ProgramSectors(sFolder, true,Index);
	// erase & program sector 2
	bResult &= EraseST7Sectors(false,Index);
	bResult &= ProgramSectors(sFolder, false,Index);

	// calculate and read back checksum
	bResult &= UpdateChkSum(Index);

	bResult &= WriteUID(UID,Index);
	bResult &= WriteManufacturerID(ManID,Index);
	bResult &= SetVpp4IAP(false,Index);

	// read back checksum
	bResult &= ReadMemOnST7(0xEFFE,Index);
	return bResult;
}

void SendFFSequence(int Index)
{
	unsigned char v[4]={0xff,0xff,0xff,0xff};
        FlashCommand_SendCommand_OutOnlyInstruction(v,4,Index);
}

