#include "../include/kernel.h"

#define VIDEO_MEMORY			0xB8000

unsigned char* VideoRam = (unsigned char*)VIDEO_MEMORY;

void kmain(MultiBoot* mb)
{
	VideoRam[0] = 'C';
	VideoRam[2] = 'E';
	asm volatile("cli; hlt");
}
