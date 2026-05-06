#include "ChipInfoDb.h"
#include "project.h"
#include <ctype.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#ifdef __FreeBSD__
#include <sys/auxv.h>
#else
#include <libxml/parser.h>
#include <libxml/tree.h>
#endif

#define pathbufsize 1024
#define testbufsize 256
#define linebufsize 512
#define filebufsize 1024 * 1024
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __FreeBSD__
FILE* openChipInfoDb(void)
{
    FILE* fp = NULL;
    char Path[linebufsize];

    memset(Path, 0, linebufsize);
    if (readlink("/proc/self/exe", Path, 512) != -1) {
        dirname(Path);
        strcat(Path, "/ChipInfoDb.dedicfg");
        if ((fp = fopen(Path, "rt")) == NULL) {
            // ChipInfoDb.dedicfg not in program directory
            dirname(Path);
            dirname(Path);
            strcat(Path, "/share/DediProg/ChipInfoDb.dedicfg");
            if ((fp = fopen(Path, "rt")) == NULL)
                fprintf(stderr, "Error opening file: %s\n", Path);
        }
    }
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
    FILE* fp;
    int found_flag = 0;
    char file_line_buf[linebufsize], *tok, *pch;
    char test[testbufsize];
    int detectICNum = 0;
    char strTypeName[32][TYPENAME_MAX_LEN];
    CHIP_INFO Chip_Info_temp;

    for (int i = 0; i < 32; i++)
        memset(strTypeName[i], '\0', TYPENAME_MAX_LEN);

    memset(chTypeName, '\0', 1024);
    memset(Chip_Info->TypeName, '\0', linebufsize);

    if ((fp = openChipInfoDb()) == NULL)
        return 1;

    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        if (pch != NULL) {
            if (found_flag == 1) {
                found_flag = 0;
                if (strlen(Chip_Info->TypeName) == 0)
                    *Chip_Info = Chip_Info_temp;
            }
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            memset(&Chip_Info_temp, 0, sizeof(CHIP_INFO));
            strcpy(Chip_Info_temp.TypeName, tok);
            continue;
        }

        pch = strstr(file_line_buf, "UniqueID");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("UniqueID"));
            tok = strtok(test, "\"= \t");
            Chip_Info_temp.UniqueID = strtol(tok, NULL, 16);
            if ((UniqueID == Chip_Info_temp.UniqueID)) {
                found_flag = 1;
                strcpy(strTypeName[detectICNum], Chip_Info_temp.TypeName);
                strcat(chTypeName, " ");
                strcat(chTypeName, strTypeName[detectICNum]);
                detectICNum++;
            }
            continue;
        }

        pch = strstr(file_line_buf, "Manufacturer");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Manufacturer"));
            tok = strtok(test, "\"= \t");
            strcpy(Chip_Info_temp.Manufacturer, tok);
            continue;
        }
    }

    fclose(fp);

    if (detectICNum)
        found_flag = 1;

    Chip_Info->MaxErasableSegmentInByte =
        max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);

    return found_flag;
}

int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO* Chip_Info)
{
    FILE* fp;
    char file_line_buf[linebufsize], *tok, *pch;
    char test[testbufsize];
    CHIP_INFO Chip_Info_temp;
    int found_flag = 0;

    if ((fp = openChipInfoDb()) == NULL)
        return 1;

    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            memset(&Chip_Info_temp, 0, sizeof(CHIP_INFO));
            strcpy(Chip_Info_temp.TypeName, tok);

            if (!strcmp(Chip_Info_temp.TypeName, TypeName)) {
                found_flag = 1;
                *Chip_Info = Chip_Info_temp;
                break;
            }
        }
    }

    fclose(fp);

    if (!found_flag) {
        Chip_Info->TypeName[0] = 0;
        Chip_Info->UniqueID = 0;
    }

    Chip_Info->MaxErasableSegmentInByte =
        max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);

    return found_flag;
}

bool Dedi_List_AllChip(void)
{
    FILE* fp;
    char file_line_buf[linebufsize], *tok, *pch;
    char test[testbufsize];
    char Type[256] = {0};

    if ((fp = openChipInfoDb()) == NULL)
        return false;

    while (fgets(file_line_buf, linebufsize, fp) != NULL) {
        pch = strstr(file_line_buf, "TypeName");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("TypeName"));
            tok = strtok(test, "\"= \t");
            strcpy(Type, tok);
        }

        pch = strstr(file_line_buf, "Manufacturer");
        if (pch != NULL) {
            memset(test, '\0', testbufsize);
            strcpy(test, pch + strlen("Manufacturer"));
            tok = strtok(test, "\"= \t");
            printf("%s\t\tby %s\n", Type, tok);
        }
    }

    fclose(fp);
    return true;
}
#else

// ---------------- 工具函式 ----------------
static char* get_prop_string(xmlNodePtr node, const char* propName) {
    xmlChar* val = xmlGetProp(node, (const xmlChar*)propName);
    if (!val) return NULL;
    char* result = strdup((char*)val);
    xmlFree(val);
    return result;
}

static long get_prop_long(xmlNodePtr node, const char* propName, int base) {
    char* val = get_prop_string(node, propName);
    if (!val) return 0;
    long result = strtol(val, NULL, base);
    free(val);
    return result;
}

static bool get_prop_bool(xmlNodePtr node, const char* propName) {
    char* val = get_prop_string(node, propName);
    if (!val) return false;
    bool result = (strstr(val, "true") != NULL);
    free(val);
    return result;
}

// ---------------- Chip 節點解析 ----------------
static void parse_chip_node(xmlNodePtr cur_node, CHIP_INFO* chip) {
    memset(chip, 0, sizeof(CHIP_INFO));

    char* str = get_prop_string(cur_node, "TypeName");
    if (str) { strcpy(chip->TypeName, str); free(str); }

    str = get_prop_string(cur_node, "ICType");
    if (str) { strcpy(chip->ICType, str); free(str); }

    str = get_prop_string(cur_node, "Class");
    if (str) { strcpy(chip->Class, str); free(str); }

    str = get_prop_string(cur_node, "Manufacturer");
    if (str) { strcpy(chip->Manufacturer, str); free(str); }

    chip->UniqueID = get_prop_long(cur_node, "UniqueID", 16);
    chip->JedecDeviceID = get_prop_long(cur_node, "JedecDeviceID", 16);
    chip->ChipSizeInByte = get_prop_long(cur_node, "ChipSizeInKByte", 10) * 1024;
    chip->SectorSizeInByte = get_prop_long(cur_node, "SectorSizeInByte", 10);
    chip->BlockSizeInByte = get_prop_long(cur_node, "BlockSizeInByte", 10);
    chip->PageSizeInByte = get_prop_long(cur_node, "PageSizeInByte", 10);

    // Voltage mapping
    str = get_prop_string(cur_node, "Voltage");
    if (str) {
        strcpy(chip->Voltage, str);
        if (strstr(str, "3.3V")) chip->VoltageInMv = 3300;
        else if (strstr(str, "2.5V")) chip->VoltageInMv = 2500;
        else if (strstr(str, "1.8V")) chip->VoltageInMv = 1800;
        else chip->VoltageInMv = 3300;
        free(str);
    }

    chip->MXIC_WPmode = get_prop_bool(cur_node, "MXIC_WPmode");
    chip->ECCEnable   = get_prop_bool(cur_node, "ECCEnable");
    chip->QPIEnable   = get_prop_bool(cur_node, "QPIEnable");
    chip->SupportLUT  = get_prop_bool(cur_node, "SupportLUT");

    chip->MaxErasableSegmentInByte = max(chip->SectorSizeInByte, chip->BlockSizeInByte);
}

// ---------------- 原本接口 ----------------
bool GetChipDbPath(char *Path) {
    FILE* fp = NULL;

    if (Path == NULL)
        return false;

    memset(Path, 0, linebufsize);
    if (readlink("/proc/self/exe", Path, 512) != -1) {
        dirname(Path);
        strcat(Path, "/ChipInfoDb.dedicfg");
        if ((fp = fopen(Path, "rt")) == NULL) {
            dirname(Path);
            dirname(Path);
            strcat(Path, "/share/DediProg/ChipInfoDb.dedicfg");
            if ((fp = fopen(Path, "rt")) == NULL) {
                fprintf(stderr, "Error opening file: %s\n", Path);
                return false;
            }
        }
        if (fp) fclose(fp);
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
    char strTypeName[32][TYPENAME_MAX_LEN];
    CHIP_INFO Chip_Info_temp;
    xmlDocPtr doc;
    xmlNodePtr cur_node;

    for (int i = 0; i < 32; i++)
        memset(strTypeName[i], '\0', TYPENAME_MAX_LEN);

    memset(chTypeName, '\0', 1024);
    memset(Chip_Info->TypeName, '\0', sizeof(Chip_Info->TypeName));

    if (GetChipDbPath(file_path) == false) {
        return 1;
    }

    doc = xmlParseFile(file_path);
    if (!doc) {
        fprintf(stderr, "Error parsing XML file: %s\n", file_path);
        return 1;
    }

    cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
    if (!cur_node) {
        xmlFreeDoc(doc);
        return 1;
    }

    cur_node = cur_node->children;
    while (cur_node && xmlStrcmp(cur_node->name, (const xmlChar*)"Portofolio") != 0) {
        cur_node = cur_node->next;
    }
    if (!cur_node) {
        xmlFreeDoc(doc);
        return 1;
    }

    cur_node = cur_node->children;
    while (cur_node != NULL) {
        if (!xmlStrcmp(cur_node->name, (const xmlChar*)"Chip")) {
            parse_chip_node(cur_node, &Chip_Info_temp);

            if (Chip_Info_temp.JedecDeviceID == UniqueID) {
                found_flag = 1;
                strcpy(strTypeName[detectICNum], Chip_Info_temp.TypeName);
                strcat(chTypeName, " ");
                strcat(chTypeName, strTypeName[detectICNum]);

                if (strlen(Chip_Info->TypeName) == 0) {
                    *Chip_Info = Chip_Info_temp; // first matched chip
                }
                detectICNum++;
            }
        }
        cur_node = cur_node->next;
    }

    xmlFreeDoc(doc);

    if (detectICNum)
        found_flag = 1;

    Chip_Info->MaxErasableSegmentInByte =
        max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);

    return found_flag;
}

int Dedi_Search_Chip_Db_ByTypeName(char* TypeName, CHIP_INFO* Chip_Info)
{
    int found_flag = 0;
    char file_path[linebufsize];
    CHIP_INFO Chip_Info_temp;
    xmlDocPtr doc;
    xmlNodePtr cur_node;

    if (GetChipDbPath(file_path) == false) {
        return 1;
    }

    doc = xmlParseFile(file_path);
    if (!doc) {
        fprintf(stderr, "Error parsing XML file: %s\n", file_path);
        return 1;
    }

    cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
    if (!cur_node) {
        xmlFreeDoc(doc);
        return 1;
    }

    cur_node = cur_node->children;
    while (cur_node && xmlStrcmp(cur_node->name, (const xmlChar*)"Portofolio") != 0) {
        cur_node = cur_node->next;
    }
    if (!cur_node) {
        xmlFreeDoc(doc);
        return 1;
    }

    cur_node = cur_node->children;
    while (cur_node != NULL) {
        if (!xmlStrcmp(cur_node->name, (const xmlChar*)"Chip")) {
            parse_chip_node(cur_node, &Chip_Info_temp);

            if (!strcmp(Chip_Info_temp.TypeName, TypeName)) {
                found_flag = 1;
                *Chip_Info = Chip_Info_temp;
                break;
            }
        }
        cur_node = cur_node->next;
    }

    xmlFreeDoc(doc);

    if (!found_flag) {
        Chip_Info->TypeName[0] = 0;
        Chip_Info->UniqueID = 0;
    }

    Chip_Info->MaxErasableSegmentInByte =
        max(Chip_Info->SectorSizeInByte, Chip_Info->BlockSizeInByte);

    return found_flag;
}

bool Dedi_List_AllChip(void)
{
    char file_path[linebufsize];
    char Type[256] = { 0 };
    xmlDocPtr doc;
    xmlNodePtr cur_node;
    xmlChar* chip_attribute;

    if (GetChipDbPath(file_path) == false) {
        return false;
    }

    doc = xmlParseFile(file_path);
    if (!doc) {
        fprintf(stderr, "Error parsing XML file: %s\n", file_path);
        return false;
    }

    cur_node = xmlDocGetRootElement(doc); // DediProgChipDatabase
    if (!cur_node) {
        xmlFreeDoc(doc);
        return false;
    }

    cur_node = cur_node->children;
    while (cur_node && xmlStrcmp(cur_node->name, (const xmlChar*)"Portofolio") != 0) {
        cur_node = cur_node->next;
    }
    if (!cur_node) {
        xmlFreeDoc(doc);
        return false;
    }

    cur_node = cur_node->children;
    while (cur_node != NULL) {
        if (!xmlStrcmp(cur_node->name, (const xmlChar*)"Chip")) {
            chip_attribute = xmlGetProp(cur_node, (const xmlChar*)"TypeName");
            if (chip_attribute) {
                strcpy(Type, (char*)chip_attribute);
                xmlFree(chip_attribute);
            }
            chip_attribute = xmlGetProp(cur_node, (const xmlChar*)"Manufacturer");
            if (chip_attribute) {
                printf("%s\t\tby %s\n", Type, (char*)chip_attribute);
                xmlFree(chip_attribute);
            }
        }
        cur_node = cur_node->next;
    }

    xmlFreeDoc(doc);
    return true;
}
#endif
