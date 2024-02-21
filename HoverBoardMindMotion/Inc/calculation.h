#include "mm32_device.h"                // Device header

#define Kp 4
#define Ki 10
#define Kd 180



int PID(int setpoint,int real);
void avgspeed();
int PID2PWM(int pid);
void avgItotal();
void avgvbat();
uint32_t updateMotorRPM(uint32_t halltime);
void PIDrst();