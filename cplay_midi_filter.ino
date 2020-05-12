#include <Adafruit_CircuitPlayground.h>
#include <MIDI.h>

// #define USE_RUNNING_STATUS

// 88 keys release ~ 84ms
// 88 keys release ~ 57ms with RunningStatus

//#define DEBUG
#define LIGHT_PIXELS

#define SUSTAIN_LED CPLAY_REDLED

#define NUM_PIXELS  10
#define HUE_FACTOR  21 // 256 / 12

#define PEDAL_DOWN_MESSAGE 67
#define CHANNEL_POTI_PIN 9
#define CHANNEL_SELECT_ON_BRIGHTNESS 30

int dest_channel = 15;
bool hold_mode = false;
bool note_off[127];
bool sustain;

bool right_button_state = false;

#ifdef LIGHT_PIXELS
uint8_t pixel_hue[NUM_PIXELS];
uint8_t pixel_notes[NUM_PIXELS];
bool pixel_switch;
bool update_pixels;
#endif

#ifdef DEBUG
#define DEBUG_PRINT(x)    Serial.print(x)
#define DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#ifdef USE_RUNNING_STATUS
struct MySettings : public midi::DefaultSettings
{
  static const bool UseRunningStatus = true;
};
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial1, MIDI, MySettings);
#else
MIDI_CREATE_DEFAULT_INSTANCE();
#endif
void setup()
{
  CircuitPlayground.strip = Adafruit_CPlay_NeoPixel();
  CircuitPlayground.strip.updateType(NEO_GRB + NEO_KHZ800);
  CircuitPlayground.strip.updateLength(NUM_PIXELS);
  CircuitPlayground.strip.setPin(CPLAY_NEOPIXELPIN);

  CircuitPlayground.strip.setBrightness(255);
  CircuitPlayground.strip.begin();
  CircuitPlayground.strip.show();

  pinMode(SUSTAIN_LED, OUTPUT);
  pinMode(0, INPUT);
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  pinMode(CPLAY_SLIDESWITCHPIN, INPUT);
  pinMode(CHANNEL_POTI_PIN, INPUT);
  DEBUG_PRINTLN("setup()");

  MIDI.setHandleControlChange(handleControlChangeCCMode);
  MIDI.setHandleNoteOn(handleNoteOnCCMode);
  MIDI.setHandleNoteOff(handleNoteOffCCMode);

  MIDI.setHandleActiveSensing(handleActiveSensing);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
}

void handleClock()
{
  DEBUG_PRINTLN(__FUNCTION__);
}

void handleStart()
{
  DEBUG_PRINTLN(__FUNCTION__);
}

void handleActiveSensing()
{
  DEBUG_PRINTLN(__FUNCTION__);
}

void handleNoteOnCCMode(byte channel, byte pitch, byte velocity)
{
  MIDI.sendNoteOn(pitch, velocity, dest_channel + 1);
}

void handleNoteOffCCMode(byte channel, byte pitch, byte velocity)
{
  MIDI.sendNoteOff(pitch, velocity, dest_channel + 1);
}

void handleControlChangeCCMode(byte channel, byte control, byte value)
{
  if (control == PEDAL_DOWN_MESSAGE) {
    digitalWrite(SUSTAIN_LED, value);
    MIDI.sendControlChange(64, value, dest_channel + 1);
  } else {
    MIDI.sendControlChange(control, value, dest_channel + 1);
  }
}

void handleNoteOnHoldMode(byte channel, byte pitch, byte velocity)
{
#ifdef LIGHT_PIXELS
  if (pixel_switch && !note_off[pitch]) {
    uint8_t pixel = pitch % NUM_PIXELS;
    pixel_hue[pixel] += pitch * HUE_FACTOR;
    pixel_notes[pixel]++;
    CircuitPlayground.strip.setPixelColor(pixel, CircuitPlayground.colorWheel(pixel_hue[pixel]));
    update_pixels = true;
  }
#endif

  if(!note_off[pitch])
  {
    MIDI.sendNoteOn(pitch, velocity, dest_channel + 1);
  }

  note_off[pitch] = false;

  DEBUG_PRINT(millis());
  DEBUG_PRINT("\tnoteOn (");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(pitch);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(velocity);
  DEBUG_PRINTLN(")");
}

void handleControlChangeHoldMode(byte channel, byte control, byte value)
{
  DEBUG_PRINT("handleControlChangeHoldMode(");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(control);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(value);
  DEBUG_PRINTLN(")");
  if (control == PEDAL_DOWN_MESSAGE) {
    sustain = value;
    digitalWrite(SUSTAIN_LED, value);

    if (!value) {
#ifdef DEBUG
      unsigned long start_millis = millis();
#endif
      DEBUG_PRINT(start_millis);
      DEBUG_PRINT("\tsendNoteOff [ ");
      for (byte i = 0; i < sizeof(note_off); i++) {
        if (note_off[i]) {
          DEBUG_PRINT(i);
          DEBUG_PRINT(" ");
          MIDI.sendNoteOff(i, 0, dest_channel + 1);
          note_off[i] = false;
#ifdef LIGHT_PIXELS
          if (pixel_switch) {
            uint8_t pixel = i % NUM_PIXELS;
            pixel_hue[pixel] -= i * HUE_FACTOR;
            pixel_notes[pixel]--;
            if (!pixel_notes[pixel]) {
              CircuitPlayground.strip.setPixelColor(pixel, 0x0);
            } else {
              CircuitPlayground.strip.setPixelColor(pixel, CircuitPlayground.colorWheel(pixel_hue[pixel]));
            }
          }
#endif
        }
      }
      DEBUG_PRINTLN(" ]");
#ifdef LIGHT_PIXELS
      if (pixel_switch) {
        update_pixels = true;
      }
#endif
#ifdef DEBUG
      Serial1.flush();
      unsigned long delayed = millis() - start_millis;
      DEBUG_PRINT("delayed by ");
      DEBUG_PRINT(delayed);
      DEBUG_PRINTLN("ms");
#endif
    }
  }
}

void handleNoteOffHoldMode(byte channel, byte pitch, byte velocity)
{
  DEBUG_PRINT(millis());
  DEBUG_PRINT("\tnoteOff(");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(pitch);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(velocity);
  DEBUG_PRINTLN(")");

  if (sustain) {
    note_off[pitch] = true;
  }
  else
  {
    MIDI.sendNoteOff(pitch, velocity, dest_channel + 1);

#ifdef LIGHT_PIXELS
    if (pixel_switch) {
      uint8_t pixel = pitch % NUM_PIXELS;
      pixel_hue[pixel] -= pitch * HUE_FACTOR;
      pixel_notes[pixel]--;
      if (!pixel_notes[pixel]) {
        CircuitPlayground.strip.setPixelColor(pixel, 0x0);
      } else {
        CircuitPlayground.strip.setPixelColor(pixel, CircuitPlayground.colorWheel(pixel_hue[pixel]));
      }
      update_pixels = true;
    }
#endif
  }
}

void updateDestChannel()
{
  int channel_poti_val = analogRead(CHANNEL_POTI_PIN);
  int channel_val = channel_poti_val / 68;

  if(dest_channel != channel_val)
  {
    dest_channel = channel_val;
    displayChannel();
  }
}

void displayChannel()
{
    CircuitPlayground.strip.clear();

    if(dest_channel < 10)
    {
      for(int i = 0; i < dest_channel + 1; i++)
      {
        CircuitPlayground.strip.setPixelColor(i, 0, CHANNEL_SELECT_ON_BRIGHTNESS, 0);
      }
    }
    if(dest_channel == 10)
    {
      for(int i = 0; i < 10; i++)
      {
        CircuitPlayground.strip.setPixelColor(i, CHANNEL_SELECT_ON_BRIGHTNESS, 0, 0);
      }
    }
    else if(dest_channel < 15)
    {
      for(int i = 0; i < dest_channel - 9; i++)
      {
        CircuitPlayground.strip.setPixelColor(i, 0, 0, CHANNEL_SELECT_ON_BRIGHTNESS);
      }
    }

    CircuitPlayground.strip.show();
}

void toggleSustainMode()
{
  CircuitPlayground.strip.clear();

  hold_mode = !hold_mode;

  for(int i = 0; i < 10; i++)
  {
    if(i % 2 == 0)
    {
      CircuitPlayground.strip.setPixelColor(i, 255, 0, 255);
    }
    else
    {
      if(hold_mode)
      {
        CircuitPlayground.strip.setPixelColor(i, 0, 0, 255);
      }
      else
      {
        CircuitPlayground.strip.setPixelColor(i, 255, 0, 0);
      }
    }
  }

  CircuitPlayground.strip.show();

  if(hold_mode)
  {
    MIDI.setHandleControlChange(handleControlChangeHoldMode);
    MIDI.setHandleNoteOn(handleNoteOnHoldMode);
    MIDI.setHandleNoteOff(handleNoteOffHoldMode);
  }
  else
  {
    MIDI.setHandleControlChange(handleControlChangeCCMode);
    MIDI.setHandleNoteOn(handleNoteOnCCMode);
    MIDI.setHandleNoteOff(handleNoteOffCCMode);
  }

  delay(250);
  displayChannel();
}

void loop()
{
  MIDI.read();
#ifdef LIGHT_PIXELS
  pixel_switch = digitalRead(CPLAY_SLIDESWITCHPIN);
  if (update_pixels) {
    CircuitPlayground.strip.show();
    update_pixels = false;
  }
#endif

  updateDestChannel();

  bool right_button = CircuitPlayground.rightButton();
  if(right_button != right_button_state)
  {
    if(right_button)
    {
      toggleSustainMode();
    }

    right_button_state = right_button;
  }
}
