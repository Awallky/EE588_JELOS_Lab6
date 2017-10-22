//  JELOS Spring 2016
// James E. Lumpp
// 4/9/2016

#ifndef _JELOS_H
#define _JELOS_H

/* Number of tasks, change this to suit your need
 */
#ifndef NUM_TASKS
#define NUM_TASKS	5
#endif

#ifndef BLOCKED
#define BLOCKED	0
#endif

#ifndef MAX_TASK_PRIORITY
#define MAX_TASK_PRIORITY	0
#endif

#define NULL 0

int CreateTask(void (*func)(void), 
                    unsigned char *stack_start, 
                    unsigned stack_size);

int StartScheduler(void);
unsigned char * StartNewTask(unsigned char * x,uint32_t y);
static void InitSystem(void);
static void NullTask(void);
void EdgeCounter_Init(void);										
void PortF_Init(void);
void SysTick_Init(void);
void OS_Sem_Init(int32_t *sem, int32_t count);
void OS_Sem_Signal(int32_t *sem);
void OS_Sem_Wait(int32_t *sem);
void OS_Suspend(void);
void PS_Calcs(void);
										
void Priority_Scheduler(unsigned char * the_sp);
void Delay(uint32_t ui32Seconds);

/* Minimum stack size for a task function that does not use "any" stack space.
 * This should be the interrupt stack frame, plus some fudge factor for
 * frame pointer, a single function call, etc.
 * For ARM, the interrupt frame is 8 registers (r0-r3,r12,lr,pc,psr)
 */
#define MIN_STACK_SIZE	(512)

#ifndef INTR_ON				
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
#endif

/* 
		Task States
*/
enum { T_CREATED, T_READY, T_RUNNING };

typedef struct TaskControlBlock
	{
	struct	TaskControlBlock *next;
	unsigned char tid;			/* task id */
	unsigned char state;		/* task state */
	void	(*func)(void);		/* function to call for that task */
	unsigned char	*stack_start;		/* stack low value */
	unsigned char	*stack_end;			/* stack high value */
	unsigned char	*sp;		/* current value of the stack pointer */
	uint32_t clk_ticks;
	int32_t *blocked; /* Determines whether the task is blocked // AMW // */
	int32_t priority;
	} TaskControlBlock;
#endif
