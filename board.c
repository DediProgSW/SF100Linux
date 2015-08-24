#include "Macro.h"
#include "usbdriver.h"
#include "board.h"
#define SerialFlash_FALSE   -1
#define SerialFlash_TRUE    1

volatile bool g_bIsSF600=false;
extern char g_board_type[8];
extern int g_firmversion;
extern unsigned int g_IO1Select;
extern unsigned int g_IO4Select;
extern Sleep(unsigned int ms);
void QueryBoard(int Index)
{
//	printf("QueryBoard\r\n");
    return;
    if(!Is_usbworking())
    {
	 printf("Do not find SFxx programmer!!\n");
        return ;
    }

    CNTRPIPE_RQ rq ;
    char vBuffer[16];

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
#if 0
const board_info&  GetBoardInfo(int Index)
	{
	   //  scott
		QueryBoard(Index);

		return m_info;
	}
#endif
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

#if 0
	bool ReadUID(int Index)
	{
        boost::mutex::scoped_lock l(mutex);
		if(! m_usb.is_open() )
		{
			return false;
		}
            if(m_info.board_type==L"SF600")
            {
                ReadOnBoardFlash(false,Index);
                m_info.dwUID=(DWORD)m_vUID[0]<<16 | (DWORD)m_vUID[1]<<8 | m_vUID[2];
                m_dwUID=m_info.dwUID;
                m_bManuID=m_vUID[3];
               return true;
            }

		CNTRPIPE_RQ rq ;
		vector<unsigned char> vBuffer(3) ;

		// read
		rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
		rq.Direction    = VENDOR_DIRECTION_IN ;
		rq.Request      = 0x7;
		rq.Value        = 0 ;
		rq.Index        = 0xEF00 ;
		rq.Length       = 3;

		if(! m_usb.InCtrlRequest(rq, vBuffer,Index))
		{
			return  false;
		}

		m_dwUID=(DWORD)vBuffer[0]<<16 | (DWORD)vBuffer[1]<<8 | vBuffer[2];
		return true;
	}


	bool WriteUID(int Index)
	{
        boost::mutex::scoped_lock l(mutex);
		if(! m_usb.is_open() )
		{
			return false;
		}
            if(m_info.board_type==L"SF600")
                {
                    return true;
                }

		CNTRPIPE_RQ rq ;

		// read
		rq.Function 	= URB_FUNCTION_VENDOR_OTHER ;
		rq.Direction	= VENDOR_DIRECTION_IN ;
		rq.Request	= 0x6;
		rq.Value		= (BYTE)(m_dwUID>>16) ;
		rq.Index		= 0xEF00 ;
		rq.Length		= 1;

		if(! m_usb.InCtrlRequest(rq, vector<unsigned char>(1),Index))
		{
			return	false;
		}

		rq.Index		= 0xEF01 ;
		rq.Value		= (BYTE)(m_dwUID>>8) ;

		if(! m_usb.InCtrlRequest(rq, vector<unsigned char>(1),Index))
		{
			return	false;
		}

		rq.Index		= 0xEF02 ;
		rq.Value		= (BYTE)(m_dwUID) ;

		if(! m_usb.InCtrlRequest(rq, vector<unsigned char>(1),Index))
		{
			return	false;
		}

		return true;
	}

	bool ReadManufacturerID(int Index)
	{
		if(! m_usb.is_open() )
		{
			return false;
		}
        if(m_info.board_type==L"SF600")
                {
                    ReadOnBoardFlash(false,Index);
                    m_bManuID=m_vUID[3];
                    return true;
                }

		CNTRPIPE_RQ rq ;
		vector<unsigned char> vBuffer(1) ;
		m_bManuID=0xFF;

		// read
		rq.Function     = URB_FUNCTION_VENDOR_OTHER ;
		rq.Direction    = VENDOR_DIRECTION_IN ;
		rq.Request      = 0x7;
		rq.Value        = 0 ;
		rq.Index        = 0xEF03;
		rq.Length       = 1;

		if(! m_usb.InCtrlRequest(rq, vBuffer,Index))
		{
			return  false;
		}
		m_bManuID=vBuffer[0];

		return true;
	}

	bool WriteManufacturerID(int Index)
	{
        boost::mutex::scoped_lock l(mutex);
		if(! m_usb.is_open() )
		{
			return false;
		}
        if(m_info.board_type==L"SF600")
                {
                    return true;
                }

		CNTRPIPE_RQ rq ;

		// read
		rq.Function 	= URB_FUNCTION_VENDOR_OTHER ;
		rq.Direction	= VENDOR_DIRECTION_IN ;
		rq.Request	= 0x6;
		rq.Value        = m_bManuID ;
		rq.Index        = 0xEF03 ;
		rq.Length       = 1;

		if(! m_usb.InCtrlRequest(rq, vector<unsigned char>(1),Index))
		{
			return	false;
		}

		return true;
	}
#endif
bool SetLEDProgBoard(size_t Color,int Index)
{
    if(! Is_usbworking())
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
    switch(Color)
    {
        case SITE_ERROR:
            return SetRedLEDOn(true,Index);
        case SITE_BUSY:
            return SetOrangeLEDOn(true,Index);
        case SITE_OK:
            return SetGreenLEDOn(true,Index);
        case SITE_NORMAL:
            return SetLEDProgBoard(0x0709,Index);// Turn off LED
    }
    return false;
}

bool SetCS(size_t value,int Index)
{
    if(! Is_usbworking())
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
    if(! Is_usbworking())
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
    if(! Is_usbworking() )
    {
        return false;
    }

    SetGreenLEDOn(true,0);

    Sleep(500);

    SetGreenLEDOn(false,0);

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
    memcpy(Data,vBuffer,4);
}
#if 0

    bool WriteSF600UID(int Index)
    {
        CNTRPIPE_RQ rq ;
         vector<unsigned char> vBuffer(16) ;

        // first control packet
        rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
        rq.Direction = VENDOR_DIRECTION_OUT ;
        rq.Request = WRITE_EEPROM ;
        rq.Value = 0;
        rq.Index = RFU ;
        rq.Length = vBuffer.size();

        vBuffer[0]=(BYTE)(m_dwUID>>16) ;
        vBuffer[1]=(BYTE)(m_dwUID>>8) ;
        vBuffer[2]=(BYTE)(m_dwUID) ;
        vBuffer[3]=m_bManuID ;

        if(! m_usb.OutCtrlRequest(rq, vBuffer,Index))
            return false ;

        return true ;
    }

    bool SetSF600HoldPin(bool boHigh, int Index)
    {
        if(! m_usb.is_open() )
		{
			return false;
		}

		// send request
		CNTRPIPE_RQ rq ;
		vector<unsigned char> vBuffer(0) ;
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = SET_HOLD;
		rq.Value = (boHigh? 0x0C:0x08);
		rq.Index = RFU ;
		rq.Length = 0 ;

		if(! m_usb.OutCtrlRequest(rq, vBuffer,Index))
		{
			return	false;
		}

		return true;
    }

    bool SF600HoldPinControlBySW(bool boEnable, int Index)
    {
        if(! m_usb.is_open() )
		{
			return false;
		}

		// send request
		CNTRPIPE_RQ rq ;
		vector<unsigned char> vBuffer(0) ;
		rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
		rq.Direction = VENDOR_DIRECTION_OUT ;
		rq.Request = SET_HOLD;
		rq.Value = (boEnable? 0x08:0x00);
		rq.Index = RFU ;
		rq.Length = 0 ;

		if(! m_usb.OutCtrlRequest(rq, vBuffer,Index))
		{
			return	false;
		}

		return true;
    }
#endif

bool LeaveSF600Standalone(bool Enable,int USBIndex)
{
    if(! Is_usbworking())
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

    if(OutCtrlRequest(&rq,&vBuffer,0,USBIndex)==SerialFlash_FALSE)
    {
        return false;
    }

    return true;
}

bool SetSPIClockValue(unsigned short v,int Index)
{
    if(!Is_usbworking() )
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
    if(! Is_usbworking() )
    {
        return false;
    }
    unsigned int dwUID=0;

    if(g_bIsSF600==true)
    {
        unsigned char vUID[4];
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

