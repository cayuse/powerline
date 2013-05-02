
#include <string.h>
#include <SPI.h>
#include "Modem.h"

static char buffer[128];

Modem::Modem()
{
}

Modem::~Modem()
{
}

void Modem::setup() {
	SPI.begin();
	SPI.setClockDivider(SPI_CLOCK_DIV32); // 16Mhz / 32
	SPI.setDataMode(SPI_MODE0);
	SPI.setBitOrder(MSBFIRST);
	
}

void Modem::setSerial(Stream& stream) {
    _pSerial = &stream;
}

uint8_t Modem::test(uint8_t valIn) {
	uint8_t result = SPI.transfer(valIn);
	
	return result;
}

void Modem::Loop()
{
}


