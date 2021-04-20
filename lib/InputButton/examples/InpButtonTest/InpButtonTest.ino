#include "InputButton.h"


uint8_t led = 0;
uint8_t led1 = 0;
uint8_t led2 = 0;
uint8_t led_all = 0;


InputButton ib(26, true);
InputButton ib1(27, true, true);
InputButton ib2(35, false, true);

void setup()
{
	Serial.begin(115200);
	pinMode(0, OUTPUT);
	pinMode(2, OUTPUT);
	pinMode(4, OUTPUT);
	digitalWrite(0, LOW);
	digitalWrite(2, LOW);
	digitalWrite(4, LOW);
	delay(400);
	Serial.println("Hallo!!");
	Serial.printf("%.8x\n", ib2.getButtonMask());
	Serial.printf("%.8x\n", ib2.getAllBtnMask());
	Serial.printf("%.8x\n", ib2.getActiveHighMask());
	
}

void loop()
{

	if ( ib.longPress() )	// KeyPressLong muss vor KeyPressShort abgefragt werden!
	{
		digitalWrite(2, led2 ^= 1);
		Serial.printf("ib longPress (2) = %d\n", led2);
	}
	if ( ib.shortPress() )	// KeyPressLong muss vor KeyPressShort abgefragt werden!
	{
		digitalWrite(4, led1 ^= 1);
		Serial.printf("ib shortPress (4) = %d\n", led1);
	}
	
	if ( ib2.pressAndRepeat() )	// KeyPressLong muss vor KeyPressShort abgefragt werden!
	{
		int hl = (led_all ^= 1);

		digitalWrite(0, hl);
		digitalWrite(2, hl);
		digitalWrite(4, hl);
		Serial.printf("ib2 repeat shortPress (024) = %d\n", led_all);
	}

	if ( ib1.pressAndRepeat() )	// KeyPressLong muss vor KeyPressShort abgefragt werden!
	{
		digitalWrite(0, led ^= 1);
		Serial.printf("ib1 pressAndRepeat (0) = %d\n", led);
	}
}
