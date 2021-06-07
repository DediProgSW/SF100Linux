#pragma once

#ifndef _MOTOROLA_FILE_H
#define _MOTOROLA_FILE_H

#include <stdbool.h>

bool S19FileToBin(const char* filePath, unsigned char* vData, unsigned long* FileSize, unsigned char PaddingByte);
bool BinToS19File(const char* filePath, unsigned char* vData, unsigned long FileSize);

#endif //_MOTOROLA_FILE_H
