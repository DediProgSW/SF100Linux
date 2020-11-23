//--------------------------------------------------------------------------------
//NAME
//srec - S-record file and record format
//DESCRIPTION

//An S-record file consists of a sequence of specially formatted ASCII character strings. An S-record will be less than or equal to 78 bytes in length.
//The order of S-records within a file is of no significance and no particular order may be assumed.

//The general format of an S-record follows:

//+-------------------//------------------//-----------------------+
//| type | count | address | data | checksum |
//+-------------------//------------------//-----------------------+

//type -- A char[2] field. These characters describe the type of record (S0, S1, S2, S3, S5, S7, S8, or S9).
//count -- A char[2] field. These characters when paired and interpreted as a hexadecimal value, display the count of remaining character pairs in the record.

//address -- A char[4,6, or 8] field. These characters grouped and interpreted as a hexadecimal value, display the address at which the data field is to be loaded into memory. The length of the field depends on the number of bytes necessary to hold the address. A 2-byte address uses 4 characters, a 3-byte address uses 6 characters, and a 4-byte address uses 8 characters.

//data -- A char [0-64] field. These characters when paired and interpreted as hexadecimal values represent the memory loadable data or descriptive information.

//checksum -- A char[2] field. These characters when paired and interpreted as a hexadecimal value display the least significant byte of the ones complement of the sum of the byte values represented by the pairs of characters making up the count, the address, and the data fields.

//Each record is terminated with a line feed. If any additional or different record terminator(s) or delay characters are needed during transmission to the target system it is the responsibility of the transmitting program to provide them.

//S0 Record. The type of record is 'S0' (0x5330). The address field is unused and will be filled with zeros (0x0000). The header information within the data field is divided into the following subfields.

//mname is char[20] and is the module name.
//ver is char[2] and is the version number.
//rev is char[2] and is the revision number.
//description is char[0-36] and is a text comment.

//Each of the subfields is composed of ASCII bytes whose associated characters, when paired, represent one byte hexadecimal values in the case of the version and revision numbers, or represent the hexadecimal values of the ASCII characters comprising the module name and description.
//S1 Record. The type of record field is 'S1' (0x5331). The address field is intrepreted as a 2-byte address. The data field is composed of memory loadable data.

//S2 Record. The type of record field is 'S2' (0x5332). The address field is intrepreted as a 3-byte address. The data field is composed of memory loadable data.

//S3 Record. The type of record field is 'S3' (0x5333). The address field is intrepreted as a 4-byte address. The data field is composed of memory loadable data.

//S5 Record. The type of record field is 'S5' (0x5335). The address field is intrepreted as a 2-byte value and contains the count of S1, S2, and S3 records previously transmitted. There is no data field.

//S7 Record. The type of record field is 'S7' (0x5337). The address field contains the starting execution address and is intrepreted as 4-byte address. There is no data field.

//S8 Record. The type of record field is 'S8' (0x5338). The address field contains the starting execution address and is intrepreted as 3-byte address. There is no data field.

//S9 Record. The type of record field is 'S9' (0x5339). The address field contains the starting execution address and is intrepreted as 2-byte address. There is no data field.

//EXAMPLE

//Shown below is a typical S-record format file.

//S00600004844521B
//S1130000285F245F2212226A000424290008237C2A
//S11300100002000800082629001853812341001813
//S113002041E900084E42234300182342000824A952
//S107003000144ED492
//S5030004F8
//S9030000FC
//The file consists of one S0 record, four S1 records, one S5 record and an S9 record.

//The S0 record is comprised as follows:

//S0 S-record type S0, indicating it is a header record.
//06 Hexadecimal 06 (decimal 6), indicating that six character pairs (or ASCII bytes) follow.
//00 00 Four character 2-byte address field, zeroes in this example.

//48 44 52 ASCII H, D, and R - "HDR".
//1B The checksum.
//The first S1 record is comprised as follows:
//S1 S-record type S1, indicating it is a data record to be loaded at a 2-byte address.
//13 Hexadecimal 13 (decimal 19), indicating that nineteen character pairs, representing a 2 byte address, 16 bytes of binary data, and a 1 byte checksum, follow.
//00 00 Four character 2-byte address field; hexidecimal address 0x0000, where the data which follows is to be loaded.
//28 5F 24 5F 22 12 22 6A 00 04 24 29 00 08 23 7C Sixteen character pairs representing the actual binary data.
//2A The checksum.
//The second and third S1 records each contain 0x13 (19) character pairs and are ended with checksums of 13 and 52, respectively. The fourth S1 record contains 07 character pairs and has a checksum of 92.

//The S5 record is comprised as follows:

//S5 S-record type S5, indicating it is a count record indicating the number of S1 records
//03 Hexadecimal 03 (decimal 3), indicating that three character pairs follow.
//00 04 Hexadecimal 0004 (decimal 4), indicating that there are four data records previous to this record.
//F8 The checksum.
//The S9 record is comprised as follows:

//S9 S-record type S9, indicating it is a termination record.
//03 Hexadecimal 03 (decimal 3), indicating that three character pairs follow.
//00 00 The address field, hexadecimal 0 (decimal 0) indicating the starting execution address.
//FC The checksum.

//--------------------------------------------------------------------------------

//Instructor Notes
//There isn't any evidence that Motorola ever has made use of the header information within the data field of the S0 record, as described above. This must have been used by some third party vendors.
//This is the only place that a 78-byte limit on total record length or 64-byte limit on data length is documented. These values shouldn't be trusted for the general case.
//The count field can have values in the range of 0x3 (2 bytes of address + 1 byte checksum = 3, a not very useful record) to 0xff; this is the count of remaining character pairs, including checksum.
//If you write code to convert S-Records, you should always assume that a record can be as long as 514 (decimal) characters in length (255 * 2 = 510, plus 4 characters for the type and count fields),
//plus any terminating character(s). That is, in establishing an input buffer in C, you would declare it to be an array of 515 chars, thus leaving room for the terminating null character.
#include "MotorolaFile.h"
#include "project.h"
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern unsigned char* pBufferforLoadedFile;
extern unsigned long g_ulFileSize;

bool S19FileToBin(const char* filePath, unsigned char* vData, unsigned long* FileSize, unsigned char PaddingByte)
{
    FILE* Filin; /* input files */

    /* size in bytes */
    const long MEMORY_SIZE = 1024 * 1024 * 4;
    const long ADDRESS_MASK = 0x003FFFFF;
    const int MAX_LINE_SIZE = 512;

    const int CKS_8 = 0;

    int PadByte = PaddingByte;

    /* line inputted from file */
    char Line[MAX_LINE_SIZE];

    /* flag that a file was read */
    bool Enable_Checksum_Error = false;

    /* cmd-line parameter # */
    unsigned char* p;
    unsigned int i;

    /* Application specific */

    unsigned int Nb_Bytes;
    unsigned int Address, Lowest_Address, Highest_Address, Starting_Address;
    unsigned int Phys_Addr, sType;
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
    Filin = fopen(filePath, "rt");
    if (Filin == 0L)
        return false;

    /* allocate a buffer */
    unsigned char* Memory_Block = (unsigned char*)malloc(MEMORY_SIZE);
    memset(Memory_Block, PadByte, MEMORY_SIZE);

    /* To begin, assume the lowest address is at the end of the memory.
    subsequent addresses will lower this number. At the end of the input
    file, this value will be the lowest address. */
    Lowest_Address = MEMORY_SIZE - 1;
    Highest_Address = 0;

    //    int readCnt = 0 ;
    //    int checkCnt = 0 ;

    /* Now read the file & process the lines. */
    do /* repeat until EOF(Filin) */
    {
        /* Read a line from input file. */
        if (fgets(Line, MAX_LINE_SIZE, Filin) == NULL)
            break;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(Line) - 1;

        if (Line[i] == '\n')
            Line[i] = '\0';

        // check if this file is an S-file
        if ('S' != Line[0] && Line[0] != '\0')
            return false;

        /* Scan starting address and nb of bytes. */
        /* Look at the record sType after the 'S' */
        switch (Line[1]) {
            /* 16 bits address */
        case '1':
            sscanf(Line, "S%1x%2x%4x%s", &sType, &Nb_Bytes, &Address, Data_Str);
            Checksum = Nb_Bytes + (Address >> 8) + (Address & 0xFF);

            /* Adjust Nb_Bytes for the number of data bytes */
            Nb_Bytes = Nb_Bytes - 3;
            break;

            /* 24 bits address */
        case '2':
            sscanf(Line, "S%1x%2x%6x%s", &sType, &Nb_Bytes, &Address, Data_Str);
            Checksum = Nb_Bytes + (Address >> 16) + (Address >> 8) + (Address & 0xFF);

            /* Adjust Nb_Bytes for the number of data bytes */
            Nb_Bytes = Nb_Bytes - 4;
            break;

            /* 32 bits address */
        case '3':
            sscanf(Line, "S%1x%2x%8x%s", &sType, &Nb_Bytes, &Address, Data_Str);
            Checksum = Nb_Bytes + (Address >> 24) + (Address >> 16) + (Address >> 8) + (Address & 0xFF);

            /* Adjust Nb_Bytes for the number of data bytes */
            Nb_Bytes = Nb_Bytes - 5;
            break;

        default:
            break; // 20040617+ Added to remove GNU compiler warning about label at end of compound statement
        }

        p = Data_Str;

        /* If we're reading the last record, ignore it. */
        switch (sType) {
            /* Data record */
        case 1:
        case 2:
        case 3:
            Phys_Addr = Address & ADDRESS_MASK;

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
                for (i = Nb_Bytes; i > 0; i--) {
                    sscanf((const char*)(p), "%2x", &temp2);
                    p += 2;
                    Memory_Block[Phys_Addr++] = temp2;
                    Checksum = (Checksum + temp2) & 0xFF;
                };

                /* Read the Checksum value. */
                sscanf((const char*)(p), "%2x", &temp2);

                /* Verify Checksum value. */
                Checksum = (0xFF - Checksum) & 0xFF;

                if ((temp2 != Checksum) && Enable_Checksum_Error) {
                }
            } else {
                //fprintf(stderr,"Data record skipped at %8X\n",Phys_Addr);
            }
            break;

            /* Ignore all other records */
        default:
            break; // 20040617+ Added to remove GNU compiler warning about label at end of compound statement
        }
    } while (!feof(Filin));
    fclose(Filin);
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

        //fprintf(stdout,"8-bit Checksum = %02X\n",wCKS & 0xff);
        if (Cks_Addr_set) {
            wCKS = Cks_Value - (wCKS - Memory_Block[Cks_Addr]);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS & 0xff);
            //fprintf(stdout,"Addr %08X set to %02X\n",Cks_Addr, wCKS & 0xff);
        }
        break;

    case 2:

        for (i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i + 1] | ((unsigned short)Memory_Block[i] << 8);
            wCKS += w;
        }

        //fprintf(stdout,"16-bit Checksum = %04X\n",wCKS);
        if (Cks_Addr_set) {
            w = Memory_Block[Cks_Addr + 1] | ((unsigned short)Memory_Block[Cks_Addr] << 8);
            wCKS = Cks_Value - (wCKS - w);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS >> 8);
            Memory_Block[Cks_Addr + 1] = (unsigned char)(wCKS & 0xff);
            //fprintf(stdout,"Addr %08X set to %04X\n",Cks_Addr, wCKS);
        }
        break;

    case 1:

        for (i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i] | ((unsigned short)Memory_Block[i + 1] << 8);
            wCKS += w;
        }

        //fprintf(stdout,"16-bit Checksum = %04X\n",wCKS);
        if (Cks_Addr_set) {
            w = Memory_Block[Cks_Addr] | ((unsigned short)Memory_Block[Cks_Addr + 1] << 8);
            wCKS = Cks_Value - (wCKS - w);
            Memory_Block[Cks_Addr + 1] = (unsigned char)(wCKS >> 8);
            Memory_Block[Cks_Addr] = (unsigned char)(wCKS & 0xff);
            //fprintf(stdout,"Addr %08X set to %04X\n",Cks_Addr, wCKS);
        }

    default:
        break;
    }

    if (Starting_Address_Setted) {
        Lowest_Address = Starting_Address;
    }

    if (pBufferforLoadedFile != NULL)
        free(pBufferforLoadedFile);
    //    pBufferforLoadedFile=(unsigned char*)malloc(Highest_Address-Lowest_Address+1);
    //    g_ulFileSize=Highest_Address -Lowest_Address+ 1;
    //    memcpy(pBufferforLoadedFile,Memory_Block+Lowest_Address,Highest_Address-Lowest_Address+1);
    pBufferforLoadedFile = (unsigned char*)malloc(Highest_Address + 1);
    g_ulFileSize = Highest_Address + 1;
    memcpy(pBufferforLoadedFile, Memory_Block, Highest_Address + 1);

    free(Memory_Block);
    return true;
}

bool BinToS19File(const char* filePath, unsigned char* vData, unsigned long FileSize)
{
    //ANSI only
    FILE* FileOut; /* input files */
    FileOut = fopen(filePath, "wt");
    if (FileOut == NULL)
        return false;

    int i;
    int chksum;
    int total = 0;
    int addr = 0x00000000;
    long l1total = FileSize;
    unsigned char recbuf[32];
    char s19buf[128];

    while (l1total >= 32) {
        // copy data
        total = 32;
        memcpy(recbuf, vData + addr, total);

        chksum = total + 5; /* total bytes in this record */
        chksum += addr & 0xff;
        chksum += (addr >> 8) & 0xff;
        chksum += (addr >> 16) & 0xff;
        chksum += (addr >> 24) & 0xff;

        fprintf(FileOut, "S3");
        fprintf(FileOut, "%02X%08X", total + 5, addr);

        memset(s19buf, 0, sizeof(s19buf));
        for (i = 0; i < total; i++) {
            chksum += recbuf[i];
            sprintf(s19buf + 2 * i, "%02X", (recbuf[i] & 0xff));
        }
        fprintf(FileOut, "%s", s19buf);

        memset(s19buf, 0, sizeof(s19buf));
        chksum = ~chksum; /* one's complement */
        fprintf(FileOut, "%02X\n", (chksum & 0xff));

        addr += 32;
        l1total -= 32;
    }
    if (l1total) {
        total = l1total;
        memcpy(recbuf, vData + addr, total);

        chksum = total + 5; /* total bytes in this record */
        chksum += addr & 0xff;
        chksum += (addr >> 8) & 0xff;
        chksum += (addr >> 16) & 0xff;
        chksum += (addr >> 24) & 0xff;

        fprintf(FileOut, "S3"); /* record header preamble */
        fprintf(FileOut, "%02X%08X", total + 5, addr);

        memset(s19buf, 0, sizeof(s19buf));
        for (i = 0; i < total; i++) {
            chksum += recbuf[i];
            sprintf(s19buf + 2 * i, "%02X", (recbuf[i] & 0xff));
        }
        fprintf(FileOut, "%s", s19buf);

        memset(s19buf, 0, sizeof(s19buf));
        chksum = ~chksum; /* one's complement */
        fprintf(FileOut, "%02X\n", (chksum & 0xff));

        addr += l1total;
    }

    fclose(FileOut);
    return true;
}
