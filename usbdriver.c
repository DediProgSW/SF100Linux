#include "usbdriver.h"
#include "FlashCommand.h"
#include "project.h"
#include <libusb-1.0/libusb.h>
#include <string.h>

unsigned int m_nbDeviceDetected = 0;
unsigned char DevIndex = 0;
extern volatile bool g_bIsSF600[16];
extern volatile bool g_bIsSF700[16];
extern volatile bool g_bIsSF600PG2[16];
extern int g_CurrentSeriase;
extern char g_board_type[8];
extern int g_firmversion;
extern CHIP_INFO Chip_Info;
extern unsigned int g_uiDevNum;
extern bool isSendFFsequence;

#define SerialFlash_FALSE -1
#define SerialFlash_TRUE 1

static int dev_index;
static usb_device_entry_t usb_device_entry[MAX_Dev_Index];
static libusb_device_handle* dediprog_handle[MAX_Dev_Index];

static libusb_context* ctx = NULL;
/*
 * Filter based on a bus:device combo, to make sure we *only* touch
 * that device
 */
unsigned g_usb_devnum = -1;
unsigned g_usb_busnum = -1;

bool Is_NewUSBCommand(int Index)
{
    if (is_SF100nBoardVersionGreaterThan_5_5_0(Index) || is_SF600nBoardVersionGreaterThan_6_9_0(Index) || is_SF700_Or_SF600PG2(Index)) { 
        return true;
    } 
    return false;
}
extern unsigned char GetFPGAVersion(int Index);

void usb_dev_init(void)
{
    int r;
    r = libusb_init(&ctx);
    if (r < 0) {
        printf("initialization failed!");
        return;
    }
#if LIBUSB_API_VERSION < 0x01000106
    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_INFO);
#else
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
#endif
}

void usb_db_init(void)
{
    int i;
    for (i = 0; i < MAX_Dev_Index; i++) {
        usb_device_entry[i].valid = 0;
        dediprog_handle[i] = NULL;
    }
}

void AssignSF600orSF700var(int Index)
{  
    if (Index == -1)
        Index = DevIndex; 
    g_bIsSF600[Index] = false;
    g_bIsSF700[Index] = false;
    g_bIsSF600PG2[Index] = false;

    CNTRPIPE_RQ rq;
    unsigned char vBuffer[32];
    int fw[3];
    memset(vBuffer, '\0', 32);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = 0x08;
    rq.Value = 0;
    rq.Index = 0;
    rq.Length = 32;
 
    if (InCtrlRequest(&rq, vBuffer, 32, Index) == SerialFlash_FALSE)
        return; 

    memcpy(g_board_type, &vBuffer[0], 8);
     
    //memcpy(g_firmversion,&vBuffer[10],8);
    sscanf((char*)&vBuffer[8], "V:%d.%d.%d", &fw[0], &fw[1], &fw[2]);
    g_firmversion = ((fw[0] << 16) | (fw[1] << 8) | fw[2]);

    if (strstr(g_board_type, "SF700") != NULL) 
        g_bIsSF700[Index] = true;  
    else if (strstr(g_board_type, "SF600") != NULL)
    { 
	if (strstr(g_board_type, "SF600PG2") != NULL)
	{  
  	    g_bIsSF600PG2[Index] = true;  
	    Sleep(2);
        }
	else
        {
            g_bIsSF600[Index] = true;  
        }
    }  


    GetFPGAVersion(Index);
}

int get_usb_dev_cnt(void)
{
    return dev_index;
}

/* Might be useful for other USB devices as well. static for now. */
static int FindUSBDevice(void)
{
    unsigned int vid = 0x0483;
    unsigned int pid = 0xdada;

    usb_db_init();

    ssize_t count;
    libusb_device** devs;
    count = libusb_get_device_list(ctx, &devs);
    for (size_t idx = 0; idx < count; ++idx) {
        libusb_device* device = devs[idx];
        struct libusb_device_descriptor desc = { 0 };
        int rc = libusb_get_device_descriptor(device, &desc);
        if (rc < 0) {
            printf("Error getting device descriptor\n");
            break;
        }
        if (desc.idVendor == vid && desc.idProduct == pid) {
            usb_device_entry[dev_index].usb_device = device;
            usb_device_entry[dev_index].valid = 1;
            dev_index++;
        }
    }
    printf(" \n"); 
    return dev_index;
}

int OutCtrlRequest(CNTRPIPE_RQ* rq, unsigned char* buf, unsigned long buf_size, int Index)
{ 
    int requesttype;
    int ret = 0;

    if (Index == -1)
        Index = DevIndex;

    if ((rq->Function != URB_FUNCTION_VENDOR_ENDPOINT) && ((g_bIsSF600[Index] == true) ||  is_SF700_Or_SF600PG2(Index) == true))
        return true;

    requesttype = 0x00;

    if (rq->Direction == VENDOR_DIRECTION_IN)
        requesttype |= 0x80;
    if (rq->Function == URB_FUNCTION_VENDOR_DEVICE)
        requesttype |= 0x40;
    if (rq->Function == URB_FUNCTION_VENDOR_INTERFACE)
        requesttype |= 0x41;
    if (rq->Function == URB_FUNCTION_VENDOR_ENDPOINT)
        requesttype |= 0x42;
    if (rq->Function == URB_FUNCTION_VENDOR_OTHER)
        requesttype |= 0x43;

    if (dediprog_handle[Index]) {
        ret = libusb_control_transfer(dediprog_handle[Index], requesttype, rq->Request, rq->Value, rq->Index, buf, buf_size, DEFAULT_TIMEOUT);
    } else {
        printf("no device");
    }
    if (ret != buf_size) { 
        printf("Error: %s\n", libusb_strerror(ret));
        return -1;
    }
    return ret;
}

int InCtrlRequest(CNTRPIPE_RQ* rq, unsigned char* buf, unsigned long buf_size, int Index)
{
    unsigned int requesttype;
    unsigned int ret = 0;

 
    if ((rq->Function != URB_FUNCTION_VENDOR_ENDPOINT) && ((g_bIsSF600[Index] == true) || is_SF700_Or_SF600PG2(Index) == true))
        return true;
 
 
    if (Index == -1)
        Index = DevIndex;

    if (sizeof(buf) == 0)
        return 0;

    requesttype = 0x00;

    if (rq->Direction == VENDOR_DIRECTION_IN)
        requesttype |= 0x80;
    if (rq->Function == URB_FUNCTION_VENDOR_DEVICE)
        requesttype |= 0x40;
    if (rq->Function == URB_FUNCTION_VENDOR_INTERFACE)
        requesttype |= 0x41;
    if (rq->Function == URB_FUNCTION_VENDOR_ENDPOINT)
        requesttype |= 0x42;
    if (rq->Function == URB_FUNCTION_VENDOR_OTHER)
        requesttype |= 0x43;

    if (dediprog_handle[Index]) {
        ret = libusb_control_transfer(dediprog_handle[Index], requesttype, rq->Request, rq->Value, rq->Index, buf, buf_size, DEFAULT_TIMEOUT);
    } else {
        printf("no device");
    }

    if (ret != 0 /*!= buf_size*/) 
{ 
	return ret;
 }
   else
    { 
#if 0

        printf("InCtrlRequest ---ret=%X\n",ret);
        printf("evInCtrlRequesty ---Control Pipe output error!\n");
        printf("InCtrlRequest ---rq->Direction=%lX\n",rq->Direction);
        printf("InCtrlRequest ---rq->Function=%X\n",rq->Function);
        printf("InCtrlRequest ---rq->Request=%X\n",rq->Request);
        printf("InCtrlRequest ---rq->Value=%X\n",rq->Value);
        printf("InCtrlRequest ---rq->Index=%X\n",rq->Index);
        printf("InCtrlRequest ---rq->Length=%lX\n",rq->Length);
        printf("InCtrlRequest ---buf_size=%lX\n",buf_size);
        printf("InCtrlRequest ---buf[0]=%X\n",buf[0]);
        //printf("g_bIsSF600=%d\n",g_bIsSF600);
#endif
        printf("Control Pipe input error!\n");
        return -1;
    }

    return ret;
}

// part of USB driver , open usb pipes for data transfor
// should be called after usb successfully opens pipes.
int dediprog_start_appli(int Index)
{  
   // if((g_bIsSF600[Index] != false) ||  is_SF700_Or_SF600PG2(Index)!=false)
//	return 1;

    CNTRPIPE_RQ rq;
    int ret;
    unsigned char vInstruction;

    // special instruction
    vInstruction = 0;

    rq.Function = URB_FUNCTION_VENDOR_OTHER;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = 0xb;
    rq.Value = 0x00;
    rq.Index = 0x00;
    rq.Length = 0x01;

    ret = OutCtrlRequest(&rq, &vInstruction, 1, Index);

    return ret;
}

int dediprog_get_chipid(int Index)
{
    //printf("\n===>usbdrive.c --- dediprog_get_chipid\n");
    CNTRPIPE_RQ rq;
    int ret;
    unsigned char vInstruction[3];

    memset(vInstruction, 0, 3);
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = 0x1;
    rq.Value = 0xff;
    if (Is_NewUSBCommand(Index))
        rq.Index = 0;
    else
        rq.Index = 0x1;

    rq.Length = 0x1;

    vInstruction[0] = 0x9f;

    ret = OutCtrlRequest(&rq, vInstruction, 1, Index);

    // special instruction
    memset(vInstruction, 0, 3);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = 0x1;
    rq.Value = 0x0;
    rq.Index = 0x00;
    rq.Length = 0x03;

    ret = InCtrlRequest(&rq, vInstruction, 3, Index);
    return ret;
}

// return size read
// if fail , return -1
//long CUSB::BulkPipeRead(PBYTE pBuff, UINT sz, UINT timeOut) const
int BulkPipeRead(unsigned char* pBuff, unsigned int timeOut, int Index)
{
    int ret, actual_length;
    if (Index == -1)
        Index = DevIndex;
 
    unsigned long cnRead = 512; 
    ret = libusb_bulk_transfer(dediprog_handle[Index], 2 | LIBUSB_ENDPOINT_IN, pBuff, cnRead, &actual_length, DEFAULT_TIMEOUT);  
    if (ret != 0) //libusb_bulk_transfer return false
        return 0; 
    return cnRead;
}

//long CUSB::BulkPipeWrite(PBYTE pBuff, UINT sz, UINT timeOut) const
int BulkPipeWrite(unsigned char* pBuff, unsigned int size, unsigned int timeOut, int Index)
{
    int ret, actual_length;
    int nWrite = 512;
    unsigned char pData[512];

    memset(pData, 0xFF, 512); // fill buffer with 0xFF

    memcpy(pData, pBuff, size);

    if (Index == -1)
        Index = DevIndex;

    ret = libusb_bulk_transfer(dediprog_handle[Index], ((g_bIsSF600[Index] == true) || is_SF700_Or_SF600PG2(Index) == true) ? 0x01 : 0x02, pData, nWrite, &actual_length, DEFAULT_TIMEOUT);
    nWrite = ret;
    return nWrite;
}

int dediprog_set_spi_voltage(int v, int Index)
{
    int ret = 0;
    //    int voltage_selector;
    CNTRPIPE_RQ rq;
    //    unsigned char vBuffer[12];

    if (0 == v)
        Sleep(200);

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = SET_VCC;
    rq.Length = 0;

    if (Is_NewUSBCommand(Index)) {
        rq.Value = v;
        rq.Index = 0;
    } else {
        rq.Value = v;
        rq.Index = 0x04 | g_CurrentSeriase; // ID detect mode
    }
    ret = OutCtrlRequest(&rq, NULL, 0, Index);

    if (0 != v) {
        Sleep(200);
        if (isSendFFsequence) {
            unsigned char v[4] = { 0xff, 0xff, 0xff, 0xff };
            FlashCommand_SendCommand_OutOnlyInstruction(v, 4, Index);
        }
    } 
    return ret;
}

int dediprog_set_vpp_voltage(int volt, int Index)
{ 
    int ret;
    int voltage_selector;
    CNTRPIPE_RQ rq;

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

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = 0x03;
    rq.Value = voltage_selector;
    rq.Index = 0;
    rq.Length = 0x0;

    ret = OutCtrlRequest(&rq, NULL, 0, Index);

    if (ret == SerialFlash_FALSE) {
        //		printf("Command Set VPP Voltage 0x%x failed!\n", voltage_selector);
        return 1;
    }
    return 0;
}

int dediprog_set_spi_clk(int khz, int Index)
{
    return 0;
    int ret;
    int hz_selector;
    CNTRPIPE_RQ rq;

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

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = 0x61;
    rq.Value = hz_selector;
    rq.Index = 0;
    rq.Length = 0x0;

    ret = OutCtrlRequest(&rq, NULL, 0, Index);

    if (ret == SerialFlash_FALSE) {
        //		printf("Command Set SPI clk 0x%x failed!\n", hz_selector);
        return 1;
    }
    return 0;
}

int usb_driver_init(void)
{
    //printf("\n===>usbdrive.c --- usb_driver_init\n");
    //  struct usb_bus *bus;
    // struct usb_device *dev;
    bool result = false;
    int device_cnt = 0;
    int ret;

    for (int i = 0; i < MAX_Dev_Index; i++) {
        dediprog_handle[i] = NULL;
    }
    usb_dev_init();

    device_cnt = FindUSBDevice();

    if (g_uiDevNum == 0) {
        for (int i = 0; i < device_cnt; i++) {
            if (usb_device_entry[i].valid == 0) {
                printf("Error: Programmers are not connected.\n");
                return 0;
            }

            ret = libusb_open(usb_device_entry[i].usb_device, &dediprog_handle[i]);
            if (dediprog_handle[i] == NULL) {
                printf("Error: Programmers are not connected.\n");
                return 0;
            }
            ret = libusb_set_configuration(dediprog_handle[i], 1);

            if (ret) {
                printf("Error: Programmers USB set configuration: 0x%x.\n", ret);
                return 0;
            }
            ret = libusb_claim_interface(dediprog_handle[i], 0);
            if (ret) {
                printf("Error: Programmers USB claim interface: 0x%x.\n", ret);
                return 0;
            }

            dediprog_start_appli(i);
            AssignSF600orSF700var(i); 
            result = (dediprog_handle[i] != NULL);
        }
    } else {
        if (usb_device_entry[g_uiDevNum - 1].valid == 0) {
            printf("Error: Programmers are not connected.\n");
            return 0;
        }

        ret = libusb_open(usb_device_entry[g_uiDevNum - 1].usb_device, &dediprog_handle[g_uiDevNum - 1]);
        if (dediprog_handle[g_uiDevNum - 1] == NULL) {
            printf("Error: Programmers are not connected.\n");
            return 0;
        }
        //printf("dediprog_handle[%d]=%p\n", g_uiDevNum - 1, dediprog_handle[g_uiDevNum - 1]);
        ret = libusb_set_configuration(dediprog_handle[g_uiDevNum - 1], 1);

        if (ret) {
            printf("Error: Programmers USB set configuration: 0x%x.\n", ret);
            return 0;
        }
        ret = libusb_claim_interface(dediprog_handle[g_uiDevNum - 1], 0);
        if (ret) {
            printf("Error: Programmers USB claim interface: 0x%x.\n", ret);
            return 0;
        }

        dediprog_start_appli(g_uiDevNum - 1);

        AssignSF600orSF700var(g_uiDevNum - 1);

        result = (dediprog_handle[g_uiDevNum - 1] != NULL);
    }

    return result; //((dediprog_handle[i] != NULL)? 1:0);
}

int usb_driver_release(void)
{
    for (int i = 0; i < dev_index; i++) {
        if (dediprog_handle[i] == NULL)
            return 0;
        libusb_release_interface(dediprog_handle[i], 0);
        libusb_close(dediprog_handle[i]);
    }
    return 0;
}
 
bool Is_usbworking(int Index)
{
    usleep(1000); // unknow reson
    return ((dediprog_handle[Index] != NULL) ? true : false);
}
//long long flash_ReadId(boost::tuple<unsigned int /*RDID code*/, unsigned int/*inByteCount*/, unsigned int/*outByteCount*/> command,int Index)
long flash_ReadId(unsigned int read_id_code, unsigned int out_data_size, int Index)
{
    // read status
    //    if(! m_usb.is_open() )
    //        return 0 ;

    // RDID is not decoded when in DP mode
    // so , first release from Deep Power Down mode or read signature
    // DoRDP() ;

    // send request
 

    CNTRPIPE_RQ rq;
    unsigned char vInstruction[8];
    unsigned long rc = 0;
    int i;

    // first control packet
    vInstruction[0] = read_id_code;
    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_OUT;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = RESULT_IN;
        rq.Index = 0;
    } else {
        rq.Value = RFU;
        rq.Index = RESULT_IN;
    }
    rq.Length = 1;

    if (OutCtrlRequest(&rq, vInstruction, (unsigned long)1, Index) == SerialFlash_FALSE)
        return rc; //OutCtrlRequest() return error

    // second control packet : fetch data
    memset(vInstruction, 0, sizeof(vInstruction));

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT;
    rq.Direction = VENDOR_DIRECTION_IN;
    rq.Request = TRANSCEIVE;
    if (Is_NewUSBCommand(Index)) {
        rq.Value = 0x01;
        rq.Index = 0;
    } else {
        rq.Value = CTRL_TIMEOUT;
        rq.Index = NO_REGISTER;
    }
    rq.Length = out_data_size;

    if (InCtrlRequest(&rq, vInstruction, (unsigned long)out_data_size, Index) == SerialFlash_FALSE)
        return rc;
    for (i = 0; i < out_data_size; i++) {
        rc = (rc << 8) + vInstruction[i];
    }
    // printf("\n(Simon)Flash ID 0x%x (0x%x, %d)\n",rc, read_id_code, out_data_size);
    return rc;
}
