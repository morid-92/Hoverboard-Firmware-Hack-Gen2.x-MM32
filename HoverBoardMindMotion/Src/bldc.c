#include "mm32_device.h"                // Device header
#include "hal_gpio.h"
#include "../Src/pinout.h"
#include "../Src/remoteUartBus.h"
#include "../Src/bldc.h"
#include "../Src/calculation.h"
#include "hal_tim.h"
#include "hal_conf.h"
#include "math.h"


extern uint8_t step;
extern int speed;
extern int pwm;
extern bool dir;
extern int realspeed;
extern int frealspeed;
extern uint32_t millis;
uint32_t lastcommutate;
uint32_t lasttestrotate;
int testrotatedir=1;
uint8_t hallpos(uint8_t dir);
bool lastdir=0;




const uint8_t hall_to_pos[8] =
{
	// annotation: for example SA=0 means hall sensor pulls SA down to Ground
  0, // hall position [-] - No function (access from 1-6) 
  3, // hall position [1] (SA=1, SB=0, SC=0) -> PWM-position 3
  5, // hall position [2] (SA=0, SB=1, SC=0) -> PWM-position 5
  4, // hall position [3] (SA=1, SB=1, SC=0) -> PWM-position 4
  1, // hall position [4] (SA=0, SB=0, SC=1) -> PWM-position 1
  2, // hall position [5] (SA=1, SB=0, SC=1) -> PWM-position 2
  6, // hall position [6] (SA=0, SB=1, SC=1) -> PWM-position 6
  0, // hall position [-] - No function (access from 1-6) 
};


uint8_t hallpos(uint8_t dir){
	
	uint8_t HallA;
	uint8_t HallB;
	uint8_t HallC;
	uint8_t HallValue;
	
	HallA = GPIO_ReadInputDataBit(HALLAPORT,HALLAPIN);
	HallB = GPIO_ReadInputDataBit(HALLBPORT,HALLBPIN);
	HallC = GPIO_ReadInputDataBit(HALLCPORT,HALLCPIN);
	
	if(dir == 1)
	{
		HallValue = HallC*4 + HallB*2 + HallA;
	}
	else
	{
		HallValue = 7-(HallC*4 + HallB*2 + HallA);
	}
	
	return hall_to_pos[HallValue];

}


void commutate(){
//U,V,W(A,B,C)
	if(step == 1) {//1,0,-1
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Enable);
	}
	else if(step == 2) {//0,1,-1
 		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Enable);		
  }
	else if(step == 3) {//-1,1,0
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Enable);			
}
	else if(step == 4) {//-1,0,1
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Enable);	
  }
	else if(step == 5) {//0,-1,1
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Enable);	
	}
	else if(step == 6) {//1,-1,0
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Enable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_OCMode_PWM1);
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Enable);  
  }else{//0,0,0
		TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Disable);
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
	}
	TIM_GenerateEvent(TIM1, TIM_EventSource_COM);//apply changes
}





void speedupdate(){
  #ifdef TESTROTATE	
	if(millis-lasttestrotate>100){
		speed+=2*testrotatedir; //keep changing speed
		if(speed>150){    //reverse dir
			testrotatedir=-1;
		}
		if(speed<-150){
			testrotatedir=1;
		}	
		lasttestrotate=millis;
	}
	#else	
	RemoteUpdate();
	#endif
	
	avgspeed();//speed filter prevent oscilation
	if(millis-lastcommutate>500){//zero out speed
		realspeed=0;
		frealspeed=0;
	}	
	#ifdef CONSTSPEED	
	pwm= PID(speed,frealspeed);
	//pwm=(speed-frealspeed)*30;
	if(speed==0){
		pwm=0;
	}
	#else
	pwm=speed*4;//1000~-1000
	#endif
	
	if(pwm>0){    // 4095~-4095
	dir=1;
  }else{
	dir=0;
	}

	unsigned int abspeed=fabs((double)pwm);
	TIM1->CCR1=abspeed;
	TIM1->CCR2=abspeed;
	TIM1->CCR3=abspeed;
}





















