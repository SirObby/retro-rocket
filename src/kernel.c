#include <kernel.h>
#include <limine-requests.h>
#include <initialisation-functions.h>

void kmain()
{
	init();
	
	if (!filesystem_mount("/", "cd0", "iso9660")) {
		preboot_fail("Failed to mount boot drive to VFS!");
	}
	filesystem_mount("/devices", NULL, "devfs");
	const char* rd = init_ramdisk_from_storage("hd0");
	if (rd) {
		filesystem_mount("/harddisk", rd, "fat32");
	}

	init_debug();
	init_rtl8139();

	if (fs_delete_file("/harddisk/test2.txt")) {
		dprintf("It worked!\n");
	}

	init_process();
}
