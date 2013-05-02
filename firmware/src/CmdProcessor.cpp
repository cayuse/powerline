
#include <stdlib.h>
#include <string.h>
#include "CmdProcessor.h"

//! Construct a new CmdProcessor.
//! Pass in reference to the HardwareSerial class to use
//! for command processing. Store the serial pointer and
//! then initialize the internal data strings used during
//! command input processing and output processing.
CmdProcessor::CmdProcessor()
{
    _pHW = 0;
    
    _pCmdString = (char*)malloc(128);
    _pCmd = 0;
    _pCmdTerm = (char*)malloc(3);
    strcpy(_pCmdTerm,"\n\r");
    _pCmdDelim = (char*)malloc(3);
    strcpy(_pCmdDelim," \t");
    resetCmd();
}

//! Destructor. Release memory allocated in constructor.
CmdProcessor::~CmdProcessor()
{
    free(_pCmdString);
    free(_pCmdDelim);
    free(_pCmdTerm);
}

void CmdProcessor::setSerial(Stream& stream) {
    _pHW = &stream;
}

//! Return pointer to termination string.
char* CmdProcessor::cmdTerm() { return _pCmdTerm; }

//! Set a new command terminator. Free memory for previous
//! value, allocate new memory and save the new value.
void CmdProcessor::cmdTerm(char* t) 
{ 
    free(_pCmdTerm);
    _pCmdTerm = (char*)malloc(strlen(t) + 1);
    strcpy(_pCmdTerm,t);
}

//! Return current delimiter string.
const char* CmdProcessor::cmdDelim()
{
    return _pCmdDelim;
}

//! Set new delimiter string. 
//! Free memory, allocate new memory and copy new value.
void CmdProcessor::cmdDelim(const char* d)
{
    free(_pCmdDelim);
    _pCmdDelim = (char*)malloc(strlen(d) + 1);
    strcpy(_pCmdDelim,d);
}

//! Read new characters from the serial port
//! Read any new characters into the command buffer.
//! Look for the command terminator. If the terminator
//! is found, store the command, process the command buffer
//! and return 1 to indicate that a new command is availble.
//! If a full command is not yet present, then return zero.
bool CmdProcessor::checkCommands()
{
    while (_pHW->available() > 0) {
        unsigned char c = _pHW->read();
        if (strchr(_pCmdTerm,c) != 0) {
            if (_cmdPos > 0) {
                // Done with this command.
                _pCmdString[_cmdPos] = 0; // Null terminate command
                processCmd();
                return 1;
            } else {
                _pHW->print("Ok\n");
            }
        } else {
            _pCmdString[_cmdPos++] = c;
        }
    }
    return 0;
}

//! Process the commands in the command buffer
//! Split the command into parameters based on the
//! command delimiter. The maximum number of command
//! tokens is 10.
void CmdProcessor::processCmd()
{
    // See if the command delimiter exists in the
    // command. if it does not, then the command
    // is the entire string.
    if (strpbrk(_pCmdString,_pCmdDelim)) {
        _pCmd = strtok(_pCmdString,_pCmdDelim);
        char* pTok = strtok(0,_pCmdDelim);
        int i = 0;
        while (i < 10 && pTok) {
            _pTokens[i++] = pTok;
            pTok = strtok(0,_pCmdDelim);
        }
        _paramCnt = i;
        _validCmd = true;
    } else {
        _pCmd = _pCmdString;
        _paramCnt = 0;
        _validCmd = true;
    }
}

//! Clear the command status values so a new command can be started.
void CmdProcessor::resetCmd()
{
    _cmdPos = 0;
    _validCmd = false;
    _paramCnt = 0;
}

//! Return the command string.
const char* CmdProcessor::getCmd()
{
    return _pCmd;
}

//! Return the number of parameters parsed from the current command.
uint8_t CmdProcessor::paramCnt()
{
    return _paramCnt;
}


/** @name Parameter Extraction Functions
 getParam is overloaded on the variable type, this means that each possible
 type has a unique function. The type of the parameter you are seeking
 will determine the exact function that is called, which will then do the
 right thing to convert the string paramter value to an unsigned int, double
 etc. 
 
*/
//@{

//! Parse the index parameter into a unsigned 16 bit integer.
void CmdProcessor::getParam(uint8_t idx,uint16_t &p)
{
    if (idx < _paramCnt) {
        p = atoi(_pTokens[idx]);
    }
}

//! Parse the index parameter into a unsigned 8 bit integer.
void CmdProcessor::getParam(uint8_t idx,uint8_t &p)
{
    if (idx < _paramCnt) {
        p = atoi(_pTokens[idx]);
    }
}

void CmdProcessor::getParam(uint8_t idx,int &p)
{
    if (idx < _paramCnt) {
        p = atoi(_pTokens[idx]);
    }
}

void CmdProcessor::getParam(uint8_t idx,long &l)
{
    if (idx < _paramCnt) {
        l = atol(_pTokens[idx]);
    }
}

//! Parse the index parameter into a double.
void CmdProcessor::getParam(uint8_t idx,double &p)
{
    if (idx < _paramCnt) {
        uint8_t nScans;
        nScans = sscanf(_pTokens[idx],"%lf", &p);
        //p = atof(_pTokens[idx]);
    }
}

//! Parse the index parameter into a string with the length specified.
void CmdProcessor::getParam(uint8_t idx,char*& p, uint8_t maxlen)
{
    if (idx < _paramCnt) {
        strncpy(p,_pTokens[idx],maxlen);
    }
}

//@}
