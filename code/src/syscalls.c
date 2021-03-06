#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "syscalls.h"
#include "console.h"

/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */
int _write(int file, char *ptr, int len)
{
	int i;

	if (file == STDOUT_FILENO || file == STDERR_FILENO) {
		for (i = 0; i < len; i++) {
			if (ptr[i] == '\n') {
			  console_send_blocking('\r');
			}
			console_send_blocking(ptr[i]);
		}
		return i;
	}
	errno = EIO;
	return -1;
}
