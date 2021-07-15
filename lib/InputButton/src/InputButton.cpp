// 
// Quelle: Peter Dannegger
// adapted for ESP32 by anp59
//

#if defined(ARDUINO_ARCH_ESP32)

#include "InputButton.h"

mask_t DebounceButtons::all_keys_mask = 0;
mask_t DebounceButtons::activ_high_mask = 0;
mask_t DebounceButtons::repeat_mask = 0;
volatile mask_t DebounceButtons::key_press = 0;			// key press detect
volatile mask_t DebounceButtons::key_state = 0;			// debounced and inverted key state: bit = 1: key pressed
volatile mask_t DebounceButtons::key_rpt_state = 0;		// debounced and inverted key state: bit = 1: key long press and repeat

hw_timer_t * DebounceButtons::timer = NULL;
portMUX_TYPE DebounceButtons::timerMux = portMUX_INITIALIZER_UNLOCKED;

mask_t DebounceButtons::keyPressed()					// 1 = Taste gedrückt. Tasten werden nicht zur�ckgesetzt
{
	return key_press;
}
	
mask_t DebounceButtons::getKeyPress(mask_t key_mask)	// Taste auslesen und zurücksetzen
{
	portENTER_CRITICAL(&DebounceButtons::timerMux);
	key_mask &= key_press;       // read key(s)
	key_press ^= key_mask;       // clear key(s)
	portEXIT_CRITICAL(&DebounceButtons::timerMux);
	return key_mask;
}
	
mask_t DebounceButtons::getKeyRpt(mask_t key_mask)
{
	portENTER_CRITICAL(&DebounceButtons::timerMux);
	key_mask &= key_rpt_state;
	key_rpt_state ^= key_mask;
	portEXIT_CRITICAL(&DebounceButtons::timerMux);
	return key_mask;
}
	
mask_t DebounceButtons::getKeyPressShort(mask_t key_mask)
{
	mask_t i;
	portENTER_CRITICAL(&DebounceButtons::timerMux);
	i = getKeyPress( ~key_state & key_mask );
	portEXIT_CRITICAL(&DebounceButtons::timerMux);
	return i;
}
	
mask_t DebounceButtons::getKeyPressLong(mask_t key_mask)
{
	return getKeyPress(getKeyRpt(key_mask));
}
	
mask_t DebounceButtons::getKeyCommon( mask_t key_mask )
{
	return getKeyPress((key_press & key_mask) == key_mask ? key_mask : 0);
}
	
	
void IRAM_ATTR DebounceButtons::onTimer()
{
	static mask_t ct0 = ~0, ct1 = ~0;		// ct0 und ct1 mit 0xFFFFFFFF initialisieren 
	static uint16_t rpt;
	mask_t i;
	portENTER_CRITICAL_ISR(&DebounceButtons::timerMux);
	
	if ( all_keys_mask & 0xF1100000 )	// GPIO > 31 verwendet
	{
		i = REG_READ(GPIO_IN1_REG);
		#ifdef GPIO_36_37
			i = ((i & 0x000000F0) << 24) | ((i & 0x00000008) << 21) | ((i & 0x00000004) << 18); // mit GPIO 36/37 - ohne 32/33
		#else
			i = ((i & 0x0000000F) << 28) | ((i & 0x00000010) << 16) | ((i & 0x00000080) << 17);	// ohne GPIO 36/37 - mit 32/33
		#endif
	}
	else
		i = 0;

	i = ( key_state ^ ~( (i | ( REG_READ(GPIO_IN_REG) & 0x0EEFFFFF )) ^ activ_high_mask) ) & all_keys_mask;

	ct0 = ~(ct0 & i);									// reset or count ct0
	ct1 = ct0 ^ (ct1 & i);								// reset or count ct1
	i &= ct0 & ct1;										// count until roll over ?
	key_state ^= i;										// then toggle debounced state
	key_press |= key_state & i;							// teste ob Bit x sowohl in key_state als auch in i gesetzt ist.
														// Wenn 0->1: key press detect
	if( (key_state & repeat_mask) == 0 )				// check repeat function
		rpt = REPEAT_START;								// start delay
	if( --rpt == 0 )
	{
		rpt = REPEAT_NEXT;								// repeat delay
		key_rpt_state |= key_state & repeat_mask;
	}
	portEXIT_CRITICAL_ISR(&DebounceButtons::timerMux);
}

void DebounceButtons::begin()
{
	timer = timerBegin(0, 80, true);		// Divider 80 --> ESP32 clock is 80 MHz --> HardwareTimer every 1us
	timerAttachInterrupt(timer, &DebounceButtons::onTimer, true);
	timerAlarmWrite(timer, 1000 * TIMER_INTERVAL, true);	// aller 1 ms
	delay(0);
	timerAlarmEnable(timer);
}

void DebounceButtons::end()
{
	timerEnd(timer);
}


InputButton::InputButton(int gpio, bool repeat, bool active_high)
{
	if ( all_keys_mask == 0 )
		DebounceButtons::begin();	
	button = 0;
	gpio_nr = gpio;
	button_active_high = active_high;
	if ( gpio >= 0 && gpio < 40 )
	{
		#ifdef GPIO_36_37
			gpio = gpio == 34 ? 20 : gpio == 35 ? 24 : gpio > 35 ? gpio - 8 : gpio; 
		#else
			gpio = gpio == 36 ? 20 : gpio == 39 ? 24 : gpio < 36 && gpio > 31 ? gpio - 4 : gpio; 
		#endif

		all_keys_mask |= (button = (mask_t)1 << gpio);
		if ( active_high ) activ_high_mask |= button;
		if ( repeat ) repeat_mask |= button;
		// gpio 34-39 dont have software pullup/down functions.  
		if ( active_high ) 
			pinMode(gpio_nr, INPUT_PULLDOWN);
		else 
			pinMode(gpio_nr, INPUT_PULLUP);
	}
}

InputButton::~InputButton()
{
	repeat_mask &= ~button;
	all_keys_mask &= ~button;
	activ_high_mask &= ~ button;
	if ( all_keys_mask == 0 )
		DebounceButtons::end();	
}

#else
	#error This library only supports boards with an ESP32!
#endif


