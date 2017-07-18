#include <stdio.h>
#include <string.h>
#include <usb.h>
#include "Macro.h"
#include "usbdriver.h"
#include "project.h"

unsigned int        m_nbDeviceDetected = 0;
unsigned char       DevIndex = 0;
extern volatile bool g_bIsSF600;
extern int g_CurrentSeriase;
extern char g_board_type[8];
extern int g_firmversion;
extern void Sleep(unsigned int ms);
extern CHIP_INFO Chip_Info;
#define SerialFlash_FALSE   -1
#define SerialFlash_TRUE    1

static usb_dev_handle *dediprog_handle;

bool Is_NewUSBCommand(int Index)
{
	if(is_SF100nBoardVersionGreaterThan_5_5_0(Index) || is_SF600nBoardVersionGreaterThan_6_9_0(Index))
		return true;
	return false;
}
extern unsigned char GetFPGAVersion(int Index);

void usb_dev_init(void)
{
	usb_init();
	usb_find_busses();
	usb_find_devices();
}

void usb_db_init(void)
{
    int i;
    for(i=0; i<MAX_Dev_Index; i++) {
        usb_device_entry[i].valid = 0;
    }
    dediprog_handle = NULL;
}

void IsSF600(int Index)
{
    if( Index==-1 )  Index = DevIndex;

    CNTRPIPE_RQ rq ;
    unsigned char vBuffer[16] ;
	int fw[3];
    memset(vBuffer, '\0', 16);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = 0x08 ;
    rq.Value = 0 ;
    rq.Index = 0 ;
    rq.Length = 16;

    if(InCtrlRequest(&rq, vBuffer,16,Index)==SerialFlash_FALSE)
        return ;

    memcpy(g_board_type,&vBuffer[0],8);
	sscanf((char*)&vBuffer[8],"V:%d.%d.%d",&fw[0],&fw[1],&fw[2]);
	g_firmversion=((fw[0]<<16) | (fw[1]<<8) | fw[2]);
//	printf("g_firmversion=%x\r\n",g_firmversion);
//	printf("g_board_type=%s\r\n",g_board_type);
    if(strstr(g_board_type,"SF600") != NULL)
        g_bIsSF600=true;
    else
        g_bIsSF600=false;
    GetFPGAVersion(Index);
}


/* Might be useful for other USB devices as well. static for now. */
static int FindUSBDevice(void)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    int dev_index = 0;
    unsigned int vid = 0x0483;
    unsigned int pid = 0xdada;
    usb_db_init();
    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if ((dev->descriptor.idVendor == vid) && (dev->descriptor.idProduct == pid))
            {
                usb_device_entry[dev_index].usb_device_handler = *dev;
                usb_device_entry[dev_index].valid = 1;
                dev_index++;
                break;
            }
        }
    }
    return dev_index;
}


int OutCtrlRequest( CNTRPIPE_RQ *rq, unsigned char *buf, unsigned long buf_size ,int Index )
{
//    unsigned long  bytesWrite;
    int            requesttype;
    int            ret = 0;

    if( Index==-1 )
        Index = DevIndex;

    if( (rq->Function!=URB_FUNCTION_VENDOR_ENDPOINT) && (g_bIsSF600==true)) return true;

    requesttype = 0x00;

    if( rq->Direction==VENDOR_DIRECTION_IN )            requesttype |= 0x80;
    if( rq->Function==URB_FUNCTION_VENDOR_DEVICE )      requesttype |= 0x40;
    if( rq->Function==URB_FUNCTION_VENDOR_INTERFACE )   requesttype |= 0x41;
    if( rq->Function==URB_FUNCTION_VENDOR_ENDPOINT )    requesttype |= 0x42;
    if( rq->Function==URB_FUNCTION_VENDOR_OTHER )       requesttype |= 0x43;


    if (dediprog_handle ) {
        ret = usb_control_msg(dediprog_handle, requesttype, rq->Request, rq->Value, rq->Index, (char *)buf, buf_size, DEFAULT_TIMEOUT);
    }// else
      //  printf("no device");
    if(ret != buf_size)
    {
    #if 0
        printf("Control Pipe output error!\n");
        printf("rq->Direction=%X\n",rq->Direction);
        printf("rq->Function=%X\n",rq->Function);
        printf("rq->Request=%X\n",rq->Request);
        printf("rq->Value=%X\n",rq->Value);
        printf("rq->Index=%X\n",rq->Index);
        printf("rq->Length=%X\n",rq->Length);
        printf("buf_size=%X\n",buf_size);
        printf("buf[0]=%X\n",buf[0]);
        printf("g_bIsSF600=%d\n",g_bIsSF600);
    #endif
		//printf("Error=0x%x\r\n",usb_strerror());
        return -1;
    }
    return ret;
}


int InCtrlRequest( CNTRPIPE_RQ *rq, unsigned char *buf, unsigned long buf_size, int Index )
{
	//boost::mutex::scoped_lock l(mutex);

//	unsigned long   bytesRead;
    int             requesttype;
    int             ret = 0;

    if( (rq->Function!=URB_FUNCTION_VENDOR_ENDPOINT) && (g_bIsSF600==true)) return true;
    if( Index==-1 )
        Index = DevIndex;

    if(sizeof(buf) == 0)
        return 0;


    requesttype = 0x00;

    if( rq->Direction==VENDOR_DIRECTION_IN )            requesttype |= 0x80;
    if( rq->Function==URB_FUNCTION_VENDOR_DEVICE )      requesttype |= 0x40;
    if( rq->Function==URB_FUNCTION_VENDOR_INTERFACE )   requesttype |= 0x41;
    if( rq->Function==URB_FUNCTION_VENDOR_ENDPOINT )    requesttype |= 0x42;
    if( rq->Function==URB_FUNCTION_VENDOR_OTHER )       requesttype |= 0x43;

    if (dediprog_handle ) {
        ret = usb_control_msg(dediprog_handle, requesttype, rq->Request, rq->Value, rq->Index, (char *)buf, buf_size, DEFAULT_TIMEOUT);
    }// else
      //  printf("no device");

    if(ret != buf_size)
    {
//        printf("Control Pipe input error!\n");
        return -1;
    }

    return ret;
}


// part of USB driver , open usb pipes for data transfor
// should be called after usb successfully opens pipes.
int dediprog_start_appli(int Index)
{
	//IsSF600(Index);
    CNTRPIPE_RQ rq ;
	int ret;
    unsigned char vInstruction;

    // special instruction
    vInstruction = 0 ;

    rq.Function = URB_FUNCTION_VENDOR_OTHER ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = 0xb ;
    rq.Value = 0x00 ;
    rq.Index = 0x00 ;
    rq.Length = 0x01 ;

    ret = OutCtrlRequest(&rq, &vInstruction, 1, 0);

    return ret;
}

int dediprog_get_chipid(int Index)
{
	//IsSF600(Index);
    CNTRPIPE_RQ rq ;
	int ret;
    unsigned char vInstruction[3];

    memset(vInstruction, 0, 3);
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = 0x1 ;
	rq.Value = 0xff ;
	if(Is_NewUSBCommand(Index))
    	rq.Index = 0 ;
	else
    	rq.Index = 0x1 ;

    rq.Length = 0x1 ;

    vInstruction[0]= 0x9f;

    ret = OutCtrlRequest(&rq, vInstruction, 1, Index);


    // special instruction
    memset(vInstruction, 0, 3);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = 0x1 ;
    rq.Value = 0x0 ;
    rq.Index = 0x00 ;
    rq.Length = 0x03 ;

    ret = InCtrlRequest(&rq, vInstruction, 3, Index);
    return ret;
}

// return size read
// if fail , return -1
//long CUSB::BulkPipeRead(PBYTE pBuff, UINT sz, UINT timeOut) const
int BulkPipeRead(unsigned char *pBuff, unsigned int timeOut, int Index)
{
    int ret;
    if( Index==-1 )   Index = DevIndex;

    unsigned long cnRead = 512;
    ret = usb_bulk_read(dediprog_handle, 0x82, (char*)pBuff, cnRead, DEFAULT_TIMEOUT);
    cnRead = ret;
    return cnRead ;
}


//long CUSB::BulkPipeWrite(PBYTE pBuff, UINT sz, UINT timeOut) const
int BulkPipeWrite(unsigned char *pBuff, unsigned int size,unsigned int timeOut, int Index)
{
    int ret;
    int nWrite = 512 ;
    char pData[512];

    memset(pData, 0xFF, 512);   // fill buffer with 0xFF

    memcpy(pData, pBuff , size);

    if( Index==-1 )  Index = DevIndex;

    ret = usb_bulk_write(dediprog_handle, (g_bIsSF600==true)? 0x01:0x02,pData, nWrite, DEFAULT_TIMEOUT);
    nWrite = ret;
    return nWrite;
}


int dediprog_set_spi_voltage(int v,int Index)
{
    int ret = 0;
//    int voltage_selector;
    CNTRPIPE_RQ rq ;
//    unsigned char vBuffer[12];

    if(0 == v) Sleep(200);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = SET_VCC ;
	rq.Length = 0 ;

	if(Is_NewUSBCommand(Index))
	{
//		if(g_bIsSF600==true && (strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxB) != NULL || strstr(Chip_Info.Class,SUPPORT_ATMEL_45DBxxxD) != NULL))
//			v |= 0x8000;
//		printf("v==%x\n",v);
		rq.Value = v ;
		rq.Index = 0;
	}
	else
	{
		rq.Value = v ;
		rq.Index = 0x04 | g_CurrentSeriase; // ID detect mode
	}
	ret = OutCtrlRequest(&rq, NULL, 0, Index);
//	printf("ret=%x\r\n",ret);
		#if 0
    if(g_bIsSF600==true && v!=0)
    {
        unsigned char Vol[36]={
            0xD0,0x07,0x00,0x00,0x10,0x0E,0x00,0x00,0x10,0x0E,0x00,0x00,// VccL=2700mV, VccP=3600mV, VccBuffer=3600mV
            0xD0,0x07,0x00,0x00,0x8C,0x0A,0x00,0x00,0x8C,0x0A,0x00,0x00,// VccL=2000mV, VccP=2700mV, VccBuffer=2700mV
            0xB0,0x04,0x00,0x00,0xD0,0x07,0x00,0x00,0xD0,0x07,0x00,0x00// VccL=1200mV, VccP=2000mV, VccBuffer=2000mV
            };
        rq.Index = 0x04 | g_CurrentSeriase; // ID detect mode
        rq.Length = 12 ;
        if(v>=1800 && v<4500)
        {
            unsigned char vol[12]={
                ((float)v*0.8),(unsigned int)((float)v*0.8)>>8,(unsigned int)((float)v*0.8)>>16,(unsigned int)((float)v*0.8)>>24,
                v, v>>8, v>>16, v>>24,
                v, v>>8, v>>16, v>>24
                };
            int i=0;
            for(i; i<12; i++)
                vBuffer[i]=vol[i];
        }
        else
        {
            int loop=(v & 0x03)*12;
            int i=loop;
            for(i; i<(loop+12) ;i++)
                vBuffer[i-loop]=Vol[i];
        }
        ret = OutCtrlRequest(&rq, vBuffer, 12, Index);
    }
    else
    {
        rq.Index = 0x00 ;
        rq.Length = 0 ;
        ret = OutCtrlRequest(&rq, NULL, 0, Index);
    }

	if (ret != rq.Length) {
		printf("Command Set SPI Voltage 0x%x failed!\n", voltage_selector);
		return false;
	}
#endif
    if(0 != v) Sleep(200);
	return ret;
}


int dediprog_set_vpp_voltage(int volt)
{
	int ret;
	int voltage_selector;
    	CNTRPIPE_RQ rq ;


	switch (volt) {
	case 0:
		/* Admittedly this one is an assumption. */
		voltage_selector = 0x0;
		break;
	case 9:
		voltage_selector = 0x1;
		break;
	case 12:
		voltage_selector = 0x2;
		break;
	default:
		return 1;
	}

    	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    	rq.Direction = VENDOR_DIRECTION_OUT ;
    	rq.Request = 0x03 ;
    	rq.Value = voltage_selector ;
    	rq.Index = 0 ;
    	rq.Length = 0x0 ;

    	ret = OutCtrlRequest(&rq, NULL, 0, 0);

	if (ret == SerialFlash_FALSE) {
//		printf("Command Set VPP Voltage 0x%x failed!\n", voltage_selector);
		return 1;
	}
	return 0;
}

int dediprog_set_spi_clk(int khz)
{
    return 0;
	int ret;
	int hz_selector;
	//IsSF600(Index);
    	CNTRPIPE_RQ rq ;


	switch (khz) {
	case 24000:
		/* Admittedly this one is an assumption. */
		hz_selector = 0x0;
		break;
	case 8000:
		hz_selector = 0x1;
		break;
	case 12000:
		hz_selector = 0x2;
		break;
	case 3000:
		hz_selector = 0x3;
		break;
	case 2180:
		hz_selector = 0x4;
		break;
	case 1500:
		hz_selector = 0x5;
		break;
	case 750:
		hz_selector = 0x6;
		break;
	case 375:
		hz_selector = 0x7;
		break;
	default:
		printf("Unknown clk %i KHz! Aborting.\n", khz);
		return 1;
	}
//	printf("Setting SPI clk to %u.%03u MHz\n", khz / 1000,
//		 khz % 1000);

    	rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    	rq.Direction = VENDOR_DIRECTION_OUT ;
    	rq.Request = 0x61 ;
    	rq.Value = hz_selector ;
    	rq.Index = 0 ;
    	rq.Length = 0x0 ;

    	ret = OutCtrlRequest(&rq, NULL, 0, 0);

	if (ret == SerialFlash_FALSE) {
//		printf("Command Set SPI clk 0x%x failed!\n", hz_selector);
		return 1;
	}
	return 0;
}


int usb_driver_init(void)
{
    int device_cnt = 0;
    int ret;
    dediprog_handle=NULL;
    usb_dev_init();

    device_cnt = FindUSBDevice();
    if(usb_device_entry[device_cnt-1].valid==0)
    {
        printf("Error: Programmers are not connected.\n");
        return 0;
    }

    dediprog_handle = usb_open(&usb_device_entry[device_cnt-1].usb_device_handler);
    if(dediprog_handle==NULL)
    {
        printf("Error: Programmers are not connected.\n");
        return 0;
    }
    ret = usb_set_configuration(dediprog_handle, 1);
	if(ret)
	{
		printf("Error: Programmers USB set configuration: 0x%x.\n",ret);
		return 0;
	}
    ret = usb_claim_interface(dediprog_handle, 0);
	if(ret)
	{
		printf("Error: Programmers USB claim interface: 0x%x.\n",ret);
		return 0;
	}
    g_bIsSF600=false;

    IsSF600(0);
    dediprog_start_appli(0);
    IsSF600(0);

    return ((dediprog_handle != NULL)? 1:0);
}


int usb_driver_release(void)
{
	if(dediprog_handle==NULL) return 0;
    usb_release_interface(dediprog_handle, 0);
    usb_close (dediprog_handle);
    return 0;
}


#if 0 //Simon: unit test code
int usb_driver_test(void)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	unsigned int vid = 0x0483;
	unsigned int pid = 0xdada;
	int device_cnt = 0;
	int ret, i;
	char string[256];
	unsigned char readaddr[3];
	unsigned char writeaddr[3];
    usb_dev_handle *udev;
    usb_dev_init();

    device_cnt = FindUSBDevice();
//    printf("\ndevice_cnt =%d\n", device_cnt);
    dediprog_handle = usb_open(&usb_device_entry[device_cnt-1].usb_device_handler);
    ret = usb_set_configuration(dediprog_handle, 1);
    ret = usb_claim_interface(dediprog_handle, 0);
    dediprog_start_appli(0);

	dediprog_set_spi_voltage(3300);

    dediprog_get_chipid(0);

    dediprog_set_leds(PASS_ON | BUSY_ON | ERROR_ON);
    usb_close (dediprog_handle);

    return 0;
}
#endif


bool Is_usbworking(void)
{
    usleep(1000); // unknow reson
    return ((dediprog_handle != NULL)? true:false);
}
//long long flash_ReadId(boost::tuple<unsigned int /*RDID code*/, unsigned int/*inByteCount*/, unsigned int/*outByteCount*/> command,int Index)
long flash_ReadId(unsigned int read_id_code, unsigned int out_data_size ,int Index)
{
    // read status
//    if(! m_usb.is_open() )
//        return 0 ;

    // RDID is not decoded when in DP mode
    // so , first release from Deep Power Down mode or read signature
    // DoRDP() ;

    // send request
    CNTRPIPE_RQ rq ;
    unsigned char vInstruction[8];
    unsigned long rc = 0;
    int i;

    // first control packet
    vInstruction[0] = read_id_code;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = RESULT_IN ;
    	rq.Index = 0 ;
	}
	else
	{
	    rq.Value = RFU ;
    	rq.Index = RESULT_IN ;
	}
    rq.Length = 1;

    if(OutCtrlRequest( &rq, vInstruction, (unsigned long)1 , Index ) == SerialFlash_FALSE )
        return rc;  //OutCtrlRequest() return error

    // second control packet : fetch data
    memset(vInstruction, 0, sizeof(vInstruction));

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_IN ;
    rq.Request = TRANSCEIVE ;
	if(Is_NewUSBCommand(Index))
	{
		rq.Value = 0x01 ;
    	rq.Index = 0 ;
	}
	else
	{
	    rq.Value = CTRL_TIMEOUT ;
    	rq.Index = NO_REGISTER ;
	}
    rq.Length = out_data_size ;

    if(InCtrlRequest(&rq, vInstruction, (unsigned long)out_data_size, Index)==SerialFlash_FALSE)
        return rc;
    for(i=0; i<out_data_size; i++) {
        rc =  (rc<<8) +  vInstruction[i];
    }
    //printf("\n(Simon)Flish ID 0x%x (0x%x, %d)\n",rc, read_id_code, out_data_size);
    return rc;
}




