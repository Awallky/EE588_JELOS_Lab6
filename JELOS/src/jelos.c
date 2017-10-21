// JEL OS
// Fall 2016
// James E. Lumpp, Jr.
// 4/11/16

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "./../include/mylib.h"

static TaskControlBlock task_list[NUM_TASKS], *TASK_LIST_PTR;
static TaskControlBlock *CURRENT_TASK;
unsigned int *sem;
int i;
int task_state[NUM_TASKS], stack_size[NUM_TASKS];
int task_ticks[NUM_TASKS], stack_array[NUM_TASKS];
int stack_start_array[NUM_TASKS], stack_end_array[NUM_TASKS]; // Will change percent_cpu to float later on
int percent_stack[NUM_TASKS],  percent_cpu[NUM_TASKS];
//float percent_stack[NUM_TASKS], percent_cpu[NUM_TASKS];
unsigned int total_num_ticks = 1;
uint32_t ui32Error; // error code for interrupts

static int NEXT_TID;
static unsigned char null_task_stack[60];  // is not used, null task uses original system stack

//*****************************************************************************
//
// The count of interrupts received.  This is incremented as each interrupt
// handler runs, and its value saved into interrupt handler specific values to
// determine the order in which the interrupt handlers were executed.
//
//*****************************************************************************
volatile uint32_t g_ui32Index;

//*****************************************************************************
//
// The value of g_ui32Index when the INT_GPIOA interrupt was processed.
//
//*****************************************************************************
volatile uint32_t g_ui32GPIOa;

//*****************************************************************************
//
// The value of g_ui32Index when the INT_GPIOB interrupt was processed.
//
//*****************************************************************************
volatile uint32_t g_ui32GPIOb;

//*****************************************************************************
//
// The value of g_ui32Index when the INT_GPIOC interrupt was processed.
//
//*****************************************************************************
volatile uint32_t g_ui32GPIOc;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif


/* Start the multi-tasking system */
int StartScheduler(void)
	{
	if (CURRENT_TASK == NULL)
		return -1;
	OS_Sem_Init(sem, 1);
	ROM_FPULazyStackingEnable();
	PortF_Init();
	SysTick_Init();
	//foo_init();
	EnableInterrupts();
	//EdgeCounter_Init();           // initialize GPIO Port F interrupt OR SysTick OR ...
	
  NullTask();                   // Will not return
	return 0;	 
	}

/* 
		Create a new process and link it to the task list
*/
int CreateTask(void (*func)(void), 
                    unsigned char *stack_start,
                    unsigned stack_size)
					//,unsigned ticks)
	{
//	long ints;
	TaskControlBlock *p, *next;

	if (TASK_LIST_PTR == 0)
		InitSystem();
	
//	ints=StartCritical();
	p = TASK_LIST_PTR;
	TASK_LIST_PTR = TASK_LIST_PTR->next;
	p->func = func;
	p->state = T_CREATED;
	p->tid = NEXT_TID++;

	       /* stack grows from high address to low address */
	p->stack_start = stack_start;
	p->stack_end = stack_start+stack_size-1;
	//p->stack_size = stack_size;
	
	p->sp = p->stack_end;
	p->clk_ticks = 0;

	           /* create a circular linked list */
	if (CURRENT_TASK == NULL)
		p->next = p, CURRENT_TASK = p;
	else
		next = CURRENT_TASK->next, CURRENT_TASK->next = p, p->next = next;
//  EndCritical(ints);
	return p->tid;
	}

/* 
		Initialize the system.
*/
static void InitSystem(void)
	{
	int i;

	         /* initialize the free list  */
	for (i = 0; i < NUM_TASKS-1; i++)
		task_list[i].next = &task_list[i+1];
	TASK_LIST_PTR = &task_list[0];

	         /* null task has tid of 0 */
	CreateTask(NullTask, null_task_stack, sizeof (null_task_stack));
	}


/* 
		Always runnable task. This has the tid of zero
*/
static void NullTask(void)
	{

	while (1) 
		;          //  putchar('n');
	 
	}


// Schedule will save the current SP and then call teh scheduler
//	SHOULD ONLY BE CALLED IN ISR
/* Schedule(): Run a different task. Set the current task as the next one in the (circular)
 * list, then set the global variables and call the appropriate asm routines
 * to do the job. 
 */
unsigned char * Schedule(unsigned char * the_sp)  
	{
	 unsigned char * sp; // save the current sp and schedule		
	 //
	 // AMW
	 //
   unsigned int period = ROM_SysTickPeriodGet();
	 unsigned int tick_val = ROM_SysTickValueGet();
	 
	 CURRENT_TASK->clk_ticks = period - tick_val; // amount of time left after non-SysTick interrupt, AMW
	 //printf("p: %d\n", (period - tick_val));	 
	 CURRENT_TASK->sp = the_sp;
	 CURRENT_TASK->state = T_READY;
	 
	 //
	 // AMW
	 //
	 task_state[CURRENT_TASK->tid] = T_READY;	 
	 CURRENT_TASK = CURRENT_TASK->next;
	 while(CURRENT_TASK->blocked){ // skip task if blocked
		 CURRENT_TASK = CURRENT_TASK->next;
	 }
	 
	 if(CURRENT_TASK->state == T_READY){
		  CURRENT_TASK->state = T_RUNNING;
			task_state[CURRENT_TASK->tid] = T_RUNNING;
	    sp = CURRENT_TASK->sp;
			CURRENT_TASK->clk_ticks = 0;
	 }
	 else{     /* 
									task->state == T_CREATED so make it "ready"
	                give it an interrupt frame and then launch it
	    		        (with a blr sith 0xfffffff9 in LR in StartNewTask())
						 */
		  CURRENT_TASK->state = T_RUNNING;
		  task_state[CURRENT_TASK->tid] = 1;
			sp = StartNewTask(CURRENT_TASK->sp,(uint32_t) CURRENT_TASK->func); // Does not return!
	 }
		
		/*
				AMW
				Determine the ps values for the current tasks
		*/
		PS_Calcs();
		
		return(sp);
	}

//	
//AMW
//
void PortF_Init(void){
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); 
	ROM_SysCtlDelay(1); 
	ROM_GPIOPinTypeGPIOOutput(PORTF_BASE_ADDR, PINS_1_2_3); 
}

void SysTick_Init(void){
		ROM_SysTickEnable();   
		ROM_SysTickPeriodSet(16700000);   
		ROM_SysTickIntEnable();
}

	
void ps(void){ 
	
	/* Print contents of task list */
	printf("\nUSER\tTID\t%%CPU\tSTK_SZ\t%%STK\tSTATE\t\tADDR\n");
	for( i = 0; i < NUM_TASKS; i++ ){
				printf("ROOT\t");
				printf("%02d\t", i);				
				if( i!=0){
					printf("%d.0\t", percent_cpu[i]);
					printf("%d\t", stack_size[i]);
					printf("%02d.0\t", percent_stack[i]);
				}
				else{
					printf("0.0\t%d\t0.0\t", stack_size[i]);
				}				
				
				if( task_state[i] == T_READY ){
					printf("RDY\t\t0x%x\n", stack_array[i]);
				}
				else if( task_state[i] == T_RUNNING ){
					printf("RUN\t\t0x%x\n", stack_array[i]);
				}
		}
}

void PS_Calcs(void){
		total_num_ticks = 0;
	
		/* 
			AMW
			Get the total number of ticks among all processes
			Function PS_Calcs
		*/
		for(i = 0; i < NUM_TASKS; i++ ){
			total_num_ticks += (CURRENT_TASK->clk_ticks);
			task_ticks[CURRENT_TASK->tid] = CURRENT_TASK->clk_ticks;			
		}
		
		for(i = 0; i < NUM_TASKS; i++ ){
			stack_array[CURRENT_TASK->tid] = (int)CURRENT_TASK->sp;	
			stack_end_array[CURRENT_TASK->tid] = (int)CURRENT_TASK->stack_end;
			stack_start_array[CURRENT_TASK->tid] = (int)CURRENT_TASK->stack_start;
			stack_size[CURRENT_TASK->tid] = (stack_end_array[CURRENT_TASK->tid]-stack_start_array[CURRENT_TASK->tid]+1);
			percent_cpu[CURRENT_TASK->tid] = (100*task_ticks[CURRENT_TASK->tid])/total_num_ticks; /// ((float)total_num_ticks); // / (float)total_num_ticks;
			percent_stack[CURRENT_TASK->tid] = ((stack_end_array[CURRENT_TASK->tid]-stack_array[CURRENT_TASK->tid])*100) / stack_size[CURRENT_TASK->tid];
			
			CURRENT_TASK = CURRENT_TASK->next;
		}
}


void OS_Sem_Init(unsigned int *sem, unsigned int count){
	DisableInterrupts();
	/*
			CRITICAL SECTION OF CODE
	*/
	*sem = count;
	/*
			CRITICAL SECTION OF CODE
	*/
	EnableInterrupts();
}

void OS_Sem_Signal(unsigned int *sem){
	DisableInterrupts();
	*sem += 1;
	if( *sem > 0 ){
		CURRENT_TASK = CURRENT_TASK->next;
		while( CURRENT_TASK->blocked != T_BLOCKED ){
			CURRENT_TASK = CURRENT_TASK->next;
		}
	EnableInterrupts();
}
	
	//DisableInterrupts();
	/*
			CRITICAL SECTION OF CODE
	*/
	//*sem = *sem + 1;
	/*
			CRITICAL SECTION OF CODE
	*/
	//EnableInterrupts();
}

void OS_Sem_Wait(unsigned int *sem){
	DisableInterrupts();
	*sem -= 1;
	if( *sem != 0 ){
		CURRENT_TASK->blocked = T_BLOCKED;
		EnableInterrupts(); // allow scheduler a chance to be called
		OS_Suspend();
	}
	DisableInterrupts(); // turn off interrupts when we go to exit the loop
	
	//while( *sem == 0 ){
		//EnableInterrupts(); // allow scheduler a chance to be called
		//OS_Suspend();
		//DisableInterrupts(); // turn off interrupts when we go to exit the loop
			// It's a block party!!!
			// (Process is blocked)
	//}
	/*
			CRITICAL SECTION OF CODE
	*/
	//*sem = *sem - 1;	
	/*
			CRITICAL SECTION OF CODE
	*/	
	//EnableInterrupts();
}

/* 
		AMW
		Trigger SysTick Interrupt 
*/
void OS_Suspend(void){
		//
    // Trigger the interrupt for GPIO C.
    //
    HWREG(NVIC_ST_CTRL) |= NVIC_INT_CTRL_PENDSTSET;
		//HWREG(NVIC_ST_CTRL) |= NVIC_INT_CTRL_VEC_PEN_TICK;
		//HWREG(NVIC_ST_CURRENT) = 0x01;
		printf("%x\t%x\n", (0x00ffffff&ROM_SysTickPeriodGet()), (0x00ffffff&ROM_SysTickValueGet()));
}


//*****************************************************************************
//
// Delay for the specified number of seconds.  Depending upon the current
// SysTick value, the delay will be between N-1 and N seconds (i.e. N-1 full
// seconds are guaranteed, aint32_t with the remainder of the current second).
//
//*****************************************************************************
void
Delay(uint32_t ui32Seconds)
{
    //
    // Loop while there are more seconds to wait.
    //
    while(ui32Seconds--)
    {
        //
        // Wait until the SysTick value is less than 1000.
        //
        while(ROM_SysTickValueGet() > 1000)
        {
        }

        //
        // Wait until the SysTick value is greater than 1000.
        //
        while(ROM_SysTickValueGet() < 1000)
        {
        }
    }
}

//*****************************************************************************
//
// Display the interrupt state on the UART.  The currently active and pending
// interrupts are displayed.
//
//*****************************************************************************
void
DisplayIntStatus(void)
{
    uint32_t ui32Temp;
		char *out_str;
		int success;
    //
    // Display the currently active interrupts.
    //
    ui32Temp = HWREG(NVIC_ACTIVE0);
		success = sprintf(out_str, "\rActive: %c%c%c ", (ui32Temp & 1) ? '1' : ' ',
               (ui32Temp & 2) ? '2' : ' ', (ui32Temp & 4) ? '3' : ' ');
		if( success >= 0 ){
			printf("%s",out_str);
		}
		else{
			printf("%s",out_str);
		}

    //
    // Display the currently pending interrupts.
    //
    ui32Temp = HWREG(NVIC_PEND0);
		success = sprintf(out_str, "Pending: %c%c%c", (ui32Temp & 1) ? '1' : ' ',
               (ui32Temp & 2) ? '2' : ' ', (ui32Temp & 4) ? '3' : ' ');
    if( success >= 0 ){
			printf("%s",out_str);
		}
		else{
			printf("%s",out_str);
		}
}

//*****************************************************************************
//
// This is the handler for INT_GPIOA.  It simply saves the interrupt sequence
// number.
//
//*****************************************************************************
void
IntGPIOa(void)
{
    //
    // Set PE1 high to indicate entry to this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_PIN_1);

    //
    // Put the current interrupt state on the display.
    //
    DisplayIntStatus();

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Save and increment the interrupt sequence number.
    //
    g_ui32GPIOa = g_ui32Index++;

    //
    // Set PE1 low to indicate exit from this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1, 0);
}

//*****************************************************************************
//
// This is the handler for INT_GPIOB.  It triggers INT_GPIOA and saves the
// interrupt sequence number.
//
//*****************************************************************************
void
IntGPIOb(void)
{
    //
    // Set PE2 high to indicate entry to this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_PIN_2);

    //
    // Put the current interrupt state on the display.
    //
    DisplayIntStatus();

    //
    // Trigger the INT_GPIOA interrupt.
    //
    HWREG(NVIC_SW_TRIG) = INT_GPIOA - 16;
		//IntTrigger (INT_GPIOA);

    //
    // Put the current interrupt state on the display.
    //
    DisplayIntStatus();

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Save and increment the interrupt sequence number.
    //
    g_ui32GPIOb = g_ui32Index++;

    //
    // Set PE2 low to indicate exit from this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_2, 0);
}

//*****************************************************************************
//
// This is the handler for INT_GPIOC.  It triggers INT_GPIOB and saves the
// interrupt sequence number.
//
//*****************************************************************************
void
IntGPIOc(void)
{
    //
    // Set PE3 high to indicate entry to this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_PIN_3);

    //
    // Put the current interrupt state on the display.
    //
    DisplayIntStatus();

    //
    // Trigger the INT_GPIOB interrupt.
    //
    HWREG(NVIC_SW_TRIG) = INT_GPIOB - 16;
		//IntTrigger(INT_GPIOB);

    //
    // Put the current interrupt state on the display.
    //
    DisplayIntStatus();

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Save and increment the interrupt sequence number.
    //
    g_ui32GPIOc = g_ui32Index++;

    //
    // Set PE3 low to indicate exit from this interrupt handler.
    //
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3, 0);
}


void foo_init(void){
	//
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    //
    // Initialize the UART.
    //
    //ConfigureUART();

    printf("\033[2JInterrupts\n");	

    //
    // Configure the PE0-PE2 to be outputs to indicate entry/exit of one
    // of the interrupt handlers.
    //
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 |
                                               GPIO_PIN_3);
    ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0);

    //
    // Set up and enable the SysTick timer.  It will be used as a reference
    // for delay loops in the interrupt handlers.  The SysTick timer period
    // will be set up for one second.
    //
    //ROM_SysTickPeriodSet(ROM_SysCtlClockGet());
    //ROM_SysTickEnable();

    //
    // Reset the error indicator.
    //
    ui32Error = 0;

    //
    // Enable interrupts to the processor.
    //
    ROM_IntMasterEnable();

    //
    // Enable the interrupts.
    //
    ROM_IntEnable(INT_GPIOA);
    ROM_IntEnable(INT_GPIOB);
    ROM_IntEnable(INT_GPIOC);
}

void foo(void){
		char* out_str;
		int success;
    //
    // Indicate that the equal interrupt priority test is beginning.
    //
    UART_OutString("\nEqual Priority\n");

    //
    // Set the interrupt priorities so they are all equal.
    //
    ROM_IntPrioritySet(INT_GPIOA, 0x00);
    ROM_IntPrioritySet(INT_GPIOB, 0x00);
    ROM_IntPrioritySet(INT_GPIOC, 0x00);

    //
    // Reset the interrupt flags.
    //
    g_ui32GPIOa = 0;
    g_ui32GPIOb = 0;
    g_ui32GPIOc = 0;
    g_ui32Index = 1;

    //
    // Trigger the interrupt for GPIO C.
    //
    HWREG(NVIC_SW_TRIG) = INT_GPIOC - 16;
		//IntTrigger(INT_GPIOC);

    //
    // Put the current interrupt state on the LCD.
    //
    DisplayIntStatus();

    //
    // Verify that the interrupts were processed in the correct order.
    //
    if((g_ui32GPIOa != 3) || (g_ui32GPIOb != 2) || (g_ui32GPIOc != 1))
    {
        ui32Error |= 1;
    }

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Indicate that the decreasing interrupt priority test is beginning.
    //
    printf("\nDecreasing Priority\n");

    //
    // Set the interrupt priorities so that they are decreasing (i.e. C > B >
    // A).
    //
    ROM_IntPrioritySet(INT_GPIOA, 0x80);
    ROM_IntPrioritySet(INT_GPIOB, 0x40);
    ROM_IntPrioritySet(INT_GPIOC, 0x00);

    //
    // Reset the interrupt flags.
    //
    g_ui32GPIOa = 0;
    g_ui32GPIOb = 0;
    g_ui32GPIOc = 0;
    g_ui32Index = 1;

    //
    // Trigger the interrupt for GPIO C.
    //
    HWREG(NVIC_SW_TRIG) = INT_GPIOC - 16;
		//IntTrigger(INT_GPIOC);

    //
    // Put the current interrupt state on the UART.
    //
    DisplayIntStatus();

    //
    // Verify that the interrupts were processed in the correct order.
    //
    if((g_ui32GPIOa != 3) || (g_ui32GPIOb != 2) || (g_ui32GPIOc != 1))
    {
        ui32Error |= 2;
    }

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Indicate that the increasing interrupt priority test is beginning.
    //
    printf("\nIncreasing Priority\n");

    //
    // Set the interrupt priorities so that they are increasing (i.e. C < B <
    // A).
    //
    ROM_IntPrioritySet(INT_GPIOA, 0x00);
    ROM_IntPrioritySet(INT_GPIOB, 0x40);
    ROM_IntPrioritySet(INT_GPIOC, 0x80);

    //
    // Reset the interrupt flags.
    //
    g_ui32GPIOa = 0;
    g_ui32GPIOb = 0;
    g_ui32GPIOc = 0;
    g_ui32Index = 1;

    //
    // Trigger the interrupt for GPIO C.
    //
    HWREG(NVIC_SW_TRIG) = INT_GPIOC - 16;
		//IntTrigger(INT_GPIOC);

    //
    // Put the current interrupt state on the UART.
    //
    DisplayIntStatus();

    //
    // Verify that the interrupts were processed in the correct order.
    //
    if((g_ui32GPIOa != 1) || (g_ui32GPIOb != 2) || (g_ui32GPIOc != 3))
    {
        ui32Error |= 4;
    }

    //
    // Wait two seconds.
    //
    Delay(2);

    //
    // Disable the interrupts.
    //
    ROM_IntDisable(INT_GPIOA);
    ROM_IntDisable(INT_GPIOB);
    ROM_IntDisable(INT_GPIOC);

    //
    // Disable interrupts to the processor.
    //
    //ROM_IntMasterDisable();
		
		//
    // Print out the test results.
    //
		success = sprintf(out_str, "\nInterrupt Priority =: %s  >: %s  <: %s\n",
										 (ui32Error & 1) ? "Fail" : "Pass",
										 (ui32Error & 2) ? "Fail" : "Pass",
										 (ui32Error & 4) ? "Fail" : "Pass");
		if( success >= 0 ){
			printf("%s",out_str);
		}
		else{
			printf("Failure to write string\n");
		}
		//ROM_IntMasterEnable();
}
