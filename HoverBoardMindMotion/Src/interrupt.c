#include "hal_tim.h"
#include "hal_conf.h"
#include "hal_adc.h"
#include "stdio.h"
#include "../Inc/pinout.h"
#include "../Inc/bldc.h"
#include "../Inc/remoteUartBus.h"
#include "../Inc/calculation.h"

#include "../Inc/ipark.h"
#include "../Inc/park.h"
#include "../Inc/clarke.h"
#include "../Inc/PID.h"
#include "../Inc/FOC_Math.h"
#include "../Inc/pwm_gen.h"
#include "../Inc/hallhandle.h"

extern uint8_t step;
extern bool uart;
extern bool adc;
extern bool comm;
extern int8_t dir;
extern double rpm;
extern uint32_t lastcommutate;
extern uint32_t millis;
extern int vbat;
extern int itotal;
extern int realspeed;
extern int frealspeed;
extern uint8_t hallposprev;
extern int32_t iOdom;
uint32_t itotaloffset=0;
uint8_t poweron=0;
int iphasea;
int iphaseb;
uint32_t iphaseaoffset=0;
uint32_t iphaseboffset=0;
extern MM32ADC adcs[10];
float vcc;
uint8_t realdir=0;
extern uint8_t hall_to_pos[8];
extern uint16_t abspwm;

int16_t	TestAngle = 0;

//commutation interrupt
void TIM1_BRK_UP_TRG_COM_IRQHandler(void){
  TIM_ClearITPendingBit(TIM1, TIM_IT_COM);
}

void DMA1_Channel2_3_IRQHandler(void){
  if(DMA_GetITStatus(DMA1_IT_TC3)) {
    DMA_ClearITPendingBit(DMA1_IT_GL3);
    serialit();
  }
}	

void DMA1_Channel4_5_IRQHandler(void){
  if(DMA_GetITStatus(DMA1_IT_TC5)) {
    DMA_ClearITPendingBit(DMA1_IT_GL5);
    serialit();
  }
}	
void ADC1_COMP_IRQHandler(void){
	if(ADC_GetFlagStatus(ADC1, ADC_IT_AWD) != RESET) {
    ADC_ClearFlag(ADC1, ADC_IT_AWD);
		TIM_GenerateEvent(TIM1, TIM_EventSource_Break);
  }
	if(RESET != ADC_GetITStatus(ADC1, ADC_IT_EOC)) {
		if(poweron<127){//cancel out adc offset
			itotaloffset+=analogRead(ITOTALPIN);
			iphaseaoffset+=analogRead(IPHASEAPIN);
			iphaseboffset+=analogRead(IPHASEBPIN);
			poweron++;
		}else if(poweron==127){//divide by 128
			itotaloffset = itotaloffset>>7;
			iphaseaoffset = iphaseaoffset>>7;
			iphaseboffset = iphaseboffset>>7;
			if(AWDG){
				ADC_AnalogWatchdogCmd(ADC1, ENABLE);
				ADC_AnalogWatchdogThresholdsConfig(ADC1, (uint16_t)(AWDG+itotaloffset), 0);
				for(uint8_t i=0;i<ADCCOUNT;i++){
					if(adcs[i].io==ITOTALPIN){
						ADC_AnalogWatchdogSingleChannelConfig(ADC1, adcs[i].channel);
						break;
					}
				}
				ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
			}
			poweron++;
		}else{
			uint16_t tmp = ADC1->ADDR15;
			vcc=(double)4915.2/tmp;
			vbat = (double)VBAT_DIVIDER*analogRead(VBATPIN)*vcc*100/4096;//read adc register
			tmp = analogRead(ITOTALPIN);//prevent overflow on negative value
			itotal = (double)ITOTAL_DIVIDER*(tmp-itotaloffset)*vcc*100/4096;
			iphasea = iphaseaoffset-analogRead(IPHASEAPIN);
			iphaseb = iphaseboffset-analogRead(IPHASEBPIN);
			avgvbat();
			avgItotal();
			if(itotal>SOFT_ILIMIT){
				TIM_CtrlPWMOutputs(TIM1, DISABLE);
			}else{
				TIM_CtrlPWMOutputs(TIM1, ENABLE);
				HALL1.CMDDIR = -dir;
				HALLModuleCalc(&HALL1);
				
				if(!(DRIVEMODE==COM_VOLT||DRIVEMODE==COM_SPEED)){
					if(DRIVEMODE==FOC_VOLT||DRIVEMODE==FOC_SPEED||DRIVEMODE==FOC_TORQUE){
						//CLARK
						clarke1.As = iphasea;
						clarke1.Bs = iphaseb;
						CLARKE_MACRO1(&clarke1);
						//PARK
						park1.Alpha = clarke1.Alpha;
						park1.Beta = clarke1.Beta;
						park1.Theta = HALL1.Angle;
						PARK_MACRO1(&park1);	
						//filter
						IDData.NewData = park1.Ds;
						MovingAvgCal(&IDData);
						
						IQData.NewData = park1.Qs;
						MovingAvgCal(&IQData);		
						//PID
						CurID.qInRef = 0;
						CurID.qInMeas = IDData.Out;
						CalcPI(&CurID);
						
						CurIQ.qInRef = Speed.qOut;
						CurIQ.qInMeas = IQData.Out;
						CalcPI(&CurIQ);
					}
					//IPARK
					if(DRIVEMODE==SINE_VOLT||DRIVEMODE==SINE_SPEED){
						ipark1.Ds = 0;
						ipark1.Qs = abspwm;
					}else{
						ipark1.Ds = CurID.qOut;
						ipark1.Qs = CurIQ.qOut;	
					}
					//TestAngle += 50;
					//ipark1.Theta = TestAngle;	
					ipark1.Theta = HALL1.Angle;    
					IPARK_MACRO1(&ipark1);
					//SVPWM
					//pwm_gen.Mode = FIVEMODE;
					pwm_gen.Mode = SEVENMODE;
					pwm_gen.Alpha = ipark1.Alpha;
					pwm_gen.Beta  = ipark1.Beta;
					PWM_GEN_calc(&pwm_gen);
					//Update duty cycle
					Update_PWM(&pwm_gen);
				}
			}
		}
		ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
	}
}	







	