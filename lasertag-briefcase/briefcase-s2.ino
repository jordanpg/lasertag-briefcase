//We always have to include the library
#include "LedControl.h"
#include "Wtv020sd16p.h"

/*
 Now we need a LedControl to work with.
 ***** These pin numbers will probably not work with your hardware *****
 pin 12 is connected to the DataIn 
 pin 11 is connected to the CLK 
 pin 10 is connected to LOAD 
 We have only a single MAX72XX.
 */
LedControl lc=LedControl(12,11,10,1);

/* we always wait a bit between updates of the display */
unsigned long delaytime=10;

unsigned long countdownStart = 0;
unsigned long armLastProg = 0;
int progress = 0;
bool armed = false;
bool arming = false;
bool held = false;
bool finished = false;

const unsigned long armProgTime = 250;
const unsigned long countdown = 60000 * 3;

unsigned long deltaTime = 0;
unsigned long lastTime = 0;

const int buttonPin = 8;
const int LEDPin = 9;
const int resetPin = 2;
const int clockPin = 3;
const int dataPin = 4;
const int busyPin = 5;
//const int soundPin = 6;

int LEDValue = 0;
int LEDMod = 1;
unsigned long lastFlash = 0;
unsigned long flashOnTime = 250;
unsigned long flashOffTime = 500;
unsigned long flashFinishedOn = 100;
unsigned long flashFinishedOff = 100;

unsigned int mirrorState = 0;
unsigned long lastMirrorUpdate = 0;

unsigned long remaining = 0;

/* AUDIO FILE INDICES
 * 0: Armed
 * 1: Disarmed
 * 2: Warning
 * 3: Boom
*/
Wtv020sd16p audio(resetPin, clockPin, dataPin, busyPin);
const int sfxArmed = 0;
const int sfxDisarmed = 1;
const int sfxWarning = 2;
const int sfxBoom = 3;

const int warningTime = 30000;
bool warned = false;

const int LEDMaxFinishedValue = 255;
const int LEDMaxValue = 127;
const float LEDMaxMod = 15;

const float pi = 3.1415;
const float hpi = pi * 0.5;

const bool useSineAcceleration = false;

const unsigned int mirrorIntervalArm = B1111;
const unsigned int mirrorIntervalDisarm = 0xF000;
const unsigned int mirrorDefault = 0x8888;
const int mirrorUpdatePeriod = 50;

const bool armingFreezes = false;

const unsigned int finishFlashLength = 10000;

void displayDARC()
{
  lc.clearDisplay(0);
  lc.setChar(0, 4, 'c', false);
  lc.setRow(0, 3, 0x05);
  lc.setChar(0, 2, 'a', false);
  lc.setChar(0, 1, 'd', false);
  delay(3000);
  lc.clearDisplay(0);
}

void setup() {
  //set pin 9 PWM signal to ~30kHz
  TCCR1B = TCCR1B & B11111000 | B00000001;
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0,false);
  /* Set the brightness to a high values (15) 8 is medium */
  lc.setIntensity(0,15);
  /* and clear the display */
  lc.clearDisplay(0);

  pinMode(buttonPin, INPUT);
 // pinMode(soundPin, OUTPUT);

  audio.reset();

//  Serial.begin(9600);

// the infinite mirror LEDs
  lc.setRow(0, 0, 0);
  lc.setRow(0, 7, 0);

  displayDARC();
}

void showProgress(int prog)
{
  int sec1 = 0;
  int sec2 = 0;

  if(prog >= 8)
  {
    sec1 = 0xFF;
    
    if(prog >= 16)
      sec2 = 0xFF;
    else
      sec2 = 256 - (1 << 8 - (prog % 8));
  }
  else if(prog > 0)
    sec1 = (1 << prog) - 1;

  lc.setRow(0, 6, sec1);
  lc.setRow(0, 5, sec2);
}

void displayTime(unsigned long ms, char mode, bool colon=false)
{
  //ms *= timeStretch;
  int mils = ms % 1000;
  int secs = ms / 1000;
  int mins = secs / 60;

  switch(mode)
  {
    case 0: //Display minutes:seconds
      secs %= 60;
      lc.setDigit(0, 4, (byte)(secs % 10), false);
      lc.setDigit(0, 3, (byte)(secs / 10), false);
      lc.setDigit(0, 2, (byte)(mins % 10), false);
      lc.setDigit(0, 1, (byte)(mins / 10), false);
      break;
    case 1: //Display seconds:centiseconds
      int cents = (ms / 10) % 100;
      
      lc.setDigit(0, 4, (byte)(cents % 10), false);
      lc.setDigit(0, 3, (byte)(cents / 10), false);
      lc.setDigit(0, 2, (byte)(secs % 10), false);
      lc.setDigit(0, 1, (byte)(secs / 10), false);
      break;
  }

  if(colon)
  
     lc.setLed(0, 4,0, HIGH);

    //lc.setRow(0, 0, 0xFF);
  else
  
     lc.setLed(0, 4,0, LOW);

    //lc.setRow(0, 0, B0);
}

int updateProgress(unsigned long mils)
{
  bool button = digitalRead(buttonPin);

  if(held)
  {
    if(button == LOW) 
      held = false;
      
    return progress;
  }
  
  if(!arming)
  {
    if(button == LOW)
      return progress = constrain(progress + (armed ? 1 : -1), 0, 16);

    finished = false;
    arming = true;
    armLastProg = mils + armProgTime;
  }

  if(arming && button == LOW)
  {
    arming = false;
    armLastProg = 0;

    return progress;
  }
  else if(arming && (mils - armLastProg) >= armProgTime)
  {
    progress += (armed ? -1 : 1);

    if(armed && progress <= 0)
    {
      armed = false;
      arming = false;
      finished = false;
      armLastProg = mils;

      held = true;
      
      LEDValue = LEDMaxValue;
      LEDMod = -1;
      
      playSound(sfxDisarmed);
      for (int i=1;i<=4;i++) lc.setRow(0,i,0);
      
      return progress = 0;
    }
    else if(!armed && progress >= 16)
    {
      armed = true;
      arming = false;
      finished = false;
      armLastProg = mils;

      countdownStart = mils;
      
      playSound(sfxArmed);

      held = true;
      warned = false;

      return progress = 16;
    }
    
    armLastProg = mils;
  }

  return constrain(progress, 0, 16);
}

void updateLED(unsigned long mils)
{
  char state = (finished << 2 | armed << 1 | arming);
  int tempMod = 0;
  int timePassed = 0;
  
  switch(state)
  {
    case 1: LEDMod = (LEDMod > 0 ? 1 : -1);
    case 2:
      LEDValue = constrain(LEDValue + LEDMod, 0, LEDMaxValue);
      if(LEDValue == LEDMaxValue || LEDValue == 0)
        LEDMod *= -1;

      if(armed && useSineAcceleration)
        tempMod = (int)((LEDMod > 0 ? 1.0 : -1.0) * (sin(hpi * (float)((mils - countdownStart) / (float)countdown)) * LEDMaxMod));
      else if(armed)
        tempMod = (int)((LEDMod > 0 ? 1.0 : -1.0) * ((float)((mils - countdownStart) / (float)countdown) * LEDMaxMod));
        
      if(tempMod != 0)
        LEDMod = tempMod;
    break;

    case 4: //passthrough for finished flash
    case 3:
      if(LEDValue == 0 && mils - lastFlash >= (finished ? flashFinishedOff : flashOffTime))
      {
        LEDValue = (finished ? LEDMaxFinishedValue : LEDMaxValue);
        lastFlash = mils;
      }
      else if(LEDValue > 0 && mils - lastFlash >= (finished ? flashFinishedOn : flashOnTime))
      {
        LEDValue = 0;
        lastFlash = mils;
      } 

      break;

    default:
      if(LEDValue > 0)
        LEDValue = constrain(LEDValue - abs(LEDMod), 0, LEDMaxValue);
  }

  analogWrite(LEDPin, LEDValue);
}

void playSound(int index)
{
      audio.stopVoice();
      audio.asyncPlayVoice(index);

      //digitalWrite(soundPin, HIGH);
}

void loop() { 
  unsigned long mils = millis();
  //writeArduinoOn7Segment();
  //scrollDigits();
  //segmentTest();
  deltaTime = mils - lastTime;
  lastTime = mils;
  
  progress = updateProgress(mils);
  
  //displayTime(millis(), 0);
  showProgress(progress);

  //if(digitalRead(busyPin) == LOW)
  //  digitalWrite(soundPin, LOW);
  //digitalWrite(soundPin, digitalRead(busyPin));

  if(arming && mils - lastMirrorUpdate > mirrorUpdatePeriod)
  {
    if(mirrorState == 0 || mirrorState == 0xFFFF)
      mirrorState = mirrorDefault;

    mirrorState = (armed ? mirrorState >> 1 : mirrorState << 1);
    Serial.println((mirrorState & (armed ? mirrorIntervalDisarm : mirrorIntervalArm)));
    if((mirrorState & (armed ? mirrorIntervalDisarm : mirrorIntervalArm)) == 0x0000)
    {
      mirrorState += (armed ? 0x8000 : 1);
    }

    lastMirrorUpdate = mils;
  }

  if(armed)
  { 
    /*
    lc.setRow(0, 0, 0xFF);
    lc.setRow(0, 7, 0xFF);
    */
    if(!arming)
      mirrorState = 0xFFFF;

    //lc.setRow(0, 5, 0);
    if(arming && armingFreezes)
      countdownStart += deltaTime;
    else if(!arming)
      remaining = countdown - (mils - countdownStart);
    if(remaining == 0 || remaining > countdown)
    {
      remaining = 0;

      armed = false;
      finished = true;

      countdownStart = mils;
      
      playSound(sfxBoom);

      lc.clearDisplay(0);
      
      for (int i=1;i<=4;i++) lc.setRow(0,i,0);
      //timer out
    }
    else if(remaining <= warningTime && !warned)
    {
      warned = true;

      playSound(sfxWarning);
    }

    if(!finished)
      displayTime(remaining, 0, true);
  }
  else if(!armed)
  {
     lc.setLed(0, 1,0, HIGH);

// the infinite mirror LEDs  
    /*   
    lc.setRow(0, 0, 0);
    lc.setRow(0, 7, 0);
    */
    if(!arming)
      mirrorState = 0;

    /*
    lc.setRow(0, 5, 0x40);

    lc.setRow(0, 0, 0);
    lc.setRow(0, 1, 0x80);
    lc.setRow(0, 2, 0x80);
    lc.setRow(0, 3, 0x80);
    lc.setRow(0, 4, 0x80);
    */
  }

  if(finished && (mils - countdownStart) >= finishFlashLength)
  {
    finished = false;
  }

  /*
  if(progress >= 16)
  {
    progress = 16;
    armed = true;
  }
  else if(progress <= 0)
  {
    progress = 0;
    armed = false;
  }
  progress += (armed ? -1 : 1);
  */
  updateLED(mils);

  lc.setRow(0, 0, mirrorState >> 8);
  lc.setRow(0, 7, mirrorState % 0x100);

  delay(delaytime);
}
