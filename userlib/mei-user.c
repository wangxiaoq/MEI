#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define MEI_MAGIC 'M'
#define SET_READ_PADDR _IOW(MEI_MAGIC, 0, unsigned long)

#define FILENAME "/dev/mei"

#define FATAL(s) do { \
	fprintf(stderr, "%s\n", s); \
	exit(-1); \
} while(0)

char read_byte_from_addr(unsigned long addr)
{
	char value;
	int fd = open(FILENAME, O_RDWR);
	if (fd < 0)
		FATAL("MEI: open device error");

	ioctl(fd, SET_READ_PADDR, addr);
	if (read(fd, &value, 1) < 0)
		FATAL("read error");

	return value;
}
