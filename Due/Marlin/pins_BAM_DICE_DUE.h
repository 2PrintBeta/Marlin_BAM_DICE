/****************************************************************************************
* Arduino Due pin assignment
*
* for RAMPS-FD
****************************************************************************************/

//
#ifndef __SAM3X8E__
 #error Oops!  Make sure you have 'Arduino Due' selected from the 'Tools -> Boards' menu.
#endif

#define X_STEP_PIN         54
#define X_DIR_PIN          55
#define X_ENABLE_PIN       38
#define X_MIN_PIN           3
#define X_MAX_PIN           2

#define Y_STEP_PIN         60
#define Y_DIR_PIN          61
#define Y_ENABLE_PIN       56
#define Y_MIN_PIN          43
#define Y_MAX_PIN          45

#define Z_STEP_PIN         46
#define Z_DIR_PIN          48
#define Z_ENABLE_PIN       62
#define Z_MIN_PIN          40
#define Z_MAX_PIN          42

#define E0_STEP_PIN        26
#define E0_DIR_PIN         28
#define E0_ENABLE_PIN      24

#define E1_STEP_PIN        36
#define E1_DIR_PIN         34
#define E1_ENABLE_PIN      30

#define SDPOWER            -1
#define SDSS               53
#define LED_PIN            13

#define BEEPER             -1

#define FAN_PIN            9

#define CONTROLLERFAN_PIN  -1 //Pin used for the fan to cool controller

#define PS_ON_PIN          -1

#define KILL_PIN           41


#define HEATER_BED_PIN     8    // BED

#define HEATER_0_PIN       10
#define HEATER_1_PIN       9

#define TEMP_BED_PIN       10   // ANALOG NUMBERING

#define TEMP_0_PIN         9   // ANALOG NUMBERING
#define TEMP_1_PIN         -1  // 11    // ANALOG NUMBERING
#define TEMP_2_PIN         -1  //     // ANALOG NUMBERING

#define TEMP_3_PIN         -1   // ANALOG NUMBERING
#define TEMP_4_PIN         -1   // ANALOG NUMBERING

// for software SPI
#define S_SPI_MOSI_PIN       51
#define S_SPI_MISO_PIN       50
#define S_SPI_SCK_PIN        52

//for ESP
#define ESP_RESET_PIN   	67
#define ESP_CH_DOWN_PIN 	44
#define ESP_PROG_PIN		A5
#define ESP_IO_PIN      	A14   

  #ifdef NUM_SERVOS
    #define SERVO0_PIN         11

    #if NUM_SERVOS > 1
      #define SERVO1_PIN         6
    #endif

    #if NUM_SERVOS > 2
      #define SERVO2_PIN         5
    #endif

    #if NUM_SERVOS > 3
      #define SERVO3_PIN         4
    #endif
  #endif


  #ifdef ULTRA_LCD

    #ifdef NEWPANEL
      // ramps-fd lcd adaptor
      #define LCD_PINS_RS 16
      #define LCD_PINS_ENABLE 17
      #define LCD_PINS_D4 23
      #define LCD_PINS_D5 25
      #define LCD_PINS_D6 27
      #define LCD_PINS_D7 29

      #ifdef REPRAP_DISCOUNT_SMART_CONTROLLER
        #define BEEPER 37

        #define BTN_EN1 33
        #define BTN_EN2 31
        #define BTN_ENC 35

        #define SDCARDDETECT 49
        #endif

      #endif

  #endif //ULTRA_LCD


// SPI for Max6675 Thermocouple

#ifndef SDSUPPORT
// these pins are defined in the SD library if building with SD support
  #define MAX_SCK_PIN          52
  #define MAX_MISO_PIN         50
  #define MAX_MOSI_PIN         51
  #define MAX6675_SS       53
#else
  #define MAX6675_SS       49
#endif

// --------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------
