#include <util/atomic.h>
#ifndef sbi 
# define sbi(reg, bitToSet) (reg |= (1 << bitToSet))
#endif
#ifndef cbi
# define cbi(reg, bitToClear) (reg &= ~(1 << bitToClear))
#endif

#define ledPin 13

void setup()
{
  pinMode(ledPin, OUTPUT);

  Serial.begin(9600);
/*Timer 0 control registers map:
         7       6       5       4       3     2     1     0
TCCR0A:  COM0A1  COM0A0  COM0B1  COM0B0  -     -     WGM01 WGM00
TCCR0B:  FOC0A   FOC0B   -       -       WGM02 CS02  CS01  CS00
*/
  
  Serial.print("TCCR2A: ");
  Serial.println(TCCR2A, HEX);
  Serial.print("TCCR2B: ");
  Serial.println(TCCR2B, HEX);
  // initialize timer2 
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {     // disable all interrupts
    /*set Clear Timer on Compare (CTC): wgm20, WGM21, wgmX2 (16 bit timers only)) */
    cbi(TCCR2A, WGM20);
    sbi(TCCR2A, WGM21);
    /* not applicable for 8bit timers cbi(TCCR2B, WGMX2); */
    /*normal operation, disable PB7(OC2A/PCINT15 [Output Compare and PWM Output A for Timer/Counter2]):  com2a0, com2a1*/
    cbi(TCCR2A, COM2A0);
    cbi(TCCR2A, COM2A1);
    /*set 1024 prescale, (1/ClockFreq(16Mhz))*1024 = 0.000064 seconds per tick: CS20, cs21, CS22  */
    sbi(TCCR2B, CS20);
    cbi(TCCR2B, CS21);
    sbi(TCCR2B, CS22);
    /* Set timer counter compare match: interrupt every  0.01632 sec (0.000064 * 255) */
    OCR2A = 255 - 1;
    /*enable interrupt TimerCounter2 Compare Match A: OCIE2A*/
    sbi(TIMSK2, OCIE2A);
    /*set counter to 0*/
    TCNT2 = 0;
  }
  Serial.print("TCCR2A: ");
  Serial.println(TCCR2A, HEX);
  Serial.print("TCCR2B: ");
  Serial.println(TCCR2B, HEX);
  
}
//TODO: expected 1.6sec on/off I am getting about 1/4 sec on/off why???
volatile uint8_t counter;
volatile uint8_t on = 0;
volatile unsigned long led_switched = 0;
volatile unsigned long vector_called = 0;
ISR(TIMER2_COMPA_vect)          // timer compare interrupt service routine
{
  vector_called++;
  counter++;
  if(counter > 100) {
    counter = 0;
    on = on ^ 1;
    led_switched++;
    digitalWrite(ledPin, on);   // toggle LED pin
  }
}
int stopPrintCount = 40;
void loop()
{
    delay(1000);
    if(--stopPrintCount > 0) {
      //Serial.println(stopPrintCount);
      Serial.print("ISR count: ");
      Serial.print(vector_called);
      Serial.print("; Led toggled: ");
      Serial.print(led_switched);
      Serial.print("; MS: ");
      Serial.println(millis());
    }
}