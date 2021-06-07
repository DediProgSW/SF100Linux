//
//
/****INTEL HEX-RECORD FORMAT, copied from http://www.cs.net/lucid/intel.htm****

INTRODUCTION
Intel's Hex-record format allows program or data files to be encoded in a
printable (ASCII) format. This allows viewing of the object file with standard
tools and easy file transfer from one computer to another, or between a host
and target. An individual Hex-record is a single line in a file composed of
many Hex-records.

HEX-RECORD CONTENT
Hex-Records are character strings made of several fields which specify the
record type, record length, memory address, data, and checksum. Each byte of
binary data is encoded as a 2-character hexadecimal number: the first ASCII
character representing the high-order 4 bits, and the second the low-order 4
bits of the byte.

The 6 fields which comprise a Hex-record are defined as follows:
    Field       Characters  Description
1   Start code  1           An ASCII colon, ":".
2   Byte count  2           The count of the character pairs in the data field.
3   Address     4           The 2-byte address at which the data field is to be loaded into memory.
4   Type        2           00, 01, or 02.
5   Data        0-2n        From 0 to n bytes of executable code, or memory loadable data.
                            n is normally 20 hex (32 decimal) or less.
6   Checksum    2           The least significant byte of the two's complement sum of the values represented by all the pairs of characters in the record except the start code and checksum.

Each record may be terminated with a CR/LF/NULL. Accuracy of transmission is ensured by the byte count and checksum fields.

HEX-RECORD TYPES
There are three possible types of Hex-records.
00
A record containing data and the 2-byte address at which the data is to reside.
01
A termination record for a file of Hex-records. Only one termination record is allowed per file and it must be the last line of the file. There is no data field.
02
A segment base address record. This type of record is ignored by Lucid programmers.


HEX-RECORD EXAMPLE
Following is a typical Hex-record module consisting of four data records and a termination record.

     :10010000214601360121470136007EFE09D2190140
     :100110002146017EB7C20001FF5F16002148011988
     :10012000194E79234623965778239EDA3F01B2CAA7
     :100130003F0156702B5E712B722B732146013421C7
     :00000001FF

The first data record is explained as follows:
     :     Start code.

     10    Hex 10 (decimal 16), indicating 16 data character
           pairs, 16 bytes of binary data, in this record.

     01    Four-character 2-byte address field:  hex address 0100,
     00    indicates location where the following data is to be loaded.

     00    Record type indicating a data record.

The next 16 character pairs are the ASCII bytes of the actual program data.

     40    Checksum of the first Hex-record.


The termination record is explained as follows:

     :     Start code.

     00    Byte count is zero, no data in termination record.

     00    Four-character 2-byte address field, zeros.
     00

     01    Record type 01 is termination.

     FF    Checksum of termination record.


--------------------------------------------------------------------------------

[ Back to Data & Documentation ] [ Home ]
--------------------------------------------------------------------------------


*/

/* Intel Hex format specifications

The 8-bit Intel Hex File Format is a printable ASCII format consisting of one
 or more data records followed by an end of file record. Each
record consists of one line of information. Data records may appear in any
 order. Address and data values are represented as 2 or 4 hexadecimal
digit values.

Record Format
:LLAAAARRDDDD......DDDDCC


LL
AAAA
RR
DD
CC
Length field. Number of data bytes.
Address field. Address of first byte.
Record type field. 00 for data and 01 for end of record.
Data field.
Checksum field. One's complement of length, address, record type and data
 fields modulo 256.
CC = LL + AAAA + RR + all DD = 0

Example:
:06010000010203040506E4
:00000001FF

The first line in the above example Intel Hex file is a data record addressed
 at location 100H with data values 1 to 6. The second line is the end
of file record, so that the LL field is 0

*/

#ifndef INTELHEXFILE_H
#define INTELHEXFILE_H

#include <stdbool.h>

#define LL_MAX_LINE 16
typedef struct {
    unsigned char intel_lg_data;
    unsigned short intel_adr;
    unsigned char intel_type;
    unsigned char intel_data[LL_MAX_LINE];
    unsigned char intel_lrc;
} t_one_line;

#define INTEL_DATA_TYPE 0

//save binary data in vBuffer to file in Intel Hex format
bool BinToHexFile(const char* filePath, unsigned char* vBuffer, unsigned long FileSize);

// read binary data from Intel Hex file
bool HexFileToBin(const char* filePath, unsigned char* vOutData, unsigned long* FileSize, unsigned char PaddingByte);

#endif // end of header file
