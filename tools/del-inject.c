#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MODNAME "/dev/mei"
#define MEI_MAGIC 'M'
#define DEL_INJ_ERR _IOW(MEI_MAGIC, 1, unsigned long)
#define FATAL(s) do { \
	fprintf(stderr, "%s\n", s); \
	exit(-1); \
} while(0)

int main(int argc, char *argv[])
{
	int fd;
	unsigned long paddr;
	const char *addr;

	if (argc < 2) {
		fprintf(stderr, "Usage: ./del-inject physical_address\n");
		exit(-1);
	}

	addr = (const char *)argv[1];
	paddr = strtoul(addr, NULL, 10);

	fd = open(MODNAME, O_RDWR);
	if (fd < 0)
		FATAL("open device file error");

	ioctl(fd, DEL_INJ_ERR, paddr);
	return 0;
}
