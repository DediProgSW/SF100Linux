#pragma once

#ifndef _PARSE_H
#define _PARSE_H
int Dedi_Search_Chip_Db(long RDIDCommand, long UniqueID, CHIP_INFO *Chip_Info, int search_all);
int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO *Chip_Info, int Index);
void Dedi_List_AllChip();
#endif
