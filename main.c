/*
 * File:   main.c
 * Author: Thorsten
 *
 * Created on 5. März 2024, 21:54
 */


#define _XTAL_FREQ 4000000  
//#define _XTAL_FREQ 4194304  

#include <xc.h>

// CONFIG
//#pragma config FOSC = INTRCIO   // Oscillator Selection bits (INTOSC oscillator: I/O function on RA4/OSC2/CLKOUT pin, I/O function on RA5/OSC1/CLKIN)
#pragma config FOSC = XT   // Oscillator Selection bits (INTOSC oscillator: I/O function on RA4/OSC2/CLKOUT pin, I/O function on RA5/OSC1/CLKIN)

#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = OFF      // RA3/MCLR pin function select (RA3/MCLR pin function is digital I/O, MCLR internally tied to VDD)
#pragma config BOREN = ON       // Brown-out Detect Enable bit (BOD enabled)
#pragma config CP = OFF         // Code Protection bit (Program Memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)

volatile char previous_RA0 = 1;
volatile char previous_RA1 = 1;


#define NORMAL_SPEED 5
#define FAST_SPEED 2

volatile int pressed_cnt = FAST_SPEED;

unsigned char tmr0_start = 60;
long pressed_ticks = 0;

volatile uint8_t is_clockwise = 1;                    // 1: is clockwise, 0: is counter clock wise

volatile uint8_t is_pulstrain = 1;                    // 1: is in pulstrain, 0: is between puls trains
volatile uint8_t pulstrain_steps = 0;
#define NORMAL_STEP_DUR 165
#define FAST_STEP_DUR 200
#define VFAST_STEP_DUR 250
volatile uint8_t pulstrain_step_duration = NORMAL_STEP_DUR;


volatile unsigned long target_ticks = 0;

volatile int8_t step = 0;              // starts the switch case at 0 on startup
volatile  int8_t max_step =5;

volatile uint24_t tmr_start =0;

unsigned int cnt = 0;

unsigned int total_steps =0;
unsigned int total_trains =0;

uint8_t speed_level = 0;


void do_stepper(void) {
    unsigned char prev = PORTC & 0x20; // RC4 is not saved
    switch(step){
  
/*      //Without RC4 blicking
        case 5: PORTC = prev | 9; break;  // = 1001 RC0 , RC3 HIGH 
        case 4: PORTC = prev | 8; break;  // = 1000 RC3 HIGH
        case 3: PORTC = prev | 14; break; // = 1110 RC1 , RC2, RC3 HIGH
        case 2: PORTC = prev | 6; break;  // = 0110 RC1 , RC2 HIGH
        case 1: PORTC = prev | 7; break;  // = 0111 RC0, RC1 , RC2 HIGH
        case 0: PORTC = prev | 1; break;  // = 0111 RC0, RC1 , RC2 HIGH            
  */      
        // With RC4 blicking
        case 5: PORTC = prev | 9; break;  // = 1001 RC0 , RC3 HIGH 
        case 4: PORTC = prev | 24; break;  // = 1000 RC3 HIGH
        case 3: PORTC = prev | 14; break; // = 1110 RC1 , RC2, RC3 HIGH
        case 2: PORTC = prev | 22; break;  // = 0110 RC1 , RC2 HIGH
        case 1: PORTC = prev | 7; break;  // = 0111 RC0, RC1 , RC2 HIGH
        case 0: PORTC = prev | 17; break;  // = 0111 RC0, RC1 , RC2 HIGH            

    }
    if (is_clockwise == 1) {
        step ++;
        if (step >max_step)
            step = 0;
    }
    else {
        step --;
        if (step <0)
            step = max_step;
    }
}


void __interrupt() ISR() {
    if (RAIF == 1) {
        // Button
        __delay_ms(50);

        char current_PORTA = PORTA;

        char current_PORT_RA0 = (current_PORTA & 0x01);
        char current_PORT_RA1 = (current_PORTA & 0x02) >> 1;

        if (current_PORT_RA0 != previous_RA0) {
            previous_RA0 = current_PORT_RA0;
            if (current_PORT_RA0 == 0) {
                // button pressed
                pressed_ticks = TMR0 -255;
                speed_level = 1;
                is_clockwise = 0;
                PORTCbits.RC5 = 1;
                if ((is_pulstrain == 0) && (target_ticks > 255))
                    target_ticks = 10;
            }
            else {
                // button released
                PORTCbits.RC5 = 0;
                speed_level = 0;
                is_clockwise = 1;   // clockwise again
                pulstrain_step_duration = NORMAL_STEP_DUR;
            }
        } 
        if (current_PORT_RA1 != previous_RA1) {
            previous_RA1 = current_PORT_RA1;
            if (current_PORT_RA1 == 0) {
                // Button pressed
                pressed_ticks = TMR0 -255;
                speed_level = 1;
                is_clockwise = 1;   // clockwise 
                PORTCbits.RC5 = 1;
                if ((is_pulstrain == 0) && (target_ticks > 255))
                    target_ticks = 10;
                //tmr_start = TMR0;
            }
            else {
                // button released 
                speed_level = 0;
                is_clockwise = 1;   // clockwise 
                pulstrain_step_duration = NORMAL_STEP_DUR;
                PORTCbits.RC5 = 0;
            }
        } 
        RAIF = 0;

        __delay_ms(100);
        RAIF = 0;
        
        // end buttons
    }
    
    if (T0IF == 1) {
        // Timer0
        if (is_pulstrain == 1) {
            // Within Pulstrain
            do_stepper();
            pulstrain_steps ++;
            if (pulstrain_steps < 6) {
                TMR0 = pulstrain_step_duration; //255 - 181;
            }
            else {
                // transit to sleep
                PORTC = PORTC & 0x30;
                pulstrain_steps = 0;
                step = 0;
                is_pulstrain = 0;
                switch (speed_level) {
                    default:
                    case 0: 
                        // Normal Speed
                         target_ticks = 232514; // Ideal Oszillator
                        break;
                    case 1: 
                        target_ticks = 1235; // ~500ms
                        break;
                    case 2: 
                        target_ticks = 550; // ~1000ms
                        break;
                    case 3: 
                        target_ticks = 0; // ~1500ms
                        break;
                    case 4: 
                        target_ticks = 0; 
                        break;
                }
            }
        }
        else {
            // sleeping, while between pulstrains
            if (speed_level != 0) {
                // Pressed Button
                pressed_ticks += 255;            

                if (pressed_ticks > 1225) {
                    if (pressed_ticks > 3675) {
                        pulstrain_step_duration = VFAST_STEP_DUR;
                        speed_level = 4;
                    }
                    else if (pressed_ticks > 2300) {
                            pulstrain_step_duration = FAST_STEP_DUR;
                            speed_level = 3;
                        }
                        else
                            speed_level = 2;
                }
            }
            PORTAbits.RA2 ^=1;
            if (target_ticks > 255) {
                TMR0 = 0;
                target_ticks -= 255;
            }
            else {
                // transit from sleep to pulstrain                 
                TMR0 = 255 - (unsigned char )target_ticks;
                is_pulstrain = 1;
            }
        }
        T0IF = 0;

      }
  }


void main(void) {
    PORTA = 0X00;                      // All PORT A are set to LOW
    TRISA0 = 1;                        // 1 = Make RA0 an input
    TRISA1 = 1;                        // 1 = Make RA1 an input                         
    TRISA2 = 0;
    TRISC = 0x00;                      // All PORT C are outputs
    CMCON = 0x07;                      // Disable Comparator

    PORTA = 0X00;                      // All PORT A are set to LOW    
    PORTC = 0x00;                      // All PORT A are set to LOW
    
    OPTION_REGbits.nRAPU = 0;   // 1 = PORTA pull-ups are disabled
    WPUAbits.WPUA0 = 1;         //Pin RA0 has NOT its pull enabled holding pin at 5 volts    
    WPUAbits.WPUA1 = 1;         //Pin RA1 has NOT its pull enabled holding pin at 5 volts        
    
    GIE=1;                      // Global Interrupt Enabled
    
    // Button Interrupts
    RAIF = 0;
    RAIE = 1;  
    IOCA = 0x03;                // RA0 and RA1 are interrupt enabled.
    
    //timer0 Interrupt
    T0CS = 0;                   // Selection Internal instruction cycle clock
    PSA = 0;                    // Select Timer0 Prescaler
    OPTION_REGbits.PS = 0x07;   // Select Prescaler 1:256
    T0IE = 1;                   // Enable TMR0 interruprts    
    T0IF = 0;                   // TMR0 Flag
    TMR0=0;                     // Clear TMR0 register
    TMR0=250;                   // should quickly start

    PORTA;
    
    
    while(1){                          // start the forever loop
    }
    
}