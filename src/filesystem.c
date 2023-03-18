#include <kernel.h>
#include <filesystem.h>

FS_FileSystem* filesystems, *dummyfs;
FS_StorageDevice* storagedevices = NULL;
FS_Tree* fs_tree;
static uint32_t fd_last = 0;
static uint32_t fd_alloc = 0;
FS_Handle* filehandles[FD_MAX] = { NULL };

int register_filesystem(FS_FileSystem* newfs)
{
	/* Add the new filesystem to the start of the list */
	newfs->next = filesystems;
	filesystems = newfs;
	return 1;
}

FS_FileSystem* find_filesystem(const char* name)
{
	FS_FileSystem* cur = filesystems;
	for(; cur; cur = cur->next) {
		if (!strcmp(name, cur->name)) {
			return cur;
		}
	}
	return NULL;
}


int register_storage_device(FS_StorageDevice* newdev)
{
	/* Add the new storage device to the start of the list */
	newdev->next = storagedevices;
	storagedevices = newdev;
	return 1;
}

FS_StorageDevice* find_storage_device(const char* name)
{
	FS_StorageDevice* cur = storagedevices;
	for(; cur; cur = cur->next) {
		if (!strcmp(name, cur->name)) {
			return cur;
		}
	}
	return NULL;
}

int read_storage_device(const char* name, uint64_t start_block, uint32_t bytes, unsigned char* data)
{
	FS_StorageDevice* cur = find_storage_device(name);
	if (!cur || !cur->blockread) {
		return 0;
	}
	return cur->blockread(cur, start_block, bytes, data);
}

int write_storage_device(const char* name, uint64_t start_block, uint32_t bytes, const unsigned char* data)
{
	FS_StorageDevice* cur = find_storage_device(name);
	if (!cur || !cur->blockwrite) {
		return 0;
	}

	return cur->blockwrite(cur, start_block, bytes, data);
}

/* Allocate a new file descriptor and attach it to 'file' */
int alloc_filehandle(FS_HandleType type, FS_DirectoryEntry* file, uint32_t ibufsz, uint32_t obufsz)
{
	/* Check we havent used up all available fds */
	if (fd_alloc >= FD_MAX)
		return -1;

	uint32_t iter = 0;
	while (iter++ < FD_MAX)
	{
		/* Found an empty slot */
		if (filehandles[fd_last] == NULL)
		{
			/* Initialise FS_Handle struct */
			filehandles[fd_last] = (FS_Handle*)kmalloc(sizeof(FS_Handle));
			filehandles[fd_last]->type = type;
			filehandles[fd_last]->file = file;
			filehandles[fd_last]->seekpos = 0;

			filehandles[fd_last]->inbufsize = ibufsz;
			filehandles[fd_last]->outbufsize = obufsz;
			if (ibufsz)
				filehandles[fd_last]->inbuf = (unsigned char*)kmalloc(ibufsz);
			else
				filehandles[fd_last]->inbuf = NULL;
			if (obufsz)
				filehandles[fd_last]->outbuf = (unsigned char*)kmalloc(obufsz);
			else
				filehandles[fd_last]->outbuf = NULL;

			fd_alloc++;
			fd_last++;
			return fd_last - 1;
		}
		else
		{
			fd_last++;

			/* Reached the end of the list? loop back around */
			if (fd_last >= FD_MAX)
				fd_last = 0;
		}
	}
	return -1;
}

/* Free a file descriptor */
uint32_t destroy_filehandle(uint32_t descriptor)
{
	/* Sanity checks */
	if (descriptor >= FD_MAX || filehandles[descriptor] == NULL)
		return 0;
	else
	{
		if (filehandles[descriptor]->inbuf)
			kfree(filehandles[descriptor]->inbuf);
		if (filehandles[descriptor]->outbuf)
			kfree(filehandles[descriptor]->outbuf);
		kfree(filehandles[descriptor]);
		filehandles[descriptor] = NULL;
		/* Make the search faster for the next process to ask
		 * for an fd, and try to keep some semblence of POSIX
		 * compliance by making this fd immediately available
		 * as the next fd if it is lower than what we're handing
		 * out next.
		 */
		if (descriptor < fd_last)
			fd_last = descriptor;
		fd_alloc--;
	}
	return 1;
}

/* Open a file for access */
int _open(const char* filename, int oflag)
{
	/* First check if we can find the file in the filesystem */
	FS_DirectoryEntry* file = fs_get_file_info(filename);
	if (file == NULL)
		return -1;

	/* Allocate a file handle. NOTE, currently restricted to
	 * read-only file access by hard-coding the param here.
	 */
	int fd = alloc_filehandle(file_input, file, IOBUFSZ, 0);
	if (fd == -1)
		return -1;

	filehandles[fd]->cached = 0;
	/* Read an initial buffer into the structure up to fd->inbufsize in size */
	if (!fs_read_file(filehandles[fd]->file, filehandles[fd]->seekpos,
			file->size <= filehandles[fd]->inbufsize ? file->size : filehandles[fd]->inbufsize,
			filehandles[fd]->inbuf))
	{
		/* If we couldnt get the initial buffer, there is something wrong.
		 * Give up the filehandle and return error.
		 */
		destroy_filehandle(fd);
		return -1;
	}
	else
	{
		if (file->size <= filehandles[fd]->inbufsize)
			filehandles[fd]->cached = 1;
	}

	//kprintf("cached=%d\n", filehandles[fd]->cached);

	/* Return the allocated file descriptor */
	return fd;
}

/* Write out buffered data to a file open for writing */
void flush_filehandle(uint32_t descriptor)
{
	/* Until we have writeable filesystems this is a stub */
}

/* Close an open file descriptor */
int _close(uint32_t descriptor)
{
	/* Sanity checks */
	if (descriptor >= FD_MAX || filehandles[descriptor] == NULL)
		return -1;

	/* Flush any files that arent readonly */
	if (filehandles[descriptor]->type != file_input)
		flush_filehandle(descriptor);

	return destroy_filehandle(descriptor) ? 0 : -1;
}

long _lseek(int fd, long offset, int origin)
{
	//kprintf("LSEEK ");
	if (fd < 0 || fd >= FD_MAX || filehandles[fd] == NULL)
	{
		//kprintf("Bad FD %d\n", fd);
		return -1;
	}
	else
	{
		//kprintf("1 ");
		if (offset + origin > filehandles[fd]->file->size)
		{
			//kprintf("offset + origin > %d\n", filehandles[fd]->file->size);
			return -1;
		}
		else
		{
			//kprintf("2 ");
			filehandles[fd]->seekpos = offset + origin;
			/* Flush output before seeking */
			flush_filehandle(fd);
			/* Refresh input buffer */
			if (!fs_read_file(filehandles[fd]->file, filehandles[fd]->seekpos, filehandles[fd]->inbufsize, filehandles[fd]->inbuf))
			{
				//kprintf("Read buffer failure");
				return -1;
			}
			else
			{
				//kprintf("3 %d ", filehandles[fd]->seekpos);
				return filehandles[fd]->seekpos;
			}
		}
	}
	//kprintf("E ");
	return -1;
}

long _tell(int fd)
{
	if (fd < 0 || fd >= FD_MAX || filehandles[fd] == NULL)
		return -1;
	else
		return filehandles[fd]->seekpos;
}

/* Read bytes from an open file */
int _read(int fd, void *buffer, unsigned int count)
{
	/* Sanity checks */
	if (fd < 0 || fd >= FD_MAX || filehandles[fd] == NULL)
		return -1;

	if (filehandles[fd]->seekpos >= filehandles[fd]->file->size)
		return 0;

	/* Check that the size of the request and current position
	 * don't place any part of the buffer past the bounds of the file
	 */
	if ((filehandles[fd]->seekpos + count) > filehandles[fd]->file->size)
	{
		/* Request too large, truncate it to EOF */
		count = filehandles[fd]->file->size - filehandles[fd]->seekpos;
	}

	if (((filehandles[fd]->seekpos % filehandles[fd]->inbufsize) + count) > filehandles[fd]->inbufsize)
	{
		/* The requested buffer size is too large to read in one operation.
		 * Continually read into the input buffer in filehandles[fd]->inbufsize
		 * chunks maximum until all data is read.
		 */
		int readbytes = 0;
		while (count > 0)
		{
			int rb;
			if (count > filehandles[fd]->inbufsize)
				rb = filehandles[fd]->inbufsize;
			else
				rb = count;

			if (!fs_read_file(filehandles[fd]->file, filehandles[fd]->seekpos, rb, filehandles[fd]->inbuf))
				return -1;

			memcpy(((unsigned char*)buffer) + readbytes, filehandles[fd]->inbuf, rb);

			filehandles[fd]->seekpos += rb;
			readbytes += rb;
			count -= rb;
		}
		count = readbytes;


	}
	else
	{
		/* we can do the entire read from only the current IO buffer */

		/* Read the entire lot in one go */
		if (filehandles[fd]->cached == 0)
		{
			if (!fs_read_file(filehandles[fd]->file, filehandles[fd]->seekpos, count, filehandles[fd]->inbuf))
				return -1;
			memcpy(buffer, filehandles[fd]->inbuf, count);
		}
		else
		{
			memcpy(buffer, filehandles[fd]->inbuf + filehandles[fd]->seekpos, count);
		}

		filehandles[fd]->seekpos += count;
	}

	return count;
}

int _eof(int fd)
{
	if (fd < 0 || fd >= FD_MAX || filehandles[fd] == NULL)
		return -1;

	return (filehandles[fd]->seekpos >= filehandles[fd]->file->size);
}

void retrieve_node_from_driver(FS_Tree* node)
{
	/* XXX: Check there isnt already content in node->files, if there is,
	 * delete the old content first to avoid a memleak.
	 */

	//kprintf("retrieve_node_from_driver\n");
	if (node == NULL)
		return;

	FS_FileSystem* driver = (FS_FileSystem*)node->responsible_driver;

	if (driver == dummyfs)
		return;

	//kprintf("drv=%08x getdir=%08x\n", driver, driver->getdir);
	if (driver == NULL || driver->getdir == NULL)
	{
		/* Driver does not implement getdir() */
		kprintf("*** BUG *** Driver %08x on node '%s' is null or does not support getdir()! (getdir=%08x)\n", driver, node->name, driver->getdir);
		return;
	}

	//kprintf("call getdir, node->lbapos=%d node->name=%s\n", node->lbapos, node->name);

	node->files = driver->getdir(node);
	node->dirty = 0;
	node->child_dirs = NULL;

	if (node->files == NULL)
	{
		node->parent = node;
		return;
	}
	/* XXX: For directories that already contain items,
	 * enumerate the list of directories, take note of the
	 * responsible driver for this directory, and reuse these
	 * responsible driver pointers on the new entries if they
	 * crop up again. If we don't do this, then we will claim
	 * ownership of another filesystems mountpoint!
	 */
	FS_DirectoryEntry* x = (FS_DirectoryEntry*)node->files;
	for (; x; x = x->next)
	{
		//kprintf("Parse entry '%s'@%08x\n", x->filename, x);
		if (x->flags & FS_DIRECTORY)
		{
			/* Insert a new child directory into node->child_dirs,
			 * Make each dir empty and 'dirty' and get its opaque
			 * from the parent dir
			 */
			FS_Tree* newnode = (FS_Tree*)kmalloc(sizeof(FS_Tree));

			newnode->name = x->filename;
			newnode->lbapos = x->lbapos;
			newnode->size = x->size;
			newnode->device = x->device;
			strlcpy(newnode->device_name, x->device_name, 16);
			newnode->opaque = node->opaque;
			newnode->dirty = 1;
			newnode->parent = node;
			newnode->responsible_driver = node->responsible_driver;

			newnode->next = node->child_dirs;
			node->child_dirs = newnode;
		}
	}
}

typedef struct DirStack_t
{
	char* name;
	struct DirStack_t* next;
} DirStack;

FS_Tree* walk_to_node_internal(FS_Tree* current_node, DirStack* dir_stack)
{
	if (current_node != NULL && current_node->dirty != 0) {
		retrieve_node_from_driver(current_node);
	}
	
	if (current_node != NULL && dir_stack && current_node->name != NULL && dir_stack->name != NULL && !strcmp(current_node->name, dir_stack->name)) {
		dir_stack = dir_stack->next;
		if (!dir_stack) {
			return current_node;
		}
	}

	if (current_node != NULL) {
		FS_Tree* dirs = current_node->child_dirs;
		for (; dirs; dirs = dirs->next) {
			FS_Tree* result = walk_to_node_internal(dirs, dir_stack);
			if (result != NULL)
				return result;
		}
	}

//kprintf("walk to node internal 8\n");
	return NULL;
}

uint8_t verify_path(const char* path)
{
	/* Valid paths must start with a / and not end with a / */
	/* The filesystem internals all work with fully qualified
	 * pathnames. Client processes must represent 'current'
	 * directories to the user if they are required.
	 */
	uint32_t pathlen = strlen(path);
	return ((*path == '/' && *(path + pathlen - 1) != '/') || !strcmp(path, "/"));
}

FS_Tree* walk_to_node(FS_Tree* current_node, const char* path)
{
//kprintf("walk to node 0\n");
	if (!verify_path(path))
		return NULL;
//kprintf("walk to node 01\n");
	if (!strcmp(path, "/"))
		return fs_tree;
//kprintf("walk to node 012\n");
	/* First build the dir stack */
	DirStack* ds = (DirStack*)kmalloc(sizeof(DirStack));
	DirStack* walk = ds;
	char* copy = strdup(path);
	char* parse;
	char* last = copy + 1;

	for (parse = copy + 1; *parse; ++parse)
	{
		if (*parse == '/')
		{
			*parse = 0;
			walk->name = strdup(last);
			last = parse + 1;

                        DirStack* next = (DirStack*)kmalloc(sizeof(DirStack));
			walk->next = next;
			next->next = 0;
			next->name = NULL;
			walk = next;
		}
	}
	walk->next = NULL;
	walk->name = strdup(last);
//kprintf("walk to node 199\n");
	FS_Tree* result = walk_to_node_internal(current_node, ds);
	for(; ds; ds = ds->next)
	{
		kfree(ds->name);
		kfree(ds);
	}
	return result;
}

FS_DirectoryEntry* find_file_in_dir(FS_Tree* directory, const char* filename)
{
	if (!directory)
		return NULL;

	//kprintf("find_file_in_dir %s %d", directory->name, directory->lbapos);

	FS_DirectoryEntry* entry = (FS_DirectoryEntry*)directory->files;
	for (; entry; entry = entry->next)
	{
		/* Don't find directories, only files */
		if (((entry->flags & FS_DIRECTORY) == 0) && (!strcmp(filename, entry->filename)))
			return entry;
	}
	return NULL;
}

int fs_read_file(FS_DirectoryEntry* file, uint32_t start, uint32_t length, unsigned char* buffer)
{
	FS_FileSystem* fs = (FS_FileSystem*)file->directory->responsible_driver;
	if (fs)
		return fs->readfile(file, start, length, buffer);
	else
		return 0;
}

FS_DirectoryEntry* fs_get_file_info(const char* pathandfile)
{
	if (!verify_path(pathandfile))
		return NULL;

	/* First, split the path and file components */
	uint32_t namelen = strlen(pathandfile);
	char* pathinfo = strdup(pathandfile);
	char* filename = NULL;
	char* pathname = NULL;
	char* ptr;
	for (ptr = pathinfo + namelen; ptr >= pathinfo; --ptr)
	{
		if (*ptr == '/')
		{
			*ptr = 0;
			filename = strdup(ptr + 1);
			pathname = strdup(pathinfo);
			break;
		}
	}
	kfree(pathinfo);

	if (!filename || !pathname || !*filename)
	{
//		kprintf("fs_get_file_info: Malformed pathname '%s'\n", pathandfile);
		return NULL;
	}
	if (*pathname == 0)
	{
		/* A file located on the root directory -- special case */
		kfree(pathname);
		pathname = strdup("/");
	}
	FS_Tree* directory = walk_to_node(fs_tree, pathname);
	if (!directory)
	{
//		kprintf("fs_get_file_info: No such path '%s'\n", pathname);
		return NULL;
	}
	FS_DirectoryEntry* fileinfo = find_file_in_dir(directory, filename);
	if (!fileinfo)
	{
//		kprintf("fs_get_file_info: No such file '%s' in dir '%s'\n", filename, pathname);
		return NULL;
	}
	return fileinfo;
}

int attach_filesystem(const char* virtual_path, FS_FileSystem* fs, void* opaque)
{
	FS_Tree* item = walk_to_node(fs_tree, virtual_path);
	if (item) {
		item->responsible_driver = (void*)fs;
		item->opaque = opaque;
		item->dirty = 1;
		item->files = NULL;
		item->child_dirs = NULL;
		item->lbapos = 0; // special value always refers to root dir
		retrieve_node_from_driver(item);
	} else {
		return 0;
	}
	return 1;
}

void init_filesystem()
{
	filesystems = (FS_FileSystem*)kmalloc(sizeof(FS_FileSystem));
	fs_tree = (FS_Tree*)kmalloc(sizeof(FS_Tree));

	strlcpy(filesystems->name, "DummyFS", 31);
	filesystems->mount = NULL;
	filesystems->getdir = NULL;
	filesystems->readfile = NULL;
	filesystems->writefile = NULL;
	filesystems->rm = NULL;
	filesystems->next = NULL;

	dummyfs = filesystems;

	fs_tree->name = NULL;
	fs_tree->parent = fs_tree;
	fs_tree->child_dirs = NULL;
	fs_tree->files = NULL;
	fs_tree->dirty = 0;
	fs_tree->opaque = NULL;

	attach_filesystem("/", filesystems, NULL);
}

FS_DirectoryEntry* fs_get_items(const char* pathname)
{
	FS_Tree* item = walk_to_node(fs_tree, pathname);
	return (FS_DirectoryEntry*)(item ? item->files : NULL);
}

int filesystem_mount(const char* pathname, const char* device, const char* filesystem_driver)
{
	FS_FileSystem *driver = find_filesystem(filesystem_driver);
	int success = (driver && driver->mount(device, pathname));
	kprintf(
		"%s %s to %s%s%s\n",
		success ? "Mounted" : "Failed to mount",
		pathname,
		device ? device : "",
		device ? " as " : "",
		filesystem_driver
	);
	return success;

}