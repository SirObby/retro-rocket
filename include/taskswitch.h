/**
 * @file taskswitch.h
 * @author Craig Edwards (craigedwards@brainbox.cc)
 * @copyright Copyright (c) 2012-2023
 */
#pragma once

#define PROC_RUNNING	0
#define PROC_IDLE	1
#define PROC_DELETE	2

typedef struct process_t {
	/*Identification */
	uint32_t		pid;	/*PROCESS ID */
	uint32_t		ppid;	/*Parent PID */
	uint32_t		uid;	/*User id - Future use */
	uint32_t		gid;	/*Group id - Future use */
	uint32_t		state;	/*Running state */
	uint32_t		start_time;

	uint32_t		waitpid;	/* PID we are waiting on compltion of */

	uint8_t			cpu;

	char*			directory;
	char*			name;
	uint32_t		size;
	unsigned char*		text;

	struct console*		cons;

	struct ubasic_ctx*	code;	/* uBASIC context */

	/*Process Management */
	struct process_t*	prev;	/* Prev Process */
	struct process_t*	next;	/* Next Process */
} process_t;

typedef struct proc_id_t {
	uint32_t id;
	process_t* proc;
} proc_id_t;

/**
 * @brief Types of idle task
 */
typedef enum idle_type_t {
	IDLE_FOREGROUND, //!< A foreground idle task that runs in the task switch loop between context switches
	IDLE_BACKGROUND, //!< A background idle task that runs via the LAPIC timer ISR
} idle_type_t;

// Timer that drivers may register to be called during the idle time
typedef void (*proc_idle_timer_t)(void);

typedef struct idle_timer {
	proc_idle_timer_t func;
	struct idle_timer* next;
} idle_timer_t;

process_t* proc_load(const char* fullpath, struct console* cons);
process_t* proc_find(uint32_t pid);
process_t* proc_cur();
void proc_wait(process_t* proc, uint32_t otherpid);
void proc_run(process_t* proc);
int proc_ended(process_t* proc);
void proc_kill(process_t* proc);
void proc_show_list();
void proc_loop();
void proc_timer();
int64_t proc_total();
const char* proc_name(int64_t index);
uint32_t proc_id(int64_t index);

/**
 * @brief Register a function to be called periodically during idle time
 * 
 * @param handler handler function, void(void)
 * @param type type of idle to register
 */
void proc_register_idle(proc_idle_timer_t handler, idle_type_t type);

void init_process();

