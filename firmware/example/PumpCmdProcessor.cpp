
#include <string.h>
#include "PumpCmdProcessor.h"
#include "PumpControl.h"

static char buffer[1024];

PumpCmdProcessor::PumpCmdProcessor(PumpControl* pc) : CmdProcessor()
{
    _pPC = pc;
}

PumpCmdProcessor::~PumpCmdProcessor()
{
}

void PumpCmdProcessor::Loop()
{
    // Process commands from the command interface.
    //if (false) {
    if (checkCommands()) {
        // Process the command

        const char *pCmd = getCmd();

        // What is the best way? An enum would work, but hard to
        // manage. A string is easy, but inefficient... but easy.
        if (strcmp(pCmd,"status") == 0) {
              sprintf(buffer,"Ok:Ditch:%d Sump:%d PC:%d P:%d NC:%d N:%d SC:%d S:%d ST:%d STen:%d\n",
                _pPC->ditchCurr,
                _pPC->sumpCurr,
                _pPC->pumpCall,
                _pPC->isPumpOn(),
                _pPC->northCall, _pPC->isNorthOn(),
                _pPC->southCall, _pPC->isSouthOn(),
                _pPC->sumpLowTrigger,
                _pPC->enableSumpTrigger
                );
            _pHW->print(buffer);

        } else if (strcmp(pCmd,"levels") == 0) {
              sprintf(buffer,"Ok:%d %d\n",
                _pPC->ditchCurr,
                _pPC->sumpCurr
                );
            _pHW->print(buffer);

        } else if(strcmp(pCmd,"pump") == 0) {
            int idx = -1;
            if (paramCnt() > 0) {
                getParam(0,idx);
                _pPC->setPump(idx != 0);
                _pHW->print("Ok\n");
            } else {
                _pHW->print("Fail:pump requires a single param: 1 | 0.\n");
            }

        } else if(strcmp(pCmd,"pump?") == 0) {
            sprintf(buffer,"Ok:%d %d\n",
                _pPC->pumpCall,_pPC->isPumpOn()
                );
            _pHW->print(buffer);
        } else if(strcmp(pCmd,"north?") == 0) {
            sprintf(buffer,"Ok:%d %d\n",
                _pPC->northCall, _pPC->isNorthOn()
                );
            _pHW->print(buffer);
        } else if(strcmp(pCmd,"south?") == 0) {
            sprintf(buffer,"Ok:%d %d\n",
                _pPC->southCall, _pPC->isSouthOn()
                );
            _pHW->print(buffer);
        } else if(strcmp(pCmd,"north") == 0) {
            int idx = -1;
            if (paramCnt() > 0) {
                getParam(0,idx);
                _pPC->setNorthCall(idx != 0);
                _pHW->print("Ok\n");
            } else {
                _pHW->print("Fail:north requires a single param: 1 | 0.\n");
            }
        } else if(strcmp(pCmd,"south") == 0) {
            int idx = -1;
            if (paramCnt() > 0) {
                getParam(0,idx);
                _pPC->setSouthCall(idx != 0);
                _pHW->print("Ok\n");
            } else {
                _pHW->print("Fail:south requires a single param.: 1 | 0\n");
            }
        } else if(strcmp(pCmd,"sump_trigger") == 0) {
            int lvl = -1;
            if (paramCnt() > 0) {
                getParam(0,lvl);
                _pPC->setSumpTrigger(lvl);
                _pHW->print("Ok\n");
            } else {
                _pHW->print("Fail:sump_trigger requires a single int value.\n");
            }
        } else if(strcmp(pCmd,"sump_trig_en") == 0) {
            int idx = -1;
            if (paramCnt() > 0) {
                getParam(0,idx);
                _pPC->setSumpTriggerEnable(idx != 0);
                _pHW->print("Ok\n");
            } else {
                _pHW->print("Fail:sump_trig_en requires a single param.\n");
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


