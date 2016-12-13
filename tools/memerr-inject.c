#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>

#define PAGE_SIZE 4096

#define BYTESIZE 8

#define FILENAME "/dev/mei"

#define MEI_MAGIC 'M'
#define SET_READ_PADDR _IOW(MEI_MAGIC, 0, unsigned long)

#define FATAL(s) { \
	fprintf(stderr, "%s\n", s); \
	exit(-1); \
}

struct list_head {
	struct list_head *next, *prev;
};

struct inject_memory_err {
	unsigned long phy_addr; /* where the error to inject in physical address */
	int err_bit_num; /* number of error bits in a byte */
	int bit[BYTESIZE]; /* which bit in the byte have error */
	int bit_value[BYTESIZE]; /* the bits stuck-at error values */
	struct list_head lists;
};

void inject_err(struct inject_memory_err *inj_err)
{
	int fd = open(FILENAME, O_RDWR);
	if (fd < 0)
		FATAL("open error");
	
	if (write(fd, inj_err, sizeof(*inj_err)) < 0)
		FATAL("write error");

	close(fd);
}

static void usage(void)
{
	fprintf(stderr, "Usage: ./memerr-inject inject-file-name\n");
	exit(0);
}

static void parse_arguments_and_inject(char *filename)
{
	int ret;
	struct inject_memory_err inj_err;

	FILE *fp = fopen(filename, "r");
	if (!fp)
		FATAL("open file error");

	do {
		ret = fscanf(fp, "%lu %d  %d %d %d %d %d %d %d %d  %d %d %d %d %d %d %d %d", &(inj_err.phy_addr),
			&(inj_err.err_bit_num), &(inj_err.bit[0]), &(inj_err.bit[1]), &(inj_err.bit[2]), &(inj_err.bit[3]),
			&(inj_err.bit[4]), &(inj_err.bit[5]), &(inj_err.bit[6]), &(inj_err.bit[7]),
			&(inj_err.bit_value[0]), &(inj_err.bit_value[1]), &(inj_err.bit_value[2]), &(inj_err.bit_value[3]),
			&(inj_err.bit_value[4]), &(inj_err.bit_value[5]), &(inj_err.bit_value[6]), &(inj_err.bit_value[7]));
			inject_err(&inj_err);
	} while (ret != EOF);
}


int main(int argc, char *argv[])
{
	if (argc < 2)
		usage();

	parse_arguments_and_inject(argv[1]);

	return 0;
}
