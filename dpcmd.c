#include "dpcmd.h"
#include "ChipInfoDb.h"
#include "FlashCommand.h"
#include "SerialFlash.h"
#include "board.h"
#include "project.h"
#include "usbdriver.h"
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#define min(a, b) (a > b ? b : a)

extern unsigned char* pBufferForLastReadData[16];
extern unsigned char* pBufferforLoadedFile;
extern unsigned int g_uiFileChecksum;
extern unsigned long g_ulFileSize;
extern volatile bool g_bIsSF600[16];
extern unsigned char g_BatchIndex;

unsigned int g_Vcc = vcc3_5V;
unsigned int g_Vpp = 9;
unsigned int g_uiAddr = 0;
size_t g_uiLen = 0;
unsigned int g_ucFill = 0xFF;
unsigned int g_ucTarget = 1;
unsigned int g_uiTimeout = 1800;
unsigned int g_ucSPIClock = clk_12M;
unsigned int g_uiBlink = 0;
unsigned int g_uiDevNum = 0;
unsigned int g_uiDeviceID = 0;
unsigned int g_IO1Select = 0;
unsigned int g_IO4Select = 1;

bool g_bEnableVpp = false;
int g_StartupMode = STARTUP_APPLI_SF_1;
bool g_bStatus = true;

CHIP_INFO Chip_Info;
char* l_opt_arg;
struct CAddressRange DownloadAddrRange;
struct CAddressRange UploadAddrRange;
struct CAddressRange LockAddrrange;

unsigned int g_uiLockLen = 0;
unsigned int g_uiLockStart = 0;

char* g_parameter_read = "\0";
char* g_parameter_program = "\0";
char* g_parameter_loadfile = "\0";
char* g_parameter_auto = "\0";
char* g_parameter_batch = "\0";
char* g_parameter_fsum = "\0";
char* g_parameter_raw = "\0";
char* g_parameter_raw_return = "\0";
char* g_parameter_addr = "0";
char* g_parameter_length = "\0";
char* g_parameter_fill = "FF";
char* g_parameter_type = "\0";
char* g_parameter_timeout = "300";
char* g_parameter_target = "1";
char* g_parameter_vcc = "NO";
//char* g_parameter_spi_clk="2";
char* g_parameter_blink = "0";
char* g_parameter_fw = "\0";
char g_LogPath[512] = { 0 };

unsigned long g_ucOperation;
struct memory_id g_ChipID;
char g_board_type[8];
int g_firmversion;
int g_CurrentSeriase = Seriase_25;
int m_isCanceled = 0;
int m_bProtectAfterWritenErase = 0;
int m_boEnReadQuadIO = 0;
int m_boEnWriteQuadIO = 0;
volatile bool g_is_operation_on_going = false;
bool g_is_operation_successful[16] = { false };
bool g_bDisplayTimer = true;
bool isSendFFsequence = false;
/* only print timing progress info if this many seconds ellapsed
 * before the last time we did */
static float g_tv_display_delta_s = 0.5;

//char* const short_options = "?Ldber:p:u:sf:I:R:a:l:vx:T:S:N:B:D:F:V:t:g:c:POik:";

static const char* msg_err_communication = "Error: USB communication error or configuration file missing.\n";
//static const char*    msg_err_openfile      = "Error: Failed to open file.\n";
static const char* msg_err_identifychip = "Error: chip not identified.\n";
static const char* msg_err_timeout_abortion = "\nError: Abort abnormally due to timeout.\n*** You might need to un-plug the USB device to recover the state. ***\n";
//static const char*    msg_err_lengthoverflow_abortion= "\nError: Length parameter exceeding file size, operation aborted\n";
//static const char*    msg_err_addroverflow_abortion  = "\nError: Exceeding chip size, operation aborted\n";
//static const char*    msg_warning_optionignore_al  = "\nWarning: either --addr or --length cannot be used with --auto, will be ignored\n";
static const char* msg_info_loading = "\nLoading file, please wait ...";
static const char* msg_info_checking = "\nChecking, please wait ...";
static const char* msg_info_erasing = "\nErasing, please wait ...";
static const char* msg_info_programming = "\nProgramming, please wait ...";
static const char* msg_info_reading = "\nReading, please wait ...";
static const char* msg_info_auto = "\nAuto Sequences, please wait ...";
static const char* msg_info_verifying = "\nVerifying, please wait ...";
//static const char*    msg_info_unknownoption= "\nError: illegal option.";
static const char* msg_info_chipblank = "\nThe chip is blank";
static const char* msg_info_chipnotblank = "\nThe chip is NOT blank";
static const char* msg_info_eraseOK = "\nErase OK";
static const char* msg_info_erasefail = "\nError: Erase Failed";
static const char* msg_info_progOK = "\nProgram OK";
static const char* msg_info_progfail = "\nError: Program Failed";
static const char* msg_info_readOK = "\nRead OK";
static const char* msg_info_readfail = "\nRead Failed";
static const char* msg_info_autoOK = "\nAutomatic program OK";
static const char* msg_info_autofail = "\nError: Automatic program Failed";
static const char* msg_info_verifyOK = "\nVerify OK";
static const char* msg_info_verifyfail = "\nError: Verify Failed";
static const char* msg_info_firmwareupdate = "\nUpdating firmware, please wait...";
static const char* msg_info_firmwareupdateOK = "\nUpdate firmware OK";
static const char* msg_info_firmwareupdatefail = "\nError: Update firmware Failed";

char* const short_options = "?Ldber:p:u:z:sf:I:R:a:l:vx:T:S:N:B:t:g:c:PO:ik:1:4:U:E:";

struct option long_options[] = {
    { "help", 0, NULL, '?' },
    { "list", 0, NULL, 'L' },
    { "detect", 0, NULL, 'd' },
    { "blank", 0, NULL, 'b' },
    { "erase", 0, NULL, 'e' },
    { "read", 1, NULL, 'r' },
    { "prog", 1, NULL, 'p' },
    { "auto", 1, NULL, 'u' },
    { "batch", 1, NULL, 'z' },
    { "sum", 0, NULL, 's' },
    { "fsum", 1, NULL, 'f' },
    { "raw-instruction", 1, NULL, 'I' },
    { "raw-require-return", 1, NULL, 'R' },
    { "addr", 1, NULL, 'a' },
    { "length", 1, NULL, 'l' },
    { "verify", 0, NULL, 'v' },
    { "fill", 1, NULL, 'x' },
    { "type", 1, NULL, 'T' },
    { "lock-start", 1, NULL, 'S' },
    { "lock-length", 1, NULL, 'N' },
    { "blink", 1, NULL, 'B' },
    { "device", 1, NULL, 'D' },
    //     { "fix-device",            1,   NULL,    'F'     },
    { "list-device-id", 1, NULL, 'V' },
    { "load-file", 1, NULL, 'F' },
    { "timeout", 1, NULL, 't' },
    { "target", 1, NULL, 'g' },
    { "vcc", 1, NULL, 'c' },
    { "vpp", 0, NULL, 'P' },
    { "log", 1, NULL, 'O' },
    { "silent", 0, NULL, 'i' },
    { "spi-clk", 1, NULL, 'k' },
    { "set-io1", 1, NULL, '1' },
    { "set-io4", 1, NULL, '4' },
    { "update-fw", 1, NULL, 'U' },
    { "display-delta", 1, NULL, 'E' },
    { 0, 0, 0, 0 },
};

int OpenUSB(void);
#if 0
int verbose = 1;

void print_endpoint(struct usb_endpoint_descriptor *endpoint)
{
  printf("      bEndpointAddress: %02xh\n", endpoint->bEndpointAddress);
  printf("      bmAttributes:     %02xh\n", endpoint->bmAttributes);
  printf("      wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
  printf("      bInterval:        %d\n", endpoint->bInterval);
  printf("      bRefresh:         %d\n", endpoint->bRefresh);
  printf("      bSynchAddress:    %d\n", endpoint->bSynchAddress);
}

void print_altsetting(struct usb_interface_descriptor *interface)
{
  int i;

  printf("    bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
  printf("    bAlternateSetting:  %d\n", interface->bAlternateSetting);
  printf("    bNumEndpoints:      %d\n", interface->bNumEndpoints);
  printf("    bInterfaceClass:    %d\n", interface->bInterfaceClass);
  printf("    bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
  printf("    bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
  printf("    iInterface:         %d\n", interface->iInterface);

  for (i = 0; i < interface->bNumEndpoints; i++)
    print_endpoint(&interface->endpoint[i]);
}

void print_interface(struct usb_interface *interface)
{
  int i;

  for (i = 0; i < interface->num_altsetting; i++)
    print_altsetting(&interface->altsetting[i]);
}

void print_configuration(struct usb_config_descriptor *config)
{
  int i;

  printf("  wTotalLength:         %d\n", config->wTotalLength);
  printf("  bNumInterfaces:       %d\n", config->bNumInterfaces);
  printf("  bConfigurationValue:  %d\n", config->bConfigurationValue);
  printf("  iConfiguration:       %d\n", config->iConfiguration);
  printf("  bmAttributes:         %02xh\n", config->bmAttributes);
  printf("  MaxPower:             %d\n", config->MaxPower);

  for (i = 0; i < config->bNumInterfaces; i++)
    print_interface(&config->interface[i]);
}

int print_device(struct usb_device *dev, int level)
{
  usb_dev_handle *udev;
  char description[256];
  char string[256];
  int ret, i;

  udev = usb_open(dev);
  if (udev) {
    if (dev->descriptor.iManufacturer) {
      ret = usb_get_string_simple(udev, dev->descriptor.iManufacturer, string, sizeof(string));
      if (ret > 0)
        snprintf(description, sizeof(description), "%s - ", string);
      else
        snprintf(description, sizeof(description), "%04X - ",
                 dev->descriptor.idVendor);
    } else
      snprintf(description, sizeof(description), "%04X - ",
               dev->descriptor.idVendor);

    if (dev->descriptor.iProduct) {
      ret = usb_get_string_simple(udev, dev->descriptor.iProduct, string, sizeof(string));
      if (ret > 0)
        snprintf(description + strlen(description), sizeof(description) -
                 strlen(description), "%s", string);
      else
        snprintf(description + strlen(description), sizeof(description) -
                 strlen(description), "%04X", dev->descriptor.idProduct);
    } else
      snprintf(description + strlen(description), sizeof(description) -
               strlen(description), "%04X", dev->descriptor.idProduct);

  } else
    snprintf(description, sizeof(description), "%04X - %04X",
             dev->descriptor.idVendor, dev->descriptor.idProduct);

  printf("%.*sDev #%d: %s\n", level * 2, "                    ", dev->devnum,
         description);

  if (udev && verbose) {
    if (dev->descriptor.iSerialNumber) {
      ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber, string, sizeof(string));
      if (ret > 0)
        printf("%.*s  - Serial Number: %s\n", level * 2,
               "                    ", string);
    }
  }

  if (udev)
    usb_close(udev);

  if (verbose) {
    if (!dev->config) {
      printf("  Couldn't retrieve descriptors\n");
      return 0;
    }

    for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
      print_configuration(&dev->config[i]);
  } else {
    for (i = 0; i < dev->num_children; i++)
      print_device(dev->children[i], level + 1);
  }

  return 0;
}

int Check(int argc, char *argv[])
{
  struct usb_bus *bus;

  if (argc > 1 && !strcmp(argv[1], "-v"))
    verbose = 1;

  usb_init();

  usb_find_busses();
  usb_find_devices();

  for (bus = usb_busses; bus; bus = bus->next) {
    if (bus->root_dev && !verbose) {
      printf("root device:");
      print_device(bus->root_dev, 0);
    }
    else {
      struct usb_device *dev;

      for (dev = bus->devices; dev; dev = dev->next) {
	printf("non-root device:");
        print_device(dev, 0);
      }
    }
  }

  return 0;
}

#endif
int Sequence()
{
    // *** the calling order in the following block must be kept as is ***
    bool boResult = true;
    boResult &= BlankCheck();
    if (boResult == false) {
        if (!(g_ucOperation & ERASE))
            return EXCODE_FAIL_BLANK;
    }
    if ((!((g_ucOperation & BLANK) && (boResult == true)) || (!(g_ucOperation & BLANK)))) {
        boResult = Erase();
        if (boResult == false)
            return EXCODE_FAIL_ERASE;
    }

    boResult &= Program();
    if (boResult == false)
        return EXCODE_FAIL_PROG;

    boResult &= Read();
    if (boResult == false)
        return EXCODE_FAIL_READ;

    boResult &= Auto();
    if (boResult == false)
        return EXCODE_FAIL_BATCH;

    boResult &= CalChecksum();
    if (boResult == true) {
        if (g_ucOperation & CSUM) {
            if (g_uiLen == 0) {
                if (g_uiDevNum == 0)
                    return CRC32(pBufferForLastReadData[0], Chip_Info.ChipSizeInByte);
                else
                    return CRC32(pBufferForLastReadData[g_uiDevNum - 1], Chip_Info.ChipSizeInByte);
            } else {
                if (g_uiDevNum == 0)
                    return CRC32(pBufferForLastReadData[0], g_uiLen);
                else
                    return CRC32(pBufferForLastReadData[g_uiDevNum - 1], g_uiLen);
            }
        }
    } else
        return EXCODE_FAIL_CHKSUM;

    boResult &= Verify();
    if (boResult == false)
        return EXCODE_FAIL_VERIFY;

    return EXCODE_PASS;
}

void print_message_function(void* ptr)
{
    char* message;
    message = (char*)ptr;
    printf("%s \n", message);
}

void GetLogPath(char* pBuf)
{
    memset(pBuf, 0, 512);
    if (readlink("/proc/self/exe", pBuf, 512) != -1) {
        if (strlen(pBuf) < (511 - sizeof("/log.txt")))
            strcat(pBuf, "/log.txt");
        //		printf("%s\n",pBuf);
    }
}

void EnterStandaloneMode(int Index)
{
    if (g_bIsSF600[Index] == true)
        LeaveSF600Standalone(false, Index);
}

void LeaveStandaloneMode(int Index)
{
    if (g_bIsSF600[Index] == true)
        LeaveSF600Standalone(true, Index);
}

void WriteLog(int ErrorCode, bool Init)
{
    FILE* FileOut = NULL; /* output files */
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[512] = { 0 };
    char* Result[] = {
        " PASS\n",
        " FAIL\n",
        " Erase Fail\n",
        " Program Fail\n",
        " Verify Fail\n",
        " Read Fail\n",
        " Blank Check Fail\n",
        " Auto Batch Fail\n",
        " Checksum Fail\n",
        " Identify Fail\n",
        " Unknow Fail\n"
    };

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "%Y-%b-%d %T", timeinfo);
    if (Init) {
        FileOut = fopen(g_LogPath, "wt");
        if (FileOut != NULL)
            fprintf(FileOut, "%s%s%s", buffer, " USB communication =", Result[ErrorCode]);
    } else {
        FileOut = fopen(g_LogPath, "a+t");
        if (FileOut != NULL)
            fprintf(FileOut, "%s%s", buffer, Result[ErrorCode]);
    }

    if (FileOut != NULL)
        fclose(FileOut);
}

int GetConfigVer()
{
    char file_line_buf[512];
    char test[80];
    char *pch, *tok;
    bool bFound = 0;
    int Ver = 0;
    FILE* fp; /*Declare file pointer variable*/

    if ((fp = openChipInfoDb()) == NULL)
        return 1;

    while (fgets(file_line_buf, 512, fp) != NULL) {
        if (strstr(file_line_buf, "DediProgChipDatabase") && bFound == 0)
            bFound = 1;

        pch = strstr(file_line_buf, "Ver");
        if (bFound == 1 && pch != NULL) {
            memset(test, '\0', 80);
            strcpy(test, pch + strlen("Ver"));
            tok = strtok(test, "\"= \t");
            Ver = strtol(tok, NULL, 10);
            break;
        }
    }
    fclose(fp);
    return Ver;
}

int main(int argc, char* argv[])
{
#if 0 //Check all USB device capability
	Check(argc,argv);
	return;
#endif
    int c;
    int iExitCode = EXCODE_PASS;
    bool bDetect = false;
    bool bDevice = false;

    printf("\nDpCmd Linux 1.11.2.%02d Engine Version:\nLast Built on May 25 2018\n\n", GetConfigVer()); // 1. new feature.bug.configS

    g_ucOperation = 0;
    GetLogPath(g_LogPath);

    if (argc == 1) {
        cli_classic_usage(false);
        return 0;
    }

    if (OpenUSB() == 0)
        iExitCode = EXCODE_FAIL_USB;

    LeaveStandaloneMode(0);
    QueryBoard(0);

    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case '?':
            cli_classic_usage(true);
            CloseProject();
            ExitProgram();
            return EXCODE_PASS;
        case 'L':
            g_ucOperation |= LIST_TYPE;
            break;
        case 'd':
            bDetect = true;
            break;
        case 'b':
            g_ucOperation |= BLANK;
            break;
        case 'e':
            g_ucOperation |= ERASE;
            break;
        case 'r':
            g_parameter_read = optarg;
            g_ucOperation |= READ_TO_FILE;
            break;
        case 'p':
            g_parameter_program = optarg;
            g_ucOperation |= PROGRAM;
            break;
        case 'u':
            g_parameter_auto = optarg;
            g_ucOperation |= BATCH;
            g_BatchIndex = 2;
            break;
        case 'z':
            g_parameter_batch = optarg;
            g_ucOperation |= BATCH;
            g_BatchIndex = 1;
            break;
        case 's': // display chip checksum
            g_ucOperation |= CSUM;
            break;
        case 'f': //display file checksum
            g_parameter_fsum = optarg;
            g_ucOperation |= FSUM;
            break;
        case 'I':
            g_parameter_raw = optarg;
            break;
        case 'R': //raw-require-return
            g_parameter_raw_return = optarg;
            break;
            /* Optional Switches that add fine-tune ability to Basic Switches: */
        case 'a':
            g_parameter_addr = optarg;
            g_uiAddr = 0;
            sscanf(optarg, "%x", &g_uiAddr);
            break;
        case 'l':
            g_uiLen = 0;
            if (strstr(optarg, "0x"))
                sscanf(optarg, "%zx", &g_uiLen);
            else
                sscanf(optarg, "%zu", &g_uiLen);
            break;
        case 'v':
            g_ucOperation |= VERIFY;
            break;
        case 'x':
            g_ucFill = 0xFF;
            sscanf(optarg, "%x", &g_ucFill);
            break;
        case 'T':
            g_parameter_type = optarg; //type
            break;
        case 'S': //lock start
            sscanf(optarg, "%x", &g_uiLockStart);
            //                l_opt_arg = optarg;
            //             printf("hexadecimal starting address (with arg: %s)\n", l_opt_arg);
            break;
        case 'N': //lock length
            sscanf(optarg, "%x", &g_uiLockLen);
            //                 l_opt_arg = optarg;
            //            printf("hexadecimal length of area that will be kept unchanged while updating (with arg: %s)\n", l_opt_arg);
            break;
        case 'B':
            sscanf(optarg, "%d", &g_uiBlink);
            g_ucOperation |= BLINK;
            break;
        case 'D': // device
            //l_opt_arg = optarg;
            //printf("activate only the programmer connected to USBx (with arg: %s)\n", l_opt_arg);

            bDevice = true;
            sscanf(optarg, "%x", &g_uiDevNum);
            //devpath = optarg;
            break;
        case 'F':
            //	l_opt_arg = optarg;
            //	printf("Fix programmer serial number with programmer sequence. (with arg: %s)\n", l_opt_arg);
            //	break;
            g_parameter_loadfile = optarg;
            g_ucOperation |= LOADFILE;
            break;
        case 'V':
            sscanf(optarg, "%d", &g_uiDeviceID);
            g_ucOperation |= DEVICE_ID; //list device id
            break;
            /* Miscellaneous options: */
        case 't':
            l_opt_arg = optarg;
            sscanf(l_opt_arg, "%d", &g_uiTimeout);
            break;
        case 'g':
            g_parameter_target = optarg;
            sscanf(optarg, "%d", &g_ucTarget);
            break;
        case 'c':
            g_parameter_vcc = optarg;
            sscanf(optarg, "%d", &g_Vcc);
            break;
        case 'P':
            g_bEnableVpp = true;
            break;
        case 'O': //Log file
            strcpy(g_LogPath, optarg);
            //					printf("%s\n",g_LogPath);
            break;
        case 'i':
            g_bDisplayTimer = false;
            break;
        case 'k':
            l_opt_arg = optarg;
            sscanf(l_opt_arg, "%d", &g_ucSPIClock);
            break;
        case '1':
            l_opt_arg = optarg;
            sscanf(l_opt_arg, "%d", &g_IO1Select);
            break;
        case '4':
            l_opt_arg = optarg;
            sscanf(l_opt_arg, "%d", &g_IO4Select);
            break;
        case 'U':
            g_parameter_fw = optarg;
            iExitCode = FirmwareUpdate();
            goto Exit;
            break;
        case 'E':
            g_tv_display_delta_s = atof(optarg);
            if (g_tv_display_delta_s <= 0) {
                fprintf(stderr, "E: invalid number"
                                " of seconds for display delta"
                                " (%f); expect larger than"
                                " zero\n",
                    g_tv_display_delta_s);
                return 1;
            }
            break;
        default:
            break;
        }
    }

    int dev_cnt = get_usb_dev_cnt();
    if (bDetect == true) {
        if (bDevice == false) {
            for (int i = 0; i < dev_cnt; i++) {
                //				//printf("%s\n",g_LogPath);

                int dwUID = ReadUID(i);
                if ((dwUID / 600000) > 0) {
                    printf("\nDevice %d (SF%06d):\tdetecting chip\n", i + 1, dwUID);
                } else {
                    printf("\nDevice %d (DP%06d):\tdetecting chip\n", i + 1, dwUID);
                }
                WriteLog(iExitCode, true);
                Chip_Info = GetFirstDetectionMatch(i);
                if (Chip_Info.UniqueID != 0) {
                    printf("By reading the chip ID, the chip applies to [ %s ]\n", Chip_Info.TypeName);
                    printf("%s chip size is %zd bytes.\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);
                } else {
                    printf("%s", msg_err_identifychip);
                    iExitCode = EXCODE_FAIL_IDENTIFY;
                }
            }
        } else if (g_uiDevNum != 0) {
            WriteLog(iExitCode, true);
            printf("%d,\tdetecting chip\n", g_uiDevNum);
            Chip_Info = GetFirstDetectionMatch(g_uiDevNum - 1);
            if (Chip_Info.UniqueID != 0) {
                printf("  \tBy reading the chip ID, the chip applies to [ %s ]\n", Chip_Info.TypeName);
                printf("  \t%s chip size is %zd bytes.\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);
            } else {
                printf("%s", msg_err_identifychip);
                iExitCode = EXCODE_FAIL_IDENTIFY;
            }
        } else {
            printf("The number of programmer is not defined!\n");
            iExitCode = EXCODE_FAIL_IDENTIFY;
        }
        goto Exit;
    } else
        WriteLog(iExitCode, true);

    if (iExitCode != EXCODE_PASS)
        goto Exit;
    iExitCode = Handler();

Exit:
    printf("\n\n");
    CloseProject();
    ExitProgram();
    WriteLog(iExitCode, false);
    return iExitCode;
}

void ExitProgram(void)
{
    for (int i = 0; i < get_usb_dev_cnt(); i++) {
        if (pBufferForLastReadData[i] != NULL)
            free(pBufferForLastReadData[i]);
    }

    if (pBufferforLoadedFile != NULL)
        free(pBufferforLoadedFile);
}

int FirmwareUpdate()
{
    FW_INFO Data;
    FILE* pFile;
    size_t result, filesize;
    int fwVer;
    //	unsigned char vBuffer[8] ;
    int fw[3];

    pFile = fopen(g_parameter_fw, "rb");
    if (pFile == NULL) {
        printf("Can not open %s \n", g_parameter_fw);
        return EXCODE_FAIL_UPDATE_FW;
    }

    // obtain file size:
    filesize = fseek(pFile, 0, SEEK_END);
    if (filesize < sizeof(FW_INFO)) {
        printf("File %s too small.\n", g_parameter_fw);
        return -1;
    }
    fseek(pFile, 0, SEEK_SET);
    //    rewind (pFile);

    // copy the file into the buffer:
    result = fread((unsigned char*)&Data, 1, sizeof(FW_INFO), pFile);
    if (result != sizeof(FW_INFO)) {
        printf("File %s read problem.\n", g_parameter_fw);
        return -1;
    }
    fclose(pFile);

    if (Data.dwSignature != 0x44504657) {
        printf("This file is not DediProg firmware format.\n");
        return EXCODE_FAIL_UPDATE_FW;
    }
    sscanf(&Data.Version[0], "%d.%d.%d", &fw[0], &fw[1], &fw[2]);
    fwVer = ((fw[0] << 16) | (fw[1] << 8) | fw[2]);

    printf("Current Firmware is %x, New Firmware Version is %x\n", g_firmversion, fwVer);

    if (strstr(g_board_type, Data.Programmer) == NULL)
        printf("Error: This firmware for %s!\n", Data.Programmer);

    if ((strstr(g_board_type, Data.Programmer) == NULL) || ((fwVer & 0xff0000) != (g_firmversion & 0xff0000))) {
        printf("Error: This firmware can not be updated on this programmer!\n");
        return EXCODE_FAIL_UPDATE_FW;
    }

    printf("%s", msg_info_firmwareupdate);
    Run(UPDATE_FIRMWARE, g_uiDevNum);

    if (Wait(msg_info_firmwareupdateOK, msg_info_firmwareupdatefail))
        return 0;

    return 0;
}

void cli_classic_usage(bool IsShowExample)
{
    printf("Basic Usages:\n");
    printf("./dpcmd -uxxx\n");
    printf("./dpcmd --auto=xxx\n");
    if (IsShowExample)
        printf("Usage Examples:\n"
               "1. dpcmd -r\"f:\\file.bin\", \n    reads the chip and save it into a file \"file.bin\"\n"
               "2. dpcmd -rSTDOUT -a0x100 -l0x23, \n    reads 0x23 bytes starting from 0x100 and display it on the screen\n"
               "3. dpcmd -ufile.bin, \n    Update the whole chip with file.bin\n"
               "4. Dpcmd -ufile.bin -a0x100, \n    Replace the serial flash contents for the size of file.bin with file.bin.\n"
               "    (contents outside the replace area will be kept unchanged)\n"
               "5. dpcmd -pfile.bin -a0x100, \n    program file.bin into the serial flash starting from address 0x100\n"
               "    (the target area is assumed to be blank already)\n"
               "6. dpcmd -pfile.bin -x0xaa, \n    programs file.bin into the serial flash and fill the rest area with 0xaa\n"
               "    (the target area is assumed to be blank already)\n"
               "Remark: -a, -l only works with -p, -r, -s and -u\n"
               "Remark: -x only works with -p");

    printf("(space is not needed between the switches and parameters. E.g. dpcmd -ubio.bin)\n");
    printf("    -? [ --help ]                           show this help message\n"
           "    --list                                  print supported chip list\n"
           "    -d [ --detect ]                         detect chip\n"
           "    -b [ --blank ]                          blank check\n"
           "    -e [ --erase ]                          erase entire chip\n"
           "    -r [ --read ] arg                       read chip contents and save to a bin/hex/s19 file\n"
           "    -p [ --prog ] arg                       program chip without erase\n"
           "    -u [ --auto ] arg                       automatically run the following sequence:\n"
           "                                            - Read the memory content\n"
           "                                            - Compare the memory content\n"
           "                                            - Erase only the sectors with some differences\n"
           "                                            - Program only the erased sectors with file data from address 0\n"
           "    -z [ --batch ] arg                      automatically run the following sequence:\n"
           "                                            - check if the chip is blank or not;\n"
           "                                            - erase the entire chip(if not blank);\n"
           "                                            - program a whole file starting from address 0\n"
           "    -s [ --sum ]                            display chip content checksum\n"
           "    -f [ --fsum ] arg                       display the file checksum\n"
           "    --raw-instruction arg                   issue raw serial flash instructions.\n"
           "                                            - use spaces("
           ") to delimite bytes.\n"
           "                                            - instructions must be enclosed in double quotation marks(\"\")\n"
           "                                            Example:\n"
           "                                            dpcmd --raw-instruction \"03 FF 00 12\"\n"
           "    --raw-require-return arg (=0)           decimal bytes of result to return in decimal\n"
           "                                            after issuing raw instructions.\n"
           "                                            - used along with --raw-instruction only.\n"
           "                                            Example:\n"
           "                                            dpcmd --raw-instruction \"03 FF 00 12\" \n"
           "                                            --raw-require-return 1\n"
           "                                            dpcmd --raw-instruction \"06|01 0C|05\"  \n"
           "                                            --raw-require-return \"0|0|1\"	\n"
           "\n"
           "Optional Switches that add fine-tune ability to Basic Switches:\n"
           "    -a [ --addr ] arg                       hexadecimal starting address hexadecimal(e.g.\n"
           "                                            0x1000),\n"
           "                                            - works with --prog/read/sum/auto only\n"
           "                                            - defaults to 0, if omitted.\n"
           "    -l [ --length ] arg                     decimal/hexadecimal length to read/program in bytes,\n"
           "                                            - works with --prog/read/sum/auto only\n"
           "                                            - defaults to whole file if omitted\n"
           "    -v [ --verify ]                         verify checksum file and chip\n"
           "                                            - works with --prog/auto only\n"
           "    -x [ --fill ] arg (=FF)                 fill spare space with an hex value(e.g.FF),\n"
           "                                            - works with --prog, --auto only\n"
           "    --type arg                              Specify a type to override auto detection\n"
           "                                            - use --list arguement to look up supported type.\n"
           "    --lock-start arg                        hexadecimal starting address(e.g. 0x1000),\n"
           "                                            - works with --auto/lock-length\n"
           "                                            - defaults to 0, if omitted.\n"
           "    --lock-length arg                       hexadecimal length of area that will be kept\n"
           "                                            unchanged while updating\n"
           "                                            - used along with --auto/lock-start only.\n"
           "    --blink arg                             \n"
           //	       "                                            - 0 : Blink green LED 3 times from USB1 to USBn\n"
           //	       "                                            (Default)\n"
           //	       "                                            note: the sequence is assigned by OS during USB plug-in\n"
           "                                            - 1: Blink the programmer connected to USB1 3 times.\n"
           //	       "                                            - n: Blink the programmer connected to USBn 3 times.\n"
           "    --device arg                            (work with all Basic Switchs)\n"
           //	       "                                            - 1: activate only the programmer connected to USB1\n"
           //	       "                                            - n: activate only the programmer connected to USBn\n"
           //	       "                                            note: if '--device' is not used, the command will\n"
           //	       "                                            be executed on all connected programmer.\n"
           //	       "    --fix-device arg                        Fix programmer serial number with programmer sequence.\n"
           //	       "                                            - instructions must be enclosed in double quotation marks(\"\")\n"
           //	       "                                            Example:\n"
           //	       "                                            dpcmd --fix-device \"1 DP000001\"\n"
           "    --list-device-id arg                    \n"
           //	       "                                            - 0 : List all ID of programmers from USB1 to USBn (Default)\n"
           //	       "                                            note: the sequence is assigned by OS during USB plug-in.\n"
           "                                            - 1: Prompt the device ID of programmer connected to USB1.\n"
           //	       "                                            - n: Prompt the device ID of programmer connected to USBn.\n"
           "    --load-file arg                         -Load a bin/hex/s19 file and compare with the memory conten.\n"
           "                                            -work with --verify only"
           "\n"
           "Miscellaneous options:\n"
           "    -t [ --timeout ] arg (=300)             Timeout value in seconds\n"
           "    -g [ --target ] arg (=1)                Target Options\n"
           "                                            Available values:\n"
           "                                                1, Chip 1(Default)\n"
           "                                                2, Chip 2\n"
           "                                                3, Socket\n"
           "                                                0, reference card\n"
           "    --vcc arg (=0)                          specify vcc\n"
           "                                                0, 3.5V\n"
           "                                                1, 2.5V\n"
           "                                                2, 1.8V\n"
           "                                                1800 ~ 3800, 1.8V ~ 3.8V\n"
           "                                                (minimum step 100mV)(For SF600/SF600Plus only)\n"
           "    --vpp                                   apply vpp when the memory chip supports it\n"
           "    --log arg                               Record the operation result in given/appointed .txt file\n"
           "                                            Example:\n"
           "                                            ./dpcmd --log /tmp/log.txt\n"
           "                                            Note: If user didn't use this command, \n"
           "                                                  the operation result will be recorded in \"log.txt\"\n"
           "                                            - work with --prog and --erase.\n"
           "    -i [ --silent ]                         supress the display of real-time timer counting\n"
           "                                            - used when integrating with 3rd-party tools(e.g. IDE)\n"
           "    --spi-clk arg (=2)                      specify SPI clock:\n"
           "                                                2, 12 MHz(Default)\n"
           "                                                0, 24 MHz\n"
           "                                                1, 8 MHz\n"
           "                                                3, 3 MHz\n"
           "                                                4, 2.18 MHz\n"
           "                                                5, 1.5 MHz\n"
           "                                                6, 750 KHz\n"
           "                                                7, 375 KHz\n"
           "    --set-io1 arg (=0)                      specify Level of IO1(SF100) or GPIO1(SF600/SF600Plus):\n"
           "                                                0, Low(Default)\n"
           "                                                1, High\n"
           "    --set-io4 arg (=1)                      specify Level of IO4(SF100) or GPIO2(SF600/SF600Plus):\n"
           "                                                0, Low\n"
           "                                                1, High(Default)\n"
           "    -e|--display-delta SECONDS (=0.5)       wait this many seconds\n"
           "                                            before refreshing the screen\n"
           "\n\n\n");
}

int OpenUSB(void)
{
    return usb_driver_init();
}

void sin_handler(int sig)
{
    if (sig == SIGINT) {
        ;
    }
}

int Handler(void)
{
    if (Is_usbworking(0) == true) {
#if 0
        if(m_vm.count("fix-device"))
        {
            FixProgrammer();
            return EXCODE_PASS;
        }

        AssignedDevice();
#endif

        if ((g_ucOperation & BLINK) == BLINK) {
            BlinkProgrammer();
            return EXCODE_PASS;
        }

        if ((g_ucOperation & DEVICE_ID) == DEVICE_ID) {
            ListSFSerialID();
            return EXCODE_PASS;
        }

    } else {
        printf("Error: Programmers are not connected.\n");
        return EXCODE_FAIL_OTHERS;
    }

    if (!InitProject())
        return EXCODE_FAIL_OTHERS;
    //my_timer t;                           // opertion timer

    if (ListTypes())
        return EXCODE_PASS;

    if (strlen(g_parameter_type) > 0) {
        int dev_cnt = get_usb_dev_cnt();
        if (g_uiDevNum == 0) {
            for (int i = 0; i < dev_cnt; i++) {
                RawInstructions(i);
            }
        } else if (g_uiDevNum <= dev_cnt) {
            RawInstructions(g_uiDevNum - 1);

        } else {
            printf("Error: Did not find the programmer.\n");
            return EXCODE_FAIL_OTHERS;
        }
        return Sequence();
    } else if (DetectChip()) {
        return Sequence();
    } else
        return EXCODE_FAIL_IDENTIFY;

    return EXCODE_FAIL_OTHERS;
}

bool InitProject(void)
{
    int dev_cnt = get_usb_dev_cnt();
    if (Is_usbworking(0)) {
        int targets[4] = {
            STARTUP_APPLI_CARD,
            STARTUP_APPLI_SF_1,
            STARTUP_APPLI_SF_2,
            STARTUP_APPLI_SF_SKT,
        };

        if (g_uiDevNum == 0) {
            for (int i = 0; i < dev_cnt; i++) {
                g_StartupMode = targets[g_ucTarget];
                SetTargetFlash((unsigned char)targets[g_ucTarget], i);
                SetSPIClock(i);
                SetVcc(i);

                if (strlen(g_parameter_type) > 0) {
                    if (Dedi_Search_Chip_Db_ByTypeName(g_parameter_type, &Chip_Info)) {
                        printf("Chip Type %s is applied manually.\n", Chip_Info.TypeName);
                        printf("%s chip size is %zd bytes.\n\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);
                        ProjectInitWithID(Chip_Info, i);
                        if (!strcmp(Chip_Info.Class, "N25Qxxx_Large"))
                            isSendFFsequence = true;
                    } else {
                        printf("Chip Type Unknow is applied manually.\n");
                        return false;
                    }

                } else {
                    ProjectInit(i);
                }
            }
        } else if (g_uiDevNum <= dev_cnt) {
            g_StartupMode = targets[g_ucTarget];
            SetTargetFlash((unsigned char)targets[g_ucTarget], g_uiDevNum - 1);
            SetSPIClock(g_uiDevNum - 1);
            SetVcc(g_uiDevNum - 1);

            if (strlen(g_parameter_type) > 0) {
                if (Dedi_Search_Chip_Db_ByTypeName(g_parameter_type, &Chip_Info)) {
                    printf("Chip Type %s is applied manually.\n", Chip_Info.TypeName);
                    printf("%s chip size is %zd bytes.\n\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);
                    ProjectInitWithID(Chip_Info, g_uiDevNum - 1);
                } else {
                    printf("Chip Type Unknow is applied manually.\n");
                    return false;
                }

            } else {
                ProjectInit(g_uiDevNum - 1);
            }
        } else
            printf("Error: Did not find the programmer.\n");

        return true;
    }
    return false;
}

void CloseProject(void)
{
    for (int i = 0; i < get_usb_dev_cnt(); i++) {
        EnterStandaloneMode(i);
    }
    usb_driver_release();
}

bool DetectChip(void)
{
    int dev_cnt = get_usb_dev_cnt();
    Chip_Info = GetFirstDetectionMatch(0);
    if (g_uiDevNum == 0) {
        for (int i = 0; i < dev_cnt; i++) {
            if (!Is_usbworking(i)) {
                printf("%s", msg_err_communication);
                return false;
            }
            if (0 == Chip_Info.UniqueID) {
                printf("%s", msg_err_identifychip);
                return false;
            }
            printf("By reading the chip ID, the chip applies to [ %s ]\n", Chip_Info.TypeName);
            printf("%s parameters to be applied by default\n", Chip_Info.TypeName);
            printf("%s chip size is %zd bytes.\n\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);

            RawInstructions(i);
        }
    } else if (g_uiDevNum <= dev_cnt) {
        if (!Is_usbworking(g_uiDevNum - 1)) {
            printf("%s", msg_err_communication);
            return false;
        }
        if (0 == Chip_Info.UniqueID) {
            printf("%s", msg_err_identifychip);
            return false;
        }
        printf("By reading the chip ID, the chip applies to [ %s ]\n", Chip_Info.TypeName);
        printf("%s parameters to be applied by default\n", Chip_Info.TypeName);
        printf("%s chip size is %zd bytes.\n\n", Chip_Info.TypeName, Chip_Info.ChipSizeInByte);

        RawInstructions(g_uiDevNum - 1);

    } else
        printf("Error: Did not find the programmer.\n");

    return true;
}

/* Simon: add later */
void SetVpp(int Index)
{
}

void SetSPIClock(int Index)
{
    //    sscanf(g_parameter_spi_clk,"%d",&g_ucSPIClock);
    SetSPIClockValue(g_ucSPIClock, Index);
}

void SetVcc(int Index)
{
    //    sscanf(g_parameter_vcc,"%d",&g_Vcc);
    if (!(g_Vcc <= 3800 && g_Vcc >= 1800 && g_bIsSF600[Index] == true))
        g_Vcc = (0x10 | (g_Vcc & 0x03));
}

int do_loadFile(void)
{
    char* filename = NULL;
    if (g_ucOperation & PROGRAM)
        filename = g_parameter_program;
    else if (g_ucOperation & LOADFILE)
        filename = g_parameter_loadfile;
    else if (g_ucOperation & BATCH) {
        switch (g_BatchIndex) {
        case 1:
            filename = g_parameter_batch;
            break;
        case 2:
        default:
            filename = g_parameter_auto;
            break;
        }
    } else if (g_ucOperation & FSUM)
        filename = g_parameter_fsum;
    printf("%s", msg_info_loading);
    printf("(%s)", filename);
    return LoadFile(filename);
}

bool ListTypes(void)
{
    if ((g_ucOperation & LIST_TYPE) != LIST_TYPE)
        return false;
    return Dedi_List_AllChip();
}

void BlinkProgrammer(void)
{
    bool IsV5 = is_BoardVersionGreaterThan_5_0_0(0);
    if (IsV5 == false)
        return;
    int dev_cnt = get_usb_dev_cnt();

    if (g_uiBlink == 0) {
        for (int i = 0; i < dev_cnt; i++) {
            printf("Blink LED on USB device %d \n", i);
            BlinkProgBoard(IsV5, i);
            Sleep(500);
            BlinkProgBoard(IsV5, i);
            Sleep(500);
            BlinkProgBoard(IsV5, i);
        }
    } else if (g_uiBlink <= dev_cnt) {
        printf("Blink LED on USB device %d \n", g_uiBlink);
        BlinkProgBoard(IsV5, g_uiBlink - 1);
        Sleep(500);
        BlinkProgBoard(IsV5, g_uiBlink - 1);
        Sleep(500);
        BlinkProgBoard(IsV5, g_uiBlink - 1);
    } else
        printf("Error: Did not find the programmer.\n");
}

void ListSFSerialID(void)
{
    unsigned int dev_cnt = 0;
    dev_cnt = get_usb_dev_cnt();

    unsigned int dwUID = 0;
    if (g_uiDeviceID < 1) {
        for (unsigned int i = 0; i < dev_cnt; i++) {
            dwUID = ReadUID(i);
            if ((dwUID / 600000) == 0)
                printf("%d,\tDP%06d\n", i + 1, dwUID);
            else
                printf("%d,\tSF%06d\n", i + 1, dwUID);
        }
    } else {
        if (g_uiDeviceID > dev_cnt)
            printf("The number of programmer is not defined!\n");
        else {
            dwUID = ReadUID(g_uiDeviceID - 1);
            if ((dwUID / 600000) == 0)
                printf("%d,\tDP%06d\n", g_uiDeviceID, dwUID);
            else
                printf("%d,\tSF%06d\n", g_uiDeviceID, dwUID);
        }
    }
}

void do_BlankCheck(void)
{
    printf("%s\n", msg_info_checking);
    Run(BLANKCHECK_WHOLE_CHIP, g_uiDevNum);

    Wait(msg_info_chipblank, msg_info_chipnotblank);
}

void do_Erase(void)
{
    printf("%s \n", msg_info_erasing);
    Run(ERASE_WHOLE_CHIP, g_uiDevNum);

    Wait(msg_info_eraseOK, msg_info_erasefail);
}

void do_Program(void)
{
    if (!do_loadFile())
        return;
    SaveProgContextChanges();

    printf("%s\n", msg_info_programming);
    Run(PROGRAM_CHIP, g_uiDevNum);

    if (Wait(msg_info_progOK, msg_info_progfail)) {
        printf("\nChecksum(whole file): %08X", g_uiFileChecksum);
        printf("\nChecksum(Written part of file): %08X\n", CRC32(pBufferforLoadedFile, min(DownloadAddrRange.end - DownloadAddrRange.start, g_ulFileSize)));
    }
}

void do_Read(void)
{
    DownloadAddrRange.start = g_uiAddr;
    DownloadAddrRange.length = g_uiLen;
    DownloadAddrRange.end = g_uiAddr + g_uiLen;

    printf("%s\n", msg_info_reading);
    Run(READ_ANY_BY_PREFERENCE_CONFIGURATION, g_uiDevNum);
    Wait(msg_info_readOK, msg_info_readfail);
}

void do_DisplayOrSave(void)
{
    int dev_cnt = get_usb_dev_cnt();

    if (g_uiDevNum == 0) {
        for (int icnt = 0; icnt < dev_cnt; icnt++) {
            if (strcmp(g_parameter_read, "STDOUT") == 0) {
                int i, Len = UploadAddrRange.end - UploadAddrRange.start;
                printf("\n");
                printf("Hex value display (starting from 0x%02X, 0x%02zX bytes in total): \n", g_uiAddr, g_uiLen);
                for (i = 0; i < Len; i++) {
                    if ((i % 16 == 0) && i != 0) {
                        printf("\n");
                    }
                    printf("%02X  ", *(pBufferForLastReadData[icnt] + i));
                }
                printf("\n\n");
            } else {
                UploadAddrRange.length = UploadAddrRange.end - UploadAddrRange.start;
                char str[64];
                sprintf(str, "%d_", icnt + 1);
                strcat(str, g_parameter_read);

                if (WriteFile((const char*)str, pBufferForLastReadData[icnt], UploadAddrRange.length) == 1)
                    printf("\nSuccessfully saved into file:%s \n", str);
                else
                    printf("\nFailed to save into file: %s\n", str);
            }
        }
    } else if (g_uiDevNum != 0) {
        if (strcmp(g_parameter_read, "STDOUT") == 0) {
            int i, Len = UploadAddrRange.end - UploadAddrRange.start;
            printf("\n");
            printf("Hex value display (starting from 0x%02X, 0x%02zX bytes in total): \n", g_uiAddr, g_uiLen);
            for (i = 0; i < Len; i++) {
                if ((i % 16 == 0) && i != 0)
                    printf("\n");
                printf("%02X  ", *(pBufferForLastReadData[g_uiDevNum - 1] + i));
            }
            printf("\n\n");
        } else {
            UploadAddrRange.length = UploadAddrRange.end - UploadAddrRange.start;
            if (WriteFile((const char*)g_parameter_read, pBufferForLastReadData[g_uiDevNum - 1], UploadAddrRange.length) == 1)
                printf("\nSuccessfully saved into file:%s \n", g_parameter_read);
            else
                printf("\nFailed to save into file: %s\n", g_parameter_read);
        }
    } else
        printf("The number of programmer is not defined!\n");
}

void SaveProgContextChanges(void)
{
    LockAddrrange.start = g_uiLockStart;
    LockAddrrange.length = g_uiLockLen;
    LockAddrrange.end = g_uiLockStart + g_uiLockLen;
#if 0
    using numeric_conversion::hexstring_to_size_t;

    m_context.Prog_PaddingCharacterWhenProgrammingWholeChip = hexstring_to_size_t(m_vm["fill"].as<string>());
    m_context.Prog_CustomizedAddrFrom = hexstring_to_size_t(m_vm["addr"].as<string>());
    m_context.Prog_CustomizedDataLength = hexstring_to_size_t(m_vm["length"].as<string>());
    m_context.LockStartFrom = hexstring_to_size_t(m_vm["lock-start"].as<string>());
    m_context.LockLength = hexstring_to_size_t(m_vm["lock-length"].as<string>());
#endif
}

void do_Auto(void)
{
    if (do_loadFile() == 0)
        return;
    SaveProgContextChanges();

    printf("%s", msg_info_auto);
    Run(AUTO, g_uiDevNum);

    if (Wait(msg_info_autoOK, msg_info_autofail)) {
        printf("\nChecksum(whole file): %08X", g_uiFileChecksum);
        printf("\nChecksum(Written part of file): %08X\n", CRC32(pBufferforLoadedFile, DownloadAddrRange.end - DownloadAddrRange.start));
    }
}

void do_Verify(void)
{
    if (g_ucOperation & LOADFILE)
        do_loadFile();

    printf("%s", msg_info_verifying);
    Run(VERIFY_CONTENT, g_uiDevNum);

    Wait(msg_info_verifyOK, msg_info_verifyfail);
}

void do_ReadSR(int Index)
{
    unsigned char rdsr = 0x05;
    unsigned char cSR;
    if (FlashCommand_SendCommand_OneOutOneIn(&rdsr, 1, &cSR, 1, Index) == 0)
        printf("SR unavailable\n");
    else
        printf("SR=0x%02X\n", cSR);
}

void do_RawInstructinos_2(int outDLen, char* para, int Index)
{
    unsigned char vOut[512];
    unsigned char vIn[512] = { 0xFF, 0xFF, 0xFF, 0 };
    int i = 0;
    char* pch;

    pch = strtok(para, " ,;-");
    while (pch != NULL) {
        sscanf(pch, "%02hhx", &vOut[i]);
        i++;
        pch = strtok(NULL, " ,;-");
    }

    // if(strlen(g_parameter_raw_return)>0)
    //	sscanf(g_parameter_raw_return,"%hhu",&Length);

    if (outDLen > 0)
        FlashCommand_SendCommand_OneOutOneIn(vOut, i, vIn, outDLen, Index);
    else
        FlashCommand_SendCommand_OutOnlyInstruction(vOut, i, Index);

    if (outDLen > 0) {
        printf("issuing raw instruction \"%s\" returns %d bytes as required:\n", g_parameter_raw, outDLen);
        for (i = 0; i < outDLen; i++)
            printf("%02X ", vIn[i]);
        printf("\n");
    }

    do_ReadSR(Index);
}

void do_RawInstructions(int Index)
{
    char* pch[30];
    char* pchR;
    unsigned char pchReturn[30];
    char parameter[40];

    unsigned char Length = 0;
    char retrunParameter[40];
    int iReturn = 0;
    int i = 0;
    strcpy(parameter, g_parameter_raw);
    TurnONVcc(Index);
    pch[i] = strtok(parameter, "|");
    while (pch[i] != NULL) {
        i++;
        pch[i] = strtok(NULL, "|");
    }

    if (strlen(g_parameter_raw_return) > 0) {
        sscanf(g_parameter_raw_return, "%hhu", &Length);
        strcpy(retrunParameter, g_parameter_raw_return);

        pchR = strtok(retrunParameter, "|");
        while (pchR != NULL) {
            sscanf(pchR, "%02hhx", &pchReturn[iReturn]);
            iReturn++;
            pchR = strtok(NULL, "|");
        }
        if (iReturn != i) {
            printf("\nError: value of --raw-require-return failed. No return value!\n\n");
            memset(pchReturn, 0, 30);
        }
    }

    for (int j = 0; j < i; j++) {
        do_RawInstructinos_2(pchReturn[j], pch[j], Index);
    }
}

void RawInstructions(int Index)
{
    if (strlen(g_parameter_raw) > 0) {
        printf("\nDevice %d:\n", Index + 1);
        do_RawInstructions(Index);
    }
}

bool BlankCheck(void)
{
    if (g_ucOperation & BLANK)
        do_BlankCheck();

    return g_bStatus;
}

bool Erase(void)
{
    if (g_ucOperation & ERASE)
        do_Erase();
    return g_bStatus;
}

bool Program(void)
{
    if (g_ucOperation & PROGRAM)
        do_Program();

    return g_bStatus;
}

bool Read(void)
{
    if (g_ucOperation & READ_TO_FILE) {
        do_Read();
        do_DisplayOrSave();
    }
    return g_bStatus;
}

bool Auto(void)
{
    if (g_ucOperation & BATCH)
        do_Auto();
    return g_bStatus;
}

bool Verify(void)
{
    if (g_ucOperation & VERIFY)
        do_Verify();
    return g_bStatus;
}

bool CalChecksum(void)
{
    int dev_cnt = get_usb_dev_cnt();
    if (g_ucOperation & FSUM) {
        do_loadFile();
        printf("\nChecksum(file): 0x%08X\n", g_uiFileChecksum);
    }
    if (g_ucOperation & CSUM) {
        do_Read();
        if (g_uiDevNum == 0) {
            for (int i = 0; i < dev_cnt; i++) {
                int dwUID = ReadUID(i);
                if (g_uiAddr == 0 && g_uiLen == 0) {
                    if ((dwUID / 600000) == 0)
                        printf("\nDevice %d (DP%06d):", i + 1, dwUID);
                    else
                        printf("\nDevice %d (SF%06d):", i + 1, dwUID);

                    printf("Checksum of the whole chip(address starting from: 0x%X, 0x%zX bytes in total): %08X\n", g_uiAddr, Chip_Info.ChipSizeInByte, CRC32(pBufferForLastReadData[i], Chip_Info.ChipSizeInByte));
                } else {
                    if ((dwUID / 600000) == 0)
                        printf("\nDevice %d (DP%06d):", i + 1, ReadUID(i));
                    else
                        printf("\nDevice %d (SF%06d):", i + 1, ReadUID(i));

                    printf("Checksum(address starting from: 0x%X, 0x%zX bytes in total): %08X\n", g_uiAddr, g_uiLen, CRC32(pBufferForLastReadData[i], min(g_uiLen, Chip_Info.ChipSizeInByte)));
                }
            }
        } else if (g_uiDevNum != 0) {
            int dwUID = ReadUID(g_uiDevNum - 1);
            if (g_uiAddr == 0 && g_uiLen == 0) {
                if ((dwUID / 600000) == 0)
                    printf("\nDevice %d (DP%06d):", g_uiDevNum, ReadUID(g_uiDevNum - 1));
                else
                    printf("\nDevice %d (SF%06d):", g_uiDevNum, ReadUID(g_uiDevNum - 1));

                printf("Checksum of the whole chip(address starting from: 0x%X, 0x%zX bytes in total): %08X\n", g_uiAddr, Chip_Info.ChipSizeInByte, CRC32(pBufferForLastReadData[g_uiDevNum - 1], Chip_Info.ChipSizeInByte));
            } else {
                if ((dwUID / 600000) == 0)
                    printf("\nDevice %d (DP%06d):", g_uiDevNum, ReadUID(g_uiDevNum - 1));
                else
                    printf("\nDevice %d (SF%06d):", g_uiDevNum, ReadUID(g_uiDevNum - 1));
                printf("Checksum(address starting from: 0x%X, 0x%zX bytes in total): %08X\n", g_uiAddr, g_uiLen, CRC32(pBufferForLastReadData[g_uiDevNum - 1], min(g_uiLen, Chip_Info.ChipSizeInByte)));
            }
        } else
            printf("The number of programmer is not defined!\n");
    }

    return g_bStatus;
}

bool Wait(const char* strOK, const char* strFail)
{
    size_t timeOut = g_uiTimeout;
    struct timeval tv, basetv, diff;
    int dev_cnt = get_usb_dev_cnt();
    gettimeofday(&basetv, NULL);
    Sleep(100); // wait till the new thread starts ....
    printf("\n");

    while (g_is_operation_on_going == true) {
        for (volatile unsigned int dly = 100000; dly; dly--)
            __asm("nop");
        gettimeofday(&tv, NULL);
        if (tv.tv_sec - basetv.tv_sec > timeOut) {
            timersub(&tv, &basetv, &diff);
            printf("\r%0.2fs elapsed", diff.tv_sec + 0.000001 * diff.tv_usec);
            printf("%s", msg_err_timeout_abortion);
            g_is_operation_on_going = false;
            g_bStatus = false;
            return false;
        }

        if (g_bDisplayTimer == true) {
            timersub(&tv, &basetv, &diff);
            printf("\r%0.1fs elapsed", diff.tv_sec + 0.000001 * diff.tv_usec);
            Sleep(500);
        }
    }
    printf("\n");
    if (g_uiDevNum == 0) //mutiple
    {
        for (int i = 0; i < dev_cnt; i++) {
            int dwUID = ReadUID(i);
            if ((dwUID / 600000) == 0)
                printf("\nDevice %d (DP%06d):", i + 1, dwUID);
            else
                printf("\nDevice %d (SF%06d):", i + 1, dwUID);
            printf("%s\n", g_is_operation_successful[i] ? strOK : strFail);
            g_bStatus = g_is_operation_successful[i];
        }
    } else if (g_uiDevNum <= dev_cnt) {
        int dwUID = ReadUID(g_uiDevNum - 1);
        if ((dwUID / 600000) == 0)
            printf("\nDevice %d (DP%06d):", g_uiDevNum, dwUID);
        else
            printf("\nDevice %d (SF%06d):", g_uiDevNum, dwUID);

        printf("%s\n", g_is_operation_successful[g_uiDevNum - 1] ? strOK : strFail);
        g_bStatus = g_is_operation_successful[g_uiDevNum - 1];
    }
    return g_bStatus;
}

int FlashIdentifier(CHIP_INFO* Chip_Info, int search_all, int Index)
{
    long UniqueID = 0;
    int rc = 0;

    for (int c = 0; c < 2; c++) {
        if (c == 1)
            SendFFSequence(Index);
        UniqueID = flash_ReadId(0x9f, 4, Index);
        if (UniqueID != 0) {
            rc = Dedi_Search_Chip_Db(0x9f, UniqueID, Chip_Info, search_all);
            if (rc && (search_all == 0)) {
                if (c == 1)
                    isSendFFsequence = true;
                return rc;
            }
        }
        UniqueID = 0;
        rc = 0;
        UniqueID = flash_ReadId(0x9f, 3, Index);

        if (UniqueID != 0) {
            rc = Dedi_Search_Chip_Db(0x9f, UniqueID, Chip_Info, search_all);
            if (rc && (search_all == 0)) {
                if (c == 1)
                    isSendFFsequence = true;
                return rc;
            }
        }
        UniqueID = 0;
        rc = 0;
        UniqueID = flash_ReadId(0x9f, 2, Index);
        if (UniqueID != 0) {
            rc = Dedi_Search_Chip_Db(0x9f, UniqueID, Chip_Info, search_all);
            if (rc && (search_all == 0)) {
                if (c == 1)
                    isSendFFsequence = true;
                return rc;
            }
        }
    }

    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0x15, 2, Index);
    if (UniqueID != 0) {
        rc = Dedi_Search_Chip_Db(0x15, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0xab, 3, Index);
    if (UniqueID != 0) {
        rc = Dedi_Search_Chip_Db(0xab, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0xab, 2, Index);
    if (UniqueID != 0) {
        rc = Dedi_Search_Chip_Db(0xab, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0x90, 3, Index);
    if (UniqueID != 0) {
        rc = Dedi_Search_Chip_Db(0x90, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0x90, 2, Index);
    if (UniqueID != 0) {
        rc = Dedi_Search_Chip_Db(0x90, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    UniqueID = 0;
    rc = 0;
    UniqueID = flash_ReadId(0x90, 5, Index); // for 25LF020
    if (UniqueID != 0) {
        UniqueID = UniqueID - 0xFFFF0000;
        rc = Dedi_Search_Chip_Db(0x90, UniqueID, Chip_Info, search_all);
        if (rc && (search_all == 0))
            return rc;
    }
    return rc;
}
