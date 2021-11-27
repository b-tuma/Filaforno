#include <thermistor.h>
#include <OneButton.h>
#include <U8glib.h>
#include <Encoder.h>

#define BUTTON_PIN 10
#define PROBE_PIN A0
#define ENCODER_A_PIN 2
#define ENCODER_B_PIN 3
#define RELAY_PIN 6

Encoder encoder(ENCODER_A_PIN, ENCODER_B_PIN);
THERMISTOR thermistor(PROBE_PIN, 100000, 4066, 99200);
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NONE);	// I2C / TWI
OneButton button(BUTTON_PIN, false); 


const char* filaments[] = {"PLA", "ABS", "PETG/CPE", "Nylon", "Dessicant", "PVA", "TPU/TPE", "ASA", "PP", "HIPS", "PC", "PEEK", "Custom"};
int temps[] = {50, 65, 65, 70, 65, 45, 55, 60, 55, 60, 70, 70};
double times[] = {300, 300, 300, 1200, 300, 1000, 400, 400, 600, 400, 600, 600};

bool zeroPosReset = false;

int pidOutput = 0;
int setpoint = 0;
int desiredTemperature = 0;
int currentTemperature = 0;
unsigned long desiredDuration = 0;
unsigned long currentDuration = 0;
bool timeKeeping = false;
unsigned long timeOffset = 0;

void u8g_prepare(void) {
  u8g.setFont(u8g_font_6x10);
  u8g.setRot180();
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}

void screen_startup()
{
  u8g.setScale2x2();
  u8g.drawStr( 5, 0, "FilaForno");
  u8g.undoScale();
  u8g.drawBox(15,29,94,12);
  u8g.setColorIndex(0);
  u8g.drawStr( 18, 30, "by Bruno Mansur");
  u8g.setColorIndex(1);
  u8g.drawFrame(47,49,29,12);
  u8g.drawStr( 50, 50, "v1.0");
}

long zeroPos = 0;
int currentFilament = 0;

void screen_selector(uint8_t a)
{
  long newPosition = encoder.read();
  if(zeroPosReset)
  {
    zeroPos = newPosition;
    zeroPosReset = false;
  }
  currentFilament = constrain((newPosition - zeroPos)/200, 0, 12);
  u8g.drawStr( 0, 5, (currentFilament == 0) ? "" : filaments[currentFilament - 1]);
  u8g.setScale2x2();
  u8g.drawStr( 0, 9, filaments[currentFilament]);
  u8g.undoScale();
  u8g.drawStr( 0, 40, (currentFilament + 1 > 12) ? "" : filaments[currentFilament + 1]);
  u8g.drawStr( 0, 50, (currentFilament + 2 > 12) ? "" : filaments[currentFilament + 2]);
  u8g.drawStr( 115, 56, "OK");
  u8g.drawDisc(108, 60, 3);
}

void screen_setting(uint8_t a)
{
  long newPosition = encoder.read();
  if(zeroPosReset)
  {
    zeroPos = newPosition;
    zeroPosReset = false;
  }
  
  desiredTemperature = temps[currentFilament];
  u8g.drawStr( 0, 0, "Temperatura:");
  u8g.setScale2x2();
  char s[5];
  sprintf(s, "%02doC", desiredTemperature);
  s[2] = 0xB0;
  u8g.drawStr( 1, 5, s);
  u8g.undoScale();
  char s2[9] = "Duracao:";
  s2[4] = 231;
  s2[5] = 227;
  u8g.drawStr( 0, 29, s2);
  u8g.drawBox(70,39,94,16);
  u8g.setScale2x2();
  int hours = times[currentFilament] / 60;
  int minutes = times[currentFilament] - (hours * 60);
  char time[6];
  sprintf(time, "%02dh%02d", hours, minutes);
  u8g.drawStr( 1, 19, time);
  u8g.setColorIndex(0);
  long currentPos = constrain((newPosition - zeroPos)/50, 0, 288);  // 5 min increments up to 24h
  int addHours = currentPos / 12;
  int addMinutes = (currentPos - (addHours * 12)) * 5;
  char addTime[6];
  sprintf(addTime, "%02dh%02d", addHours, addMinutes);
  u8g.drawStr( 35, 19, addTime);
  u8g.setColorIndex(1);
  u8g.undoScale();
  u8g.drawStr( 62, 42, "+");
  u8g.drawStr90( 128, 0, filaments[currentFilament]);
  u8g.drawStr( 115, 56, "OK");
  u8g.drawDisc(108, 60, 3);
  u8g.drawStr( 18, 56, "Voltar");
  u8g.drawDisc(3, 60, 3);
  u8g.drawDisc(12, 60, 3);

  desiredDuration = times[currentFilament] + (currentPos * 5);
}

void screen_custom(uint8_t a)
{
  u8g.drawStr( 0, 0, "Temperatura:");
  u8g.drawBox(0,11,62,16);
  u8g.setScale2x2();
  char s[5] = "00oC";
  s[3] = 0xB0;
  u8g.setColorIndex(0);
  u8g.drawStr( 1, 5, s);
  u8g.setColorIndex(1);
  u8g.undoScale();
  char s2[9] = "Duracao:";
  s2[4] = 231;
  s2[5] = 227;
  u8g.drawStr( 0, 29, s2);
  u8g.drawBox(0,39,62,16);
  u8g.setScale2x2();
  u8g.setColorIndex(0);
  u8g.drawStr( 1, 19, "00h00");
  u8g.setColorIndex(1);
  u8g.undoScale();
  u8g.drawStr90( 128, 0, "Custom");
  u8g.drawStr( 115, 56, "OK");
  u8g.drawDisc(108, 60, 3);
  u8g.drawStr( 18, 56, "Voltar");
  u8g.drawDisc(3, 60, 3);
  u8g.drawDisc(12, 60, 3);
}

void screen_heating(uint8_t a)
{
  u8g.drawStr( 0, 0, "Temperatura:");
  u8g.setScale2x2();
  char ds[5];
  sprintf(ds, "%02dC", currentTemperature);
  ds[2] = 0xB0;
  u8g.drawStr( 1, 5, ds);
  u8g.undoScale();
  u8g.drawStr( 0, 29, "Tempo Restante:");
  u8g.setScale2x2();
  long timeDiff = desiredDuration - currentDuration;
  int hours = timeDiff / 60;
  int minutes = timeDiff - (hours * 60);
  char time[6];
  sprintf(time, "%02dh%02d", hours, minutes);
  u8g.drawStr( 1, 19, time);
  u8g.undoScale();
  char s[5];
  sprintf(s, "%02doC", desiredTemperature);
  s[2] = 0xB0;
  u8g.drawStr( 97, 1, s);
  u8g.drawFrame(95, 0, 33, 11);
  u8g.drawStr( 82, 56, "Cancelar");
  u8g.drawDisc(76, 60, 3);
  u8g.setColorIndex(0);
  u8g.drawDisc(76, 60, 1);
  u8g.setColorIndex(1);
}

void screen_Finished()
{
  u8g.setScale2x2();
  u8g.drawStr( 11, 3, "Secagem");
  u8g.drawStr( 2, 15, "Finalizada");
  u8g.undoScale();
  u8g.drawFrame(0, 4, 126, 46);
  u8g.drawStr( 73, 56, "Reiniciar");
  u8g.drawDisc(67, 60, 3);
}

uint8_t draw_state = 0;


void changeScene(int i)
{
  u8g_prepare();
  switch(i)
  {
    case 0: screen_startup(); break;
    case 1: screen_selector(0); break;
    case 2: screen_setting(0); break;
    case 3: screen_custom(0); break;
    case 4: screen_custom(0); break;
    case 5: screen_heating(0); break;
    case 6: screen_Finished(); break;
  }
}

void singleClick()
{
  switch(draw_state)
  {
    case 1:
      draw_state = currentFilament != 12 ? 2 : 3;
      zeroPosReset = true;
      break;
    case 2:
      draw_state = 5;
      timeOffset = millis();
      timeKeeping = true;
      setpoint = desiredTemperature * 10;
      break;
    case 6:
      draw_state = 1;
      break;
  }
}

void doubleClick()
{
  switch(draw_state)
  {
    case 2: case 3:
      draw_state = 1;
      break;
  }
}

void longClick()
{
  switch(draw_state)
  {
    case 5:
      draw_state = 6;
      timeKeeping = false;
      digitalWrite(RELAY_PIN, LOW);
      break;
  }
}

void setup(void)
{
  pinMode(RELAY_PIN, OUTPUT);
  button.attachClick(singleClick);
  button.attachDoubleClick(doubleClick);
  button.attachLongPressStart(longClick);
  draw_state = 0;
}

void loop(void)
{
  if(timeKeeping)
  {
    currentDuration = (millis() - timeOffset) / 60000;
    currentTemperature = thermistor.read();
    if(currentTemperature >= setpoint - 15)
    {
      digitalWrite(RELAY_PIN, LOW);
    }
    else if (currentTemperature - 40 < setpoint)
    {
      digitalWrite(RELAY_PIN, HIGH);
    }
    if(desiredDuration <= currentDuration) longClick();
  }
  button.tick();
  // picture loop  
  u8g.firstPage();  
  do {
    changeScene(draw_state);
    button.tick();
  } 
  while(u8g.nextPage());

  if(draw_state == 0)
  {
    delay(3000);
    zeroPosReset = true;
    draw_state = 1;
  }
}
