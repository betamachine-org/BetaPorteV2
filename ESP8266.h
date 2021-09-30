// definition de IO pour beta four

#define __BOARD__ESP8266__

#define LED_ON LOW



//Pin out NODEMCU ESP8266
#define D0  16    //!LED_BUILTIN
#define D1  5     //       I2C_SCL
#define D2  4     //       I2C_SDA
#define D3  0     //!FLASH    Beep
#define D4  2     //!LED2     POWER_PN532

#define D5  14    //!SPI_CLK   
#define D6  12    //!SPI_MISO  
#define D7  13    //!SPI_MOSI   
#define D8  15    //            

#define pinBP0   D7                 //  High to Low = will wleep in, 5 minutes 
//#define pinLed0  LED_BUILTIN   //   By default Led0 is on LED_BUILTIN you can change it
#define I2C_SDA  D2
#define I2C_SCL  D1

#define BeepPin D3
#define POWER_PN532 D4
