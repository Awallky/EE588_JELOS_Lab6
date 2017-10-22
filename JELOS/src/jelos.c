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
int32_t sem_addr;
int32_t *sem = &sem_addr;
int i;
int task_state[NUM_TASKS], stack_size[NUM_TASKS];
int task_ticks[NUM_TASKS], stack_array[NUM_TASKS];
int stack_start_array[NUM_TASKS], stack_end_array[NUM_TASKS]; // Will change percent_cpu to float later on
int percent_stack[NUM_TASKS],  percent_cpu[NUM_TASKS];
//float percent_stack[NUM_TASKS], percent_cpu[NUM_TASKS];
uint32_t total_num_ticks = 1;
uint32_t ui32Error, previous_systick_value; // error code for interrupts
uint32_t deadlock_flag = 0;

static int NEXT_TID;
static unsigned char null_task_stack[60];  // is not used, null task uses original system stack

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

	while (1){ 
		OS_Sem_Wait(sem);
		OS_Sem_Signal(sem);          //  putchar('n');
		OS_Suspend();
	}
	 
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
   uint32_t period;
	 uint32_t tick_val;
	 //printf("\n\n");
	
	 period = ROM_SysTickPeriodGet();
	 tick_val = ROM_SysTickValueGet();	 
	 CURRENT_TASK->clk_ticks = period - tick_val; // amount of time left after non-SysTick interrupt, AMW
		
	 /* Track changes to the stack pointer, Update task state */
	 CURRENT_TASK->sp = the_sp;
	 CURRENT_TASK->state = T_READY;
	 task_state[CURRENT_TASK->tid] = T_READY;
		
	 /* Search for a task blocked on *sem */
	 CURRENT_TASK = CURRENT_TASK->next;
	 while(CURRENT_TASK->blocked){
			CURRENT_TASK = CURRENT_TASK->next; // skip task if blocked
	 }
	 
	 if(CURRENT_TASK->state == T_READY){
		  CURRENT_TASK->state = T_RUNNING;
			task_state[CURRENT_TASK->tid] = T_RUNNING;
	    sp = CURRENT_TASK->sp;
			CURRENT_TASK->clk_ticks = 0;
	 }
	 else {     /* 
									task->state == T_CREATED so make it "ready"
	                give it an interrupt frame and then launch it
	    		        (with a blr sith 0xfffffff9 in LR in StartNewTask())
						 */
		  CURRENT_TASK->state = T_RUNNING;
		  task_state[CURRENT_TASK->tid] = T_RUNNING;
			sp = StartNewTask(CURRENT_TASK->sp,(uint32_t) CURRENT_TASK->func); // Does not return!
	 }
		
		/*
				AMW
				Determine the ps values for the current tasks
		*/
		DisableInterrupts();
		PS_Calcs();		
		EnableInterrupts();
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
	printf("\nUSER\tTID\t%%CPU\tSTK_SZ\t%%STK\tSTATE\tADDR\n");
	for( i = 0; i < NUM_TASKS; i++ ){
				printf("ROOT\t");
				printf("%02d\t", i);				
				if( i!=0 ){
					printf("%d.0\t", percent_cpu[i]);
					printf("%d\t", stack_size[i]);
					printf("%02d.0\t", percent_stack[i]);
				}
				else{
					printf("0.0\t%d\t0.0\t", stack_size[i]);
				}				
				
				if( task_state[i] == T_READY ){
					printf("RDY\t0x%x\n", stack_array[i]);
				}
				else if( task_state[i] == T_RUNNING ){
					printf("RUN\t0x%x\n", stack_array[i]);
				}
		}
}

void PS_Calcs(void){
		TaskControlBlock *tcb_ptr;
		tcb_ptr = CURRENT_TASK;
	
		/* 
			AMW
			Get the total number of ticks among all processes
			Function PS_Calcs
		*/
		
		// adjust the clocks from theone holding the max ticks
		for(i = 0; i < NUM_TASKS; i++ ){
			task_ticks[tcb_ptr->tid] = tcb_ptr->clk_ticks;
			tcb_ptr = tcb_ptr->next;
		}
		tcb_ptr = CURRENT_TASK;
		
		// count the total number of task ticks
		for(i = 0; i < NUM_TASKS; i++ ){
			total_num_ticks += task_ticks[i];
		}
		
		
		for(i = 0; i < NUM_TASKS; i++ ){
			stack_array[tcb_ptr->tid] = (int)tcb_ptr->sp;
			stack_end_array[tcb_ptr->tid] = (int)tcb_ptr->stack_end;
			stack_start_array[tcb_ptr->tid] = (int)tcb_ptr->stack_start;
			stack_size[tcb_ptr->tid] = (stack_end_array[tcb_ptr->tid]-stack_start_array[tcb_ptr->tid]+1);
			percent_cpu[tcb_ptr->tid] = (100*task_ticks[tcb_ptr->tid])/total_num_ticks;
			percent_stack[CURRENT_TASK->tid] = ((stack_end_array[tcb_ptr->tid]-stack_array[tcb_ptr->tid])*100) / stack_size[tcb_ptr->tid];
			//printf("stack_end: 0x%x\tstack_sp: 0x%x\n", stack_end_array[tcb_ptr->tid], stack_array[tcb_ptr->tid]);
			tcb_ptr = tcb_ptr->next;
		}
		total_num_ticks = 1;
}


void OS_Sem_Init(int32_t *sem, int32_t count){
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

void OS_Sem_Wait(int32_t *sem){
	DisableInterrupts();
	*sem -= 1;
	if( *sem < 0 ){
		CURRENT_TASK->blocked = sem; // blocked on *sem
		EnableInterrupts(); // allow scheduler a chance to be called
		OS_Suspend();
	}	
	EnableInterrupts(); // turn off interrupts when we go to exit the loop
}

void OS_Sem_Signal(int32_t *sem){
	DisableInterrupts();
	*sem += 1;
	if( *sem <= 0 ){
		TaskControlBlock *task_ptr;
		task_ptr = CURRENT_TASK->next;		
		while( task_ptr->blocked != sem ){
			task_ptr = task_ptr->next;		
		}
		task_ptr->blocked = 0; // unblock this task
	}
	EnableInterrupts();
}
/* 
		AMW
		Trigger SysTick Interrupt
		Interrupt Control Reg. Base Address: E000E000
		Interrupt Control Reg. Address Offset: 0x0D04
*/
void OS_Suspend(void){
    //HWREG(INTCTRL) = SYSTICK_TRIGGER; // Trigger SysTick Interrupt
	  INTCTRL = SYSTICK_TRIGGER; // Trigger SysTick Interrupt
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


