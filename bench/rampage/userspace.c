#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>

#define MODNAME "/dev/phys_mem"
#define PAGESIZE 4096
#define MAXMEMORY 8071476/4

#define PHYS_MEM_IOC_REQUEST_PAGES 0x40184b00
#define PHYS_MEM_IOC_MARK_FRAME_BAD 0x40104b01

#define IOCTL_REQUEST_VERSION 1

#define SOURCE_FREE_BUDDY_PAGE 0x00002
#define SOURCE_HOTPLUG 0x00080
#define SOURCE_SHAKING 0x00100

#define FATAL(s) do { \
	fprintf(stderr, "%s\n", s); \
	exit(-1); \
} while(0)

#define MEI_MAGIC 'M'
#define SET_READ_PADDR _IOW(MEI_MAGIC, 0, unsigned long)

#define FILENAME "/dev/mei"

uint64_t get_elapsed(struct timespec *start, struct timespec *end)
{
	uint64_t dur;
	if (start->tv_nsec > end->tv_nsec)
		dur = (uint64_t)(end->tv_sec-1-start->tv_sec) * 1000000000 +
			(1000000000 + end->tv_nsec - start->tv_nsec);
	else
		dur = (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000 +
			(end->tv_nsec - start->tv_nsec);

	return dur;
}

char read_byte_from_addr(unsigned long addr)
{
        char value;
        int fd = open(FILENAME, O_RDWR);
        if (fd < 0)
                FATAL("MEI: open device error");

        ioctl(fd, SET_READ_PADDR, addr);
        if (read(fd, &value, 1) < 0)
                FATAL("read error");
	close(fd);

        return value;
}

struct phys_mem_frame_request {
	unsigned long requested_pfn;
	unsigned long allowed_sources; /* Bitmask of SOURCE_* */
};

struct phys_mem_request {
	unsigned long protocol_version; /* The protocol/struct version of this IOCTL call. Must be IOCTL_REQUEST_VERSION */
	unsigned long num_requests;     /* The number of frame requests */
	struct phys_mem_frame_request *req; /* A pointer to the array of requests. The array must contain at least num_requests items */
};

struct mark_page_poison{
	unsigned long protocol_version; /* The protocol/struct version of this IOCTL call. Must be IOCTL_REQUEST_VERSION */
	unsigned long bad_pfn;     /* The bad pfn */
};

void test(unsigned long vaddr, int size, int pfn)
{
	struct timespec start, end;
	uint64_t dur;
	char value;
	int i = 0;
	char *addr = (char*)vaddr;

	for (i = 0; i < size; i++)
		addr[i] = 0;

	for (i = 0; i < size; i++) {
//		clock_gettime(CLOCK_REALTIME, &start);
		value = read_byte_from_addr((unsigned long)&addr[i]);
//		clock_gettime(CLOCK_REALTIME, &end);
//		dur = get_elapsed(&start, &end);
//		printf("%lu\n", dur);
	//	if (read_byte_from_addr((unsigned long)&addr[i]) != 0) {
		if (value != 0) {
			printf("0-write: find page frame error at pfn %d\n", pfn);
			return;
		}
	}

	for (i = 0; i < size; i++)
		addr[i] = 0xff;

	for (i = 0; i < size; i++) {
		if (read_byte_from_addr((unsigned long)&addr[i]) != (char)0xff) {
			printf("1-write: find page frame error at pfn %d\n", pfn);
			return;
		}
	}
}

void test_cycle(int fd)
{
	int pfn;
	unsigned long addr;
	struct phys_mem_frame_request pf_req;
	struct phys_mem_request p_req;
	int ret;


	for (pfn = 0; pfn < MAXMEMORY; pfn++) {
		memset(&pf_req, 0, sizeof(pf_req));
		memset(&p_req, 0, sizeof(p_req));

		pf_req.requested_pfn = pfn;
		pf_req.allowed_sources = SOURCE_FREE_BUDDY_PAGE|SOURCE_HOTPLUG|SOURCE_SHAKING;

		p_req.protocol_version = IOCTL_REQUEST_VERSION;
		p_req.num_requests = 1;
		p_req.req = &pf_req;
		/* get the page frame */
		ret = ioctl(fd, PHYS_MEM_IOC_REQUEST_PAGES, &p_req);
		if (ret == 0xff) {
			void *av = mmap((void*)0, PAGESIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
			if (av != MAP_FAILED) {
			//	printf("in userspace addr: %p\n", av);
				addr = (unsigned long)av;
				test(addr, PAGESIZE, pfn);
				munmap(av, PAGESIZE);
			}
		} else {
			continue;
		}
	}
}

int main(void)
{
	int fd = open(MODNAME, O_RDWR);
	if (fd < 0)
		FATAL("open file "MODNAME" error");

	test_cycle(fd);

	return 0;
}
