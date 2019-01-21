#include "tm1637.h"

uint8_t Cmd_SetData;
uint8_t Cmd_SetAddr;
uint8_t Cmd_DispCtrl;
volatile uint8_t _PointFlag;
uint8_t _DispType;
int8_t coding(int8_t DispData);
uint8_t Clkpin;
uint8_t Datapin;
uint8_t DecPoint;
uint8_t BlankingFlag;

void TM1637_writeByte(int8_t wr_data)
{
	uint8_t i;
	for(i=0;i<8;i++)        
	{
		DispPort &= ~(1<<Clkpin);
		if(wr_data & 0x01)
			{ DispPort |= 1<<Datapin; }
		else {DispPort &= ~(1<<Datapin);}
		_delay_us(3);
		wr_data = wr_data>>1;      
		DispPort |= 1<<Clkpin;
		_delay_us(3);  
	}  

	DispPort &= ~(1<<Clkpin);
	_delay_us(5);
	DDRC &= ~(1<<Datapin);
	while((PINC & (1<<Datapin))); 
	DDRC |= (1<<Datapin);
	DispPort |= 1<<Clkpin;
	_delay_us(2);
	DispPort &= ~(1<<Clkpin);  
}

void TM1637_start(void)
{
	DispPort |= 1<<Clkpin; 
	DispPort |= 1<<Datapin;
	_delay_us(2);
	DispPort &= ~(1<<Datapin); 
} 

void TM1637_stop(void)
{
	DispPort &= ~(1<<Clkpin);
	_delay_us(2);
	DispPort &= ~(1<<Datapin);
	_delay_us(2);
	DispPort |= 1<<Clkpin;;
	_delay_us(2);
	DispPort |= 1<<Datapin;
}

void TM1637_coding_all(uint8_t DispData[]) 
{
	uint8_t PointData;
	if(_PointFlag == POINT_ON)PointData = 0x80;
	else PointData = 0;
	for(uint8_t i = 0;i < 4;i ++)
	{
		if(DispData[i] == 0x7f)DispData[i] = 0x00;
		else DispData[i] = TubeTab[DispData[i]];
		DispData[i] += PointData;
	}
	if((_DispType == D4056A)&&(DecPoint != 3))
	{
		DispData[DecPoint] += 0x80;
		DecPoint = 3;
	}
}
int8_t TM1637_coding(uint8_t DispData)
{
	uint8_t PointData;
	if(_PointFlag == POINT_ON)PointData = 0x80;
	else PointData = 0;
	if(DispData == 0x7f) DispData = 0x00 + PointData;
	else DispData = TubeTab[DispData] + PointData;
	return DispData;
}

void TM1637_display_all(uint8_t DispData[]) 
{
	uint8_t SegData[4];
	uint8_t i;
	for(i = 0;i < 4;i ++)
	{
		SegData[i] = DispData[i];
	}
	TM1637_coding_all(SegData);
	TM1637_start();
	TM1637_writeByte(ADDR_AUTO);
	TM1637_stop();
	TM1637_start();
	TM1637_writeByte(Cmd_SetAddr);
	for(i=0;i < 4;i ++)
	{
		TM1637_writeByte(SegData[i]);
	}
	TM1637_stop();
	TM1637_start();
	TM1637_writeByte(Cmd_DispCtrl);
	TM1637_stop();
}

void TM1637_display(uint8_t BitAddr,int8_t DispData)
{
	int8_t SegData;
	SegData = TM1637_coding(DispData);
	TM1637_start();
	TM1637_writeByte(ADDR_FIXED);
	TM1637_stop();
	TM1637_start();
	TM1637_writeByte(BitAddr|0xc0);
	TM1637_writeByte(SegData);
	TM1637_stop();
	TM1637_start();
	TM1637_writeByte(Cmd_DispCtrl);
	TM1637_stop();
}

void TM1637_display_int_decimal(int16_t Decimal)
{
	uint8_t temp[4];
	if((Decimal > 9999)||(Decimal < -999))return;
	if(Decimal < 0)
	{
		temp[0] = INDEX_NEGATIVE_SIGN;
		Decimal = (Decimal & 0x7fff);
		temp[1] = Decimal/100;
		Decimal %= 100;
		temp[2] = Decimal / 10;
		temp[3] = Decimal % 10;
		if(BlankingFlag)
		{
			if(temp[1] == 0)
			{
				temp[1] = INDEX_BLANK;
				if(temp[2] == 0) temp[2] = INDEX_BLANK;
			}
		}
	}
	else
	{
		temp[0] = Decimal/1000;
		Decimal %= 1000;
		temp[1] = Decimal/100;
		Decimal %= 100;
		temp[2] = Decimal / 10;
		temp[3] = Decimal % 10;
		if(BlankingFlag)
		{
			if(temp[0] == 0)
			{
				temp[0] = INDEX_BLANK;
				if(temp[1] == 0)
				{
					temp[1] = INDEX_BLANK;
					if(temp[2] == 0) temp[2] = INDEX_BLANK;
				}
			}
		}
	}
	BlankingFlag = 1;
	TM1637_display_all(temp);
} 

void TM1637_display_float_decimal(double Decimal)
{
	int16_t temp;
	if(Decimal > 9999) return;
	else if(Decimal < -999) return;
	uint8_t j = 3;
	if(Decimal > 0)
	{
		for( ;j > 0; j --)
		{
			if(Decimal < 1000)Decimal *= 10;
			else break;
		}
		temp = (int)Decimal;
		if((Decimal - temp)>0.5)temp++;
	}
	else
	{
		for( ;j > 1; j --)
		{
			if(Decimal > -100)Decimal *= 10;
			else break;
		}
		temp = (int)Decimal;
		if((temp - Decimal)>0.5)temp--;
	}
	DecPoint = j;
	BlankingFlag = 0;
	TM1637_display_int_decimal(temp);
}

void TM1637_clearDisplay(void)
{
	TM1637_display(0x00,0x7f);
	TM1637_display(0x01,0x7f);
	TM1637_display(0x02,0x7f);
	TM1637_display(0x03,0x7f);  
}
void TM1637_init(uint8_t DispType,uint8_t Clk, uint8_t Data)
{
	Clkpin = Clk;
	Datapin = Data;
	DDRD |= (1<<Clkpin) | (1<<Datapin);
	_DispType = DispType;
	_PointFlag = POINT_ON;
	BlankingFlag = 1;
	DecPoint = 3;
	TM1637_clearDisplay();
}
void TM1637_set(uint8_t brightness,uint8_t SetData,uint8_t SetAddr)
{
	Cmd_SetData = SetData;
	Cmd_SetAddr = SetAddr;
	Cmd_DispCtrl = 0x88 + brightness;
}
void TM1637_point(uint8_t PointFlag)
{
	_PointFlag = PointFlag;
}