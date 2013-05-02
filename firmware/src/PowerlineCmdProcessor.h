#ifndef POWERLINECMDPROCESSOR_H
#define POWERLINECMDPROCESSOR_H

#include "CmdProcessor.h"
#include "Modem.h"

class PowerlineCmdProcessor : public CmdProcessor
{

	Modem* _pModem;

public:
    PowerlineCmdProcessor(Modem& rModem);
    ~PowerlineCmdProcessor();
    
    
    void Loop();
    
};

#endif
