#include "hal_uart.h"
#include "../Inc//pinout.h"

void UART_Send_Byte(u8 dat)
{
    UART_SendData(UARTEN, dat);
    while(!UART_GetFlagStatus(UARTEN, UART_CSR_TXC));
}

void UART_Send_Group(u8* buf, u16 len)
{
    while(len--)
        UART_Send_Byte(*buf++);
}

void UART_SendString(u8* str)
{
    while(*str)
        UART_Send_Byte((u8)(*str++));
}























