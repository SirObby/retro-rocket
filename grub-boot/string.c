#include "string.h"

unsigned int strlen(const char* str)
{
	int len = 0;
	for(; *str; ++str)
		len++;
	return len;
}

