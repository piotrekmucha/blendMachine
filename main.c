#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "tm1637.h"

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#define bit_is_clear(sfr,bit) \
(!(_SFR_BYTE(sfr) & _BV(bit)))

#define TIMERVIEW 0
#define SPEEDVIEW 1

volatile uint8_t pointFlag = 0;
volatile char RUNNING = 0;
unsigned long PRESSED_LEVEL = 0;
char VIEWMODE = TIMERVIEW;
volatile short num1 = 1;
volatile short num2 = 2;
volatile short num3 = 3;
volatile short num4 = 4;
volatile short num5 = 0;
volatile short SPEEDVAL = 20;
volatile int stepNumber = 0;
char EDITMODE = 0;
volatile uint8_t DIGIT = 0;
double STEPDELAY = 25;
int blinkCounter = 0;
int blinkFlag = 1;
volatile uint8_t buzzerFlag = 0;
volatile uint8_t buzzerTimer = 0;
volatile uint8_t fotoFlag = 1;
volatile uint8_t fotoLevel = 0;

void getHorizontal(void)
{
	while(!bit_is_clear(PINB, 4))
	{
		switch(stepNumber) {
			case 0:
				PORTC |= 1 << PINC5;
				PORTC &= ~(1 << PINC4);
				PORTC &= ~(1 << PINC3);
				PORTC &= ~(1 << PINC2);
				break;
			case 1:
				PORTC &= ~(1 << PINC5);
				PORTC &= ~(1 << PINC4);
				PORTC |= 1 << PINC3;
				PORTC &= ~(1 << PINC2);
				break;
			case 2:
				PORTC &= ~(1 << PINC5);
				PORTC |= 1 << PINC4;
				PORTC &= ~(1 << PINC3);
				PORTC &= ~(1 << PINC2);
				break;
			case 3:
				PORTC &= ~(1 << PINC5);
				PORTC &= ~(1 << PINC4);
				PORTC &= ~(1 << PINC3);
				PORTC |= 1 << PINC2;
				break;
		}
		
		stepNumber = (stepNumber+1) % 4;
		_delay_ms(STEPDELAY);
	}
}

ISR(PCINT0_vect)
{
	if(bit_is_clear(PINB, 3))
	{
		RUNNING = 0;
		_delay_ms(1000);
	}
}

void initializeInterrupts(void)
{
	//Licznik
	TCCR1B |= ((1 << CS10) | (1 << CS11)) | (1 << WGM12); //preskaler + CTC
	TCNT1 = 0;
	OCR1A = 15625; //wartosc maksymalna licznika
	TIMSK1 |= (1 << OCIE1A); //wlaczenie przerwania
	
	//Przycisk stop
	PCICR |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT3);
	
}

void refreshLCD(void)
{
	if(VIEWMODE == TIMERVIEW)
	{
		TM1637_display_int_decimal(num1*1000 + num2*100 + num3*10 + num4);
	}
	else
	{
		TM1637_display_int_decimal((uint8_t)SPEEDVAL);
	}
}

ISR(TIMER1_COMPA_vect)
{
	if(buzzerFlag)
	{
		PORTB |= 1 << PINB5;
		if(buzzerTimer >= 120)
		{
			PORTB &=~(1 << PINB5);
			buzzerFlag = 0;
			buzzerTimer = 0;
		}
		buzzerTimer++;
		return;
	}
	if(!fotoFlag && fotoLevel==0 && !bit_is_clear(PINB, 4))
	{
		fotoFlag = 1;
		fotoLevel = 1;
	}
	else if(!fotoFlag && fotoLevel==1 && bit_is_clear(PINB, 4))
	{
		fotoFlag = 1;
		fotoLevel = 0;
	}
	if(num1==0 && num2==0 && num3==0 && num4==0 && num5==0)
	{
		buzzerFlag = 1;
		RUNNING = 0;
		getHorizontal();
	}
	else
	{
		num5 = num5-1;
		if(num5==-1)
		{
			num4 = num4-1;
			num5 = 59;
			if(!fotoFlag)
			{
				buzzerFlag = 1;
				RUNNING = 0;
			}
			fotoFlag = 0;
			if(num4==-1)
			{
				num3 = num3-1;
				num4 = 9;
				if(num3==-1)
				{
					num2 = num2-1;
					num3 = 5;
					if(num2==-1)
					{
						num1 = num1-1;
						num2 = 9;
					}
				}
			}
		}
	}
	if(pointFlag)
	{
		pointFlag = 0;
		TM1637_point(POINT_OFF);
	}
	else
	{
		pointFlag = 1;
		TM1637_point(POINT_ON);
	}
	refreshLCD();
}

void initialize(void)
{
	//Piny wejscia wyjscia
	DDRB = 0b11100001;
	DDRC = 0xFF;
	DDRD = 0xFF;
	
	SPEEDVAL = 20;
	num1 = 1; num2 = 2; num3 = 3; num4 = 4; num5 = 5;
	
	//Wyswietlacz
	TM1637_init(0,1,0);
	TM1637_set(2,0,0xC0);
	refreshLCD();
	
	//Silnik
	stepNumber = 0;
}

void TimerEdit(void)
{
	EDITMODE = 1;
	VIEWMODE = TIMERVIEW;
	int next = 0;
	refreshLCD();
	DIGIT = 0;
	do
	{
		PRESSED_LEVEL = 0;
		while(bit_is_clear(PINB, 1)) //Przycisk przejscia dalej
		{
			PRESSED_LEVEL++;
			next = 1;
		}
		if(next && PRESSED_LEVEL > 50)
		{
			next = 0;
			DIGIT++;
		}
		PRESSED_LEVEL = 0;
		while(bit_is_clear(PINB, 2)) //Przycisk zmiany wartosci
		{
			PRESSED_LEVEL++;
			_delay_ms(1);
		}
		if(PRESSED_LEVEL > 50)
		{
			if(DIGIT==0)
				num1 = (num1+1)%10;
			else if(DIGIT==1)
				num2 = (num2+1)%10;
			else if(DIGIT==2)
				num3 = (num3 == 5) ? 0 : num3+1;
			else if(DIGIT==3)
				num4 = (num4+1)%10;
			refreshLCD();
			blinkFlag = 1;
			blinkCounter = 0;
		}
		
		if(blinkCounter > 600)
		{
			if(blinkFlag)
			{
				TM1637_display(DIGIT, 0x7f);
				blinkFlag = 0;
			}
			else
			{
				refreshLCD();
				blinkFlag = 1;
			}
			blinkCounter = 0;
		}
		else
		{
			blinkCounter++;
			_delay_ms(1);
		}
	} while(DIGIT < 4);
	num5 = 0;
	blinkFlag = 1;
	blinkCounter = 0;
	EDITMODE = 0;
	TM1637_display_int_decimal(8888);
	_delay_ms(2500);
	refreshLCD();
}

void SpeedEdit(void)
{
	EDITMODE = 1;
	VIEWMODE = SPEEDVIEW;
	refreshLCD();
	PRESSED_LEVEL = 0;
	while(EDITMODE == 1) {
		PRESSED_LEVEL = 0;
		while(bit_is_clear(PINB, 2))
		{
			PRESSED_LEVEL++;
			_delay_ms(1);
		}
		if(PRESSED_LEVEL > 50)
		{
			SPEEDVAL = (SPEEDVAL == 30) ? 20 : SPEEDVAL+1;
			refreshLCD();
			blinkFlag = 1;
			blinkCounter = 0;
		}
		while(bit_is_clear(PINB, 1)) //Przycisk przejscia dalej
		{
			EDITMODE  = 0;
		}
		if(blinkCounter > 600)
		{
			if(blinkFlag)
			{
				TM1637_clearDisplay();
				blinkFlag = 0;
			}
			else
			{
				refreshLCD();
				blinkFlag = 1;
			}
			blinkCounter = 0;
		}
		else
		{
			blinkCounter++;
			_delay_ms(1);
		}
	}
	blinkFlag = 1;
	blinkCounter = 0;
	EDITMODE = 0;
	TM1637_display_int_decimal(8888);
	_delay_ms(2500);
	refreshLCD();
}

int main(void)
{
	initialize();
	initializeInterrupts();
	
	while (1)
	{
		while(!RUNNING)
		{
			while(bit_is_clear(PINB, 1) && PRESSED_LEVEL < 3001) //Przycisk wyswietlacza
			{
				PRESSED_LEVEL++;
				_delay_ms(1);
			}
			if (PRESSED_LEVEL >= 3000) 
			{
				if(VIEWMODE == TIMERVIEW)
				{
					TM1637_display_int_decimal(8888);
					_delay_ms(2500);
					TimerEdit();
				}
				else
				{
					TM1637_display_int_decimal(8888);
					_delay_ms(2500);
					SpeedEdit();
				}
				
			}
			else
			{
				if(PRESSED_LEVEL > 80)
				{
					VIEWMODE = (VIEWMODE) ? TIMERVIEW : SPEEDVIEW;
					if(VIEWMODE == TIMERVIEW)
					{
						pointFlag = 1;
						TM1637_point(POINT_ON);
					}
					else
					{
						pointFlag = 0;
						TM1637_point(POINT_OFF);
					}
					refreshLCD();
				}
			}
			PRESSED_LEVEL = 0;
			
			while(bit_is_clear(PINB, 3)) //Przycisk Start
			{
				PRESSED_LEVEL++;
			}
			if (PRESSED_LEVEL > 80)
			{
				VIEWMODE = TIMERVIEW;
				RUNNING = 1;
			}
			PRESSED_LEVEL = 0;
		}
	
		STEPDELAY = -1.5 * SPEEDVAL + 55;
		fotoFlag = 1;
		_delay_ms(1000);
		sei();
		//Wlaczenie silnika
		PORTC |= 1 << PINC1;

		while(RUNNING || buzzerFlag)
		{
			if(!buzzerFlag)
			{
				switch(stepNumber) {
					case 0:
						PORTC |= 1 << PINC5;
						PORTC &= ~(1 << PINC4);
						PORTC &= ~(1 << PINC3);
						PORTC &= ~(1 << PINC2);
						break;
					case 1:
						PORTC &= ~(1 << PINC5);
						PORTC &= ~(1 << PINC4);
						PORTC |= 1 << PINC3;
						PORTC &= ~(1 << PINC2);
						break;
					case 2:
						PORTC &= ~(1 << PINC5);
						PORTC |= 1 << PINC4;
						PORTC &= ~(1 << PINC3);
						PORTC &= ~(1 << PINC2);
						break;
					case 3:
						PORTC &= ~(1 << PINC5);
						PORTC &= ~(1 << PINC4);
						PORTC &= ~(1 << PINC3);
						PORTC |= 1 << PINC2;
						break;
				}
			
				stepNumber = (stepNumber+1) % 4;
				_delay_ms(STEPDELAY);
			}
		}
		
		cli();
		
		getHorizontal();
		
		TM1637_point(POINT_ON);
	}
	
	
}