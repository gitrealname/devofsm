/* Tomato garden:
Problem statement:

Wiring:
    1. [Pin12] --> [Diode+]   [Diode-] --> [R(220 Ohm)] --> GND
    2. [Pin2]  --> [ButtonTerminalA]     [ButtonTerminalB] --> GND
Interactive Simulation:
    1. Build command: g++ -Wall -std=c++11 -fexceptions -std=c++11 -I../../src -g  -o tomatoGarden -x c++ tomatoGarden.ino
Note:
*/

#include "tomatoGarden.h"
#include <ofsm.impl.h>

#define DutyCyclePeriodTicks 3*60L  /* full duty cycle 3min (180 ticks when tick == 1 sec) */
#define MaxPumpingPctOfDutyCycle 33 /* 33% ~ 1 min if DC = 3min */
#define MinPumpingPctOfDutyCycle 3  /* 3%  ~ 6 secs if DC = 3min */
#define PumpingPctRange (MaxPumpingPctOfDutyCycle - MinPumpingPctOfDutyCycle)

#if OFSM_MCU_BLOCK
static long lastVccMv; /*last measured Vcc voltage in millivolts*/
#   define AnalogReadVoltageScale 1121508L
#   define analogReadMv(pin) (lastVccMv * analogRead(pin) / 1023)

static long readVccMv(long avarageCount = 1) {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  lastVccMv = 0;
  for(int i = 0; i < avarageCount; i++) {
    delay(2); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both

    lastVccMv += ((high<<8) | low);
  }

  lastVccMv = AnalogReadVoltageScale * avarageCount / lastVccMv; // Calculate Vcc (in mV)
  return lastVccMv; // Vcc in millivolts
}
#endif /*OFSM_MCU_BLOCK*/

/*define events and states*/
enum States {Waiting = 0, Pumping};
enum Events {TIMEOUT = 0, PUMPIMP_RATE_CALCULATED, SIMULATION_PUMPING_PERCENT};

/* Handlers declaration */
void OnTimeout();
void OnSimulation();
void OnWaiting();
void OnPumping();

/* transition table */
OFSMTransition trasitionTable[][1 + SIMULATION_PUMPING_PERCENT] = {
    /* TIMEOUT                   PUMPIMP_RATE_CALCULATED    SIMULATION_PUMPING_PERCENT*/
    { { OnTimeout,    Waiting }, { OnWaiting,   Pumping },  { OnSimulation,    Waiting },  },    /*Waiting*/
    { { OnTimeout,    Pumping }, { OnPumping,   Waiting },  { OnSimulation,    Pumping },  }     /*Pumping*/
};

/*other defines*/
enum FsmId  {PumpFsm = 0};
enum FsmGrpId {Default = 0};

OFSM_DECLARE_FSM(PumpFsm, trasitionTable, 1 + SIMULATION_PUMPING_PERCENT, NULL, NULL, Waiting);
OFSM_DECLARE_GROUP_1(Default, EVENT_QUEUE_SIZE, PumpFsm);
OFSM_DECLARE_1(Default);

#define PumpRelayPin 5 /*D5*/
#define VoltageDividerPin A0

/* Setup */
void setup() {
#if OFSM_MCU_BLOCK
    /* set up Serial library at 9600 bps */
    Serial.begin(9600);
    pinMode(PumpRelayPin, OUTPUT);
    pinMode(13, OUTPUT);
#endif
    OFSM_SETUP();

    digitalWrite(PumpRelayPin, HIGH);

    //blink that it is operational
    uint8_t on = 1;
    for(int i = 0; i < 6; i++) {
      digitalWrite(13, on);
      on ^= 1;
      delay(500);
    }
}

void loop() {
    ofsm_queue_global_event(false, 0, 0); /*queue timeout to wakeup FSMs*/
    OFSM_LOOP();
}

static uint8_t PumpingPctOfDutyCycle = 50; /* actual measured pumping percent of duty cycle */

/* FSM Handlers */
void OnSimulation() {
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t pctOfPumpingRange = fsm_get_event_data(); /*in simulation mode we expect percentage (between 0 - 100%) of the allowed pumping range*/
    PumpingPctOfDutyCycle = MinPumpingPctOfDutyCycle + (uint8_t)((long)PumpingPctRange * (long)pctOfPumpingRange / 100);
    unsigned long continueSleepingFor =  fsm_get_time_left_before_timeout();
    ofsm_debug_printf(1, "Setting Pumping Percent of duty Cycle %i, continue sleeping for %lu ticks\n", PumpingPctOfDutyCycle, continueSleepingFor);
    fsm_set_transition_delay_deep_sleep(continueSleepingFor);
#endif
}

void OnTimeout() {
ofsm_debug_printf(1, "Pumping %% of DC = %i\n", PumpingPctOfDutyCycle);
fsm_queue_group_event(false, PUMPIMP_RATE_CALCULATED, PumpingPctOfDutyCycle);
#if OFSM_MCU_BLOCK
    /*turn off relay to prevent voltage "sagging"*/
    digitalWrite(PumpRelayPin, HIGH);
    /*measure Vcc (averaging over 3 measures)  */
    long vccMv = readVccMv(3);
    /*read voltage divider*/
    long vdMv = analogReadMv(VoltageDividerPin);
    /*determine Pumping Percent of Duty Cycle*/
    PumpingPctOfDutyCycle = MinPumpingPctOfDutyCycle + (uint8_t)((long)PumpingPctRange * vdMv / vccMv);
    Serial.print("Pumping "); Serial.print(PumpingPctOfDutyCycle); Serial.println("% of DC");
#endif /* OFSM_MCU_BLOCK */
}

void OnWaiting() {
    unsigned long waitSleepPeriod = (100 - PumpingPctOfDutyCycle) * DutyCyclePeriodTicks / 100;
    ofsm_debug_printf(1, "Pump OFF for = %lu ticks\n", waitSleepPeriod);
    fsm_set_transition_delay_deep_sleep(waitSleepPeriod);
#if OFSM_MCU_BLOCK
    digitalWrite(PumpRelayPin, HIGH);
    Serial.print("Pump OFF for = "); Serial.println(waitSleepPeriod);
    delay(50); /*let serial to flush*/
#endif /* OFSM_MCU_BLOCK */
}

void OnPumping() {
    unsigned long pumpingSleepPeriod = (PumpingPctOfDutyCycle) * DutyCyclePeriodTicks / 100;
    ofsm_debug_printf(1, "Pump ON for = %lu ticks\n", pumpingSleepPeriod);
    fsm_set_transition_delay_deep_sleep(pumpingSleepPeriod);
#if OFSM_MCU_BLOCK
    digitalWrite(PumpRelayPin, LOW);
    Serial.print("Pump ON for = "); Serial.println(pumpingSleepPeriod);
    delay(50); /*let serial to flush*/
#endif /* OFSM_MCU_BLOCK */
}
