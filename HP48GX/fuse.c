#include <avr/io.h>

FUSES = {
    .low = 0xEC,            //Clock Source = Ext. crystal 8-..MHz, Start-up Time = 64ms, SUT11
    .high = 0xDF,           //Enable Serial Programming = ON
    .extended = 0xFF        //Self Programming Enable = OFF
};
