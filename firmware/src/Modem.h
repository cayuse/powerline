#ifndef MODEM_H
#define MODEM_H

#include <Stream.h>
//#include "plmcfg.h"

class Modem
{
    Stream          * _pSerial;           //! Store the serial object.

public:
    Modem();
    ~Modem();
    
    void setSerial(Stream& stream);
    void setup();
    void Loop();
    
    uint8_t test(uint8_t valin);
    
};

#endif
