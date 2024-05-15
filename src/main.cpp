//Spencer Butler, CS443, Combination Lock Assignment
#include <Arduino.h>

//This always looks at the last four characters entered
//If an input is invalid for any reason, including length or combo mismatch, the light blinks

#define nop() asm("nop \n")

//time between debouncing samples, in microseconds
#define SAMPLE_INTERVAL (1000)
//number of samples to collect when debouncing
#define SAMPLE_COUNT 8

//characters per combo
#define COMBO_LENGTH 4

//number of pulses to send to the servo when updating position
#define PULSES 50
//minimum width of a pulse, in microseconds
#define PULSE_MIN_WIDTH (600)
//maximum width of a pulse, in microseconds
#define PULSE_MAX_WIDTH (2400)
//I consistently found that, while a pulse of width 1.5ms would move to the center,
//pulses of width 1ms and 2ms were not moving to the expected extremes 


//returns the current status of the keypad
//returns 0 if no buttons are pressed
char currentPress();

//evaluates if all values are equal, returns that value
//returns -1 if they are not
int stableVal(char values[SAMPLE_COUNT]);

//waits for a button to be pressed, returns that button
char debouncedPress();

//the character label for each button on the keypad
const char labels[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};

//rows[i] is the value n so that PORTDn corresponds to row i 
const int rows[4] = {0, 7, 6, 4};
//cols[i] is the value n so that PORTDn corresponds to col i 
const int cols[4] = {5, 3, 2, 1};

enum State {
  OPEN,
  CLOSED
};

class OutputManager {
  public:
    OutputManager();
    //handles an input ending in * -- entering the combo to open the safe
    void starHandler(char input[COMBO_LENGTH]);
    //handles an input ending in # -- defining the combo and closing the safe
    void sharpHandler(char input[COMBO_LENGTH]);

  private:
    State state;
    char combo[COMBO_LENGTH];
    //updates the position of the servo
    void updateServo(double position);
    //opens the safe
    void open();
    //closes the safe
    void close();
    //blinks the led to indicate an error
    void error();
};


OutputManager *om;

void setup() {
  int i;

  //led as output
  DDRB = DDRB | (1 << 5);

  //servo signal as output
  DDRB = DDRB | (1 << 0);
  om = new OutputManager();

  //rows as inputs
  for(i = 0; i < 4; i++) {
    DDRD = DDRD & ~(1 << rows[i]);
  }

  //cols as inputs, with pullup resistor
  for(i = 0; i < 4; i++) {
    DDRD = DDRD & ~(1 << cols[i]);
    PORTD = PORTD | (1 << cols[i]);
  }

}

void loop() {
  int i;
  char buffer = 0;
  char input[COMBO_LENGTH];
  for(i = 0; i < COMBO_LENGTH; i++) {
    input[i] = 0;
  }

  while(buffer != '*' && buffer != '#') {
    for(i = COMBO_LENGTH - 1; i > 0; i--) {
      input[i] = input[i - 1];
    }
    input[0] = buffer;
    buffer = debouncedPress();
  }

  if(buffer == '*') {
    om->starHandler(input);
  }

  if(buffer == '#') {
    om->sharpHandler(input);
  }
}


//returns the current button pressed on the keypad
//returns 0 if no buttons are pressed
char currentPress() {
  int row, col;
  int reading = -1;

  for(row = 0; row < 4; row++) {
    PORTD = PORTD | (1 << rows[row]);
    DDRD = DDRD | (1 << rows[row]);
    PORTD = PORTD & ~(1 << rows[row]);
    nop();
    nop();

    for(col = 0; col < 4; col++) {
      if(!(PIND & (1 << cols[col]))) {
        reading = (row * 4) + col;
      }
    }

    PORTD = PORTD | (1 << rows[row]);
    DDRD = DDRD & ~(1 << rows[row]);

    if(reading != -1) {
      return labels[reading];
    }
  }

  return 0;
}

//evaluates if all values are equal, returns that value
//returns -1 if they are not
int stableVal(char values[SAMPLE_COUNT]) {
  int i;
  int ret = (int) values[0];
  for(i = 1; i < SAMPLE_COUNT; i++) {
    if(((int) values[i]) != ret) {
      return -1;
    }
  }
  return ret;
}

//waits for a button to be pressed, returns what button
char debouncedPress() {
  volatile unsigned long start;
  char history[SAMPLE_COUNT];
  int buffer;
  int i;

  //wait for stable-off
  history[0] = currentPress();
  while(stableVal(history) != 0) {
    start = micros();
    while(micros() < start + SAMPLE_INTERVAL);
    for(i = SAMPLE_COUNT - 1; i > 0; i--) {
      history[i] = history[i - 1];
    }
    history[0] = currentPress();
  }

  //wait for stable-pressed
  do {
    start = micros();
    while(micros() < start + SAMPLE_INTERVAL);
    for(i = SAMPLE_COUNT - 1; i > 0; i--) {
      history[i] = history[i - 1];
    }
    history[0] = currentPress();

    buffer = stableVal(history);
  } while(buffer == 0 || buffer == -1);

  return (char) buffer;
}

OutputManager::OutputManager() {
  open();
}

//handles an input ending in * -- entering the combo to open the safe
void OutputManager::starHandler(char input[COMBO_LENGTH]) {
  int i;
  if(state == OPEN) {
    error();
    return;
  }
  for(i = 0; i < COMBO_LENGTH; i++) {
    if(input[i] != combo[i]) {
      error();
      return;
    }
  }
  open();
}

//handles an input ending in # -- defining the combo and closing the safe
void OutputManager::sharpHandler(char input[COMBO_LENGTH]) {
  int i;
  if(state == CLOSED) {
    error();
    return;
  }
  if(input[COMBO_LENGTH - 1] == 0) {
    error();
    return;
  }
  for(i = 0; i < COMBO_LENGTH; i++) {
    combo[i] = input[i];
  }
  close();
}

//updates the position of the servo
void OutputManager::updateServo(double position) {
  int i;
  volatile unsigned long start, mid, end;
  for(i = 0; i < PULSES; i++) {
    PORTB = PORTB | (1 << 0);

    start = micros();
    mid = start + PULSE_MIN_WIDTH + position * (PULSE_MAX_WIDTH - PULSE_MIN_WIDTH);
    end = start + 1000 * 20;

    while(micros() < mid);
    PORTB = PORTB & ~(1 << 0);
    while(micros() < end); 
  }
}

//opens the safe
void OutputManager::open() {
  updateServo(1.0);
  //updateServo(1.40);
  PORTB = PORTB & ~(1 << 5);
  state = OPEN;
}

//closes the safe
void OutputManager::close() {
  updateServo(0.0);
  //updateServo(-0.40);
  PORTB = PORTB | (1 << 5);
  state = CLOSED;
}

//blinks the led to indicate an error
void OutputManager::error() {
  volatile unsigned long start;
  bool startState = PORTB & (1 << 5);

  PORTB = PORTB | (1 << 5);
  start = millis();
  while(millis() < start + 250);

  PORTB = PORTB & ~(1 << 5);
  start = millis();
  while(millis() < start + 250);

  PORTB = PORTB | (1 << 5);
  start = millis();
  while(millis() < start + 250);

  if(startState) {
    PORTB = PORTB | (1 << 5);
  } else {
    PORTB = PORTB & ~(1 << 5);
  }
}