#ifndef CMDPROCESSOR_H
#define CMDPROCESSOR_H

#include <Stream.h>

class CmdProcessor
{
protected:
    Stream          * _pHW;           //! Store the serial object.
    char*           _pTokens[10];   //! List of command tokens
    char*           _pCmd;          //! Command buffer.
    char*           _pCmdString;    //! Current command
    uint8_t         _cmdPos;        //! Current position during serial read.
    bool            _validCmd;      //! Indicates a current valid command.
    char*           _pCmdTerm;      //! Store command terminator
    char*           _pCmdDelim;     //! Current command parameter delimiter
    uint8_t         _paramCnt;      //! Number of valid parameters.
    
public:
    CmdProcessor();
    ~CmdProcessor();

    void setSerial(Stream& stream);
    bool checkCommands();
    char* cmdTerm();
    void cmdTerm(char*);
    void resetCmd();
    const char* cmdDelim();
    void cmdDelim(const char*);
    const char* getCmd();
    uint8_t paramCnt();
    void getParam(uint8_t idx,uint8_t &p);
    void getParam(uint8_t idx,uint16_t &p);
    void getParam(uint8_t idx,long &l);
    void getParam(uint8_t idx,int &p);
    void getParam(uint8_t idx,double &f);
    void getParam(uint8_t idx,char*& p, uint8_t maxlen=128);
protected:
    void processCmd();

};

#endif
