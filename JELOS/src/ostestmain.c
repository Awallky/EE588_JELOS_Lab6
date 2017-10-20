//  Spring 2016 OS Main 
//  Author: James Lumpp
//  Date: 4/12/2016
//
// Pre-processor Directives
#include <stdio.h>  
#include <stdint.h> 
#include "./../include/UART.h"
#include "./../include/PLL.h"
#include "./../include/jelos.h"
#include "./../include/mylib.h"

//  Global Declarations section

unsigned char task_zero_stack[MIN_STACK_SIZE]; // Declare a seperate stack 
unsigned char task_one_stack[MIN_STACK_SIZE];  // for each task
unsigned char task_two_stack[MIN_STACK_SIZE];
unsigned char task_shell_stack[1024];
extern unsigned int *sem;

// Function Prototypes
void shell(void);

void Zero(void)
	{
		ROM_GPIOPinWrite(PORTF_BASE_ADDR, PIN_1 ,  
										 ~(ROM_GPIOPinRead(PORTF_BASE_ADDR, 
										 PIN_1)) );   // toggle LED
		while(1){
			OS_Sem_Wait(sem);
			putchar('0'); //tasks should not end
			OS_Sem_Signal(sem);
		}
	}

void One(void)
	{
		ROM_GPIOPinWrite(PORTF_BASE_ADDR, PIN_2 ,  
										 ~(ROM_GPIOPinRead(PORTF_BASE_ADDR,
										 PIN_2)) );   // toggle Blue LED 
		while(1){  
			
			OS_Sem_Wait(sem);
			putchar('1');
			OS_Sem_Signal(sem);
		}
	} 
	
void Two(void)
	{
		ROM_GPIOPinWrite(PORTF_BASE_ADDR, PIN_3 ,  
										 ~(ROM_GPIOPinRead(PORTF_BASE_ADDR,
										 PIN_3)) );   // toggle LED 
		while (1){ 
			//OS_Sem_Wait(sem);
			putchar('2'); //tasks should not end
			//OS_Sem_Signal(sem);
		}
	
	} 
	
	

// main
int main(void) {
	
	PLL_Init();     // 50 MHz (SYSDIV2 == 7, defined in pll.h)
  UART_Init();    // initialize UART
	                //115,200 baud rate (assuming 50 MHz UART clock),
                  // 8 bit word length, no parity bits, one stop bit, FIFOs enabled

  puts("\n\nWelcome to the Operating System...\n\n");

	// Create tasks that will run (these are functions that do not return)
	
	CreateTask(shell, task_shell_stack, sizeof (task_shell_stack));
	CreateTask(Zero, task_zero_stack, sizeof (task_zero_stack));
	CreateTask(One, task_one_stack, sizeof (task_one_stack));
	CreateTask(Two, task_two_stack, sizeof (task_two_stack));
	
	puts("\nStarting Scheduler...");
	
	StartScheduler();  //Start the OS Scheduling, does not return
	
	while(1) // should never get here, but just in case...
		;
} //main





