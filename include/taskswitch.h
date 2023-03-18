#ifndef __TASKSWITCH_H__
#define __TASKSWITCH_H__

#define PROC_RUNNING	0
#define PROC_IDLE	1
#define PROC_DELETE	2

struct process {
	/*Identification */
	uint32_t			pid;	/*PROCESS ID */
	uint32_t			ppid;	/*Parent PID */
	uint32_t			uid;	/*User id - Future use */
	uint32_t			gid;	/*Group id - Future use */
	uint32_t			state;	/*Running state */
	uint32_t			start_time;

	uint32_t			waitpid;	/* PID we are waiting on compltion of */

	uint8_t			cpu;

	char*			directory;
	char*			name;
	uint32_t			size;
	unsigned char*		text;

	struct console*		cons;

	struct ubasic_ctx*	code;	/* uBASIC context */

	/*Process Management */
	struct process*		prev;	/* Prev Process */
	struct process*		next;	/* Next Process */
};

struct process* proc_load(const char* fullpath, struct console* cons);
struct process* proc_find(uint32_t pid);
struct process* proc_cur();
void proc_wait(struct process* proc, uint32_t otherpid);
void proc_run(struct process* proc);
int proc_ended(struct process* proc);
void proc_kill(struct process* proc);
void proc_show_list();
void proc_loop();
void proc_timer();
int64_t proc_total();
const char* proc_name(int64_t index);
uint32_t proc_id(int64_t index);

#endif