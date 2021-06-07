#pragma once

#ifndef DEDI_USB_DRIVER
#define DEDI_USB_DRIVER

#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define URB_FUNCTION_VENDOR_DEVICE 0x0017
#define URB_FUNCTION_VENDOR_INTERFACE 0x0018
#define URB_FUNCTION_VENDOR_ENDPOINT 0x0019
#define URB_FUNCTION_VENDOR_OTHER 0x0020

#define URB_FUNCTION_CLASS_DEVICE 0x001A
#define URB_FUNCTION_CLASS_INTERFACE 0x001B
#define URB_FUNCTION_CLASS_ENDPOINT 0x001C
#define URB_FUNCTION_CLASS_OTHER 0x001F

#define PIPE_RESET 1
#define ABORT_TRANSFER 0

#define VENDOR_DIRECTION_IN 1
#define VENDOR_DIRECTION_OUT 0

#define DEFAULT_TIMEOUT 30000

#define MAX_Dev_Index 16

bool Is_usbworking(int Index);

typedef struct usb_device_entry {
    libusb_device* usb_device;
    int valid;
} usb_device_entry_t;

typedef struct {
    unsigned short Function;
    unsigned long Direction;
    unsigned char Request;
    unsigned short Value;
    unsigned short Index;
    unsigned long Length;
} CNTRPIPE_RQ, *PCNTRPIPE_RQ;

/* Set/clear LEDs on dediprog */
#define PASS_ON (0 << 0)
#define PASS_OFF (1 << 0)
#define BUSY_ON (0 << 1)
#define BUSY_OFF (1 << 1)
#define ERROR_ON (0 << 2)
#define ERROR_OFF (1 << 2)

int usb_driver_init(void);
int get_usb_dev_cnt(void);
int usb_driver_release(void);

int OutCtrlRequest(CNTRPIPE_RQ* rq, unsigned char* buf, unsigned long buf_size, int Index);

int InCtrlRequest(CNTRPIPE_RQ* rq, unsigned char* buf, unsigned long buf_size, int Index);

int BulkPipeRead(unsigned char* pBuff, unsigned int timeOut, int Index);

int dediprog_set_spi_voltage(int millivolt, int Index);
int dediprog_set_vpp_voltage(int volt, int Index);

long flash_ReadId(unsigned int read_id_code, unsigned int out_data_size, int Index);

int BulkPipeWrite(unsigned char* pBuff, unsigned int size, unsigned int timeOut, int Index);

#endif //DEDI_USB_DRIVER
