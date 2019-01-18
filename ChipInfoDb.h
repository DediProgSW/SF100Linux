#pragma once
#ifndef ChipInfo_H

#define ChipInfo_H

#define NUMBER_OF_SUPPORTING_CHIPS  3

struct m_code_api
{
	int (*m_code_api_doRDSR)(unsigned char *cSR, int Index);
	int (*m_code_api_doWRSR)(unsigned char cSR,int Index);
	int (*m_code_api_doChipErase)(int Index);
	int (*m_code_api_doProgram)(void);
	int (*m_code_api_doRead)(char *name);
	int (*m_code_api_doSegmentErase)(void);
};



int ChipInfoDbFindItem(CHIP_INFO ChipInfoDb[], int NumberOfItems, long JedecDeviceIDToFind);

void ChipInfoDump(long JedecDeviceIDToFind);

long ChipInfoDumpChipSizeInKByte(long Jedec);
int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO *Chip_Info, int Index);
void getExecPath(char* Path);


#endif








