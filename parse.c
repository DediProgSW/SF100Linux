#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Macro.h"
#include "ChipInfoDb.h"
#define linebufsize 512
#define filebufsize 1024*1024
#define min(a,b) (((a)>(b))? (b):(a))
#define max(a,b)  (((a) > (b)) ? (a) : (b))

void getExecPath(char* Path)
{
	memset(Path,0,512);
	if (readlink ("/proc/self/exe", Path, 512) != -1)
	{
//		printf("%s\r\n",Path);
		dirname (Path);
//		printf("%s\r\n",Path);
		strcat(Path,"/ChipInfoDb.dedicfg");
//		printf("%s\r\n",Path);

	}
}

long fsize(FILE *fp){
    long prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    long sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}


int Dedi_Search_Chip_Db(long RDIDCommand, long UniqueID, CHIP_INFO *Chip_Info, int search_all)
{
	FILE* fp;       /*Declare file pointer variable*/
	int    found_flag = 0;
	char file_line_buf[linebufsize], *tok, fname[256], *file_buf, test[256];
	char* pch;
	long sz=0;
	char Path[512];
	getExecPath(Path);
//	printf("%s\r\n",Path);
     /*If file doesn't exist or filetype isn't allowed exit and*/
     /*error message & return (1) control to the OS*/
	if ((fp = fopen(Path,"rt")) == NULL)
	{
		fprintf(stderr,"Error opening file: %s\n",fname);
		return 1;
	}
	sz=fsize(fp);
	file_buf = (char*)malloc(sz);

	memset(file_buf, '\0', sz);
	/*Read into the buffer contents within thr file stream*/

	while(fgets(file_line_buf, linebufsize, fp) != NULL)
	{
		pch=strstr(file_line_buf,"TypeName");
		if(pch!= NULL)
		{
			if(found_flag==1)
				break;
			memset(test, '\0', 100);
			strcpy(test,pch+strlen("TypeName"));
			tok = strtok(test,"\"= \t");
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
			Chip_Info->IDNumber = 0;
			Chip_Info->RDIDCommand = 0;
			Chip_Info->MaxErasableSegmentInByte=0;
			Chip_Info->DualID=false;;
			Chip_Info->VppSupport=0;
			Chip_Info->MXIC_WPmode=false;
			Chip_Info->Timeout=0;
			// end of struct init
			strcpy(Chip_Info->TypeName,tok);
			continue;
		}

		pch=strstr(file_line_buf,"Class");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Class"));
			tok = strtok(test,"\"= \t");
//            printf("Class = %s\n",tok);
			strcpy(Chip_Info->Class,tok);
			continue;
		}
		pch=strstr(file_line_buf,"UniqueID");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("UniqueID"));
			tok = strtok(test,"\"= \t");
//            printf("UniqueID = 0x%lx\n",strtol(tok,NULL,16));
			Chip_Info->UniqueID = strtol(tok,NULL,16);
			if((UniqueID == Chip_Info->UniqueID))
				found_flag = 1;
			continue;
        }
		pch=strstr(file_line_buf,"Manufacturer");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Manufacturer"));
			tok = strtok(test,"\"= \t");
			//printf("Manufacturer = %s\n",tok);
			strcpy(Chip_Info->Manufacturer,tok);
			continue;
		}
       pch=strstr(file_line_buf,"Voltage");
       if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Voltage"));
			tok = strtok(test,"\"= \t");
			//printf("Voltage = %s\n",tok);
			if(strcmp(tok, "3.3V") == 0)
				Chip_Info->VoltageInMv = 3300;
			else if(strcmp(tok, "2.5V") == 0)
				Chip_Info->VoltageInMv = 2500;
			else if(strcmp(tok, "1.8V") == 0)
				Chip_Info->VoltageInMv = 1800;
			else
				Chip_Info->VoltageInMv = 3300;
			continue;
		}
       pch=strstr(file_line_buf,"JedecDeviceID");
       if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("JedecDeviceID"));
			tok = strtok(test,"\"= \t");
			//printf("JedecDeviceID = 0x%lx\n",strtol(tok,NULL,16));
			Chip_Info->JedecDeviceID = strtol(tok,NULL,16);
			continue;
        }
		pch=strstr(file_line_buf,"ChipSizeInKByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("ChipSizeInKByte"));
			tok = strtok(test,"\"= \t");
			//printf("ChipSizeInKByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->ChipSizeInByte = strtol(tok,NULL,10)*1024;
			continue;
        }
		pch=strstr(file_line_buf,"SectorSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("SectorSizeInByte"));
			tok = strtok(test,"\"= \t");
			//printf("SectorSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->SectorSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"BlockSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("BlockSizeInByte"));
			tok = strtok(test,"\"= \t");
			//printf("BlockSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->BlockSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"PageSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("PageSizeInByte"));
			tok = strtok(test,"\"= \t");
			//printf("PageSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->PageSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"AddrWidth");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("AddrWidth"));
			tok = strtok(test,"\"= \t");
			//printf("AddrWidth = %ld\n",strtol(tok,NULL,10));
			Chip_Info->AddrWidth = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"IDNumber");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("IDNumber"));
			tok = strtok(test,"\"= \t");
			//printf("IDNumber = %ld\n",strtol(tok,NULL,10));
			Chip_Info->IDNumber = strtol(tok,NULL,10);
			continue;
		}
       pch=strstr(file_line_buf,"VppSupport");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("VppSupport"));
			tok = strtok(test,"\"= \t");
			//printf("IDNumber = %ld\n",strtol(tok,NULL,10));
			Chip_Info->VppSupport = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"RDIDCommand");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("RDIDCommand"));
			tok = strtok(test,"\"= \t");
			Chip_Info->RDIDCommand = strtol(tok,NULL,16);
			continue;
		}
       pch=strstr(file_line_buf,"Timeout");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Timeout"));
			tok = strtok(test,"\"= \t");
			Chip_Info->Timeout= strtol(tok,NULL,16);
			continue;
		}
       pch=strstr(file_line_buf,"MXIC_WPmode");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("MXIC_WPmode"));
			tok = strtok(test,"\"= \t");
			if(strstr(tok,"true") != NULL)
				Chip_Info->MXIC_WPmode=true;
			else
				Chip_Info->MXIC_WPmode=false;
            //starting checking input data
		}
#if 0
					 pch=strstr(file_line_buf,"Description=");
					 if( pch != NULL)
					 {
						 memset(test, '\0', 80);
						 strcpy(test,pch+strlen("Description="));
						 tok = strtok(test,"\"");
			 // 					 printf("Description = %s\n",tok);
						 strcpy(Chip_Info->Description,tok);
						 continue;
							 }
					 pch=strstr(file_line_buf,"ManufactureUrl=");
					 if(pch != NULL)
					 {
						 memset(test, '\0', 80);
						 strcpy(test,pch+strlen("ManufactureUrl="));
						 tok = strtok(test,"\"");
						 //printf("ManufactureUrl = %s\n",tok);
						 strcpy(Chip_Info->ManufactureUrl,tok);
						 continue;
					 }
					 pch=strstr(file_line_buf,"Clock=");
					 if(pch != NULL)
					 {
						 memset(test, '\0', 80);
						 strcpy(test,pch+strlen("Clock="));
						 tok = strtok(test,"\"");
						 //printf("Clock = %s\n",tok);
						 Chip_Info->Clock[0] = '\0';
						 continue;
					 }
					 pch=strstr(file_line_buf,"ManufactureID=");
					 if(pch != NULL)
					 {
						 memset(test, '\0', 80);
						 strcpy(test,pch+strlen("ManufactureID="));
						 tok = strtok(test,"\"");
						 printf("ManufactureID = 0x%lx\n",strtol(tok,NULL,16));
						 Chip_Info->ManufactureID = strtol(tok,NULL,16);
					 }
					 pch=strstr(file_line_buf,"DeviceID=");
					 if(pch != NULL)
					 {
						 memset(test, '\0', 80);
						 strcpy(test,pch+strlen("DeviceID="));
						 tok = strtok(test,"\"");
						 //printf("DeviceID = 0x%lx\n",strtol(tok,NULL,16));
						 Chip_Info->AlternativeID= strtol(tok,NULL,16);
					 }
#endif

	}/*Continue until EOF is encoutered*/
	fclose(fp); /*Close file*/
	Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
	free(file_buf);
	return found_flag; /*Executed without errors*/
}/*End main*/

int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO *Chip_Info, int search_all)
{
	FILE* fp;       /*Declare file pointer variable*/
	int    found_flag = 0,i;
	char file_line_buf[linebufsize], *tok, fname[256], *file_buf, test[256];
	char* pch;
	long sz=0;
	char Type[256]={0};
	char Path[512];
	getExecPath(Path);

	for(i=0; i<strlen(TypeName); i++)
		TypeName[i]=toupper(TypeName[i]);

     /*If file doesn't exist or filetype isn't allowed exit and*/
     /*error message & return (1) control to the OS*/
	if ((fp = fopen(Path,"rt")) == NULL){
		fprintf(stderr,"Error opening file: %s\n",fname);
		return 0;
	}
	sz= fsize(fp);
	file_buf = (char*)malloc(sz);
	memset(file_buf, '\0', sz);
//	data_temp = file_buf;
	/*Read into the buffer contents within thr file stream*/
	while(fgets(file_line_buf, linebufsize, fp) != NULL)
	{
		pch=strstr(file_line_buf,"TypeName");
		if(pch!= NULL)
		{
			if(found_flag==1)
				break;
			memset(test, '\0', 256);
			strcpy(test,pch+strlen("TypeName"));
			tok = strtok(test,"\"= \t");
			strcpy(Type,tok);
			for( i=0; i<strlen(Type); i++)
				Type[i]=toupper(tok[i]);

			if(strcmp(Type,TypeName)==0)
			{
				found_flag=1;
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
				Chip_Info->IDNumber = 0;
				Chip_Info->RDIDCommand = 0;
				Chip_Info->MaxErasableSegmentInByte=0;
				Chip_Info->DualID=false;;
				Chip_Info->VppSupport=0;
				Chip_Info->MXIC_WPmode=false;
				Chip_Info->Timeout=0;
			// end of struct init
				strcpy(Chip_Info->TypeName,tok);
			}
			else
				found_flag=0;
		}

		if(found_flag==0)
			continue;

		pch=strstr(file_line_buf,"Class");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Class"));
			tok = strtok(test,"\"= \t");
			//printf("Class = %s\n",tok);
			strcpy(Chip_Info->Class,tok);
			continue;
		}
		pch=strstr(file_line_buf,"UniqueID");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("UniqueID"));
			tok = strtok(test,"\"= \t");
			//printf("UniqueID = 0x%lx\n",strtol(tok,NULL,16));
			Chip_Info->UniqueID = strtol(tok,NULL,16);
			continue;
		}
		pch=strstr(file_line_buf,"Manufacturer");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Manufacturer"));
			tok = strtok(test,"\"= \t");
			//printf("Manufacturer = %s\n",tok);
			strcpy(Chip_Info->Manufacturer,tok);
			continue;
		}
		pch=strstr(file_line_buf,"Voltage");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Voltage"));
			tok = strtok(test,"\"= \t");
            //printf("Voltage = %s\n",tok);
			if(strcmp(tok, "3.3V") == 0)
				Chip_Info->VoltageInMv = 3300;
			else if(strcmp(tok, "2.5V") == 0)
				Chip_Info->VoltageInMv = 2500;
			else if(strcmp(tok, "1.8V") == 0)
				Chip_Info->VoltageInMv = 1800;
			else
				Chip_Info->VoltageInMv = 3300;
			continue;
		}
		pch=strstr(file_line_buf,"JedecDeviceID");
		if(pch != NULL)
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("JedecDeviceID"));
			tok = strtok(test,"\"= \t");
            //printf("JedecDeviceID = 0x%lx\n",strtol(tok,NULL,16));
			Chip_Info->JedecDeviceID = strtol(tok,NULL,16);
			continue;
		}
		pch=strstr(file_line_buf,"ChipSizeInKByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("ChipSizeInKByte"));
			tok = strtok(test,"\"= \t");
            //printf("ChipSizeInKByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->ChipSizeInByte = strtol(tok,NULL,10)*1024;
			continue;
		}
		pch=strstr(file_line_buf,"SectorSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("SectorSizeInByte"));
			tok = strtok(test,"\"= \t");
            //printf("SectorSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->SectorSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"BlockSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("BlockSizeInByte"));
			tok = strtok(test,"\"= \t");
            //printf("BlockSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->BlockSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"PageSizeInByte");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("PageSizeInByte"));
			tok = strtok(test,"\"= \t");
            //printf("PageSizeInByte = %ld\n",strtol(tok,NULL,10));
			Chip_Info->PageSizeInByte = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"AddrWidth");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("AddrWidth"));
			tok = strtok(test,"\"= \t");
            //printf("AddrWidth = %ld\n",strtol(tok,NULL,10));
			Chip_Info->AddrWidth = strtol(tok,NULL,10);
			continue;
		}
       pch=strstr(file_line_buf,"IDNumber");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("IDNumber"));
			tok = strtok(test,"\"= \t");
            //printf("IDNumber = %ld\n",strtol(tok,NULL,10));
			Chip_Info->IDNumber = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"VppSupport");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("VppSupport"));
			tok = strtok(test,"\"= \t");
            //printf("IDNumber = %ld\n",strtol(tok,NULL,10));
			Chip_Info->VppSupport = strtol(tok,NULL,10);
			continue;
		}
		pch=strstr(file_line_buf,"RDIDCommand");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("RDIDCommand"));
			tok = strtok(test,"\"= \t");
			Chip_Info->RDIDCommand = strtol(tok,NULL,16);
			continue;
		}
       pch=strstr(file_line_buf,"Timeout");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("Timeout"));
			tok = strtok(test,"\"= \t");
			Chip_Info->Timeout= strtol(tok,NULL,16);
			continue;
		}
       pch=strstr(file_line_buf,"MXIC_WPmode");
       if(pch != NULL )
		{
			memset(test, '\0', 80);
			strcpy(test,pch+strlen("MXIC_WPmode"));
			tok = strtok(test,"\"= \t");
			if(strstr(tok,"true") != NULL)
				Chip_Info->MXIC_WPmode=true;
			else
				Chip_Info->MXIC_WPmode=false;
            //starting checking input data
            continue;
		}
#if 0

			 pch=strstr(file_line_buf,"ManufactureUrl=");
			 if(pch != NULL) {
					 memset(test, '\0', 80);
					 strcpy(test,pch+strlen("ManufactureUrl="));
					 tok = strtok(test,"\"");
					 //printf("ManufactureUrl = %s\n",tok);
					 strcpy(Chip_Info->ManufactureUrl,tok);
			 }
			 pch=strstr(file_line_buf,"Description=");
			 if( pch != NULL) {
							 memset(test, '\0', 80);
							 strcpy(test,pch+strlen("Description="));
							 tok = strtok(test,"\"");
							 //printf("Description = %s\n",tok);
							 strcpy(Chip_Info->Description,tok);
					 }
					pch=strstr(file_line_buf,"Clock=");
					if(pch != NULL)  {
							 memset(test, '\0', 80);
							 strcpy(test,pch+strlen("Clock="));
							 tok = strtok(test,"\"");
							 //printf("Clock = %s\n",tok);
							 Chip_Info->Clock[0] = '\0';
					 }

					pch=strstr(file_line_buf,"ManufactureID=");
					if(pch != NULL) {
							 memset(test, '\0', 80);
							 strcpy(test,pch+strlen("ManufactureID="));
							 tok = strtok(test,"\"");
							 //printf("ManufactureID = 0x%lx\n",strtol(tok,NULL,16));
							 Chip_Info->ManufactureID = strtol(tok,NULL,16);
					 }

					pch=strstr(file_line_buf,"DeviceID=");
					if(pch != NULL) {
							 memset(test, '\0', 80);
							 strcpy(test,pch+strlen("DeviceID="));
							 tok = strtok(test,"\"");
							 //printf("DeviceID = 0x%lx\n",strtol(tok,NULL,16));
							 Chip_Info->AlternativeID= strtol(tok,NULL,16);
					 }
#endif
     }/*Continue until EOF is encoutered*/
     fclose(fp); /*Close file*/
     Chip_Info->MaxErasableSegmentInByte = max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);
     if(found_flag==0)
     {
        Chip_Info->TypeName[0]=0;
        Chip_Info->UniqueID=0;
     }
	free(file_buf);
	return found_flag; /*Executed without errors*/
}/*End main*/

void Dedi_List_AllChip()
{
	FILE* fp;       /*Declare file pointer variable*/
	char file_line_buf[linebufsize], *tok, fname[15], *file_buf, test[100];;
	char* pch;
	char TypeName[100],Manufacturer[100];
	long sz=0;
	char Path[512];
	getExecPath(Path);

	/*If file doesn't exist or filetype isn't allowed exit and*/
	/*error message & return (1) control to the OS*/
	if ((fp = fopen(Path,"rt")) == NULL){
		fprintf(stderr,"Error opening file: %s\n",fname);
		return 1;
	}
	sz=fsize(fp);
	file_buf=(char*)malloc(sz);
	memset(file_buf, '\0', sz);
	/*Read into the buffer contents within thr file stream*/
	while(fgets(file_line_buf, linebufsize, fp) != NULL)
	{
		pch=strstr(file_line_buf,"TypeName");
		if(pch!= NULL)
		{
			memset(test, '\0', 100);
			strcpy(test,pch+strlen("TypeName"));
			tok = strtok(test,"\"= \t");
            // end of struct init
			TypeName[0]='\0';
			Manufacturer[0]='\0';
			strcpy(TypeName,tok);
			continue;
		}
		pch=strstr(file_line_buf,"Manufacturer");
		if(pch != NULL)
		{
			memset(test, '\0', 100);
			strcpy(test,pch+strlen("Manufacturer"));
			tok = strtok(test,"\"= \t");
            //printf("Manufacturer = %s\n",tok);
			strcpy(Manufacturer,tok);
			if(TypeName[0] != '\0' && Manufacturer[0] != '\0')
				printf("%s\t\tby %s\r\n",TypeName,Manufacturer);
		}
     }/*Continue until EOF is encoutered*/
     fclose(fp); /*Close file*/
}/*End main*/


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
