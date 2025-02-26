#include "ChipInfoDb.h"
#include "project.h"
#include <ctype.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#ifdef __FreeBSD__
#include <sys/auxv.h>
#endif

#define pathbufsize 1024
#define testbufsize 256
#define linebufsize 512
#define filebufsize 1024 * 1024
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define max(a, b) (((a) > (b)) ? (a) : (b))

//using namespace pugi;
//xml_document doc;
#if 1
 bool GetChipDbPath(char *Path)
{
    FILE* fp = NULL;

	if(Path == NULL)
		return false;
	
	memset(Path, 0, linebufsize);
    if (readlink("/proc/self/exe", Path, 512) != -1) {
        dirname(Path);
        strcat(Path, "/ChipInfoDb.dedicfg");
        if ((fp = fopen(Path, "rt")) == NULL) {
            // ChipInfoDb.dedicfg not in program directory
            dirname(Path);
            dirname(Path);
            strcat(Path, "/share/DediProg/ChipInfoDb.dedicfg");
            //			printf("%s\n",Path);
            if ((fp = fopen(Path, "rt")) == NULL)
            {
                fprintf(stderr, "Error opening file: %s\n", Path);
				return false;
            }
        }
		if(fp)
			fclose(fp);
		//printf("\r\n%s\r\n", Path);
		return true;
	}
	return false;
}

 
int Dedi_Search_Chip_Db(char* chTypeName, long RDIDCommand,
    long UniqueID,
    CHIP_INFO* Chip_Info,
    int search_all)
{
    int found_flag = 0;
	char file_path[linebufsize];
    int detectICNum = 0;
    char strTypeName[32][100];
    CHIP_INFO Chip_Info_temp;
	xmlDocPtr    doc;
	xmlNodePtr   cur_node;
	xmlChar      *chip_attribute;

    for (int i = 0; i < 32; i++)
        memset(strTypeName[i], '\0', 100);

    memset(chTypeName, '\0', 1024);

    memset(Chip_Info->TypeName, '\0', sizeof(Chip_Info->TypeName)); //strlen(Chip_Info->TypeName)=0

    if (GetChipDbPath(file_path) == false)
    {
        return 1;
    }

	doc = xmlParseFile(file_path);
	cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
	cur_node = cur_node->children->next; //Portofolio
	cur_node = cur_node->children->next; //Chip
	//printf("UniqueID=0x%08X\n", UniqueID);
    while (cur_node != NULL) {
      if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"Chip"))) {
	  	
		if (found_flag == 1) {
			found_flag = 0;

			if (strlen(Chip_Info->TypeName) == 0)
				*Chip_Info = Chip_Info_temp; //first chip info
			detectICNum++;
			memset(&Chip_Info_temp, 0, sizeof(CHIP_INFO));
		}
        chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"TypeName");
		if(chip_attribute){
            
			strcpy(Chip_Info_temp.TypeName, (char*)chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ICType");
		if(chip_attribute){
            
			strcpy(Chip_Info_temp.ICType, (char*)chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Class");
		if(chip_attribute){
			strcpy(Chip_Info_temp.Class, (char*)chip_attribute);
		}
	  	chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Manufacturer");
		if(chip_attribute){
			strcpy(Chip_Info_temp.Manufacturer, (char*)chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"UniqueID");
		if(chip_attribute){
			Chip_Info_temp.UniqueID = strtol((char*)chip_attribute, NULL, 16);;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Voltage");
		if(chip_attribute){
			strcpy(Chip_Info_temp.Voltage, (char*)chip_attribute);
			if (strstr((char *)chip_attribute, "3.3V") != NULL)
                Chip_Info_temp.VoltageInMv = 3300;
            else if (strstr((char *)chip_attribute, "2.5V") != NULL)
                Chip_Info_temp.VoltageInMv = 2500;
            else if (strstr((char *)chip_attribute, "1.8V") != NULL)
                Chip_Info_temp.VoltageInMv = 1800;
            else
                Chip_Info_temp.VoltageInMv = 3300;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"JedecDeviceID");
		if(chip_attribute){
			Chip_Info_temp.JedecDeviceID = strtol((char*)chip_attribute, NULL, 16);
			if ((UniqueID == Chip_Info_temp.JedecDeviceID)) {
                found_flag = 1;

                strcpy(strTypeName[detectICNum], Chip_Info_temp.TypeName);
                strcat(chTypeName, " ");
                strcat(chTypeName, strTypeName[detectICNum]);
            }
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ChipSizeInKByte");
		if(chip_attribute){
			Chip_Info_temp.ChipSizeInByte = strtol((char*)chip_attribute, NULL, 10) * 1024;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SectorSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.SectorSizeInByte = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"BlockSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.BlockSizeInByte = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"PageSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.PageSizeInByte = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"AddrWidth");
		if(chip_attribute){
			Chip_Info_temp.AddrWidth = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"IDNumber");
		if(chip_attribute){
			Chip_Info_temp.IDNumber = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"VppSupport");
		if(chip_attribute){
			Chip_Info_temp.VppSupport = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDIDCommand");
		if(chip_attribute){
			Chip_Info_temp.RDIDCommand = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Timeout");
		if(chip_attribute){
			Chip_Info_temp.Timeout = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"MXIC_WPmode");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.MXIC_WPmode = true;
            else
                Chip_Info_temp.MXIC_WPmode = false;
		}
		//SPI NAND
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SpareSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.SpareSizeInByte = (strtol((char*)chip_attribute, NULL, 16)/*&0xFFFF*/);
			//printf("SpareSizeInByte=%ld\n",Chip_Info_temp.SpareSizeInByte);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"nCA_Rd");
		if(chip_attribute){
			Chip_Info_temp.nCA_Rd = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"nCA_Wr");
		if(chip_attribute){
			Chip_Info_temp.nCA_Wr = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"DefaultDataUnitSize");
		if(chip_attribute){
			Chip_Info_temp.DefaultDataUnitSize = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"DefaultErrorBits");
		if(chip_attribute){
			Chip_Info_temp.DefaultErrorBits = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"BBMark");
		if(chip_attribute){
			Chip_Info_temp.BBMark = strtol((char*)chip_attribute, NULL, 10);
		}
        chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SupportedProduct");
		if(chip_attribute){
			Chip_Info_temp.SupportedProduct = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCParityGroup");
		if(chip_attribute){
			Chip_Info_temp.ECCParityGroup = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectStartAddr");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectStartAddr = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectDis");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectDis = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectLength");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectLength = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCEnable");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.ECCEnable = true;
            else
                Chip_Info_temp.ECCEnable = false;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"QPIEnable");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.QPIEnable = true;
            else
                Chip_Info_temp.QPIEnable = false;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ChipEraseTime");
		if(chip_attribute){
			Chip_Info_temp.ChipEraseTime = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"EraseCmd");
		if(chip_attribute){
			Chip_Info_temp.EraseCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ProgramCmd");
		if(chip_attribute){
			Chip_Info_temp.ProgramCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadCmd");
		if(chip_attribute){
			Chip_Info_temp.ReadCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ProtectBlockMask");
		if(chip_attribute){
			Chip_Info_temp.ProtectBlockMask = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"UnlockCmd");
		if(chip_attribute){
			Chip_Info_temp.UnlockCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDSRCnt");
		if(chip_attribute){
			Chip_Info_temp.RDSRCnt = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRSRCnt");
		if(chip_attribute){
			Chip_Info_temp.WRSRCnt = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRSRCmd");
		if(chip_attribute){
			Chip_Info_temp.WRSRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRCRCmd");
		if(chip_attribute){
			Chip_Info_temp.WRCRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDSRCmd");
		if(chip_attribute){
			Chip_Info_temp.RDSRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDCRCmd");
		if(chip_attribute){
			Chip_Info_temp.RDCRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"QEbitAddr");
		if(chip_attribute){
			Chip_Info_temp.QEbitAddr = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ChipIDMask");
		if(chip_attribute){
			Chip_Info_temp.ChipIDMask = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SupportLUT");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.SupportLUT = true;
            else
                Chip_Info_temp.SupportLUT = false;
		}
	
        xmlFree(chip_attribute);
      }
      cur_node = cur_node->next;
  }

  xmlFreeDoc(doc);
	if(detectICNum)
		found_flag=1;
  Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
    /*Read into the buffer contents within thr file stream*/
    return found_flag; 
}
 
int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO* Chip_Info)
{
    int found_flag = 0;
	char file_path[linebufsize];
    CHIP_INFO Chip_Info_temp;
	xmlDocPtr    doc;
	xmlNodePtr   cur_node;
	xmlChar      *chip_attribute;

    if (GetChipDbPath(file_path) == false)
    {
        return 1;
    }

  
	doc = xmlParseFile(file_path);
	cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
	cur_node = cur_node->children->next; //Portofolio
	cur_node = cur_node->children->next; //C
    //	data_temp = file_buf;
    /*Read into the buffer contents within thr file stream*/
    while (cur_node != NULL) {
	    if (found_flag == 1) {
			if (strlen(Chip_Info->TypeName) == 0)
				*Chip_Info = Chip_Info_temp; //first chip info

			break;
		}
		
      if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"Chip"))) {
	      
          chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"TypeName");
		  
		  if(chip_attribute){
		  	
            if (!xmlStrcmp(chip_attribute, (const xmlChar *)TypeName)) {
				found_flag = 1;
				//printf("chip name=%s\n",chip_attribute);
			} else 
				found_flag = 0;
			memset(&Chip_Info_temp, 0, sizeof(CHIP_INFO));
			strcpy(Chip_Info_temp.TypeName, (char*)chip_attribute);
			
		  }
       }
	  if(found_flag == 0)
	  {
	  	cur_node = cur_node->next;
	 	 continue;
	  }else {
	  	chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Class");
		if(chip_attribute){
			strcpy(Chip_Info_temp.Class, (char*)chip_attribute);
        	//printf("Class: %s\n", chip_attribute);
		}
	  	chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Manufacturer");
		if(chip_attribute){
			strcpy(Chip_Info_temp.Manufacturer, (char*)chip_attribute);
        	//printf("Manufacturer: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"UniqueID");
		if(chip_attribute){
			Chip_Info_temp.UniqueID = strtol((char*)chip_attribute, NULL, 16);;
        	//printf("UniqueID: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Voltage");
		if(chip_attribute){
        	//printf("Voltage: %s\n", chip_attribute);
			if (strstr((char *)chip_attribute, "3.3V") == 0)
                Chip_Info_temp.VoltageInMv = 3300;
            else if (strstr((char *)chip_attribute, "2.5V") == 0)
                Chip_Info_temp.VoltageInMv = 2500;
            else if (strstr((char *)chip_attribute, "1.8V") == 0)
                Chip_Info_temp.VoltageInMv = 1800;
            else
                Chip_Info_temp.VoltageInMv = 3300;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"JedecDeviceID");
		if(chip_attribute){
			Chip_Info_temp.JedecDeviceID = strtol((char*)chip_attribute, NULL, 16);
        	//printf("JedecDeviceID: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ChipSizeInKByte");
		if(chip_attribute){
			Chip_Info_temp.ChipSizeInByte = strtol((char*)chip_attribute, NULL, 10) * 1024;
        	//printf("ChipSizeInKByte: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SectorSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.SectorSizeInByte = strtol((char*)chip_attribute, NULL, 10);
        	//printf("SectorSizeInKByte: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"BlockSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.BlockSizeInByte = strtol((char*)chip_attribute, NULL, 10);
        	//printf("BlockSizeInByte: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"PageSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.PageSizeInByte = strtol((char*)chip_attribute, NULL, 10);
        	//printf("PageSizeInByte: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"AddrWidth");
		if(chip_attribute){
			Chip_Info_temp.AddrWidth = strtol((char*)chip_attribute, NULL, 10);
        	//printf("AddrWidth: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
        	//printf("ReadDummyLen: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
        	//printf("ReadDummyLen: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"IDNumber");
		if(chip_attribute){
			Chip_Info_temp.IDNumber = strtol((char*)chip_attribute, NULL, 10);
        	//printf("IDNumber: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"VppSupport");
		if(chip_attribute){
			Chip_Info_temp.VppSupport = strtol((char*)chip_attribute, NULL, 10);
        	//printf("VppSupport: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDIDCommand");
		if(chip_attribute){
			Chip_Info_temp.RDIDCommand = strtol((char*)chip_attribute, NULL, 16);
        	//printf("RDIDCommand: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Timeout");
		if(chip_attribute){
			Chip_Info_temp.Timeout = strtol((char*)chip_attribute, NULL, 16);
        	//printf("Timeout: %s\n", chip_attribute);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"MXIC_WPmode");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.MXIC_WPmode = true;
            else
                Chip_Info_temp.MXIC_WPmode = false;
        	//printf("MXIC_WPmode: %s\n", chip_attribute);
		}
		//SPI NAND
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SpareSizeInByte");
		if(chip_attribute){
			Chip_Info_temp.SpareSizeInByte = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"nCA_Rd");
		if(chip_attribute){
			Chip_Info_temp.nCA_Rd = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"nCA_Wr");
		if(chip_attribute){
			Chip_Info_temp.nCA_Wr = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadDummyLen");
		if(chip_attribute){
			Chip_Info_temp.ReadDummyLen = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"DefaultDataUnitSize");
		if(chip_attribute){
			Chip_Info_temp.DefaultDataUnitSize = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"DefaultErrorBits");
		if(chip_attribute){
			Chip_Info_temp.DefaultErrorBits = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"BBMark");
		if(chip_attribute){
			Chip_Info_temp.BBMark = strtol((char*)chip_attribute, NULL, 10);
		}
        chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SupportedProduct");
		if(chip_attribute){
			Chip_Info_temp.SupportedProduct = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCParityGroup");
		if(chip_attribute){
			Chip_Info_temp.ECCParityGroup = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectStartAddr");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectStartAddr = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectDis");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectDis = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCProtectLength");
		if(chip_attribute){
			Chip_Info_temp.ECCProtectLength = strtol((char*)chip_attribute, NULL, 10);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ECCEnable");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.ECCEnable = true;
            else
                Chip_Info_temp.ECCEnable = false;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"QPIEnable");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.QPIEnable = true;
            else
                Chip_Info_temp.QPIEnable = false;
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ChipEraseTime");
		if(chip_attribute){
			Chip_Info_temp.ChipEraseTime = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"EraseCmd");
		if(chip_attribute){
			Chip_Info_temp.EraseCmd = strtol((char*)chip_attribute, NULL, 16);
        }
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ProgramCmd");
		if(chip_attribute){
			Chip_Info_temp.ProgramCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ReadCmd");
		if(chip_attribute){
			Chip_Info_temp.ReadCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"ProtectBlockMask");
		if(chip_attribute){
			Chip_Info_temp.ProtectBlockMask = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"UnlockCmd");
		if(chip_attribute){
			Chip_Info_temp.UnlockCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDSRCnt");
		if(chip_attribute){
			Chip_Info_temp.RDSRCnt = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRSRCnt");
		if(chip_attribute){
			Chip_Info_temp.WRSRCnt = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRSRCmd");
		if(chip_attribute){
			Chip_Info_temp.WRSRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"WRCRCmd");
		if(chip_attribute){
			Chip_Info_temp.WRCRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDSRCmd");
		if(chip_attribute){
			Chip_Info_temp.RDSRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"RDCRCmd");
		if(chip_attribute){
			Chip_Info_temp.RDCRCmd = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"QEbitAddr");
		if(chip_attribute){
			Chip_Info_temp.QEbitAddr = strtol((char*)chip_attribute, NULL, 16);
		}
		chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"SupportLUT");
		if(chip_attribute){
			if (strstr((char*)chip_attribute, "true") != NULL)
                Chip_Info_temp.SupportLUT = true;
            else
                Chip_Info_temp.SupportLUT = false;
		}
	  }
	  xmlFree(chip_attribute);
    }

	xmlFreeDoc(doc);
    Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
                
    if (found_flag == 0) {
        Chip_Info->TypeName[0] = 0;
        Chip_Info->UniqueID = 0;
    }
    return found_flag; /*Executed without errors*/
}


bool Dedi_List_AllChip(void)
{
	char file_path[linebufsize];
	char Type[256] = { 0 };
	xmlDocPtr    doc;
	xmlNodePtr   cur_node;
	xmlChar      *chip_attribute;

    if (GetChipDbPath(file_path) == false)
    {
        return 1;
    }

	doc = xmlParseFile(file_path);
	cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
	cur_node = cur_node->children->next; //Portofolio
	cur_node = cur_node->children->next; //C
    //	data_temp = file_buf;
    /*Read into the buffer contents within thr file stream*/
    while (cur_node != NULL) {
        if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"Chip"))) {
	  	
            chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"TypeName");
		    if(chip_attribute){
			    strcpy(Type, (char*)chip_attribute);
        	    //printf("TypeName: %s\n", chip_attribute);
		    }
			chip_attribute = xmlGetProp(cur_node, (const xmlChar *)"Manufacturer");
		    if(chip_attribute){
 			    printf("%s\t\tby %s\n", Type, (char*)chip_attribute);
		    }
			xmlFree(chip_attribute);
        }
		cur_node = cur_node->next;
    }

	xmlFreeDoc(doc);

    return true;

}

 #else
FILE* openChipInfoDb(void)
{
    FILE* fp = NULL;
    char Path[linebufsize];

    memset(Path, 0, linebufsize);
    if (readlink("/proc/self/exe", Path, 512) != -1) {
        dirname(Path);
        strcat(Path, "/ChipInfoDb.dedicfg");
        //		printf("%s\n",Path);
        if ((fp = fopen(Path, "rt")) == NULL) {
            // ChipInfoDb.dedicfg not in program directory
            dirname(Path);
            dirname(Path);
            strcat(Path, "/share/DediProg/ChipInfoDb.dedicfg");
            //			printf("%s\n",Path);
            if ((fp = fopen(Path, "rt")) == NULL)
                fprintf(stderr, "Error opening file: %s\n", Path);
        }
    }

    //xml_parse_result result = doc.load_file( Path );
    //if ( result.status != xml_parse_status::status_ok )
    //	return;

    return fp;
}

long fsize(FILE* fp)
{
    long prev = ftell(fp);
    fseek(fp, 0L, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, prev, SEEK_SET); // go back to where we were
    return sz;
}
 
int Dedi_Search_Chip_Db(char* chTypeName, long RDIDCommand,
    long UniqueID,
    CHIP_INFO* Chip_Info,
    int search_all)
{
  
    FILE* fp; /*Declare file pointer variable*/
    int found_flag = 0;
    char file_line_buf[linebufsize], *tok, *file_buf, test[testbufsize];
    char* pch;
    long sz = 0;
    int detectICNum = 0;
    char strTypeName[32][100];
    CHIP_INFO Chip_Info_temp;

    for (int i = 0; i < 32; i++)
        memset(strTypeName[i], '\0', 100);

    memset(chTypeName, '\0', 1024);

    memset(Chip_Info->TypeName, '\0', linebufsize); //strlen(Chip_Info->TypeName)=0

    if ((fp = openChipInfoDb()) == NULL)
        return 1;
    sz = fsize(fp);
    file_buf = (char*)malloc(sz);

    memset(file_buf, '\0', sz);
    /*Read into the buffer contents within thr file stream*/

    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        //printf("ile_line_buf=%s",file_line_buf) ;
        if (pch != NULL) {
            if (found_flag == 1) {
                found_flag = 0;

                if (strlen(Chip_Info->TypeName) == 0)
                    *Chip_Info = Chip_Info_temp; //first chip info
            }
            //   break;

            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            Chip_Info_temp.TypeName[0] = '\0';
            Chip_Info_temp.Class[0] = '\0';
            Chip_Info_temp.UniqueID = 0;
            Chip_Info_temp.Description[0] = '\0';
            Chip_Info_temp.Manufacturer[0] = '\0';
            Chip_Info_temp.ManufactureUrl[0] = '\0';
            Chip_Info_temp.ProgramIOMethod[0] = '\0';
            Chip_Info_temp.Voltage[0] = '\0';
            Chip_Info_temp.Clock[0] = '\0';
            Chip_Info_temp.ManufactureID = 0;
            Chip_Info_temp.JedecDeviceID = 0;
            Chip_Info_temp.AlternativeID = 0;
            Chip_Info_temp.ChipSizeInByte = 0;
            Chip_Info_temp.SectorSizeInByte = 0;
            Chip_Info_temp.BlockSizeInByte = 0;
            Chip_Info_temp.PageSizeInByte = 0;
            Chip_Info_temp.AddrWidth = 0;
            Chip_Info_temp.ReadDummyLen = 0;
            Chip_Info_temp.IDNumber = 0;
            Chip_Info_temp.RDIDCommand = 0;
            Chip_Info_temp.MaxErasableSegmentInByte = 0;
            Chip_Info_temp.DualID = false;
            Chip_Info_temp.VppSupport = 0;
            Chip_Info_temp.MXIC_WPmode = false;
            Chip_Info_temp.Timeout = 0; 
            // end of struct init
            strcpy(Chip_Info_temp.TypeName, tok);

            continue;
        }

        pch = strstr(file_line_buf, "Class");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Class"));
            tok = strtok(test, "\"= \t");
            //printf("Class = %s\n",tok);
            strcpy(Chip_Info_temp.Class, tok);
            continue;
        }
        pch = strstr(file_line_buf, "UniqueID");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("UniqueID"));
            tok = strtok(test, "\"= \t");
            //printf("UniqueID = 0x%lx\n",strtol(tok,NULL,16));
            Chip_Info_temp.UniqueID = strtol(tok, NULL, 16);
            if ((UniqueID == Chip_Info_temp.UniqueID)) {
                //found_flag = 1;
                //detectICNum++;
            }

            continue;
        }
        pch = strstr(file_line_buf, "Manufacturer");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Manufacturer"));
            tok = strtok(test, "\"= \t");
            //printf("Manufacturer = %s\n",tok);
            strcpy(Chip_Info_temp.Manufacturer, tok);
            continue;
        }
        pch = strstr(file_line_buf, "Voltage");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Voltage"));
            tok = strtok(test, "\"= \t");
            if (strcmp(tok, "3.3V") == 0)
                Chip_Info_temp.VoltageInMv = 3300;
            else if (strcmp(tok, "2.5V") == 0)
                Chip_Info_temp.VoltageInMv = 2500;
            else if (strcmp(tok, "1.8V") == 0)
                Chip_Info_temp.VoltageInMv = 1800;
            else
                Chip_Info_temp.VoltageInMv = 3300;
            continue;
        }
        pch = strstr(file_line_buf, "JedecDeviceID");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("JedecDeviceID"));
            tok = strtok(test, "\"= \t");
            //printf("JedecDeviceID = 0x%lx\n",strtol(tok,NULL,16));
            Chip_Info_temp.JedecDeviceID = strtol(tok, NULL, 16);
            if ((UniqueID == Chip_Info_temp.JedecDeviceID)) {
                found_flag = 1;

                strcpy(strTypeName[detectICNum], Chip_Info_temp.TypeName);
                //printf("strTypeName[%d] = %s\n",detectICNum,strTypeName[detectICNum]);
                strcat(chTypeName, " ");
                strcat(chTypeName, strTypeName[detectICNum]);
                detectICNum++;
            }
            continue;
        }
        pch = strstr(file_line_buf, "ChipSizeInKByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("ChipSizeInKByte"));
            tok = strtok(test, "\"= \t");
            //printf("ChipSizeInKByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.ChipSizeInByte = strtol(tok, NULL, 10) * 1024;
            continue;
        }
        pch = strstr(file_line_buf, "SectorSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("SectorSizeInByte"));
            tok = strtok(test, "\"= \t");
            //printf("SectorSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.SectorSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "BlockSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("BlockSizeInByte"));
            tok = strtok(test, "\"= \t");
            //printf("BlockSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.BlockSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "PageSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("PageSizeInByte"));
            tok = strtok(test, "\"= \t");
            //printf("PageSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.PageSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "AddrWidth");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("AddrWidth"));
            tok = strtok(test, "\"= \t");
            //printf("AddrWidth = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.AddrWidth = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "ReadDummyLen");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("ReadDummyLen"));
            tok = strtok(test, "\"= \t");
            //printf("ReadDummyLen = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.ReadDummyLen = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "IDNumber");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("IDNumber"));
            tok = strtok(test, "\"= \t");
            //printf("IDNumber = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.IDNumber = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "VppSupport");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("VppSupport"));
            tok = strtok(test, "\"= \t");
            //printf("IDNumber = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.VppSupport = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "RDIDCommand");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("RDIDCommand"));
            tok = strtok(test, "\"= \t");
            //printf("RDIDCommand = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.RDIDCommand = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "Timeout");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Timeout"));
            tok = strtok(test, "\"= \t");
            //printf("Timeout = %ld\n",strtol(tok,NULL,10));
            Chip_Info_temp.Timeout = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "MXIC_WPmode");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("MXIC_WPmode"));
            tok = strtok(test, "\"= \t");
            if (strstr(tok, "true") != NULL)
                Chip_Info_temp.MXIC_WPmode = true;
            else
                Chip_Info_temp.MXIC_WPmode = false;
            // starting checking input data
        } 
        pch = strstr(file_line_buf, "Portofolio"); //end
        if (pch != NULL) {
            if (detectICNum)
                found_flag = 1;
        }

    } /*Continue until EOF is encoutered*/

    fclose(fp); /*Close file*/
    Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
               
    free(file_buf);
    return found_flag; /*Executed without errors*/
} /*End main*/
 
int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO* Chip_Info)
{
    //printf("Dedi_Search_Chip_Db_ByTypeName = %s\n",TypeName);
    FILE* fp; /*Declare file pointer variable*/
    int found_flag = 0, i;
    char file_line_buf[linebufsize], *tok, *file_buf, test[testbufsize];
    char* pch;
    long sz = 0;
    char Type[256] = { 0 };

    for (i = 0; i < strlen(TypeName); i++)
        TypeName[i] = toupper(TypeName[i]);

    if ((fp = openChipInfoDb()) == NULL)
        return 0;

    sz = fsize(fp);
    file_buf = (char*)malloc(sz);
    memset(file_buf, '\0', sz);
    //	data_temp = file_buf;
    /*Read into the buffer contents within thr file stream*/
    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        if (pch != NULL) {
            if (found_flag == 1)
                break;
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            strcpy(Type, tok);
            for (i = 0; i < strlen(Type); i++)
                Type[i] = toupper(tok[i]);

            if (strcmp(Type, TypeName) == 0) {
                found_flag = 1;
                Chip_Info->TypeName[0] = '\0';
                Chip_Info->Class[0] = '\0';
                Chip_Info->UniqueID = 0;
                Chip_Info->Description[0] = '\0';
                Chip_Info->Manufacturer[0] = '\0';
                Chip_Info->ManufactureUrl[0] = '\0';
                Chip_Info->ProgramIOMethod[0] = '\0';
                Chip_Info->Voltage[0] = '\0';
                Chip_Info->Clock[0] = '\0';
                Chip_Info->ManufactureID = 0;
                Chip_Info->JedecDeviceID = 0;
                Chip_Info->AlternativeID = 0;
                Chip_Info->ChipSizeInByte = 0;
                Chip_Info->SectorSizeInByte = 0;
                Chip_Info->BlockSizeInByte = 0;
                Chip_Info->PageSizeInByte = 0;
                Chip_Info->AddrWidth = 0;
                Chip_Info->ReadDummyLen = 0;
                Chip_Info->IDNumber = 0;
                Chip_Info->RDIDCommand = 0;
                Chip_Info->MaxErasableSegmentInByte = 0;
                Chip_Info->DualID = false;
                Chip_Info->VppSupport = 0;
                Chip_Info->MXIC_WPmode = false; 
                Chip_Info->Timeout = 0;
                // end of struct init
                strcpy(Chip_Info->TypeName, tok);
            } else
                found_flag = 0;
        }

        if (found_flag == 0)
            continue;

        pch = strstr(file_line_buf, "Class");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Class"));
            tok = strtok(test, "\"= \t");
            // printf("Class = %s\n",tok);
            strcpy(Chip_Info->Class, tok);
            continue;
        }
        pch = strstr(file_line_buf, "UniqueID");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("UniqueID"));
            tok = strtok(test, "\"= \t");
            // printf("UniqueID = 0x%lx\n",strtol(tok,NULL,16));
            Chip_Info->UniqueID = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "Manufacturer");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Manufacturer"));
            tok = strtok(test, "\"= \t");
            // printf("Manufacturer = %s\n",tok);
            strcpy(Chip_Info->Manufacturer, tok);
            continue;
        }
        pch = strstr(file_line_buf, "Voltage");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Voltage"));
            tok = strtok(test, "\"= \t");
            // printf("Voltage = %s\n",tok);
            if (strcmp(tok, "3.3V") == 0)
                Chip_Info->VoltageInMv = 3300;
            else if (strcmp(tok, "2.5V") == 0)
                Chip_Info->VoltageInMv = 2500;
            else if (strcmp(tok, "1.8V") == 0)
                Chip_Info->VoltageInMv = 1800;
            else
                Chip_Info->VoltageInMv = 3300;
            continue;
        }
        pch = strstr(file_line_buf, "JedecDeviceID");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("JedecDeviceID"));
            tok = strtok(test, "\"= \t");
            // printf("JedecDeviceID = 0x%lx\n",strtol(tok,NULL,16));
            Chip_Info->JedecDeviceID = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "ChipSizeInKByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("ChipSizeInKByte"));
            tok = strtok(test, "\"= \t");
            // printf("ChipSizeInKByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info->ChipSizeInByte = strtol(tok, NULL, 10) * 1024;
            continue;
        }
        pch = strstr(file_line_buf, "SectorSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("SectorSizeInByte"));
            tok = strtok(test, "\"= \t");
            // printf("SectorSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info->SectorSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "BlockSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("BlockSizeInByte"));
            tok = strtok(test, "\"= \t");
            // printf("BlockSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info->BlockSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "PageSizeInByte");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("PageSizeInByte"));
            tok = strtok(test, "\"= \t");
            // printf("PageSizeInByte = %ld\n",strtol(tok,NULL,10));
            Chip_Info->PageSizeInByte = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "AddrWidth");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("AddrWidth"));
            tok = strtok(test, "\"= \t");
            // printf("AddrWidth = %ld\n",strtol(tok,NULL,10));
            Chip_Info->AddrWidth = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "ReadDummyLen");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("ReadDummyLen"));
            tok = strtok(test, "\"= \t");
            //printf("ReadDummyLen = %ld\n",strtol(tok,NULL,10));
            Chip_Info->ReadDummyLen = strtol(tok, NULL, 10);
            continue;
        } 
        pch = strstr(file_line_buf, "IDNumber");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("IDNumber"));
            tok = strtok(test, "\"= \t");
            // printf("IDNumber = %ld\n",strtol(tok,NULL,10));
            Chip_Info->IDNumber = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "VppSupport");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("VppSupport"));
            tok = strtok(test, "\"= \t");
            // printf("IDNumber = %ld\n",strtol(tok,NULL,10));
            Chip_Info->VppSupport = strtol(tok, NULL, 10);
            continue;
        }
        pch = strstr(file_line_buf, "RDIDCommand");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("RDIDCommand"));
            tok = strtok(test, "\"= \t");
            Chip_Info->RDIDCommand = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "Timeout");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Timeout"));
            tok = strtok(test, "\"= \t");
            Chip_Info->Timeout = strtol(tok, NULL, 16);
            continue;
        }
        pch = strstr(file_line_buf, "MXIC_WPmode");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("MXIC_WPmode"));
            tok = strtok(test, "\"= \t");
            if (strstr(tok, "true") != NULL)
                Chip_Info->MXIC_WPmode = true;
            else
                Chip_Info->MXIC_WPmode = false;
            // starting checking input data
            continue;
        } 
    } /*Continue until EOF is encoutered*/
    fclose(fp); /*Close file*/
    Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
                
    if (found_flag == 0) {
        Chip_Info->TypeName[0] = 0;
        Chip_Info->UniqueID = 0;
    }
    free(file_buf);
    return found_flag; /*Executed without errors*/
}

bool Dedi_List_AllChip(void)
{

    FILE* fp; /*Declare file pointer variable*/
    char file_line_buf[linebufsize], *tok, *file_buf, test[testbufsize];
    char* pch;
    char TypeName[100], Manufacturer[100];
    long sz = 0;

    if ((fp = openChipInfoDb()) == NULL)
        return false;
    sz = fsize(fp);
    file_buf = (char*)malloc(sz);
    memset(file_buf, '\0', sz);
    /*Read into the buffer contents within thr file stream*/
    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            // end of struct init
            TypeName[0] = '\0';
            Manufacturer[0] = '\0';
            strcpy(TypeName, tok);
            continue;
        }
        pch = strstr(file_line_buf, "Manufacturer");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Manufacturer"));
            tok = strtok(test, "\"= \t");
            // printf("Manufacturer = %s\n",tok);
            strcpy(Manufacturer, tok);
            if (TypeName[0] != '\0' && Manufacturer[0] != '\0')
                printf("%s\t\tby %s\n", TypeName, Manufacturer);
        }
    } /*Continue until EOF is encoutered*/
    fclose(fp); /*Close file*/
    return true;

} /*End main*/

#endif
/***********************PARSE.TXT (CONTENT)**********************/
/* "Move each word to the next line"                            */
/***********************PARSE.EXE (OUTPUT)***********************/
/*  C:\>parse                                                   */
/*  Enter filename: parse.txt                                   */
/*                                                              */
/*  Move                                                        */
/*  each                                                        */
/*  word                                                        */
/*  to                                                          */
/*  the                                                         */
/*  next                                                        */
/*  line.                                                       */
/*                                                              */
/*  Press any key to continue . . .                             */
/****************************************************************/
