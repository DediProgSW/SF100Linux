#include "IntelHexFile.h"
#include "project.h"
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
extern unsigned char* pBufferforLoadedFile;
extern unsigned long g_ulFileSize;
/*
 * the choice for the total length (16) of a line, but the specification
 * can support an another value
 */
bool BinToHexFile(const char* filePath, unsigned char* vBuffer, unsigned long FileSize)
{
    // +anderson_variable 06/10/11 ///////////////////////////////////////
    //    unsigned char    unch = 0x12;
    // int        j=0;
    // int        str1_cnt=0;
    // int        str2_cnt=0;

    // CString str;
    // CString    str1 = L"";
    // CString    str2 = L"";
    // CString str_tmp = L"";
    // -anderson_variable 06/10/11 ///////////////////////////////////////
    FILE* FileOut; /* input files */
    FileOut = fopen(filePath, "wt");
    if (FileOut == NULL)
        return false;

    size_t i;
    t_one_line one_record;
    one_record.intel_adr = 0;
    one_record.intel_type = INTEL_DATA_TYPE;

    char buf[256] = { 0 };

    int iULBA = (unsigned int)((FileSize >> 16) & 0xFFFF);
    unsigned int idx = 0;

    do // for(unsigned int idx = 0; idx < iULBA; ++idx)
    {
        // Extended Linear Address Record (32-bit format only)
        memset(buf, 0, sizeof(buf));
        fprintf(FileOut, ":02000004%04X%02X\n", idx, (unsigned char)(0x100 - 0x06 - (idx & 0xFF) - ((idx >> 8) & 0xFF)));

        size_t len = iULBA ? 0x10000 : (FileSize & 0xFFFF);
        while (len > 0) {
            one_record.intel_lg_data = (LL_MAX_LINE > len) ? len : LL_MAX_LINE;
            memcpy(one_record.intel_data, vBuffer + one_record.intel_adr + (idx << 16), one_record.intel_lg_data);
            //            copy(vBuffer + one_record.intel_adr + (idx<<16),
            //                vBuffer + one_record.intel_adr + (idx<<16) + one_record.intel_lg_data,
            //                one_record.intel_data) ;

            len -= one_record.intel_lg_data;

            one_record.intel_lrc = one_record.intel_lg_data;
            one_record.intel_lrc += ((one_record.intel_adr >> 8) & 0xff);
            one_record.intel_lrc += (one_record.intel_adr & 0xff);
            one_record.intel_lrc += one_record.intel_type;

            memset(buf, 0, sizeof(buf));
            fprintf(FileOut, ":%02X%04X%02X", one_record.intel_lg_data, one_record.intel_adr, one_record.intel_type);

            for (i = 0; i < one_record.intel_lg_data; ++i) {
                memset(buf, 0, sizeof(buf));
                fprintf(FileOut, "%02X", (one_record.intel_data[i] & 0xff));
                one_record.intel_lrc += one_record.intel_data[i];
            }

            memset(buf, 0, sizeof(buf));
            fprintf(FileOut, "%02X\n", ((0x100 - one_record.intel_lrc) & 0xff));
            one_record.intel_adr += one_record.intel_lg_data;
        }
        idx++;
    } while (iULBA--);
    fprintf(FileOut, ":00000001FF\n");

    fclose(FileOut);

    return true;
}

bool HexFileToBin(const char* filePath, unsigned char* vOutData, unsigned long* FileSize, unsigned char PaddingByte)
{
    FILE* Filin; /* input files */

    /* size in bytes */
    const long MEMORY_SIZE = 16 * 1024 * 1024;
    const long ADDRESS_MASK = 0x00FFFFFF;
    const int MAX_LINE_SIZE = 256;
    const int NO_ADDRESS_TYPE_SELECTED = 0;
    const int LINEAR_ADDRESS = 1;
    const int SEGMENTED_ADDRESS = 2;

    unsigned int Seg_Lin_Select = NO_ADDRESS_TYPE_SELECTED;

    const int CKS_8 = 0;

    /* line inputted from file */
    char Line[MAX_LINE_SIZE];

    /* flag that a file was read */
    bool Enable_Checksum_Error = false;

    /* cmd-line parameter # */
    unsigned char* p;
    unsigned int i;

    /* Application specific */

    unsigned int Nb_Bytes;
    unsigned int First_Word, Address, Segment, Upper_Address;
    unsigned int Lowest_Address, Highest_Address, Starting_Address;
    unsigned int Phys_Addr, hType;
    unsigned int temp;

    bool Starting_Address_Setted = false;

    int temp2;

    unsigned char Data_Str[MAX_LINE_SIZE];
    unsigned char Checksum = 0;

    unsigned short int wCKS;
    unsigned short int w;
    unsigned int Cks_Type = CKS_8;
    unsigned int Cks_Start, Cks_End, Cks_Addr, Cks_Value;
    bool Cks_range_set = false;
    bool Cks_Addr_set = false;

    /* Just a normal file name */
    Filin = fopen(filePath, "r");
    if (Filin == 0L)
        return false;

    /* allocate a buffer */
    unsigned char* Memory_Block = (unsigned char*)malloc(MEMORY_SIZE);

    /* To begin, assume the lowest address is at the end of the memory.
  subsequent addresses will lower this number. At the end of the input
  file, this value will be the lowest address. */
    Lowest_Address = MEMORY_SIZE - 1;
    Highest_Address = 0;

    bool boEndofFile = false;

    /* To begin, assume the lowest address is at the end of the memory.
  subsequent addresses will lower this number. At the end of the input
  file, this value will be the lowest address. */
    Segment = 0;
    Upper_Address = 0;
    Lowest_Address = MEMORY_SIZE - 1;
    Highest_Address = 0;

    /* Now read the file & process the lines. */
    do /* repeat until EOF(Filin) */
    {
        /* Read a line from input file. */
        if (fgets(Line, MAX_LINE_SIZE, Filin) == NULL)
            break;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(Line) - 1;

        if (':' != Line[0] && boEndofFile == false) {
            free(Memory_Block);
            return false; // 0x0a != Line[0] && NULL != Line[0]) return false;
        }

        if (Line[i] == '\n')
            Line[i] = '\0';

        /* Scan the first two bytes and nb of bytes.
           The two bytes are read in First_Word since it's use depend on the
           record type: if it's an extended address record or a data record.
        */
        sscanf(Line, ":%2x%4x%2x%s", &Nb_Bytes, &First_Word, &hType, Data_Str);

        Checksum = Nb_Bytes + (First_Word >> 8) + (First_Word & 0xFF) + hType;

        p = Data_Str;

        /* If we're reading the last record, ignore it. */
        switch (hType) {
            /* Data record */
        case 0:
            Address = First_Word;

            if (Seg_Lin_Select == SEGMENTED_ADDRESS)
                Phys_Addr = (Segment << 4) + Address;
            else
                /* LINEAR_ADDRESS or NO_ADDRESS_TYPE_SELECTED
                   Upper_Address = 0 as specified in the Intel spec. until an extended address
                   record is read. */
                Phys_Addr = ((Upper_Address << 16) & ADDRESS_MASK) + Address;

            /* Check that the physical address stays in the buffer's range. */
            if ((Phys_Addr + Nb_Bytes) <= MEMORY_SIZE - 1) {
                /* Set the lowest address as base pointer. */
                if (Phys_Addr < Lowest_Address)
                    Lowest_Address = Phys_Addr;

                /* Same for the top address. */
                temp = Phys_Addr + Nb_Bytes - 1;

                if (temp > Highest_Address)
                    Highest_Address = temp;

                /* Read the Data bytes. */
                /* Bytes are written in the Memory block even if checksum is wrong. */
                if (Nb_Bytes == 0) {
                    free(Memory_Block);
                    return false;
                }
                for (i = Nb_Bytes; i > 0; i--) {
                    sscanf((const char*)(p), "%2x", &temp2);
                    p += 2;
                    Memory_Block[Phys_Addr++] = temp2;
                    Checksum = (Checksum + temp2) & 0xFF;
                };

                /* Read the Checksum value. */
                sscanf((const char*)(p), "%2x", &temp2);

                /* Verify Checksum value. */
                Checksum = (Checksum + temp2) & 0xFF;

                if ((Checksum != 0) && Enable_Checksum_Error) {
                }
            } else {
                if (Seg_Lin_Select == SEGMENTED_ADDRESS)
                    fprintf(stderr, "Data record skipped at %4X:%4X\n", Segment, Address);
                else
                    fprintf(stderr, "Data record skipped at %8X\n", Phys_Addr);
            }

            break;

            /* End of file record */
        case 1:
            /* Simply ignore checksum errors in this line. */
            boEndofFile = true;
            break;

            /* Extended segment address record */
        case 2:
            /* First_Word contains the offset. It's supposed to be 0000 so
        we ignore it. */

            /* First extended segment address record ? */
            if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED)
                Seg_Lin_Select = SEGMENTED_ADDRESS;

            /* Then ignore subsequent extended linear address records */
            if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
                sscanf((const char*)(p), "%4x%2x", &Segment, &temp2);

                /* Update the current address. */
                Phys_Addr = (Segment << 4) & ADDRESS_MASK;

                /* Verify Checksum value. */
                Checksum = (Checksum + (Segment >> 8) + (Segment & 0xFF) + temp2) & 0xFF;

                //                if ((Checksum != 0) && Enable_Checksum_Error)
                //                    Status_Checksum_Error = true;
            }
            break;

            /* Start segment address record */
        case 3:
            /* Nothing to be done since it's for specifying the starting address for
        execution of the binary code */
            break;

            /* Extended linear address record */
        case 4:
            /* First_Word contains the offset. It's supposed to be 0000 so
        we ignore it. */

            /* First extended linear address record ? */
            if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED)
                Seg_Lin_Select = LINEAR_ADDRESS;

            /* Then ignore subsequent extended segment address records */
            if (Seg_Lin_Select == LINEAR_ADDRESS) {
                sscanf((const char*)(p), "%4x%2x", &Upper_Address, &temp2);

                /* Update the current address. */
                Phys_Addr = Upper_Address << 4;

                /* Verify Checksum value. */
                Checksum = (Checksum + (Upper_Address >> 8) + (Upper_Address & 0xFF) + temp2) & 0xFF;

                //                if ((Checksum != 0) && Enable_Checksum_Error)
                //                    Status_Checksum_Error = true;
            }
            break;

            /* Start linear address record */
        case 5:
            /* Nothing to be done since it's for specifying the starting address for
               execution of the binary code */
            break;
        default:
            break; // 20040617+ Added to remove GNU compiler warning about label at end of compound statement
        }
    } while (!feof(Filin));
    /*-----------------------------------------------------------------------------*/

    //     fprintf(stdout,"Lowest address  = %08X\n",Lowest_Address);
    //     fprintf(stdout,"Highest address = %08X\n",Highest_Address);
    //     fprintf(stdout,"Pad Byte        = %X\n",  PadByte);

    wCKS = 0;
    if (!Cks_range_set) {
        Cks_Start = Lowest_Address;
        Cks_End = Highest_Address;
    }
    switch (Cks_Type) {
    case 0:

        for (i = Cks_Start; i <= Cks_End; i++) {
            wCKS += Memory_Block[i];
        }

        // fprintf(stdout,"8-bit Checksum = %02X\n",wCKS & 0xff);
        if (Cks_Addr_set) {
            wCKS = Cks_Value - (wCKS - Memory_Block[Cks_Addr]);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS & 0xff);
            // fprintf(stdout,"Addr %08X set to %02X\n",Cks_Addr, wCKS & 0xff);
        }
        break;

    case 2:

        for (i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i + 1] | ((unsigned short)Memory_Block[i] << 8);
            wCKS += w;
        }

        // fprintf(stdout,"16-bit Checksum = %04X\n",wCKS);
        if (Cks_Addr_set) {
            w = Memory_Block[Cks_Addr + 1] | ((unsigned short)Memory_Block[Cks_Addr] << 8);
            wCKS = Cks_Value - (wCKS - w);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS >> 8);
            Memory_Block[Cks_Addr + 1] = (unsigned char)(wCKS & 0xff);
            // fprintf(stdout,"Addr %08X set to %04X\n",Cks_Addr, wCKS);
        }
        break;

    case 1:

        for (i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i] | ((unsigned short)Memory_Block[i + 1] << 8);
            wCKS += w;
        }

        // fprintf(stdout,"16-bit Checksum = %04X\n",wCKS);
        if (Cks_Addr_set) {
            w = Memory_Block[Cks_Addr] | ((unsigned short)Memory_Block[Cks_Addr + 1] << 8);
            wCKS = Cks_Value - (wCKS - w);
            Memory_Block[Cks_Addr + 1] = (unsigned char)(wCKS >> 8);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS & 0xff);
            // fprintf(stdout,"Addr %08X set to %04X\n",Cks_Addr, wCKS);
        }

    default:;
    }

    /* This starting address is for the binary file,

       ex.: if the first record is :nn010000ddddd...
       the data supposed to be stored at 0100 will start at 0000 in the binary file.

       Specifying this starting address will put FF bytes in the binary file so that
       the data supposed to be stored at 0100 will start at the same address in the
       binary file.
     */

    if (Starting_Address_Setted) {
        Lowest_Address = Starting_Address;
    }

    if (pBufferforLoadedFile != NULL)
        free(pBufferforLoadedFile);
    //    pBufferforLoadedFile=(unsigned char*)malloc(Highest_Address - Lowest_Address + 1);
    pBufferforLoadedFile = (unsigned char*)malloc(Highest_Address + 1);

    if (Lowest_Address < Highest_Address) {
        //        memcpy(pBufferforLoadedFile,Memory_Block + Lowest_Address, Highest_Address - Lowest_Address + 1 ) ;
        //        g_ulFileSize=(Highest_Address -Lowest_Address+ 1); ;
        memcpy(pBufferforLoadedFile, Memory_Block, Highest_Address + 1);
        g_ulFileSize = (Highest_Address + 1);
    }

    free(Memory_Block);
    return true;
}
