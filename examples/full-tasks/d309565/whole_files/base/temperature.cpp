/*
  temperature.c - temperature control
  Part of Marlin
  
 Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 This firmware is a mashup between Sprinter and grbl.
  (https://github.com/kliment/Sprinter)
  (https://github.com/simen/grbl/tree)
 
 It has preliminary support for Matthew Roberts advance algorithm 
    http://reprap.org/pipermail/reprap-dev/2011-May/003323.html

 */


#include "Marlin.h"
#include "ultralcd.h"
#include "temperature.h"
#include "watchdog.h"

//===========================================================================
//=============================public variables============================
//===========================================================================
int target_raw[EXTRUDERS] = { 0 };
int target_raw_bed = 0;
#ifdef BED_LIMIT_SWITCHING
int target_bed_low_temp =0;  
int target_bed_high_temp =0;
#endif
int current_raw[EXTRUDERS] = { 0 };
int current_raw_bed = 0;

#ifdef PIDTEMP
  // used external
  float pid_setpoint[EXTRUDERS] = { 0.0 };
  
  float Kp=DEFAULT_Kp;
  float Ki=DEFAULT_Ki;
  float Kd=DEFAULT_Kd;
  #ifdef PID_ADD_EXTRUSION_RATE
    float Kc=DEFAULT_Kc;
  #endif
#endif //PIDTEMP
  
  
//===========================================================================
//=============================private variables============================
//===========================================================================
static bool temp_meas_ready = false;

static unsigned long  previous_millis_bed_heater;
//static unsigned long previous_millis_heater;

#ifdef PIDTEMP
  //static cannot be external:
  static float temp_iState[EXTRUDERS] = { 0 };
  static float temp_dState[EXTRUDERS] = { 0 };
  static float pTerm[EXTRUDERS];
  static float iTerm[EXTRUDERS];
  static float dTerm[EXTRUDERS];
  //int output;
  static float pid_error[EXTRUDERS];
  static float temp_iState_min[EXTRUDERS];
  static float temp_iState_max[EXTRUDERS];
  // static float pid_input[EXTRUDERS];
  // static float pid_output[EXTRUDERS];
  static bool pid_reset[EXTRUDERS];
#endif //PIDTEMP
  static unsigned char soft_pwm[EXTRUDERS];
  
#ifdef WATCHPERIOD
  static int watch_raw[EXTRUDERS] = { -1000 }; // the first value used for all
  static int watch_oldtemp[3] = {0,0,0};
  static unsigned long watchmillis = 0;
#endif //WATCHPERIOD

// Init min and max temp with extreme values to prevent false errors during startup
  static int minttemp[EXTRUDERS] = { 0 };
  static int maxttemp[EXTRUDERS] = { 16383 }; // the first value used for all
  static int bed_minttemp = 0;
  static int bed_maxttemp = 16383;
  static int heater_pin_map[EXTRUDERS] = { HEATER_0_PIN
#if EXTRUDERS > 1
                                         , HEATER_1_PIN
#endif
#if EXTRUDERS > 2
                                         , HEATER_2_PIN
#endif
#if EXTRUDERS > 3
  #error Unsupported number of extruders
#endif
  };
  static void *heater_ttbl_map[EXTRUDERS] = { (void *)heater_0_temptable
#if EXTRUDERS > 1
                                            , (void *)heater_1_temptable
#endif
#if EXTRUDERS > 2
                                            , (void *)heater_2_temptable
#endif
#if EXTRUDERS > 3
  #error Unsupported number of extruders
#endif
  };
  static int heater_ttbllen_map[EXTRUDERS] = { heater_0_temptable_len
#if EXTRUDERS > 1
                                             , heater_1_temptable_len
#endif
#if EXTRUDERS > 2
                                             , heater_2_temptable_len
#endif
#if EXTRUDERS > 3
  #error Unsupported number of extruders
#endif
  };

//===========================================================================
//=============================   functions      ============================
//===========================================================================
  
void updatePID()
{
#ifdef PIDTEMP
  for(int e = 0; e < EXTRUDERS; e++) { 
     temp_iState_max[e] = PID_INTEGRAL_DRIVE_MAX / Ki;  
  }
#endif
}
  
int getHeaterPower(int heater) {
  return soft_pwm[heater];
}

void manage_heater()
{
  #ifdef USE_WATCHDOG
    wd_reset();
  #endif
  
  float pid_input;
  float pid_output;

  if(temp_meas_ready != true)   //better readability
    return; 

  CRITICAL_SECTION_START;
  temp_meas_ready = false;
  CRITICAL_SECTION_END;

  for(int e = 0; e < EXTRUDERS; e++) 
  {

  #ifdef PIDTEMP
    pid_input = analog2temp(current_raw[e], e);

    #ifndef PID_OPENLOOP
        pid_error[e] = pid_setpoint[e] - pid_input;
        if(pid_error[e] > 10) {
          pid_output = PID_MAX;
          pid_reset[e] = true;
        }
        else if(pid_error[e] < -10) {
          pid_output = 0;
          pid_reset[e] = true;
        }
        else {
          if(pid_reset[e] == true) {
            temp_iState[e] = 0.0;
            pid_reset[e] = false;
          }
          pTerm[e] = Kp * pid_error[e];
          temp_iState[e] += pid_error[e];
          temp_iState[e] = constrain(temp_iState[e], temp_iState_min[e], temp_iState_max[e]);
          iTerm[e] = Ki * temp_iState[e];
          //K1 defined in Configuration.h in the PID settings
          #define K2 (1.0-K1)
          dTerm[e] = (Kd * (pid_input - temp_dState[e]))*K2 + (K1 * dTerm[e]);
          temp_dState[e] = pid_input;
          pid_output = constrain(pTerm[e] + iTerm[e] - dTerm[e], 0, PID_MAX);
        }
    #endif //PID_OPENLOOP
    #ifdef PID_DEBUG
    SERIAL_ECHOLN(" PIDDEBUG "<<e<<": Input "<<pid_input<<" Output "<<pid_output" pTerm "<<pTerm[e]<<" iTerm "<<iTerm[e]<<" dTerm "<<dTerm[e]);  
    #endif //PID_DEBUG
  #else /* PID off */
    pid_output = 0;
    if(current_raw[e] < target_raw[e]) {
      pid_output = PID_MAX;
    }
  #endif

    // Check if temperature is within the correct range
    if((current_raw[e] > minttemp[e]) && (current_raw[e] < maxttemp[e])) 
    {
      //analogWrite(heater_pin_map[e], pid_output);
      soft_pwm[e] = (int)pid_output >> 1;
    }
    else {
      //analogWrite(heater_pin_map[e], 0);
      soft_pwm[e] = 0;
    }
  } // End extruder for loop
  
  #ifdef WATCHPERIOD
    if(watchmillis && millis() - watchmillis > WATCHPERIOD){
        if(watch_oldtemp[TEMPSENSOR_HOTEND_0] >= degHotend(active_extruder)){
            setTargetHotend(0,active_extruder);
            LCD_MESSAGEPGM("Heating failed");
            SERIAL_ECHO_START;
            SERIAL_ECHOLN("Heating failed");
        }else{
            watchmillis = 0;
        }
    }
  #endif
  
  if(millis() - previous_millis_bed_heater < BED_CHECK_INTERVAL)
    return;
  previous_millis_bed_heater = millis();
  
  #if TEMP_BED_PIN > -1
  
    #ifndef BED_LIMIT_SWITCHING
      // Check if temperature is within the correct range
      if((current_raw_bed > bed_minttemp) && (current_raw_bed < bed_maxttemp)) {
        if(current_raw_bed >= target_raw_bed)
        {
          WRITE(HEATER_BED_PIN,LOW);
        }
        else 
        {
          WRITE(HEATER_BED_PIN,HIGH);
        }
      }
      else {
        WRITE(HEATER_BED_PIN,LOW);
      }
    #else //#ifdef BED_LIMIT_SWITCHING
      // Check if temperature is within the correct band
      if((current_raw_bed > bed_minttemp) && (current_raw_bed < bed_maxttemp)) {
        if(current_raw_bed > target_bed_high_temp)
        {
          WRITE(HEATER_BED_PIN,LOW);
        }
        else 
          if(current_raw_bed <= target_bed_low_temp)
        {
          WRITE(HEATER_BED_PIN,HIGH);
        }
      }
      else {
        WRITE(HEATER_BED_PIN,LOW);
      }
    #endif
  #endif
}

#define PGM_RD_W(x)   (short)pgm_read_word(&x)
// Takes hot end temperature value as input and returns corresponding raw value. 
// For a thermistor, it uses the RepRap thermistor temp table.
// This is needed because PID in hydra firmware hovers around a given analog value, not a temp value.
// This function is derived from inversing the logic from a portion of getTemperature() in FiveD RepRap firmware.
int temp2analog(int celsius, uint8_t e) {
  if(e >= EXTRUDERS)
  {
      SERIAL_ERROR_START;
      SERIAL_ERROR((int)e);
      SERIAL_ERRORLNPGM(" - Invalid extruder number!");
      kill();
  }
  if(heater_ttbl_map[e] != 0)
  {
    int raw = 0;
    byte i;
    short (*tt)[][2] = (short (*)[][2])(heater_ttbl_map[e]);

    for (i=1; i<heater_ttbllen_map[e]; i++)
    {
      if (PGM_RD_W((*tt)[i][1]) < celsius)
      {
        raw = PGM_RD_W((*tt)[i-1][0]) + 
          (celsius - PGM_RD_W((*tt)[i-1][1])) * 
          (PGM_RD_W((*tt)[i][0]) - PGM_RD_W((*tt)[i-1][0])) /
          (PGM_RD_W((*tt)[i][1]) - PGM_RD_W((*tt)[i-1][1]));  
        break;
      }
    }

    // Overflow: Set to last value in the table
    if (i == heater_ttbllen_map[e]) raw = PGM_RD_W((*tt)[i-1][0]);

    return (1023 * OVERSAMPLENR) - raw;
  }
  return celsius * (1024.0 / (5.0 * 100.0) ) * OVERSAMPLENR;
}

// Takes bed temperature value as input and returns corresponding raw value. 
// For a thermistor, it uses the RepRap thermistor temp table.
// This is needed because PID in hydra firmware hovers around a given analog value, not a temp value.
// This function is derived from inversing the logic from a portion of getTemperature() in FiveD RepRap firmware.
int temp2analogBed(int celsius) {
#ifdef BED_USES_THERMISTOR
    int raw = 0;
    byte i;
    
    for (i=1; i<bedtemptable_len; i++)
    {
      if (PGM_RD_W(bedtemptable[i][1]) < celsius)
      {
        raw = PGM_RD_W(bedtemptable[i-1][0]) + 
          (celsius - PGM_RD_W(bedtemptable[i-1][1])) * 
          (PGM_RD_W(bedtemptable[i][0]) - PGM_RD_W(bedtemptable[i-1][0])) /
          (PGM_RD_W(bedtemptable[i][1]) - PGM_RD_W(bedtemptable[i-1][1]));
      
        break;
      }
    }

    // Overflow: Set to last value in the table
    if (i == bedtemptable_len) raw = PGM_RD_W(bedtemptable[i-1][0]);

    return (1023 * OVERSAMPLENR) - raw;
#elif defined BED_USES_AD595
    return lround(celsius * (1024.0 * OVERSAMPLENR/ (5.0 * 100.0) ) );
#else
    #warning No heater-type defined for the bed.
    return 0;
#endif
}

// Derived from RepRap FiveD extruder::getTemperature()
// For hot end temperature measurement.
float analog2temp(int raw, uint8_t e) {
  if(e >= EXTRUDERS)
  {
      SERIAL_ERROR_START;
      SERIAL_ERROR((int)e);
      SERIAL_ERRORLNPGM(" - Invalid extruder number !");
      kill();
  }
  if(heater_ttbl_map[e] != 0)
  {
    float celsius = 0;
    byte i;  
    short (*tt)[][2] = (short (*)[][2])(heater_ttbl_map[e]);

    raw = (1023 * OVERSAMPLENR) - raw;
    for (i=1; i<heater_ttbllen_map[e]; i++)
    {
      if (PGM_RD_W((*tt)[i][0]) > raw)
      {
        celsius = PGM_RD_W((*tt)[i-1][1]) + 
          (raw - PGM_RD_W((*tt)[i-1][0])) * 
          (float)(PGM_RD_W((*tt)[i][1]) - PGM_RD_W((*tt)[i-1][1])) /
          (float)(PGM_RD_W((*tt)[i][0]) - PGM_RD_W((*tt)[i-1][0]));
        break;
      }
    }

    // Overflow: Set to last value in the table
    if (i == heater_ttbllen_map[e]) celsius = PGM_RD_W((*tt)[i-1][1]);

    return celsius;
  }
  return raw * ((5.0 * 100.0) / 1024.0) / OVERSAMPLENR;
}

// Derived from RepRap FiveD extruder::getTemperature()
// For bed temperature measurement.
float analog2tempBed(int raw) {
  #ifdef BED_USES_THERMISTOR
    int celsius = 0;
    byte i;

    raw = (1023 * OVERSAMPLENR) - raw;

    for (i=1; i<bedtemptable_len; i++)
    {
      if (PGM_RD_W(bedtemptable[i][0]) > raw)
      {
        celsius  = PGM_RD_W(bedtemptable[i-1][1]) + 
          (raw - PGM_RD_W(bedtemptable[i-1][0])) * 
          (PGM_RD_W(bedtemptable[i][1]) - PGM_RD_W(bedtemptable[i-1][1])) /
          (PGM_RD_W(bedtemptable[i][0]) - PGM_RD_W(bedtemptable[i-1][0]));

        break;
      }
    }

    // Overflow: Set to last value in the table
    if (i == bedtemptable_len) celsius = PGM_RD_W(bedtemptable[i-1][1]);

    return celsius;
    
  #elif defined BED_USES_AD595
    return raw * ((5.0 * 100.0) / 1024.0) / OVERSAMPLENR;
  #else
    #warning No heater-type defined for the bed.
  #endif
  return 0;
}

void tp_init()
{
  // Finish init of mult extruder arrays 
  for(int e = 0; e < EXTRUDERS; e++) {
    // populate with the first value 
#ifdef WATCHPERIOD
    watch_raw[e] = watch_raw[0];
#endif
    maxttemp[e] = maxttemp[0];
#ifdef PIDTEMP
    temp_iState_min[e] = 0.0;
    temp_iState_max[e] = PID_INTEGRAL_DRIVE_MAX / Ki;
#endif //PIDTEMP
  }

  #if (HEATER_0_PIN > -1) 
    SET_OUTPUT(HEATER_0_PIN);
  #endif  
  #if (HEATER_1_PIN > -1) 
    SET_OUTPUT(HEATER_1_PIN);
  #endif  
  #if (HEATER_2_PIN > -1) 
    SET_OUTPUT(HEATER_2_PIN);
  #endif  
  #if (HEATER_BED_PIN > -1) 
    SET_OUTPUT(HEATER_BED_PIN);
  #endif  
  #if (FAN_PIN > -1) 
    SET_OUTPUT(FAN_PIN);
  #endif  

  // Set analog inputs
  ADCSRA = 1<<ADEN | 1<<ADSC | 1<<ADIF | 0x07;
  DIDR0 = 0;
  #ifdef DIDR2
    DIDR2 = 0;
  #endif
  #if (TEMP_0_PIN > -1)
    #if TEMP_0_PIN < 8
       DIDR0 |= 1 << TEMP_0_PIN; 
    #else
       DIDR2 |= 1<<(TEMP_0_PIN - 8); 
    #endif
  #endif
  #if (TEMP_1_PIN > -1)
    #if TEMP_1_PIN < 8
       DIDR0 |= 1<<TEMP_1_PIN; 
    #else
       DIDR2 |= 1<<(TEMP_1_PIN - 8); 
    #endif
  #endif
  #if (TEMP_2_PIN > -1)
    #if TEMP_2_PIN < 8
       DIDR0 |= 1 << TEMP_2_PIN; 
    #else
       DIDR2 = 1<<(TEMP_2_PIN - 8); 
    #endif
  #endif
  #if (TEMP_BED_PIN > -1)
    #if TEMP_BED_PIN < 8
       DIDR0 |= 1<<TEMP_BED_PIN; 
    #else
       DIDR2 |= 1<<(TEMP_BED_PIN - 8); 
    #endif
  #endif
  
  // Use timer0 for temperature measurement
  // Interleave temperature interrupt with millies interrupt
  OCR0B = 128;
  TIMSK0 |= (1<<OCIE0B);  
  
  // Wait for temperature measurement to settle
  delay(250);

#ifdef HEATER_0_MINTEMP
  minttemp[0] = temp2analog(HEATER_0_MINTEMP, 0);
#endif //MINTEMP
#ifdef HEATER_0_MAXTEMP
  maxttemp[0] = temp2analog(HEATER_0_MAXTEMP, 0);
#endif //MAXTEMP

#if (EXTRUDERS > 1) && defined(HEATER_1_MINTEMP)
  minttemp[1] = temp2analog(HEATER_1_MINTEMP, 1);
#endif // MINTEMP 1
#if (EXTRUDERS > 1) && defined(HEATER_1_MAXTEMP)
  maxttemp[1] = temp2analog(HEATER_1_MAXTEMP, 1);
#endif //MAXTEMP 1

#if (EXTRUDERS > 2) && defined(HEATER_2_MINTEMP)
  minttemp[2] = temp2analog(HEATER_2_MINTEMP, 2);
#endif //MINTEMP 2
#if (EXTRUDERS > 2) && defined(HEATER_2_MAXTEMP)
  maxttemp[2] = temp2analog(HEATER_2_MAXTEMP, 2);
#endif //MAXTEMP 2

#ifdef BED_MINTEMP
  bed_minttemp = temp2analogBed(BED_MINTEMP);
#endif //BED_MINTEMP
#ifdef BED_MAXTEMP
  bed_maxttemp = temp2analogBed(BED_MAXTEMP);
#endif //BED_MAXTEMP
}



void setWatch() 
{  
#ifdef WATCHPERIOD
  int t = 0;
  for (int e = 0; e < EXTRUDERS; e++)
  {
    if(isHeatingHotend(e))
    watch_oldtemp[TEMPSENSOR_HOTEND_0] = degHotend(0);
    {
      t = max(t,millis());
      watch_raw[e] = current_raw[e];
    } 
  }
  watchmillis = t;
#endif 
}


void disable_heater()
{
  for(int i=0;i<EXTRUDERS;i++)
    setTargetHotend(0,i);
  setTargetBed(0);
  #if TEMP_0_PIN > -1
  target_raw[0]=0;
  soft_pwm[0]=0;
   #if HEATER_0_PIN > -1  
     digitalWrite(HEATER_0_PIN,LOW);
   #endif
  #endif
     
  #if TEMP_1_PIN > -1
    target_raw[1]=0;
    soft_pwm[1]=0;
    #if HEATER_1_PIN > -1 
      digitalWrite(HEATER_1_PIN,LOW);
    #endif
  #endif
      
  #if TEMP_2_PIN > -1
    target_raw[2]=0;
    soft_pwm[2]=0;
    #if HEATER_2_PIN > -1  
      digitalWrite(HEATER_2_PIN,LOW);
    #endif
  #endif 

  #if TEMP_BED_PIN > -1
    target_raw_bed=0;
    #if HEATER_BED_PIN > -1  
      digitalWrite(HEATER_BED_PIN,LOW);
    #endif
  #endif 
}

void max_temp_error(uint8_t e) {
  digitalWrite(heater_pin_map[e], 0);
  SERIAL_ERROR_START;
  SERIAL_ERRORLN(e);
  SERIAL_ERRORLNPGM(": Extruder switched off. MAXTEMP triggered !");
}

void min_temp_error(uint8_t e) {
  digitalWrite(heater_pin_map[e], 0);
  SERIAL_ERROR_START;
  SERIAL_ERRORLN(e);
  SERIAL_ERRORLNPGM(": Extruder switched off. MINTEMP triggered !");
}

void bed_max_temp_error(void) {
  digitalWrite(HEATER_BED_PIN, 0);
  SERIAL_ERROR_START;
  SERIAL_ERRORLNPGM("Temperature heated bed switched off. MAXTEMP triggered !!");
}

// Timer 0 is shared with millies
ISR(TIMER0_COMPB_vect)
{
  //these variables are only accesible from the ISR, but static, so they don't loose their value
  static unsigned char temp_count = 0;
  static unsigned long raw_temp_0_value = 0;
  static unsigned long raw_temp_1_value = 0;
  static unsigned long raw_temp_2_value = 0;
  static unsigned long raw_temp_bed_value = 0;
  static unsigned char temp_state = 0;
  static unsigned char pwm_count = 1;
  static unsigned char soft_pwm_0;
  static unsigned char soft_pwm_1;
  static unsigned char soft_pwm_2;
  
  if(pwm_count == 0){
    soft_pwm_0 = soft_pwm[0];
    if(soft_pwm_0 > 0) WRITE(HEATER_0_PIN,1);
    #if EXTRUDERS > 1
    soft_pwm_1 = soft_pwm[1];
    if(soft_pwm_1 > 0) WRITE(HEATER_1_PIN,1);
    #endif
    #if EXTRUDERS > 2
    soft_pwm_2 = soft_pwm[2];
    if(soft_pwm_2 > 0) WRITE(HEATER_2_PIN,1);
    #endif
  }
  if(soft_pwm_0 <= pwm_count) WRITE(HEATER_0_PIN,0);
  #if EXTRUDERS > 1
  if(soft_pwm_1 <= pwm_count) WRITE(HEATER_1_PIN,0);
  #endif
  #if EXTRUDERS > 2
  if(soft_pwm_2 <= pwm_count) WRITE(HEATER_2_PIN,0);
  #endif
  
  pwm_count++;
  pwm_count &= 0x7f;
  
  switch(temp_state) {
    case 0: // Prepare TEMP_0
      #if (TEMP_0_PIN > -1)
        #if TEMP_0_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_0_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      #ifdef ULTIPANEL
        buttons_check();
      #endif
      temp_state = 1;
      break;
    case 1: // Measure TEMP_0
      #if (TEMP_0_PIN > -1)
        raw_temp_0_value += ADC;
      #endif
      temp_state = 2;
      break;
    case 2: // Prepare TEMP_BED
      #if (TEMP_BED_PIN > -1)
        #if TEMP_BED_PIN > 7
          ADCSRB = 1<<MUX5;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_BED_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      #ifdef ULTIPANEL
        buttons_check();
      #endif
      temp_state = 3;
      break;
    case 3: // Measure TEMP_BED
      #if (TEMP_BED_PIN > -1)
        raw_temp_bed_value += ADC;
      #endif
      temp_state = 4;
      break;
    case 4: // Prepare TEMP_1
      #if (TEMP_1_PIN > -1)
        #if TEMP_1_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_1_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      #ifdef ULTIPANEL
        buttons_check();
      #endif
      temp_state = 5;
      break;
    case 5: // Measure TEMP_1
      #if (TEMP_1_PIN > -1)
        raw_temp_1_value += ADC;
      #endif
      temp_state = 6;
      break;
    case 6: // Prepare TEMP_2
      #if (TEMP_2_PIN > -1)
        #if TEMP_2_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_2_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      #ifdef ULTIPANEL
        buttons_check();
      #endif
      temp_state = 7;
      break;
    case 7: // Measure TEMP_2
      #if (TEMP_2_PIN > -1)
        raw_temp_2_value += ADC;
      #endif
      temp_state = 0;
      temp_count++;
      break;
//    default:
//      SERIAL_ERROR_START;
//      SERIAL_ERRORLNPGM("Temp measurement error!");
//      break;
  }
    
  if(temp_count >= 16) // 8 ms * 16 = 128ms.
  {
    #ifdef HEATER_0_USES_AD595
      current_raw[0] = raw_temp_0_value;
    #else
      current_raw[0] = 16383 - raw_temp_0_value;
    #endif

#if EXTRUDERS > 1    
    #ifdef HEATER_1_USES_AD595
      current_raw[1] = raw_temp_1_value;
    #else
      current_raw[1] = 16383 - raw_temp_1_value;
    #endif
#endif
    
#if EXTRUDERS > 2
    #ifdef HEATER_2_USES_AD595
      current_raw[2] = raw_temp_2_value;
    #else
      current_raw[2] = 16383 - raw_temp_2_value;
    #endif
#endif
    
    #ifdef BED_USES_AD595
      current_raw_bed = raw_temp_bed_value;
    #else
      current_raw_bed = 16383 - raw_temp_bed_value;
    #endif
    
    temp_meas_ready = true;
    temp_count = 0;
    raw_temp_0_value = 0;
    raw_temp_1_value = 0;
    raw_temp_2_value = 0;
    raw_temp_bed_value = 0;

    for(unsigned char e = 0; e < EXTRUDERS; e++) {
       if(current_raw[e] >= maxttemp[e]) {
          target_raw[e] = 0;
          #if (PS_ON != -1)
          {
            max_temp_error(e);
            kill();;
          }
          #endif
       }
       if(current_raw[e] <= minttemp[e]) {
          target_raw[e] = 0;
          #if (PS_ON != -1)
          {
            min_temp_error(e);
            kill();
          }
          #endif
       }
    }
  
#if defined(BED_MAXTEMP) && (HEATER_BED_PIN > -1)
    if(current_raw_bed >= bed_maxttemp) {
       target_raw_bed = 0;
       bed_max_temp_error();
       kill();
    }
#endif
  }
}
