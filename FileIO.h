
// file operation
// read file to buffer
// supported file format : bin,hex,s19,ram ...

#ifndef _ST_FILE_H
#define _ST_FILE_H

#include <vector>
#include <string>

using std::wstring;

class CFileIO
{


    enum FILE_FORMAT{
        BIN ,
        HEX,
        S19,
    } ;


    FILE_FORMAT GetFileFormatFromExt(const wstring& csPath) ;

    bool ReadBINFile(const wstring& csPath, std::vector<unsigned char>& buffer) ;
    bool ReadHEXFile(const wstring& csPath, std::vector<unsigned char>& buffer,unsigned char PaddingByte) ;
    bool ReadS19File(const wstring& csPath, std::vector<unsigned char>& buffer,unsigned char PaddingByte) ;

    bool WriteBINFile(const std::vector<unsigned char>& buffer, const wstring& csPath) ;
    bool WriteHEXFile(const std::vector<unsigned char>& buffer, const wstring& csPath) ;
    bool WriteS19File(const std::vector<unsigned char>& buffer, const wstring& csPath) ;

public :
    bool Read(const wstring& csPath, std::vector<unsigned char>& buffer, unsigned char PaddingByte) ;

    bool Write(const std::vector<unsigned char>& buffer,const wstring& csPath) ;

};


#endif    //_ST_FILE_H