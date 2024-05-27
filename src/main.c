#include <stm32f0xx.h>
#include "epl_clock.h"
#include "epl_usart.h"
#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define DEBUG
#define FIELD_SIZE 10
#define EMPTY 0 
#define SHIP 1
#define HIT 2
#define MISS 3
#define NUMBER_OF_SHIPS 10



uint8_t shipsize[NUMBER_OF_SHIPS] = {5, 4, 4, 3, 3, 3, 2, 2, 2, 2};	
uint8_t data[200];
uint8_t data_idx = 0;
bool newline_rcvd = false;
uint8_t field[FIELD_SIZE][FIELD_SIZE];
uint8_t opponent_field[FIELD_SIZE][FIELD_SIZE];

uint8_t last_shot_col = 0;
uint8_t last_shot_row = 0;
bool my_turn = false;

typedef enum {
    STATE_INIT,
    STATE_WAIT_START,
    STATE_GENERATE_FIELD,
    STATE_SEND_FIELD_CHECKSUM,
    STATE_WAIT_OPPONENT_CHECKSUM,
    STATE_PLAY,
    STATE_WAIT_SHOT_REPORT,
    STATE_SEND_SHOT_REPORT,
    STATE_GAMEEND,
    SEND_START_AFTER_CS
} State;

State current_state = STATE_INIT;

void delay(uint32_t time)
{
    for(uint32_t i = 0; i < time; i++ ){
        asm("nop");
    }
    return 0;
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 1000; i++) {
        asm("nop");
    }
}

void debug_log(const char* message) {
    EPL_usart_write_n_bytes((uint8_t*)message, strlen(message));
    //message++;
    //delay_ms(7000);
}

void debug_log_char(uint8_t character) {
    EPL_usart_write_n_bytes(&character, 1);
    //delay_ms(7000);
}

void USART2_IRQHandler()
{
    if ( USART2->ISR & USART_ISR_RXNE ) {
        uint8_t c = USART2->RDR;
        if (data_idx >= sizeof(data)) {
            return;
        }

        data[data_idx] = c;
        data_idx++;
        if (c == '\n') {
            newline_rcvd = true;
        }
    }
}

void UserButton_Init(void) 
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~(GPIO_MODER_MODER13);
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR13);
}

void send_start_message()
{
    uint8_t start_msg[] = "START11815630\n";
    EPL_usart_write_n_bytes(start_msg, sizeof(start_msg) - 1);
}

void send_field_checksum() {
    uint8_t checksum_msg[13] = "CS";
    int idx = 2;
    for (int col = 0; col < FIELD_SIZE; col++) {
        int count = 0;
        for (int row = 0; row < FIELD_SIZE; row++) {
            if (field[row][col] == SHIP) {
                count++;
            }
        }
        checksum_msg[idx++] = '0' + count;
    }
    checksum_msg[idx++] = '\n';
    EPL_usart_write_n_bytes(checksum_msg, idx);
    current_state = STATE_WAIT_OPPONENT_CHECKSUM;
}

void process_incoming_message() {
    if (newline_rcvd) {
        newline_rcvd = false;
        data[data_idx] = '\0'; // Null-terminate the string for debugging
        //debug_log("Received: ");
        //debug_log((char*)data);
        //debug_log("\n");

        if (strncmp((char*)data, "CS", 2) == 0) {
            current_state = SEND_START_AFTER_CS;
        ;
        } else if (strncmp((char*)data, "BOOM", 4) == 0) {
            uint8_t col = data[4] - '0';
            uint8_t row = data[5] - '0';
            char result = (field[row][col] == SHIP) ? 'T' : 'W';
            field[row][col] = (field[row][col] == SHIP) ? HIT : MISS;
            EPL_usart_write_n_bytes((uint8_t*)&result, 1);
            EPL_usart_write_n_bytes((uint8_t*)"\n", 1);
            if (result == 'W') {
                my_turn = true;
            }
            current_state = STATE_PLAY;
        } else if (data[0] == 'W' || data[0] == 'T') {
            handle_shot_report(data[0]);
        } else if (strncmp((char*)data, "START", 5) == 0) {
            current_state = STATE_GENERATE_FIELD;
        }
        data_idx = 0;
    }
}

void generate_field()
{
    for(uint8_t i = 0; i < FIELD_SIZE; i++) {
        for(uint8_t j = 0; j < FIELD_SIZE; j++) {
           field[i][j] = EMPTY;
           opponent_field[i][j] = EMPTY;
        }
    }

    for (int i = 0; i < NUMBER_OF_SHIPS; i++) {
        place_ship_randomly(shipsize[i]);
    }
}

bool can_place_ship(int start_row, int start_col, int size, int is_vertical) {
    if (is_vertical) {
        if (start_row + size > FIELD_SIZE) return false;
        for (int i = start_row; i < start_row + size; i++) {
            if (field[i][start_col] != EMPTY) return false;
        }
    } else {
        if (start_col + size > FIELD_SIZE) return false;
        for (int i = start_col; i < start_col + size; i++) {
            if (field[start_row][i] != EMPTY) return false;
        }
    }
    return true;
}

void place_ship(int start_row, int start_col, int size, int is_vertical) {
    if (is_vertical) {
        for (int i = start_row; i < start_row + size; i++) {
            field[i][start_col] = SHIP;
        }
    } else {
        for (int i = start_col; i < start_col + size; i++) {
            field[start_row][i] = SHIP;
        }
    }
}

void place_ship_randomly(int size) {
    bool placed = false;
    while (!placed) {
        int start_row = rand() % FIELD_SIZE;
        int start_col = rand() % FIELD_SIZE;
        int is_vertical = rand() % 2;

        if (can_place_ship(start_row, start_col, size, is_vertical)) {
            place_ship(start_row, start_col, size, is_vertical);
            placed = true;
        }
    }
}

void send_shot(uint8_t col, uint8_t row) {
    uint8_t shot_msg[7];
    shot_msg[0] = 'B';
    shot_msg[1] = 'O';
    shot_msg[2] = 'O';
    shot_msg[3] = 'M';
    shot_msg[4] = '0' + col;  // Spalte in ASCII-Zeichen konvertieren
    shot_msg[5] = '0' + row;  // Zeile in ASCII-Zeichen konvertieren
    shot_msg[6] = '\n';
    
    EPL_usart_write_n_bytes(shot_msg, 7);
    current_state = STATE_WAIT_SHOT_REPORT;
}

void handle_shot_report(char result) {
    if (result == 'T') {
        opponent_field[last_shot_row][last_shot_col] = HIT;
        my_turn = true; 
        current_state = STATE_PLAY; // Continue shooting if it's a hit
    } else {
        opponent_field[last_shot_row][last_shot_col] = MISS;
        my_turn = false;  
    }

    // Set state to play to check for the next move
    current_state = STATE_PLAY;
}

void game_loop() {
    switch (current_state) {
        case STATE_INIT:
            //debug_log("State: INIT\n");
            if ((GPIOC->IDR & GPIO_IDR_13) == 0) {
                my_turn = true;
                send_start_message();
                current_state = STATE_GENERATE_FIELD;
            } else {
                current_state = STATE_WAIT_START;
            }
            break;
        case SEND_START_AFTER_CS:
            //debug_log("State: SEND_START_AFTER_CS\n");
            send_start_message();
            current_state = STATE_PLAY;
            break;
        
        case STATE_WAIT_START:
            //debug_log("State: WAIT_START\n");
            process_incoming_message();
            break;
        case STATE_GENERATE_FIELD:
            //debug_log("State: GENERATE_FIELD\n");
            generate_field();
            send_field_checksum();
            break;
        case STATE_WAIT_OPPONENT_CHECKSUM:
            //debug_log("State: WAIT_OPPONENT_CHECKSUM\n");
            process_incoming_message();
            break;
        case STATE_PLAY:
            //debug_log("State: PLAY\n");
            if (my_turn) {
                uint8_t col = rand() % FIELD_SIZE;
                uint8_t row = rand() % FIELD_SIZE;
                send_shot(col, row);
                last_shot_col = col;
                last_shot_row = row;
                my_turn = false;
            
            } else {
                process_incoming_message();
            }
            break;
        case STATE_WAIT_SHOT_REPORT:
            //debug_log("State: WAIT_SHOT_REPORT\n");
            process_incoming_message();
            break;
        case STATE_SEND_SHOT_REPORT:
            //debug_log("State: SEND_SHOT_REPORT\n");
            // Implement shot reporting logic
            break;
        case STATE_GAMEEND:
            //debug_log("State: GAMEEND\n");
            // Handle game end logic
            break;
        default:
            //debug_log("State: UNKNOWN\n");
            break;
    }
}

int main(void) {
    epl_usart_t myusart;
    myusart.baudrate = 9600;
    myusart.fifo_size = 20;

    EPL_SystemClock_Config();
    EPL_init_usart(&myusart);
    UserButton_Init();
    current_state = STATE_INIT;

    while (1) {
        game_loop();
    }

    return 0;
}
