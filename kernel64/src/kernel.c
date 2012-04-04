#include <kernel.h>
#include <hydrogen.h>
#include <debugger.h>
#include <spinlock.h>
#include <kmalloc.h>
#include <timer.h>
#include <ioapic.h>

/* 64-bit IDT is at 0x0, same position as realmode IDT */
u16 idt64[5] = {0xffff, 0x0000, 0x0000, 0x0000, 0x0000 };

extern void idt_init();

void kmain_ap()
{
	//idt_init();
	//asm volatile("lidtq (%0)\n"::"r"(idt64));
	//asm volatile("sti");
	while (1) { asm volatile("hlt"); }
}

void kmain()
{
	initconsole();
	setforeground(COLOUR_LIGHTYELLOW);
	printf("Retro-Rocket ");
	setforeground(COLOUR_WHITE);
	printf("64-bit SMP kernel booting from %s...\n", "Hydrogen");
	printf("%d processors detected, %d IOAPICs\n", hydrogen_info->proc_count, hydrogen_info->ioapic_count);

	idt_init();
	asm volatile("lidtq (%0)\n"::"r"(idt64));

	heap_init();

	asm volatile("sti");

	int in = 16;
	for (in = 0; in < 3; in++)
	{
		ioapic_redir_unmask(in);
	}
	init_timer(50);
	
	//asm volatile("int $49");

	HydrogenInfoMemory* mi = hydrogen_mmap;
	int memcnt = 0;
	while (memcnt++ < hydrogen_info->mmap_count)
	{
		kprintf("Start: %016lx Length: %016lx Avail: %d\n",mi->begin, mi->length, mi->available); 
		mi =  (HydrogenInfoMemory*)((u64)mi + (u64)sizeof(HydrogenInfoMemory));
	}

	printf("Would continue boot sequence, but brain hasnt got any further!\n");
	//wait_forever();
	while (1) { continue; }
}
