/*************************************************** 
  This is a library for DICE_TC Thermocouple Sensor with 3x MAX31855K

  These PCBs use SPI to communicate, 3 pins are required to  
  interface. The DICE_TC contains 3 thermocouple sensors.

  Written by Dominik Wenger for 2Printbeta.
  Based on Adafruit_MAX31885 code by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license.
 ****************************************************/

#include "DICE_TC_lib.h"
#include <stdlib.h>


#define ERROR_MASK 0x7
#define nop asm volatile ("nop\n\t")
#include <SPI.h>

#if defined(ARDUINO_ARCH_SAM) // Arduino Due Board
// fast IO functions on arm
inline void digitalWriteDirect(int pin, boolean val){
  if(val) g_APinDescription[pin].pPort -> PIO_SODR = g_APinDescription[pin].ulPin;
  else    g_APinDescription[pin].pPort -> PIO_CODR = g_APinDescription[pin].ulPin;
}

inline int digitalReadDirect(int pin){
  return !!(g_APinDescription[pin].pPort -> PIO_PDSR & g_APinDescription[pin].ulPin);
}
#else
#warning No fast IOs for this platform
inline void digitalWriteDirect(int pin, boolean val){ digitalWrite(pin,val)}
inline int digitalReadDirect(int pin){ return digitalRead(pin);}
#endif



DICE_TC::DICE_TC(int8_t SCLK, int8_t CS, int8_t MISO,int8_t ADDR1, int8_t ADDR2,boolean softSPI) 
{
  sclk = SCLK;
  cs = CS;
  miso = MISO;
  addr1 = ADDR1;
  addr2 = ADDR2;
  m_softSPI = softSPI;
  
  //define pin modes
  pinMode(cs, OUTPUT);
  pinMode(sclk, OUTPUT); 
  pinMode(miso, INPUT);
  pinMode(addr1,OUTPUT);
  pinMode(addr2,OUTPUT);
    
  //set chipselect high
  digitalWrite(cs, HIGH);
  
  //init SPI
  if(m_softSPI == false)
  {
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV8);
    SPI.begin();	
  }
  else
  {
	//sck default high
	digitalWrite(sclk,HIGH);
  }
}


double DICE_TC::readInternal(int8_t chipnum) 
{
  uint32_t v;

  v = spiread32(chipnum);

  // ignore bottom 4 bits - they're just thermocouple data
  v >>= 4;

  // pull the bottom 11 bits off
  float internal = v & 0x7FF;
  internal *= 0.0625; // LSB = 0.0625 degrees
  // check sign bit!
  if (v & 0x800) 
    internal *= -1;

  return internal;
}

double DICE_TC::readCelsius(int8_t chipnum) 
{

  int32_t v;

  v = spiread32(chipnum);

  if (v & ERROR_MASK) {
    // chip returns error,either open, short to GND or short to VCC
    return NAN; 
  }

  // get rid of internal temp data, and any fault bits
  v >>= 18;
  
  double centigrade = v;

  // LSB = 0.25 degrees C
  centigrade *= 0.25;
  return centigrade;
}

uint8_t DICE_TC::readError(int8_t chipnum) 
{
  return spiread32(chipnum) & ERROR_MASK;
}

double DICE_TC::readFarenheit(int8_t chipnum) 
{
  float f = readCelsius(chipnum);
  f *= 9.0;
  f /= 5.0;
  f += 32;
  return f;
}

uint32_t DICE_TC::spiread32(int8_t chipnum) 
{ 
  int i;
  uint32_t d = 0;

  //select chip
  switch(chipnum)
  {
    case 0:
        digitalWriteDirect(addr1,LOW);
        digitalWriteDirect(addr2,LOW);
        break;
    case 1:
        digitalWriteDirect(addr1,HIGH);
        digitalWriteDirect(addr2,LOW);
        break;
    case 2:
        digitalWriteDirect(addr1,LOW);
        digitalWriteDirect(addr2,HIGH);
        break;
    default:
        //error, only 3 chips available
        return d;   
  }
    
	if(m_softSPI == false)
	{
	   #if !defined(ARDUINO_ARCH_SAM)   // use Software SPI on arduino DUE !
		//preserver the previous spi mode
		unsigned char oldMode =  SPCR & SPI_MODE_MASK;
	
		//if the mode is not correct set it to mode 3
		if (oldMode != SPI_MODE3) {
			SPI.setDataMode(SPI_MODE3);
		}
	
		//select the chip
		digitalWriteDirect(cs,LOW);

		//read the values
		d = SPI.transfer(0);
		d <<= 8;
		d |= SPI.transfer(0);
		d <<= 8;
		d |= SPI.transfer(0);
		d <<= 8;
		d |= SPI.transfer(0);
	
		//deselect the chip
		digitalWriteDirect(cs,HIGH); 
    
	
		//restore the previous SPI mode if neccessary
		//if the mode is not correct set it to mode 3
		if (oldMode != SPI_MODE3) {
			SPI.setDataMode(oldMode);
		}
		#endif
	}
	else
	{
	    noInterrupts();
		//select the chip
		digitalWriteDirect(cs,LOW);

		//read the values
		d = SoftSPI_Transfer(0);
		d <<= 8;
		d |= SoftSPI_Transfer(0);
		d <<= 8;
		d |= SoftSPI_Transfer(0);
		d <<= 8;
		d |= SoftSPI_Transfer(0);
	
		//deselect the chip
		digitalWriteDirect(cs,HIGH); 
		interrupts();
	}
	
    return d;
}

char DICE_TC::SoftSPI_Transfer(char SPI_byte)
{
	unsigned char SPI_count; // counter for SPI transaction
	for (SPI_count = 8; SPI_count > 0; SPI_count--) // single byte SPI loop
	{
		// set clock low
        digitalWriteDirect(sclk, LOW);
		//set data pin
	    //  digitalWriteDirect(mosi_pin, SPI_byte & 0X80);
		SPI_byte = SPI_byte << 1; // shift next bit into MSB

        nop;
        nop;

		//set clock high 
        digitalWriteDirect(sclk, HIGH);
		SPI_byte |= digitalReadDirect(miso); // capture current bit on MISO
	}
	return (SPI_byte);
} // END SPI_Transfer