//本程序时钟采用内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
#include <msp430g2553.h>
#include <tm1638.h>

//----------Constants----------//
#define V_T1s 200
#define V_T500ms 100
#define NUM_SAMPLE 50
#define CURRENT_THRESHOLD 1080
#define DEFAULT_SPEED 30

//----------Variables----------//
unsigned int ADC_mod = 0;
unsigned int ADC_currentCV[NUM_SAMPLE];
unsigned int ADC_currentCC[NUM_SAMPLE];
unsigned int ADC_ptrCV = 0;
unsigned int ADC_ptrCC = 0;
signed int ADC_data;
unsigned long Volt = 0;
unsigned long Current = 0;
unsigned int CTR_OUT = 500;

unsigned char clock1s = 0;
unsigned char clock500ms = 0;
unsigned char clock1s_flag = 0;
unsigned char clock500ms_flag = 0;

unsigned char led[8] = {0,0,0,0,0,0,0,0};
unsigned char digit[8] = {0,0,0,0,0,0,0,0};
unsigned char pnt = 0x44;						//两位小数点

unsigned char stop_flag = 0;
unsigned char speed_flag = 1;
unsigned char key_code = 0;
unsigned char key_state = 0;
unsigned char key_flag = 0;

unsigned char DAC_speed = 0;

unsigned char k = 0;
unsigned char n = 0;
unsigned int Current_test[10] = {0};
unsigned int Current_mode = 0;
unsigned int mode = 0;
unsigned long Volt_display = 0;
unsigned long Current_display = 0;

//----------Pin Definition----------//
#define CTL1_0L P1OUT &= ~BIT0	//DAC7611 LD
#define CTL1_0H P1OUT |= BIT0
#define CTL1_4L P1OUT &= ~BIT4	//DAC7611 CLK
#define CTL1_4H P1OUT |= BIT4
#define CTL1_5L P1OUT &= ~BIT5	//DAC7611 SDI
#define CTL1_5H P1OUT |= BIT5
#define CTL1_6L P1OUT &= ~BIT6	//Voltage J1 IN 
#define CTL1_6H P1OUT |= BIT6
#define CTL1_7L P1OUT &= ~BIT7	//Current J1 IN
#define CTL1_7H P1OUT |= BIT7

//----------Delay----------//
void delay_us(unsigned int time)
{
	while(time--);
}

//----------Initialization----------//
//I/O端口初始化
void Init_Ports(void)
{
	P2SEL &= ~(BIT1 + BIT7 + BIT6);			//设置为通用I/O端口，默认连接外晶振
	P2DIR |= BIT7 + BIT6 + BIT5 + BIT4;		//设置为输出 P2.5、P2.6、P2.7 连接TM1638
	P2DIR &= ~(BIT3 + BIT2 + BIT0);			//设置为输入
	P2REN |= BIT3 + BIT2 + BIT0;			//设为上拉电阻

	P1DIR &= ~(BIT3 + BIT6 + BIT7);			//设置为输入
	P1DIR |= BIT0 + BIT4 + BIT5;			//设置为输出
	P1REN |= BIT3;							//设为上拉电阻
}

//Timer0 initialize
//计时器周期5ms
void Init_Timer0(void)
{
	TA0CTL = TASSEL_2 + MC_1;				//1MHz SMCLK系统时钟，增计数模式
	TA0CCR0 = 5000;							//计满5000次为5毫秒
	TACCTL0 = CCIE;							//允许中断
}

//Timer1 initialize
//440Hz, P2.1管脚PWM输出
void Init_Timer1(void)
{
	TA1CTL = TASSEL_2 + MC_1;
	TA1CCTL1 = OUTMOD_7;					//用作PWM的模式之一
	TA1CCR0 = 1000000/440;
	TA1CCR1 = TA1CCR0/2;					//设置占空比为50%
}

void Init_ADC(void)
{
	ADC10CTL0 = 0x00;
	ADC10CTL0 = ADC10ON + ADC10SHT_3;
	ADC10CTL1 = INCH_7 + CONSEQ_0 + ADC10DIV_0 + ADC10SSEL_3;
	ADC10CTL0 |= ENC + ADC10SC;
}

void Init_DAC(void)
{
	CTL1_0L;
	CTL1_4H;
	CTL1_5L;
	delay_us(10);
}

void Init_Devices(void)
{
	WDTCTL = WDTPW + WDTHOLD;					//stop watchdog timer
	if(CALBC1_8MHZ == 0xFF || CALDCO_8MHZ == 0xFF)
		while(1);								//If calibration constants erased, trap CPU
	BCSCTL1 = CALBC1_8MHZ; 	 					//Set range
	DCOCTL = CALDCO_8MHZ;    					//Set DCO step + modulation
	BCSCTL3 |= LFXT1S_2;     					//LFXT1 = VLO
	IFG1 &= ~OFIFG;          					//Clear OSCFault flag
	BCSCTL2 |= DIVS_3;       					//SMCLK = DCO/8

	Init_Ports();
	Init_Timer0();
	Init_Timer1();

	Init_ADC();
	Init_DAC();

	_BIS_SR(GIE);								//开全局中断
}

//----------Scan Key----------//
void Scan_Key(void)
{
	switch (key_code)
	{
		// key1：pause		0: start   1: stop		default: 0
		case 1:
			if(stop_flag)	stop_flag = 0;
			else	stop_flag = 1;
			break;
		// key2：set refreshing speed	0: 0.5s   1: 1s 	default: 1
		case 2:
			if(speed_flag)	speed_flag = 0;
			else	speed_flag = 1;
			break;
		default:
			break;
	}	
}

//----------AD Converter----------//
void ADConv(void)
{
	ADC_mod++;
	if(ADC_mod == 2)
		ADC_mod = 0;
	if(ADC_mod == 0)							//电流源采样
	{
		ADC_data = (int)(ADC10MEM * (1.5167));	//定标
		ADC_currentCC[ADC_ptrCC++] = ADC_data;
		if(ADC_ptrCC == NUM_SAMPLE)
			ADC_ptrCC = 0;
		ADC10CTL0 = 0x00;						//关闭ADC
		ADC10AE0 |= BIT7;						//使能通道A7
		ADC10CTL0 = ADC10ON + ADC10SHT_3 + SREF_1 + REFON;		//REF2_5V
		ADC10CTL1 = INCH_7 + CONSEQ_0 + ADC10DIV_0 + ADC10SSEL_0;	//通道A7，单通道单次采样，1分频，ADC10OSC
		ADC10CTL0 |= ENC + ADC10SC;				//开始采样
	}
	if(ADC_mod == 1)							//电压源采样
	{
		ADC_data = (int)(ADC10MEM * (1.4324));	//定标
		ADC_currentCV[ADC_ptrCV++] = ADC_data;
		if(ADC_ptrCV == NUM_SAMPLE)
			ADC_ptrCV = 0;
		ADC10CTL0 = 0x00;						//关闭ADC
		ADC10AE0 |= BIT6;
		ADC10CTL0 = ADC10ON + ADC10SHT_3 + SREF_1 + REFON;		//REF2_5V
		ADC10CTL1 = INCH_6 + CONSEQ_0 + ADC10DIV_0 + ADC10SSEL_0;	//通道A6，单通道单次采样，1分频，ADC10OSC
		ADC10CTL0 |= ENC + ADC10SC;				//开始采样
	}

	unsigned char i = 0;
	Volt = 0;
	Current = 0;
	for(i=0; i<NUM_SAMPLE; ++i)
	{
		Volt += ADC_currentCV[i];
		Current += ADC_currentCC[i];
	}
	Volt = Volt/NUM_SAMPLE;
	Current = Current/NUM_SAMPLE;
	if(mode == 0)										//未触发过流保护
	{
		CTR_OUT = (int)(Current * 2.434 - 1.836 * Volt);
		Volt_display = Volt;
		Current_display = Current;
	}
	if(mode == 1)										//触发过流保护
	{
		CTR_OUT = (int)(Current * 2.3 - 2.0 * Volt);
		Volt_display = Volt + 10;
		Current_display = Current - 70;
	}
}

//----------DA Converter----------//
void DAConv(unsigned int A_OUT)
{
	unsigned char i = 0;
	CTL1_0H;
	CTL1_4L;
	delay_us(10);
	for(i=0; i<12; i++)
	{
		if(A_OUT & 0x800)	CTL1_5H;					//SDI置1
		else	CTL1_5L;								//SDI置0
		CTL1_4L;
		delay_us(5);
		CTL1_4H;										//产生CLK上升沿，将SDI移入DAC移位寄存器
		A_OUT = A_OUT << 1;
	}
	CTL1_4H;
	delay_us(5);
	CTL1_0L;											//将移位寄存器送DAC输出
}

//----------Interrupt Service Routine----------//
//Timer0中断服务程序，5ms计时到后执行
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0(void)
{
	if(++clock1s >= V_T1s)
	{
		clock1s_flag = 1;						//10s滑动平均判断是否过流保护
		clock1s = 0;
		Current_test[k++] = Current;
		if(k>=10)	k=0;
		for(n=0; n<10; ++n)
			Current_mode += Current_test[n];
		Current_mode = Current_mode / 10;
		if(Current_mode >= CURRENT_THRESHOLD)
			mode = 1;
		else	mode = 0;
	}

	if(++clock500ms >= V_T500ms)
	{
		clock500ms_flag = 1;
		clock500ms = 0;
	}

	if(++DAC_speed >= DEFAULT_SPEED)				//DAC调整周期设为5*default_speed
	{
		DAC_speed = 0;
		DAConv(CTR_OUT);
	}

	ADConv();									//5ms执行一次J1端采样
	TM1638_RefreshDIGIandLED(digit, pnt, led);
	key_code = TM1638_Readkeyboard();

	switch (key_state)
	{
		case 0:
			if (key_code > 0)
			{ 
				key_state = 1;
				key_flag = 1; 
			}
			break;
		case 1:
			if (key_code == 0)
				key_state = 0; 
			break;
	}
}

//----------Main Function----------//
void main(void)
{
	Init_Devices();
	while (clock1s<12);   					//延时60ms等待TM1638上电完成
	init_TM1638();

	while(1)
	{
		if((clock1s_flag && speed_flag) && !stop_flag)
		{
			clock1s_flag = 0;
			digit[0] = Volt_display/1000;
			digit[1] = (Volt_display - digit[0] * 1000)/100;
			digit[2] = (Volt_display - digit[0] * 1000 - digit[1] * 100)/10;
			digit[3] = Volt_display - digit[0] * 1000 - digit[1] * 100 - digit[2] * 10;
			digit[4] = Current_display/1000;
			digit[5] = (Current_display - digit[4] * 1000)/100;
			digit[6] = (Current_display - digit[4] * 1000 - digit[5] * 100)/10;
			digit[7] = Current_display - digit[4] * 1000 - digit[5] * 100 - digit[6] * 10;
		}

		if((clock500ms && !speed_flag) && !stop_flag)
		{
			clock500ms_flag = 0;
			digit[0] = Volt_display/1000;
			digit[1] = (Volt_display - digit[0] * 1000)/100;
			digit[2] = (Volt_display - digit[0] * 1000 - digit[1] * 100)/10;
			digit[3] = Volt_display - digit[0] * 1000 - digit[1] * 100 - digit[2] * 10;
			digit[4] = Current_display/1000;
			digit[5] = (Current_display - digit[4] * 1000)/100;
			digit[6] = (Current_display - digit[4] * 1000 - digit[5] * 100)/10;
			digit[7] = Current_display - digit[4] * 1000 - digit[5] * 100 - digit[6] * 10;
		}

		if (key_flag == 1)
		{
			key_flag = 0;
			Scan_Key();
		}
	}
}
