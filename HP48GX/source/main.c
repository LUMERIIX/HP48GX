/*
 */

#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <avr/iotn88.h>
//#include <usbconfig.h>

#define PWRLED (PORTD | 0b00001000)
#define A1     (PINC0)

int main(void)
{

    DDRD = DDRD | 0b00001111;   //Ausgang
    DDRC = 0x00;
    PORTC = 0x00;               //Eingang

    while(1)
    {

      PORTD = 0b00000110;
      if( !(PINC & ( 1<<PINC2 )))
      {
          PORTD = PWRLED;
          _delay_ms(1000);
          PORTD = PORTD & 0b11110111;
      }

    }

    return 0;
}
