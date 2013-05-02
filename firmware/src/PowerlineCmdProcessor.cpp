
#include <string.h>
#include "PowerlineCmdProcessor.h"

static char buffer[1024];

PowerlineCmdProcessor::PowerlineCmdProcessor(Modem& rModem) : CmdProcessor()
{
	_pModem = &rModem;
}

PowerlineCmdProcessor::~PowerlineCmdProcessor()
{
}

void PowerlineCmdProcessor::Loop()
{
    // Process commands from the command interface.
    //if (false) {
    if (checkCommands()) {
        // Process the command

        const char *pCmd = getCmd();

        // What is the best way? An enum would work, but hard to
        // manage. A string is easy, but inefficient... but easy.
        if (strcmp(pCmd,"test") == 0) {
        	uint8_t in;
            if (paramCnt() > 0) {
                getParam(0,in);
	        	uint8_t out = _pModem->test(in);
				sprintf(buffer,"Ok:test result:%d\n",out);
				_pHW->print(buffer);
            } else {
                _pHW->print("Fail:test require a single param.\n");
            }
        } else if(strcmp(pCmd,"help") == 0) {
			_pHW->print("Ok:valid commands are=> status, levels, pump, north, south, sump_trigger, sump_trig_en, help.\n");
        } else {
            sprintf(buffer,"Fail:This is an Invalid Cmd:%s\n",pCmd);
            _pHW->print(buffer);
        }

        resetCmd();
    }
}


