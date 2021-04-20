// InputButton.h

// Quelle: Peter Dannegger

#ifndef _INPUTBUTTON_h
#define _INPUTBUTTON_h


#include "arduino.h"

#define ACTIVE_LOW	false
#define	ACTIVE_HIGH	true

//Bitposition in Maske: #define GPIO_37_38 = mit mit GPIO 37 und 38, sonst GPIO 32 und 33
//34->20	36->20
//35->24	39->24
//36->28	32->28
//37->29	33->29
//38->30	34->30
//39->31	35->31
//#define  GPIO_37_38

typedef uint32_t mask_t;

#define TIMER_INTERVAL	(10)		// Timerintervall in ms

#define REPEAT_START    (uint16_t)(800/TIMER_INTERVAL)		// Zeitschwelle für kurz/lang. after 800ms
#define REPEAT_NEXT     (uint16_t)(300/TIMER_INTERVAL)      // every 300ms


class DebounceButtons
{
	protected:
		static mask_t all_keys_mask;
		static mask_t activ_high_mask;
		static mask_t repeat_mask;
		static void begin();
		static void end();	
		mask_t keyPressed();						// 1 = Taste gedrückt. Tasten werden nicht zurückgesetzt
		mask_t getKeyPress(mask_t key_mask);		// Taste auslesen und zurücksetzen
		mask_t getKeyRpt(mask_t key_mask);
		mask_t getKeyPressShort(mask_t key_mask);
		mask_t getKeyPressLong(mask_t key_mask);
		mask_t getKeyCommon( mask_t key_mask );
	
	private:
		static hw_timer_t * timer;
		static portMUX_TYPE timerMux;
		static void IRAM_ATTR onTimer();
		static volatile mask_t key_press;			// key press detect
		static volatile mask_t key_state;			// debounced and inverted key state: bit = 1: key pressed
		static volatile mask_t key_rpt_state;		// debounced and inverted key state: bit = 1: key long press and repeat
};

// 34 GPIOs (0-19, 21-23, 25-27, 32-39) 
// pins 20, 24, 28, 29, 30 and 31 are not exposed so there are a maximum of 34 pins available to us.
// GPIO 34-39 haben keine PULLUP/PULLDOWN-Funktion!
class InputButton : DebounceButtons
{
	protected:
		mask_t button;
		bool button_active_high;
		int gpio_nr;

	public:
		InputButton(int gpio, bool repeat = false, bool active_high = ACTIVE_LOW);
		~InputButton();
		mask_t getButtonMask() { return button; }
		mask_t getAllBtnMask() { return all_keys_mask; }
		mask_t getActiveHighMask() { return activ_high_mask; }	
		bool isPressed() { return (bool)(keyPressed() & button); }
		bool shortPress() { return (bool)getKeyPressShort(button); }
		bool longPress() { return (bool)getKeyPressLong(button); }
		bool pressAndRepeat() { return (bool)(getKeyPress(button) || getKeyRpt(button)); }
};


#endif

