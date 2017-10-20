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
unsigned int ticks = 50000000;

static int NEXT_TID;
static unsigned char null_task_stack[60];  // is not used, null task uses original system stack
static void InitSystem(void);
static void NullTask(void);
void EdgeCounter_Init(void);
void SysTick_Init(void);
void OS_Sem_Init(unsigned int *sem, unsigned int count);
void OS_Sem_Signal(unsigned int *sem);
void OS_Sem_Wait(unsigned int *sem);
void OS_Suspend(void);
void PS_Calcs(void);

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

/* Create a new process and link it to the task list
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


/* Always runnable task. This has the tid of zero
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
	 CURRENT_TASK->sp = the_sp;
	 CURRENT_TASK->state = T_READY;
	 
	 //
	 // AMW
	 //
	 task_state[CURRENT_TASK->tid] = T_READY;
	 
	 CURRENT_TASK = CURRENT_TASK->next;
	 if (CURRENT_TASK->state == T_READY){
		  CURRENT_TASK->state = T_RUNNING;
			task_state[CURRENT_TASK->tid] = T_RUNNING;
	    sp = CURRENT_TASK->sp;
			CURRENT_TASK->clk_ticks = 0;
	 } else {     /* task->state == T_CREATED so make it "ready" 
	                give it an interrupt frame and then launch it 
	    		        (with a blr sith 0xfffffff9 in LR in StartNewTask())  */
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
			percent_cpu[CURRENT_TASK->tid] = ((100*task_ticks[CURRENT_TASK->tid]/total_num_ticks)); /// ((float)total_num_ticks); // / (float)total_num_ticks;
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
	/*
			CRITICAL SECTION OF CODE
	*/
	*sem = *sem + 1;
	/*
			CRITICAL SECTION OF CODE
	*/
	EnableInterrupts();
}

void OS_Sem_Wait(unsigned int *sem){
	DisableInterrupts();
	while( *sem == 0 ){
		EnableInterrupts(); // allow scheduler a chance to be called
		//OS_Suspend();
		DisableInterrupts(); // turn off interrupts when we go to exit the loop
			// It's a block party!!!
			// (Process is blocked)
	}
	/*
			CRITICAL SECTION OF CODE
	*/
	*sem = *sem - 1;	
	/*
			CRITICAL SECTION OF CODE
	*/	
	EnableInterrupts();
}
