
/* Tomato garden:
Problem statement:

Wiring:
Interactive Simulation:
    1. Build command: g++ -Wall -std=c++11 -fexceptions -std=c++11 -I../../src -g  -o tomatoGarden -x c++ tomatoGarden.ino
Note:
*/

#include "tomatoGarden.h"
#include <ofsm.impl.h>

/*-------------------------------------------
    Configuration
---------------------------------------------*/
#ifdef OFSM_CONFIG_SIMULATION
#   define DutyCyclePeriodTicks 3*60*100L   /* full duty cycle 18sec (18000 ticks when tick == 1 msc) */
#   define InformerBlinkMaxDelay 300L       /* maximum number of ticks for informer diode to remain in on/off state before switching to opposite */
#   define InformerBlinkMinDelay 50L        /* minimum number of ticks for informer diode to remain in on/off state before switching to opposite */
#else
#   define DutyCyclePeriodTicks 3*60*1000L  /* full duty cycle 3min (180000 ticks when tick == 1 msc) */
#   define InformerBlinkMaxDelay 3000L      /* maximum number of ticks for informer diode to remain in on/off state before switching to opposite */
#   define InformerBlinkMinDelay 10L        /* minimum number of ticks for informer diode to remain in on/off state before switching to opposite */
#endif // OFSM_CONFIG_SIMULATION

#define MaxPumpingPctOfDutyCycle 33 /* 33% ~ 1 min if DC = 3min */
#define MinPumpingPctOfDutyCycle 3  /* 3%  ~ 6 secs if DC = 3min */
#define PumpingPctRange (MaxPumpingPctOfDutyCycle - MinPumpingPctOfDutyCycle)

#define PumpRelayPin 5 /*D5*/
#define InformerPin 13
#define VoltageDividerPin A0


/*-------------------------------------------
    Implementation
---------------------------------------------*/
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
enum PumpStates {Waiting = 0, Pumping};
enum PumpEvents {TIMEOUT = 0, PUMPIMP_RATE_CALCULATED, SIMULATION_PUMPING_PERCENT};

enum InformerStates {Waiting_Informer = 0};
enum InformerEvents {TIMEOUT_INFORMER = 0};

/* Handlers declaration */
void OnPumpTimeout();
void OnPumpSimulation();
void OnPumpWaiting();
void OnPumpPumping();

void OnInformerTimeout();

/* transition table */
OFSMTransition pumpTrasitionTable[][1 + SIMULATION_PUMPING_PERCENT] = {
    /* TIMEOUT                   PUMPIMP_RATE_CALCULATED    SIMULATION_PUMPING_PERCENT*/
    { { OnPumpTimeout,    Waiting }, { OnPumpWaiting,   Pumping },  { OnPumpSimulation,    Waiting },  },    /*Waiting*/
    { { OnPumpTimeout,    Pumping }, { OnPumpPumping,   Waiting },  { OnPumpSimulation,    Pumping },  }     /*Pumping*/
};

OFSMTransition informerTrasitionTable[][1 + TIMEOUT_INFORMER] = {
    /* TIMEOUT */
    { { OnInformerTimeout,    Waiting_Informer } }    /*Waiting*/
};

/*other defines*/
enum FsmId  {PumpFsm = 0, InformerFsm};
enum FsmGrpId {PumpGrp = 0, InformerGrp};

OFSM_DECLARE_FSM(PumpFsm, pumpTrasitionTable, 1 + SIMULATION_PUMPING_PERCENT, NULL, NULL, Waiting);
OFSM_DECLARE_GROUP_1(PumpGrp, 3, PumpFsm);

OFSM_DECLARE_FSM(InformerFsm, informerTrasitionTable, 1 + TIMEOUT_INFORMER, NULL, NULL, Waiting);
OFSM_DECLARE_GROUP_1(InformerGrp, 1, InformerFsm);

OFSM_DECLARE_2(PumpGrp, InformerGrp);

/* Setup */
void setup() {
#if OFSM_MCU_BLOCK
    /* set up Serial library at 9600 bps */
    Serial.begin(9600);
  /* configure pins*/
    pinMode(PumpRelayPin, OUTPUT);
    digitalWrite(PumpRelayPin, HIGH);
    pinMode(InformerPin, OUTPUT);
#endif

  OFSM_SETUP();
}

void loop() {
    ofsm_queue_global_event(false, 0, 0); /*queue timeout to wakeup FSMs*/
    OFSM_LOOP();
}

static uint8_t PumpingPctOfDutyCycle = 50; /* actual measured pumping percent of duty cycle */

/* FSM Handlers */

uint8_t informerPinState = 1;
void OnInformerTimeout() {
    //uint8_t nextState = ofsm_query_fsm_next_state(PumpGrp, PumpFsm);
    unsigned long sleepPeriod = ofsm_query_fsm_time_left_before_timeout(PumpGrp, PumpFsm);

    if(0 == sleepPeriod) {
        return;
    }
    //if(Pumping == nextState) {
        /*switch informer diode with frequency that is 1/20 of time left before pumping,
        but don't keep it in the same state longer than InformerBlinkMaxDelay tics and less than InformerBlinkMinDelay*/
        informerPinState ^= 1;
        sleepPeriod = (unsigned long)sleepPeriod/20;
        sleepPeriod = MIN(InformerBlinkMaxDelay, sleepPeriod);
        sleepPeriod = MAX(InformerBlinkMinDelay, sleepPeriod);
//    } else {
//        /*when pumping, turn diode on*/
//        informerPinState ^= 1;
//        sleepPeriod = 500;
//    }
    fsm_set_transition_delay_deep_sleep(sleepPeriod);
    ofsm_debug_printf(2, "I: %i for %lu ticks.\n", informerPinState, sleepPeriod);
#if OFSM_MCU_BLOCK
    digitalWrite(InformerPin, informerPinState);
    Serial.print((informerPinState ? "+" : "-"));
    delay(10); /*let serial to flush*/
#endif // OFSM_MCU_BLOCK
}

void OnPumpSimulation() {
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t pctOfPumpingRange = fsm_get_event_data(); /*in simulation mode we expect percentage (between 0 - 100%) of the allowed pumping range*/
    PumpingPctOfDutyCycle = MinPumpingPctOfDutyCycle + (uint8_t)((long)PumpingPctRange * (long)pctOfPumpingRange / 100);
    unsigned long continueSleepingFor =  fsm_get_time_left_before_timeout();
    ofsm_debug_printf(1, "Setting Pumping Percent of duty Cycle %i, continue sleeping for %lu ticks\n", PumpingPctOfDutyCycle, continueSleepingFor);
    fsm_set_transition_delay_deep_sleep(continueSleepingFor);
#endif
}

void OnPumpTimeout() {
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
    Serial.println(""); Serial.print("Pumping "); Serial.print(PumpingPctOfDutyCycle); Serial.println("% of DC");
#endif /* OFSM_MCU_BLOCK */
}

void OnPumpWaiting() {
    unsigned long waitSleepPeriod = (100 - PumpingPctOfDutyCycle) * DutyCyclePeriodTicks / 100;
    ofsm_debug_printf(1, "Pump OFF for = %lu ticks\n", waitSleepPeriod);
    fsm_set_transition_delay_deep_sleep(waitSleepPeriod);
#if OFSM_MCU_BLOCK
    digitalWrite(PumpRelayPin, HIGH);
    Serial.print("Pump OFF for = "); Serial.println(waitSleepPeriod);
    delay(50); /*let serial to flush*/
#endif /* OFSM_MCU_BLOCK */
}

void OnPumpPumping() {
    unsigned long pumpingSleepPeriod = (PumpingPctOfDutyCycle) * DutyCyclePeriodTicks / 100;
    ofsm_debug_printf(1, "Pump ON for = %lu ticks\n", pumpingSleepPeriod);
    fsm_set_transition_delay_deep_sleep(pumpingSleepPeriod);
#if OFSM_MCU_BLOCK
    digitalWrite(PumpRelayPin, LOW);
    Serial.print("Pump ON for = "); Serial.println(pumpingSleepPeriod);
    delay(50); /*let serial to flush*/
#endif /* OFSM_MCU_BLOCK */
}
