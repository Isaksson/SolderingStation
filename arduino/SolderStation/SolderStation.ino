//*******************************//
// Soldering Station
// Matthias Wagner
// www.k-pank.de/so
// Get.A.Soldering.Station@gmail.com
//*******************************//

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>

#include "iron.h"
#include "stationLOGO.h"

const String VERSION = "1.5";
#define INTRO

const uint8_t TFT_CS = 10;
// The reset pin should be connected to +5V through a pull-up of 1K, but real-world testing
// has shown that it is better not to have it connected at all. So we specify a 0 for this pin.
const uint8_t TFT_RST = 0;                    
const uint8_t TFT_DC = 9;

const uint8_t STANDBYin = A4;
const uint8_t POTI = A5;
const uint8_t TEMPin = A7;
const uint8_t PWMpin = 3;
const uint8_t BLpin = 5;

const uint8_t CNTRL_GAIN = 10;

const uint8_t DELAY_MAIN_LOOP = 10;
const uint8_t DELAY_MEASURE = 50;
const double ADC_TO_TEMP_GAIN = 0.53; //Mit original Weller Station verglichen
const double ADC_TO_TEMP_OFFSET = 25.0;
const int STANDBY_TEMP = 175;

const int OVER_SHOT = 2;
const int MAX_PWM_LOW = 180;
const int MAX_PWM_HI = 210;
const int MAX_POTI = 400;		//400Grad C

const int PWM_DIV = 1024;		//default: 64   31250/64 = 2ms

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const uint16_t GREY = tft.Adafruit_ST7735::Color565(190,190,190);

int pwm = 0; //pwm Out Val 0.. 255
int soll_temp = 300;
boolean standby_act = false;

void setup(void) {
	
	pinMode(BLpin, OUTPUT);
	digitalWrite(BLpin, LOW);
	
	pinMode(STANDBYin, INPUT_PULLUP);
	
	pinMode(PWMpin, OUTPUT);
	digitalWrite(PWMpin, LOW);
	setPwmFrequency(PWMpin, PWM_DIV);
	digitalWrite(PWMpin, LOW);
	
	tft.initR(INITR_BLACKTAB);
	SPI.setClockDivider(SPI_CLOCK_DIV4);  // 4MHz
	
	
	tft.setRotation(0);	// 0 - Portrait, 1 - Lanscape
	tft.fillScreen(ST7735_BLACK);
	tft.setTextWrap(true);
		
	//Print station Logo
	tft.drawBitmap(2,1,stationLOGO1,124,47,GREY);
	
	tft.drawBitmap(3,3,stationLOGO1,124,47,ST7735_YELLOW);		
	tft.drawBitmap(3,3,stationLOGO2,124,47,Color565(254,147,52));	
	tft.drawBitmap(3,3,stationLOGO3,124,47,Color565(255,78,0));
	
	//Backlight on
	digitalWrite(BLpin, HIGH);
	
	tft.setTextSize(1);
	tft.setTextColor(GREY);
	tft.setCursor(103,0);
	tft.print("v");
	tft.print(VERSION);
	
	tft.fillRect(0,47,128,125,ST7735_BLACK);
	tft.setTextColor(ST7735_WHITE);

	tft.setTextSize(1);
	tft.setCursor(1,84);
	tft.print("Tmp");
	
	tft.setTextSize(1);
	tft.setCursor(116,48);
	tft.print("o");
	
	tft.setTextSize(1);
	tft.setCursor(1,129);
	tft.print("Set");
	
	tft.setTextSize(1);
	tft.setCursor(116,93);
	tft.print("o");
	
	tft.setTextSize(1);
	tft.setCursor(1,151);
	tft.print("Pwm");
	tft.setCursor(116,146);
	tft.print("%");
}

void loop() {
	
	int actual_temperature = getTemperature();
	soll_temp = map(analogRead(POTI), 0, 1024, 0, MAX_POTI);
	
	
	//TODO: Put in Funktion
	tft.setCursor(2,55);
	if (digitalRead(STANDBYin) == true)
		tft.setTextColor(ST7735_BLACK);
	else
		tft.setTextColor(ST7735_WHITE);
	tft.print("SB");
	
	int soll_temp_tmp = soll_temp;
	
	if (digitalRead(STANDBYin) == false)
		standby_act = true;
	else
		standby_act = false;
	
	if (standby_act && (soll_temp >= STANDBY_TEMP ))
		soll_temp_tmp = STANDBY_TEMP;
	
	
	
	int diff = (soll_temp_tmp + OVER_SHOT)- actual_temperature;
	pwm = diff*CNTRL_GAIN;
	
	int MAX_PWM;

	//Set max heating Power 
	MAX_PWM = actual_temperature <= STANDBY_TEMP ? MAX_PWM_LOW : MAX_PWM_HI;
	
	//8 Bit Range
	pwm = pwm > MAX_PWM ? pwm = MAX_PWM : pwm < 0 ? pwm = 0 : pwm;
	
	
	
	//NOTfall sicherheit / Spitze nicht eingesteckt
	if (actual_temperature > 550){
		pwm = 0;
		actual_temperature = 0;
	}
	
	
	analogWrite(PWMpin, pwm);
	//digitalWrite(PWMpin, LOW);
	
	writeHEATING(soll_temp, actual_temperature, pwm);
	
	delay(DELAY_MAIN_LOOP);		//wait for some time
}


int getTemperature()
{
  analogWrite(PWMpin, 0);		//switch off heater
  delay(DELAY_MEASURE);			//wait for some time (to get low pass filter in steady state)
  int adcValue = analogRead(TEMPin); // read the input on analog pin 7:
  Serial.print("ADC Value ");
  Serial.print(adcValue);
  analogWrite(PWMpin, pwm);	//switch heater back to last value
  return round(((float) adcValue)*ADC_TO_TEMP_GAIN+ADC_TO_TEMP_OFFSET); //apply linear conversion to actual temperature
}





void writeHEATING(int tempSOLL, int tempVAL, int pwmVAL){
	static int d_tempSOLL = 2;		//Tiefpass für Anzeige (Poti zittern)

	static int tempSOLL_OLD = 	10;
	static int tempVAL_OLD	= 	10;
	static int pwmVAL_OLD	= 	10;
	//TFT Anzeige
	
	pwmVAL = map(pwmVAL, 0, 254, 0, 100);
	
	tft.setTextSize(5);
	if (tempVAL_OLD != tempVAL){
		tft.setCursor(30,57);
		tft.setTextColor(ST7735_BLACK);
		//tft.print(tempSOLL_OLD);
		//erste Stelle unterschiedlich
		if ((tempVAL_OLD/100) != (tempVAL/100)){
			tft.print(tempVAL_OLD/100);
		}
		else
			tft.print(" ");
		
		if ( ((tempVAL_OLD/10)%10) != ((tempVAL/10)%10) )
			tft.print((tempVAL_OLD/10)%10 );
		else
			tft.print(" ");
		
		if ( (tempVAL_OLD%10) != (tempVAL%10) )
			tft.print(tempVAL_OLD%10 );
		
		tft.setCursor(30,57);
		tft.setTextColor(ST7735_WHITE);
		
		if (tempVAL < 100)
			tft.print(" ");
		if (tempVAL <10)
			tft.print(" ");
		
		int tempDIV = round(float(tempSOLL - tempVAL)*8.5);
		tempDIV = tempDIV > 254 ? tempDIV = 254 : tempDIV < 0 ? tempDIV = 0 : tempDIV;
		tft.setTextColor(Color565(tempDIV, 255-tempDIV, 0));
		if (standby_act)
			tft.setTextColor(ST7735_CYAN);
		tft.print(tempVAL);
		
		tempVAL_OLD = tempVAL;
	}
	
	if ((tempSOLL_OLD+d_tempSOLL < tempSOLL) || (tempSOLL_OLD-d_tempSOLL > tempSOLL)){
		tft.setCursor(30,102);
		tft.setTextColor(ST7735_BLACK);

		if ((tempSOLL_OLD/100) != (tempSOLL/100)){
			tft.print(tempSOLL_OLD/100);
		}
		else
			tft.print(" ");
		
		if ( ((tempSOLL_OLD/10)%10) != ((tempSOLL/10)%10) )
			tft.print((tempSOLL_OLD/10)%10 );
		else
			tft.print(" ");
		
		if ( (tempSOLL_OLD%10) != (tempSOLL%10) )
			tft.print(tempSOLL_OLD%10 );
		
		//Neuen Wert in Weiß schreiben
		tft.setCursor(30,102);
		tft.setTextColor(ST7735_WHITE);
		if (tempSOLL < 100)
			tft.print(" ");
		if (tempSOLL <10)
			tft.print(" ");
		
		tft.print(tempSOLL);
		tempSOLL_OLD = tempSOLL;
		
	}
	
	
	tft.setTextSize(2);
	if (pwmVAL_OLD != pwmVAL){
		tft.setCursor(80,144);
		tft.setTextColor(ST7735_BLACK);

		if ((pwmVAL_OLD/100) != (pwmVAL/100)){
			tft.print(pwmVAL_OLD/100);
		}
		else
			tft.print(" ");
		
		if ( ((pwmVAL_OLD/10)%10) != ((pwmVAL/10)%10) )
			tft.print((pwmVAL_OLD/10)%10 );
		else
			tft.print(" ");
		
		if ( (pwmVAL_OLD%10) != (pwmVAL%10) )
			tft.print(pwmVAL_OLD%10 );
		
		tft.setCursor(80,144);
		tft.setTextColor(ST7735_WHITE);
		if (pwmVAL < 100)
			tft.print(" ");
		if (pwmVAL <10)
			tft.print(" ");
		
		tft.print(pwmVAL);
		pwmVAL_OLD = pwmVAL;		
	}
	
}




uint16_t Color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}



void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
