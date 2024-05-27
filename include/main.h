#ifndef MAIN_H_
#define MAIN_H_

#include <stm32f0xx.h>

void delay(uint32_t time);
void USART2_IRQHandler(void);
void UserButton_Init(void);
void send_start_message(void);
void send_field_checksum(void);
void process_incoming_message(void);
void generate_field(void);
void place_ship(int start_row, int start_col, int size, int is_vertical);
void place_ship_randomly(int size);
void send_shot(uint8_t col, uint8_t row);
void handle_shot_report(char result);
void game_loop(void);

#endif // MAIN_H_
