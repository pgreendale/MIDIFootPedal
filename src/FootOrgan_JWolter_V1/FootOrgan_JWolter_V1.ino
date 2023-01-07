#include <MIDI.h>
#include <EEPROM.h>

int ledPin = 13; 
int modePin = 4;
/* input Switch Configurations */
int SW_KEY[]{
  14,15,16,17,18,19,8,9
};
int SW_LEN = sizeof(SW_KEY)/sizeof(SW_KEY[0]);

/*Pins for muxing inputs*/
int MUX_PORTS[]{
  2,3
};

/*
  Key state variable, 0 = no key pressed , assuming n_mux 
  muxed switch groups with 8 bit port width and 
*/
byte KEY_STATE[sizeof(MUX_PORTS)/sizeof(MUX_PORTS[0])]{0};

struct Config{
  byte velocity; 
  byte note_offset; 
  byte channel;
  bool midi_on;
  byte debug;
};
//configuration struct 
Config conf;

//set true if generic debugging is wanted 
bool debug_mode = false ;

/*mode number definitions MODE_ALIAS Key Number*/
#define MIDI_ON_OFF 1 
#define OCT_UP 4
#define OCT_DOWN 2
#define CHROM_UP 5
#define CHROM_DOWN 3
#define CHAN_UP 8
#define CHAN_DOWN 6
#define VELOCITY 7 
#define RESET_DEFAULT 12 
#define PANIC 13
//timeout cycle in menue, maximum in 100ms steps 
#define TIMEOUT_MAX 100 

MIDI_CREATE_DEFAULT_INSTANCE();

/*
  reads keyboard and returns count of first key found (usually lowest)
*/
void debug(void);
void read_keys(int mux[], byte state[]);


int read_key_single(void){
  read_keys(MUX_PORTS, KEY_STATE); //update keys
  int temp = 0;
  for(int group_idx = 0; group_idx < sizeof(KEY_STATE)/sizeof(KEY_STATE[0]) ; group_idx++ ){ 
    byte mask = 0x01; 
    for(int key_idx = 1; key_idx <= 8 ; key_idx++){
      if((mask & KEY_STATE[group_idx]) != 0) {
          return(group_idx * 8 + key_idx);
        }
        mask = mask << 1 ; 
      }
    }
  return 0;
}
/* Changes configuration by pressing a key according to its second function*/
void modeswitch(){
  int choice  = 0;
  if (debug_mode){
        debug();
  }
  while(digitalRead(modePin)==LOW){
    int timeout = 0; 
    while( (choice==0) && (timeout < TIMEOUT_MAX) ){
      choice = read_key_single();
      if(debug_mode){
        Serial.print("Mode:");Serial.println(choice); 
      }
      digitalWrite(ledPin, HIGH);
      delay(50);
      digitalWrite(ledPin, LOW); 
      delay(50);
      timeout++;
    }
    switch(choice){
      case MIDI_ON_OFF :
        conf.midi_on = ~conf.midi_on; 
        led_blink(ledPin, 1); 
      break;
    
      case OCT_UP:
        if (conf.note_offset < 127 - 12){ 
          conf.note_offset = conf.note_offset + 12; 
        }
        led_blink(ledPin, 1);
        break; 
      case OCT_DOWN: 
        if (conf.note_offset > 12){ 
          conf.note_offset = conf.note_offset - 12; 
        }
        led_blink(ledPin, 1);
        break; 
      case CHROM_UP:
        if (conf.note_offset < 127 ){ 
          conf.note_offset = conf.note_offset + 1; 
        }
        led_blink(ledPin, 1);
        break; 
      case CHROM_DOWN: 
        if (conf.note_offset > 1){ 
          conf.note_offset = conf.note_offset -1 ; 
        }
        led_blink(ledPin, 1);
        break; 
      
      case CHAN_UP: 
        if (conf.channel< 16){
          conf.channel = conf.channel + 1 ;
        }
        led_blink(ledPin, (int)conf.channel); 
        break;
      case CHAN_DOWN:
        if (conf.channel > 1){
          conf.channel = conf.channel -1; 
        }
        led_blink(ledPin, (int)conf.channel); 
        break; 
      
      case VELOCITY:
        conf.velocity = (conf.velocity + 32 ) % 127 ; 
        led_blink(ledPin, (int)conf.velocity / 32); 
        break; 
      case RESET_DEFAULT:
        conf.velocity    = 127;
        conf.note_offset = 60; //middle C as default  
        conf.channel     = 1;
        conf.midi_on     = true;
        conf.debug       = false;
        break;
      case PANIC:
        if(~debug_mode){
          MIDI.sendControlChange(120, 0 , conf.channel);
        }
        break; 
      
      default: 
        break; 
  }
  EEPROM.put(0, conf); 
  
  while(choice != 0){
    //Wait until pressed key is released on keyboard
    digitalWrite(ledPin, HIGH);
    choice = read_key_single(); 
    if(debug_mode){
      Serial.print("Wait for Key:");Serial.println(choice);
    }
    
    
    delay(100); 
  }
  if(debug_mode){
     Serial.println("Mode Exit");
  }
  //wait for mode key to be released
  ;; 
  }
  digitalWrite(ledPin, LOW);
}
void debug(){
  Serial.print("Config.velocity\t\t\t");Serial.println(conf.velocity); 
  Serial.print("Config.note_offset\t\t");Serial.println(conf.note_offset);
  Serial.print("Config.channel\t\t\t");Serial.println(conf.channel);
  Serial.print("Debug midi on \t\t\t");Serial.println(conf.midi_on);
  Serial.print("Debug (eeprom)\t\t\t");Serial.println(conf.debug);  
  Serial.print("\n\n");
}
/*Does nblink short blinks on pin and restores previos port state*/
void led_blink(int pin, int nblink){
  int tmp = digitalRead(pin); 
  for(int i =  1; i <= nblink; i++){
    digitalWrite(pin, LOW); 
    delay(300); 
    digitalWrite(pin, HIGH); 
    delay(200); 
  }
  digitalWrite(pin, tmp);
}
/* configures all pin numbers listed in array as inputs with pullup */
void set_port_input(int arr[]){
  int * ptr = arr;
  while( * ptr != NULL){
    pinMode(*ptr, INPUT_PULLUP);
    ptr++;
  }
}

/* 
  reads input state of pins listed in input array 
  returns byte with state, this limits output to 8 pins
 */
byte read_port(int arr[]){
  int * ptr = arr;
  byte idx = 0;
  byte port_state = 0x00;
  while(*ptr != NULL){
    port_state |= (digitalRead(*ptr) << idx);
    ptr++;
    idx++;
   }
  return ~port_state;  
}

/*
  for each mux_pin in mux[] array, 
  key states will be read and written to referred state array
  mux and state array has to have the same length! 
  low-active because key inputs have pullups to vcc activated
*/ 
void read_keys(int mux[], byte state[]){
  int* mux_ptr = mux;
  byte* state_ptr = state; 
  while (*mux_ptr != NULL){
    pinMode(*mux_ptr, OUTPUT); 
    digitalWrite(*mux_ptr, LOW);
    *state_ptr = read_port(SW_KEY);
    digitalWrite(*mux_ptr, HIGH); 
    pinMode(*mux_ptr, INPUT_PULLUP); //
    mux_ptr++;
    state_ptr++;
  }
}

void setup() { 
  conf = EEPROM.get(0,conf);
  pinMode(ledPin, OUTPUT);
  pinMode(modePin, INPUT_PULLUP);
  delay(100);
  if(digitalRead(modePin)==LOW ){
    Serial.begin(9600);  
    Serial.print("***Debug mode***\n\n");
    debug_mode = true ;
  }else
  { 
    MIDI.begin(MIDI_CHANNEL_OMNI);
  }
   
  pinMode(ledPin, OUTPUT);
  set_port_input(SW_KEY); 
}

void loop() {
  byte state_old [sizeof(KEY_STATE)/sizeof(KEY_STATE[0])] = {0}; 
  byte mask = 0;
  while(1){
    if (digitalRead(modePin)==HIGH){
      /*Midi Loop happens here*/
        read_keys(MUX_PORTS,KEY_STATE);
        for(int i=0 ; i< sizeof(KEY_STATE)/sizeof(KEY_STATE[0]); i++){
          mask = 0x01; 
          byte kdiff_on  =  KEY_STATE[i] & ~state_old[i] ; // only keys which are pressed and not pressed before
          byte kdiff_off = ~KEY_STATE[i] &  state_old[i] ; // only keys which are old but not pressed anymore
          for(int key_idx = 0; key_idx < 8; key_idx++){
            if((mask & kdiff_on) != 0){
              if(debug_mode){
                Serial.print("NoteOn, ");Serial.print(conf.note_offset + 8*i  + key_idx);
                Serial.print(" Velocity:");Serial.print(conf.velocity);Serial.print(" Chan");Serial.println(conf.channel);
              }else{
                MIDI.sendNoteOn( conf.note_offset + 8*i + key_idx , conf.velocity, conf.channel);
              }
            }
            if((mask & kdiff_off) != 0){
              if(debug_mode){
                Serial.print("NoteOff, ");Serial.print(conf.note_offset + 8*i + key_idx);
                Serial.print(" Velocity:");Serial.print(0);Serial.print(" Chan");Serial.println(conf.channel);                
              }else{
                MIDI.sendNoteOff( conf.note_offset + 8*i + key_idx , 0, conf.channel);
              }
            }
              mask = mask << 1 ; //rotate mask for one_hot encoding
            }
          }
        memcpy(state_old, KEY_STATE, sizeof(KEY_STATE));   //copy actual state to old state
        delay(1); //delay 1ms to avoid polling 
    }else{
      modeswitch();
    }
    //add Delay in Debug Mode
    if(debug_mode){
      for(int i=0; i< sizeof(KEY_STATE)/sizeof(KEY_STATE[0]); i++){
        Serial.print("State");Serial.print(i);Serial.print(':');Serial.println(KEY_STATE[i]);
      }
      delay(500);
      digitalWrite(ledPin,1^digitalRead(ledPin));
    }
    delay(1);
    digitalWrite(ledPin,1^digitalRead(ledPin));
  } // end main loop 

}
