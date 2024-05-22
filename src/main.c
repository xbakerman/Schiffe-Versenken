/*
----------------------------------------------------------------------------
 * Notes:
 * This project is currently under construction and has not yet been fully tested.
 * Use with caution.
 * ----------------------------------------------------------------------------
 */

#include <stm32f0xx.h>
#include "epl_clock.h"
#include "epl_usart.h"
#include <stdbool.h>
#include <stddef.h>

#define DEBUG

uint8_t data[200];
uint8_t data_idx = 0;
bool newline_rcvd = false;

/**
 * @brief Delays the program execution for a specified amount of time.
 * @param time The amount of time to delay in number of cycles.
 * @return 0 when the delay is completed.
 */
int delay(uint32_t time)
{
    for(uint32_t i = 0; i < time; i++ ){
        asm("nop"); // No operation, used for delaying
    }
    return 0;
}


void USART2_IRQHandler()
{
    if ( USART2->ISR & USART_ISR_RXNE ) {
        uint8_t c = USART2->RDR;
        //appendFIFO(c, epl_rx_buffer);

        if (data_idx >= sizeof(data)) {
            return;
        }

        data[data_idx] = c;
        data_idx ++;
        if (c == '\n') {
            newline_rcvd = true;
        }
    }
}

void UserButton_Init(void) 
{
    // GPIOC Clock Enable
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    // PC13 als Eingang konfigurieren
    GPIOC->MODER &= ~(GPIO_MODER_MODER13); // Input mode (00)
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR13); // No pull-up, no pull-down (00)
}

size_t findNewline(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++){
        if (data[i] == '\n'){
            return i+1;
        }
    }
    return size;
}

typedef enum
{
  STATE_START,
  STATE_GENERATE_FIELD_MESSAGE,
  STATE_WAIT_FOR_FIELD,
  STATE_GAME_LOOP,
}

State;
State current_state = STATE_START;

void handle_start()
{
  if (newline_rcvd)
    {
        if (strncmp(data, "START", 5) == 0)
        {
            uint8_t checksum[] = "Hello\n";
            EPL_usart_write_n_bytes(checksum, findNewline(data, sizeof(data)));
            newline_rcvd = false;
            //memset(data, 0, sizeof(data));
            data_idx = 0; 
        }
    }
}

void generate_start_message()
{

  uint8_t start_msg[] = "START11815630\n";

  if (!(GPIOC->IDR & (1UL << 13))==1)
    {
        for (uint8_t i = 0; i < sizeof(start_msg); i++)
        {
            while (!(USART2->ISR & USART_ISR_TXE)); // Warten, bis das TXE-Flag gesetzt ist
            USART2->TDR = start_msg[i]; // Senden Sie das nächste Byte
        }

        for (uint8_t i = 0; i < 2000000; i++)   //Button entprellen
            asm("nop");
    }

    current_state = STATE_GENERATE_FIELD_MESSAGE;
        
        
        //EPL_usart_write_n_bytes(start_msg, findNewline(data, sizeof(data)));
        //newline_rcvd = false;
        //memset(data, 0, sizeof(data));
        //data_idx = 0;
}


int main(void){



    epl_usart_t myusart;
    myusart.baudrate = 9600;
    myusart.fifo_size = 20;

    // Configure the system clock to 48MHz
    EPL_SystemClock_Config();
    EPL_init_usart(&myusart);

    //generate_start_message();
    

    while (1){

        //if (newline_rcvd)
        //{
        //uint8_t msg[] = "Hello, World \r\n";
        //EPL_usart_write_n_bytes(msg, findNewline(data, sizeof(data)));
        //newline_rcvd = false;
        ////memset(data, 0, sizeof(data));
        //data_idx = 0;
        //}

        switch (current_state)
        {
        case STATE_START:
            UserButton_Init();
            generate_start_message();
            break;
        case STATE_GENERATE_FIELD_MESSAGE:
            handle_start();
            break;
        
        default:
            break;
        }
        
    }

    
  
    
}