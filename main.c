//------------------------------------------- main.c CODE STARTS -------------------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include <stdbool.h>

// clock status
#define HXT_STATUS 1 << 0  // 12 MHz
#define LXT_STATUS 1 << 1  // 32.768 kHz
#define PLL_STATUS 1 << 2  // PLLCON
#define LIRC_STATUS 1 << 3 // 10 kHz
#define HIRC_STATUS 1 << 4 // 22.11 MHz

#define PLLCON_FB_DV_VAL 10
#define CPUCLOCKDIVIDE 1
// Macro define when there are keys pressed in each column
#define C3_pressed (!(PA->PIN & (1 << 0)))
#define C2_pressed (!(PA->PIN & (1 << 1)))
#define C1_pressed (!(PA->PIN & (1 << 2)))

/*------------------------------Function Header Start----------------------------------*/

void Enable_Clocksources(void);
void Clock_Init(void);
void Keypad_Init(void);
void LED_Init(void);
void Seven_Seg_Init(void);
void TMR0_Init(void);
void TMR1_Init(void);

void LED5_toggle(void);
void LED6_toggle(void);
void LED7_toggle(void);
void Select_Digit(int digit, int number_to_display);
void Display_Digit(int digit, int number_to_display);
void Turn_Off_All_Digits(void);
void Delay(uint32_t time);
bool Check_If_K9_Is_Pressed(void);
bool Check_If_K1_Is_Pressed(void);
void Clear_Pending_GPAB_INT(void);
void Turn_Off_LED(int LED_number);
void Turn_Off_All_LEDs(void);
void Clear_Pending_TMR0_INT(void);
void Clear_Pending_TMR1_INT(void);
void Debounce_Keypad(void);
void Clear_Pending_EINT1_INT(void);
void Rotate_Button_Init(void);
void Reset_Timer0(void);
void DisplayCheckMode(void);
void Handle_Rotate_Button_Press(void);
void Record_Lap(void);

// stopwatch related
void Start_Stopwatch(void);
void Pause_Stopwatch(void);
void Resume_Stopwatch(void);
void Count_Stopwatch(void);
void Reset_Stopwatch(void);
void Display_Stopwatch(void);

// interrupt handlers
void GPAB_IRQHandler(void);
void TMR0_IRQHandler(void);
void TMR1_IRQHandler(void);
void EINT1_IRQHandler(void);

/*------------------------------Function Header End----------------------------------*/

/*------------------------------Global variables Start----------------------------------*/

// Global Array to display on 7segment for NUC140 MCU
int pattern[] = {
	//   gedbaf_dot_c
	0b10000010, // Number 0          // ---a----
	0b11101110, // Number 1          // |      |
	0b00000111, // Number 2          // f      b
	0b01000110, // Number 3          // |      |
	0b01101010, // Number 4          // ---g----
	0b01010010, // Number 5          // |      |
	0b00010010, // Number 6          // e      c
	0b11100110, // Number 7          // |      |
	0b00000010, // Number 8          // ---d----
	0b01000010, // Number 9
	0b11111111	// Blank LED
};

volatile static int current_row;
volatile static int first_digit = 0;
volatile static int second_digit = 0;
volatile static int third_digit = 0;
volatile static int fourth_digit = 0;
volatile static int millisecond_count = 0;
volatile static int second_count = 0;
volatile static int minute_count = 0;
volatile static int lap_number = 0;
volatile static int lap_number_to_display = 0;

volatile bool is_K1_Pressed = false;
volatile bool is_K9_Pressed = false;
volatile bool is_K5_Pressed = false;

typedef enum
{
	COUNT_MODE,
	IDLE_MODE,
	PAUSE_MODE,
	CHECK_MODE
} STATES;

STATES program_state;

// Lap_Record "object"
struct Lap_Record
{
	int first_digit_of_second;
	int second_digit_of_second;
	int millisecond_digit;
};

struct Lap_Record Lap_Record_List[5] = {0}; // 5 lap records

/*------------------------------Global variables End----------------------------------*/

int main(void)
{
	// Unlock protected registers
	SYS_UnlockReg();

	// Enable clock sources
	Enable_Clocksources();
	Clock_Init();

	// GPIO Init
	Keypad_Init();
	LED_Init();
	Seven_Seg_Init();
	Rotate_Button_Init();

	// Timer Init
	TMR0_Init();
	TMR1_Init();

	// Lock protected registers
	SYS_LockReg();

	// initial state
	current_row = 1;
	program_state = IDLE_MODE;
	LED5_toggle();

	while (1)
	{

		switch (program_state)
		{
		case IDLE_MODE:
			Display_Stopwatch();
			break;
		case COUNT_MODE:
			Display_Stopwatch();
			break;
		case PAUSE_MODE:
			Display_Stopwatch();
			break;
		case CHECK_MODE:
			Turn_Off_All_Digits();
			DisplayCheckMode();
			break;

		default:
			break;
		}
	}
}

void Enable_Clocksources(void)
{
	/// enable clock sources and wait for them to be stable
	CLK->PWRCON |= 1 << 0;
	while (!(CLK->CLKSTATUS & HXT_STATUS))
		;
	CLK->PWRCON |= 1 << 1;
	while (!(CLK->CLKSTATUS & LXT_STATUS))
		;
	CLK->PWRCON |= 1 << 2;
	while (!(CLK->CLKSTATUS & HIRC_STATUS))
		;
	CLK->PWRCON |= 1 << 3;
	while (!(CLK->CLKSTATUS & LIRC_STATUS))
		;
}

void Clock_Init(void)
{
	// Select CPU clock as 12MHz
	CLK->CLKSEL0 &= ~(0b111 << 0);
	// No clock division
	CLK->CLKDIV &= ~(0xF);
}

void Keypad_Init(void)
{
	// Configure GPIO for Key Matrix
	// Rows - outputs
	PA->PMD &= (~(0b11 << 6));
	PA->PMD |= (0b01 << 6);
	PA->PMD &= (~(0b11 << 8));
	PA->PMD |= (0b01 << 8);
	PA->PMD &= (~(0b11 << 10));
	PA->PMD |= (0b01 << 10);
	// COLUMN
	//  For GPIOs in the column - we will keep them as default (the GPIOs will be in quasi-mode instead of inputs)

	// interrupt configurations

	// Set Interrupt for Columns GPA.0-2
	PA->IMD &= ~((1UL << 0) | (1UL << 1) | (1UL << 2));
	// Enable GPA.0-2 Falling Edge trigger Interrupt
	PA->IEN |= ((1UL << 0) | (1UL << 1) | (1UL << 2));

	// Clear any pending GPA.0-2 Interrupt flag
	PA->ISRC |= (1UL << 0) | (1UL << 1) | (1UL << 2);

	// Enable System-level Interrupt for GPAB_INT
	NVIC->ISER[0] |= (1UL << 4);

	// Set GPAB_INT priority to 00 (highest)
	NVIC->IP[1] &= ~(0x3UL << 14);
	NVIC->IP[1] |= (0x1UL << 14);
}

void Seven_Seg_Init(void)
{
	// Configure GPIO for 7segment
	// Set mode for PC4 to PC7
	PC->PMD &= (~(0xFF << 8));	  // clear PMD[15:8]
	PC->PMD |= (0b01010101 << 8); // Set output push-pull for PC4 to PC7
	// Set mode for PE0 to PE7
	PE->PMD &= (~(0xFFFF << 0));		// clear PMD[15:0]
	PE->PMD |= 0b0101010101010101 << 0; // Set output push-pull for PE0 to PE7
}

void LED_Init(void)
{

	// Setup modes for LED 5
	PC->PMD &= ~(0b11 << 24);
	PC->PMD |= 0b01 << 24;

	// LED 7
	PC->PMD &= ~(0b11 << 28);
	PC->PMD |= 0b01 << 28;

	// LED 6

	PC->PMD &= ~(0b11 << 26);
	PC->PMD |= 0b01 << 26;
// LED 8 

	PC->PMD &= ~(0b11 << 30);
	PC->PMD |= 0b01 << 30;

}

// this doesn't work
void Debounce_Keypad(void)
{
	PA->DBEN |= (1UL << 0) | (1UL << 1) | (1UL << 2);
	GPIO->DBNCECON &= ~(1UL << 4);
	GPIO->DBNCECON &= ~(0xF << 0);
	GPIO->DBNCECON |= (0xC << 0);
}

// It's button port B 15
void Rotate_Button_Init(void)
{
	// input
	PB->PMD &= ~(0b11 << 30);
	PB->IEN |= (1 << 15);

	// interrupt config
	NVIC->ISER[0] |= (1UL << 3);
	NVIC->IP[0] &= ~(0x3UL << 30);

	// debounce
	// enable debouncing function
	PB->DBEN |= 1ul << 15;
	GPIO->DBNCECON |= 1ul << 4;
	GPIO->DBNCECON |= (5ul << 0);
}

void LED5_toggle(void)
{

	PC->DOUT ^= (1UL << 12);
}

void LED6_toggle(void)
{

	PC->DOUT ^= (1UL << 13);
}

void LED7_toggle(void)
{

	PC->DOUT ^= (1UL << 14);
}

// utility func to select on the 7-segment display
void Select_Digit(int digit, int number_to_display)
{
	switch (digit)
	{
	case 1:
		// Select the first digit U11
		PC->DOUT |= (1 << 7);  // Logic 1 to turn on the digit
		PC->DOUT &= ~(1 << 6); // SC3
		PC->DOUT &= ~(1 << 5); // SC2
		PC->DOUT &= ~(1 << 4); // SC1
		PE->DOUT = pattern[number_to_display];
		break;

	case 2:
		// Select the second digit U12
		PC->DOUT |= (1 << 6); // Logic 1 to turn on the digit
							  // 	PC->DOUT &= ~(1 << 7); // SC3
							  // PC->DOUT &= ~(1 << 5); // SC2
							  // PC->DOUT &= ~(1 << 4); // SC1
		PE->DOUT = pattern[number_to_display];
		break;

	case 3:
		// Select the third digit U13
		PC->DOUT |= (1 << 5); // Logic 1 to turn on the digit
							  // 	PC->DOUT &= ~(1 << 6); // SC3
							  // PC->DOUT &= ~(1 << 7); // SC2
							  // PC->DOUT &= ~(1 << 4); // SC1
		PE->DOUT = pattern[number_to_display];
		break;

	case 4:
		// Select the fourth digit U14
		PC->DOUT |= (1 << 4);  // Logic 1 to turn on the digit
		PC->DOUT &= ~(1 << 6); // SC3
		PC->DOUT &= ~(1 << 5); // SC2
		PC->DOUT &= ~(1 << 7); // SC1
		PE->DOUT = pattern[number_to_display];
		break;

	default:
		break;
	}
}

void Display_Digit(int digit, int number_to_display)
{
	Select_Digit(digit, number_to_display);
	Delay(5000);
	Turn_Off_All_Digits();
}

void Turn_Off_All_Digits(void)
{
	PC->DOUT &= ~(1 << 7);
	PC->DOUT &= ~(1 << 6); // SC3
	PC->DOUT &= ~(1 << 5); // SC2
	PC->DOUT &= ~(1 << 4); // SC1
}

void Delay(uint32_t time)
{
	SysTick->LOAD = time - 1;
	// Initial SysTick count value
	SysTick->VAL = 0;
	SysTick->CTRL |= (0x01ul << 0);
	while (!(SysTick->CTRL & (0x01ul << 16)))
		;
}

bool Check_If_K1_Is_Pressed(void)
{

	// Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1 << 3);
	PA->DOUT |= (1 << 4);
	PA->DOUT |= (1 << 5);
	if (C1_pressed)
	{

		return true;
	}

	return false;
}

bool Check_If_K9_Is_Pressed(void)
{
	// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT |= (1 << 3);
	PA->DOUT |= (1 << 4);
	PA->DOUT &= ~(1 << 5);
	if (C3_pressed)
	{
		return true;
	}

	return false;
}

// handle keypad button press
void Handle_Keypad_Button_Press(void)
{

	// idle -> count state: LED 5 off, LED6 on
	if (is_K1_Pressed == true && program_state == IDLE_MODE)
	{
		LED5_toggle();
		LED6_toggle();
		Start_Stopwatch();
		program_state = COUNT_MODE;
	}
	// count -> pause state: LED 6 off, LED 7 on
	else if (is_K1_Pressed == true && program_state == COUNT_MODE)
	{
		LED6_toggle();
		LED7_toggle();
		Pause_Stopwatch();
		program_state = PAUSE_MODE;
	}
	// pause mode -> count mode: LED 6 on, resume stopwatch
	else if (is_K1_Pressed == true && program_state == PAUSE_MODE) {
		LED6_toggle();
		Resume_Stopwatch();
		program_state = COUNT_MODE;
	}
	// record lap time
	else if (is_K9_Pressed == true && program_state == COUNT_MODE)
	{
		// increase lap number
		if (lap_number == 5)
		{
			lap_number = 0;
		}
		lap_number = lap_number + 1;
		Record_Lap();
	}
	// pause -> idle state: LED 7 off, LED 5 on
	else if (is_K9_Pressed == true && program_state == PAUSE_MODE)
	{
		LED7_toggle();
		LED5_toggle();

		Reset_Timer0();
		Reset_Stopwatch();
		program_state = IDLE_MODE;
	}
	// check -> pause mode
	else if (is_K9_Pressed == true && program_state == CHECK_MODE)
	{
		program_state = PAUSE_MODE;
	}
}

void GPAB_IRQHandler(void)
{

	is_K1_Pressed = Check_If_K1_Is_Pressed();
	is_K9_Pressed = Check_If_K9_Is_Pressed();

	Handle_Keypad_Button_Press();

	Delay(1000000);

	Clear_Pending_GPAB_INT();
}

void Clear_Pending_GPAB_INT(void)
{
	PA->ISRC |= (0xFFFF << 0);	 // Clear all Interrupt Source Flags from GPA
	PB->ISRC |= (0xFFFFUL << 0); // Clear all Interrupt Source Flags from GPB
}

void Turn_Off_LED(int LED_number)
{
	switch (LED_number)
	{
	case 5:
		PC->DOUT &= ~(1UL << 12);
		break;
	case 6:
		PC->DOUT &= ~(1UL << 13);
		break;
	case 7:
		PC->DOUT &= ~(1UL << 14);
		break;
	default:
		break;
	}
}

void Turn_Off_All_LEDs(void)
{
	PC->DOUT &= ~(1UL << 12);
	PC->DOUT &= ~(1UL << 13);
	PC->DOUT &= ~(1UL << 14);
}

void TMR0_Init(void)
{
	// Timer 0 initialization start--------------
	CLK->CLKSEL1 &= ~(0b111 << 8);
	CLK->CLKSEL1 |= (0b001 << 8); // Clock is 32.768Khz
	CLK->APBCLK |= (1 << 2);
	TIMER0->TCSR &= ~(0xFF << 0);
	// reset Timer 0
	TIMER0->TCSR |= (1 << 26);
	// define Timer 0 operation mode
	TIMER0->TCSR &= ~(0b11 << 27);
	TIMER0->TCSR |= (0b01 << 27);
	TIMER0->TCSR &= ~(1 << 24);

	// update continuously
	TIMER0->TCSR |= (1 << 16);

	// enable Timer 0 interrupt
	TIMER0->TCSR |= (1 << 29);

	// 0.1 seconds
	TIMER0->TCMPR = 3276 - 1;

	NVIC->ISER[0] |= (1 << 8);
	NVIC->IP[2] &= (~(3 << 6));
}

void Display_Stopwatch(void)
{

	Display_Digit(4, fourth_digit);
	Display_Digit(3, third_digit);
	Display_Digit(2, second_digit);
	Display_Digit(1, first_digit);
}

void Start_Stopwatch(void)
{
	// start counting
	TIMER0->TCSR |= (1 << 30);
}

void Count_Stopwatch(void)
{

	// increase count by 1
	millisecond_count = millisecond_count + 1;

	// display fourth digit as 0.1 seconds
	if (fourth_digit == 10)
	{
		// reset display to 0
		fourth_digit = 0;
	}
	else
	{
		fourth_digit = millisecond_count % 10;
	}

	// calculate seconds
	if (millisecond_count % 10 == 0)
	{
		second_count = second_count + 1;

		// calculate minutes
		if (second_count % 60 == 0)
		{
			minute_count = minute_count + 1;
			second_count = 0;
		}
	}

	if (third_digit == 10)
	{
		// reset display to 0
		third_digit = 0;
	}
	else
	{
		third_digit = second_count % 10;
	}

	// don't need to reset here
	second_digit = (second_count / 10) % 10;

	if (first_digit == 10)
	{
		// reset display to 0
		first_digit = 0;
	}
	else
	{
		first_digit = minute_count % 10;
	}
}

void TMR0_IRQHandler(void)
{
	PC->DOUT &= ~(1 << 15); // Time the interrupt by watching pin C.15
	Count_Stopwatch();

	Clear_Pending_TMR0_INT();
	PC->DOUT |= 1 << 15; // turn timer LED back off
}

void Pause_Stopwatch(void)
{
	TIMER0->TCSR &= ~(1 << 30);
}

void Resume_Stopwatch(void)
{
	TIMER0->TCSR |= (1 << 30);
}

// timer to scan keypad
void TMR1_Init(void)
{
	// Timer 1 initialization start--------------
	CLK->CLKSEL1 &= ~(0b111 << 8);
	CLK->CLKSEL1 |= (0b001 << 8); // Clock is 32.768Khz
	CLK->APBCLK |= (1 << 3);
	TIMER1->TCSR &= ~(0xFF << 0);

	// reset Timer 1
	TIMER1->TCSR |= (1 << 26);

	// define Timer 0 operation mode
	TIMER1->TCSR &= ~(0b11 << 27);
	TIMER1->TCSR |= (0b01 << 27);
	TIMER1->TCSR &= ~(1 << 24);

	// Enable interrupt in the Control Resigter of Timer istsle
	TIMER1->TCSR |= (1 << 29);
	TIMER1->TCMPR = 1000 - 1;

	// Setup Timer0 interrupt
	NVIC->ISER[0] |= (1 << 9);
	NVIC->IP[2] &= (~(3 << 14));

	// start counting
	TIMER1->TCSR |= (1 << 30);
}

void TMR1_IRQHandler(void)
{
	if (current_row == 3)
		current_row = 1;
	else
		current_row += 1;

	switch (current_row)
	{
	case 1:
		PA->DOUT &= ~(1UL << 3);
		PA->DOUT |= (1UL << 4);
		PA->DOUT |= (1UL << 5);

		PA->DOUT |= (1UL << 0);
		PA->DOUT |= (1UL << 1);
		PA->DOUT |= (1UL << 2);

		break;

	case 2:
		PA->DOUT &= ~(1UL << 4);
		PA->DOUT |= (1UL << 3);
		PA->DOUT |= (1UL << 5);

		PA->DOUT |= (1UL << 0);
		PA->DOUT |= (1UL << 1);
		PA->DOUT |= (1UL << 2);

		break;

	case 3:
		PA->DOUT &= ~(1UL << 5);
		PA->DOUT |= (1UL << 3);
		PA->DOUT |= (1UL << 4);

		PA->DOUT |= (1UL << 0);
		PA->DOUT |= (1UL << 1);
		PA->DOUT |= (1UL << 2);
	}

	Clear_Pending_TMR1_INT();
}

void Clear_Pending_TMR0_INT(void)
{
	TIMER0->TISR |= (1 << 0);
}

void Clear_Pending_TMR1_INT(void)
{
	TIMER1->TISR |= (1 << 0);
}

void Clear_Pending_EINT1_INT(void)
{
	PB->ISRC |= (1 << 15);
}

void Handle_Rotate_Button_Press(void)
{

	if (lap_number_to_display == 5)
	{
		lap_number_to_display = 0;
	}

	lap_number_to_display = lap_number_to_display + 1;

	if (program_state == PAUSE_MODE)
	{

		program_state = CHECK_MODE;
	}

	Delay(100000); // debounce
}

void EINT1_IRQHandler(void)
{

	Handle_Rotate_Button_Press();
	Delay(100000); // debounce

	Clear_Pending_EINT1_INT();
}

void Reset_Timer0(void)
{
	// reset Timer 0
	TIMER0->TCSR |= (1 << 26);
}

void Reset_Stopwatch(void)
{
	first_digit = 0;
	second_digit = 0;
	third_digit = 0;
	fourth_digit = 0;

	millisecond_count = 0;
	second_count = 0;
	minute_count = 0;
}

void DisplayCheckMode(void)
{

	Display_Digit(1, lap_number_to_display);
	Display_Digit(2, Lap_Record_List[lap_number_to_display - 1].first_digit_of_second);
	Display_Digit(3, Lap_Record_List[lap_number_to_display - 1].second_digit_of_second);
	Display_Digit(4, Lap_Record_List[lap_number_to_display - 1].millisecond_digit);
}

void Record_Lap(void)
{
	Lap_Record_List[lap_number - 1].first_digit_of_second = second_digit;
	Lap_Record_List[lap_number - 1].second_digit_of_second = third_digit;
	Lap_Record_List[lap_number - 1].millisecond_digit = fourth_digit;
}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------------------
