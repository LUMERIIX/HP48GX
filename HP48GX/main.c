/*---------------------------------------------------------------------------
------------------------------HP48GX-----------------------------------------
---------------------------------------------------------------------------*/

//-------------------------------defines-----------------------------------
#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include "usbconfig.h"
#include "usbdrv/usbdrv.h"
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdint.h>


#define STATE_WAIT 0
#define STATE_SEND_KEY 1
#define STATE_RELEASE_KEY 2

#define ROWS 10
#define DEBOUNCE_CYCLES 8
#define TAB2HP  (60) ///F3
//-------------------------------globale Variablen----------------------------

uchar state = STATE_WAIT;

typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report; // sent to PC
static uchar idleRate; // repeat rate for keyboards

static uint16_t activeRow = 0x01;
static uint8_t activeRowIdx = 0;
static uint8_t keyStates[ROWS] = {0};

int EnableKeyPressed = 0;
static int isrFlag = 0;
int keybuild = 0;
int OnCount = 0;
int FAIL = 0;
uint8_t changedKeys = 0;
static uint8_t debouncing[DEBOUNCE_CYCLES][ROWS]={{0}}; //uint8_t
PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xFF,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0xFF,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xFF,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0xFF,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

                    //    A5,A4,A3,A2,A1,A0
uchar ScanCode [10][6] = {{0,5,6,7,8,9},
              /*[y][x]*/  {0,11,12,13,14,15},
                          {0,17,18,19,20,21},
                          {0,23,24,25,26,27},
                          {0,88,29,28,76,42},
                          {43,22,36,37,38,84},
             //             {0,0,0,0,0,0}, // unused
                   /*225*/{225,10,33,34,35,85},
                   /*224*/{224,4,30,31,32,86},
                          {0,16,39,55,44,87},
                          {41,0,0,0,0,0,}};

//---------------------------------Funktionen-----------------------------------
usbMsgLen_t usbFunctionSetup(unsigned char data[8]) {
    usbRequest_t *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (void *)&keyboard_report; // we only have this one
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }

    return 0; // by default don't return any data
}



usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len)
{
    return 1;
}

void buildReport(uchar send_key)
{
	keyboard_report.modifier = 0;

    keyboard_report.keycode[0] = send_key;

}

ISR(TIMER0_OVF_vect)
{
    isrFlag = 1;
}
void scanMatrix(void)
{
    if(!isrFlag)
        return;
    isrFlag = 0;
    uint8_t newVal = PIND & 0xF3; /// mask data lines

//    int allEqual = 1;
//    int idx = 0;
//    for (idx = 0 ; idx < DEBOUNCE_CYCLES-1; ++idx)
//    {
//        if ( newVal != debouncing[idx][activeRowIdx] )
//            allEqual = 0;
//        debouncing[idx][activeRowIdx] = debouncing[idx+1][activeRowIdx];
//
//    }
//    if ( newVal != debouncing[idx][activeRowIdx] )
//        allEqual = 0;
//    debouncing[idx][activeRowIdx] = newVal;


    uint8_t changedKeys = keyStates[activeRowIdx] ^ newVal;
    keyStates[activeRowIdx] = newVal;
    if(/*allEqual && */changedKeys)
    {
        if((~newVal&0x01) && (changedKeys&0x01))///A5
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][0]);//A5
        }
         else if((~newVal&0x02) && (changedKeys&0x02)) ///A4
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][1]);//A0
        }
        else if((~newVal&0x10) && (changedKeys&0x10)) ///A3
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][2]);//A3
        }
        else if((~newVal&0x20) && (changedKeys&0x20)) ///A2
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][3]);//A2
        }
        else if((~newVal&0x40) && (changedKeys&0x40)) ///A1
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][4]);//A0
        }
        else if((~newVal&0x80) && (changedKeys&0x80)) ///A0
        {
            state = STATE_SEND_KEY;
            keybuild = (ScanCode[activeRowIdx][5]);//A0
        }
    }
    activeRow <<= 1;
    activeRowIdx += 1;
    if(activeRow == 0x40)
    {
        activeRow <<= 1;
    }
    if(activeRow == 0x400)
    {
        activeRow = 0x01;
        activeRowIdx = 0;
    }

    DDRC = activeRow;
    DDRA = ((activeRow >> 8) | 0x04);
}

void OnKey (void)
{
    if(state == STATE_SEND_KEY)
        return;

    int OnVal = (PINB&0x80); ///Mask PIN7
//    int allEqual = 1;
//    int idx = 0;
//    for (idx = 0; idx < DEBOUNCE_CYCLES - 1; ++idx)
//    {
//        allEqual &= (OnVal == debouncing[idx][9]);
//        debouncing[idx][9] = debouncing[idx+1][9];
//
//    }
//    allEqual &= (OnVal == debouncing[idx][9]);
//    debouncing[idx][9] = OnVal;

    uint8_t changedKeysOn = keyStates[9] ^ OnVal;
    keyStates[9] = OnVal;


    if((~OnVal&0x80) /*&& (changedKeysOn&0x80)*/)///ONKEY
    {
        PORTB = 0b00000001;
        EnableKeyPressed = 1;
    }


}

void KeyPressed (void)
{
    if(EnableKeyPressed == 0)
        return;
    if((PINB&0x80)==0)
    {
        OnCount++;

        if(OnCount == 800)///pressed and hold
        {
            keybuild = (TAB2HP);
            state = STATE_SEND_KEY;
            EnableKeyPressed = 0;
            OnCount = 0;
        }
    }
    if((OnCount < 800) && (OnCount > 600))///pressed
    {
        keybuild = (ScanCode[9][0]);
        state = STATE_SEND_KEY;
        EnableKeyPressed = 0;
    }

}

void Init (void)
{
    PCMSK0  = 0b00000000;
    PCMSK2  = 0b00000000;
    PCMSK3  = 0b00000000;
    DDRC    = 0b00000000;
    PORTC   = 0b00000000;
    DDRD   &= 0b00001100;
    PORTD  &= 0b00000000;
    DDRA    = 0b00000100;
    PORTA   = 0b00000000;
    TCCR1A  = 0b00000000;
    TCCR1B  = 0b00000000;
    TCCR1C  = 0b00000000;
    DDRB    = 0b00000001;
    PORTB   = 0b00000000;
}
//----------------------Main--------------------------------
int main(void)
{
    Init();
//-------------------------------------
    uchar i;
    for(i=0; i<sizeof(keyboard_report); i++) // clear report initially
        ((uchar *)&keyboard_report)[i] = 0;
    wdt_enable(WDTO_1S); // enable 1s watchdog timer
    usbInit();
    usbDeviceDisconnect(); // enforce re-enumeration

    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbPoll();
    usbDeviceConnect();

    cli();
    TCCR0A = 0b00000011; //clk/8 ==> timeout 1.35ms
    TIMSK0 = 0b00000001;
    sei();

    while(1)
    {
        KeyPressed();
        OnKey();
        scanMatrix();
        wdt_reset(); // keep the watchdog happy
        usbPoll();

        if(usbInterruptIsReady() && state != STATE_WAIT)
        {
            switch(state)
			{
			case STATE_SEND_KEY:
                buildReport(keybuild);
				state = STATE_RELEASE_KEY; // release next
				break;
			case STATE_RELEASE_KEY:
                buildReport(0);
                state = STATE_WAIT; // go back to waiting
                break;
			default:
                state = STATE_WAIT; // should not happen
			}
        }
        usbSetInterrupt((void *)&keyboard_report, sizeof(keyboard_report)); // start sending
    }

    return 0;
}
