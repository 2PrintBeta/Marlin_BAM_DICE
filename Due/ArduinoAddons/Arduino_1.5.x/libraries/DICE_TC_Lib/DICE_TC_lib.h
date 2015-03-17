/*************************************************** 
  This is a library for DICE_TC Thermocouple Sensor with 3x MAX31855K


  These PCBs use SPI to communicate, 3 pins are required to  
  interface. The DICE_TC contains 3 thermocouple sensors.

  Written by Dominik Wenger for 2Printbeta.
  Based on Adafruit_MAX31885 code by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license.
 ****************************************************/


#if (ARDUINO >= 100)
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

class DICE_TC {
 public:
  DICE_TC(int8_t SCLK, int8_t CS, int8_t MISO,int8_t ADDR1, int8_t ADDR2,boolean softSPI = false);

  double readInternal(int8_t chipnum);
  double readCelsius(int8_t chipnum);
  double readFarenheit(int8_t chipnum);
  uint8_t readError(int8_t chipnum);

 private:
 char SoftSPI_Transfer (char SPI_byte);
  int8_t sclk, miso, cs, addr1, addr2;
  uint32_t spiread32(int8_t chipnum);
  boolean m_softSPI;
};
