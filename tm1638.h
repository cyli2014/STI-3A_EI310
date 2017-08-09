#ifndef	_TM1638_H
#define	_TM1638_H

#include  "msp430g2553.h"

//��������TM1638�ĵ�Ƭ�����������������
#define DIO_L   P2OUT &= ~BIT5     //P2.5=0
#define DIO_H   P2OUT |=  BIT5     //P2.5=1
#define CLK_L   P2OUT &= ~BIT7     //P2.7=0
#define CLK_H   P2OUT |=  BIT7     //P2.7=1
#define STB_L   P2OUT &= ~BIT6     //P2.6=0
#define STB_H   P2OUT |=  BIT6     //P2.6=1
#define DIO_IN  P2DIR &= ~BIT5     //P2.5 ����Ϊ����
#define DIO_OUT P2DIR |=  BIT5     //P2.5 ����Ϊ���
#define DIO_DATA_IN    P2IN & BIT5

// ����ʾ���ֻ����ת��Ϊ��������ܵıʻ�ֵ
unsigned char TM1638_DigiSegment(unsigned char digit)
{
	unsigned char segment=0;
	switch (digit)
	{
	case 0:segment=0x3F;break;
	case 1:segment=0x06;break;
	case 2:segment=0x5B;break;
	case 3:segment=0x4F;break;
	case 4:segment=0x66;break;
	case 5:segment=0x6D;break;
	case 6:segment=0x7D;break;
	case 7:segment=0x07;break;
	case 8:segment=0x7F;break;
	case 9:segment=0x6F;break;
	case 10:segment=0x77;break;
	case 11:segment=0x7C;break;
	case 12:segment=0x39;break;
	case 13:segment=0x5E;break;
	case 14:segment=0x79;break;
	case 15:segment=0x71;break;
	case '_':segment=0x08;break;
	case '-':segment=0x40;break;
	case ' ':segment=0x00;break;
	
	case 'A':segment=0x77;break;
	case 'B':segment=0x7C;break;	// min
	case 'C':segment=0x39;break;
	case 'D':segment=0x5E;break;	// min
	case 'E':segment=0x79;break;
	case 'F':segment=0x71;break;
	case 'G':segment=0x3D;break;
	case 'H':segment=0x76;break;
	case 'I':segment=0x06;break;
	case 'J':segment=0x0E;break;
	// K
	case 'L':segment=0x38;break;
	// M
	case 'N':segment=0x37;break;
	case 'n':segment=0x54;break;	// min
	// O/0
	case 'O':segment=0x3F;break;
	case 'o':segment=0x5A;break;
	case 'P':segment=0x73;break;
	case 'q':segment=0x67;break;	// min
	case 'r':segment=0x50;break;	// min
	// S/5
	case 'S':segment=0x6D;break;
	// T
	case 't':segment=0x78;break;
	case 'U':segment=0x3E;break;
	// V
	// W
	// X
	case 'Y':segment=0x6E;break;
	// Z
	default:segment=0x00;break;
	}

	return segment;
}


// TM1638������������
void TM1638_Serial_Input(unsigned char	DATA)
{
	unsigned char i;
	DIO_OUT;  //P2.5 ����Ϊ���
	for(i=0;i<8;i++)
	{
		CLK_L;
		if(DATA&0X01)
			DIO_H;
		else
			DIO_L;
		DATA>>=1;
		CLK_H;
	}
}

// TM1638�����������
unsigned char TM1638_Serial_Output(void)
{
	unsigned char i;
	unsigned char temp=0;
	DIO_IN;	//P2.5 ����Ϊ����
	for(i=0;i<8;i++)
	{
		temp>>=1;
		CLK_L;
		CLK_H;
		if((DIO_DATA_IN)!=0)
			temp|=0x80;
		CLK_L;
	}
	return temp;
}


// ��ȡ���̵�ǰ״̬
unsigned char TM1638_Readkeyboard(void)
{
	unsigned char c[4],i,key_code=0;
	STB_L;
	TM1638_Serial_Input(0x42);		           //����ɨ���� ����
	__delay_cycles(10);		// �ʵ���ʱԼΪ1us
	for(i=0;i<4;i++)		
		c[i]=TM1638_Serial_Output();
	STB_H;					           //4���ֽ����ݺϳ�һ���ֽ�
	if(c[0]==0x04) key_code=1;
	if(c[0]==0x40) key_code=2;
	if(c[1]==0x04) key_code=3;
	if(c[1]==0x40) key_code=4;
	if(c[2]==0x04) key_code=5;
	if(c[2]==0x40) key_code=6;
	if(c[3]==0x04) key_code=7;
	if(c[3]==0x40) key_code=8;
	if(c[0]==0x02) key_code=9;
	if(c[0]==0x20) key_code=10;
	if(c[1]==0x02) key_code=11;
	if(c[1]==0x20) key_code=12;
	if(c[2]==0x02) key_code=13;
	if(c[2]==0x20) key_code=14;
	if(c[3]==0x02) key_code=15;
	if(c[3]==0x20) key_code=16;
	return key_code;  //key_code=0������ǰû�м�������
}

// ˢ��8λ����ܣ���С���㣩��8��ָʾ�ƣ�ÿ��2ֻ����4������ģʽ��
void TM1638_RefreshDIGIandLED(unsigned char digit_buf[8],unsigned char pnt_buf,unsigned char led_buf[8])
{
	unsigned char i,mask,buf[16];

	mask=0x01;
	for(i=0;i<8;i++)
	{
		//�����
		buf[i*2]=TM1638_DigiSegment(digit_buf[i]);
		if ((pnt_buf&mask)!=0) buf[i*2]|=0x80;
		mask=mask*2;

		//ָʾ��
		buf[i*2+1]=led_buf[i];
	}

	STB_L;TM1638_Serial_Input(0x40);STB_H;       //���õ�ַģʽΪ�Զ���һ
	STB_L;TM1638_Serial_Input(0xC0);             //������ʼ��ַ
	for (i=0;i<16;i++)
	{
		TM1638_Serial_Input(buf[i]);
	}
	STB_H;
}


//TM1638��ʼ������
void Init_TM1638(void)
{
	STB_L;TM1638_Serial_Input(0x8A);STB_H;       //�������� (0x88-0x8f)8�����ȿɵ�
}
#endif