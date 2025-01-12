#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

unsigned long get_gfn_with_offset(void* addr) {
    unsigned long pfn;
    int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (pagemap_fd < 0) {
        perror("Failed to open pagemap");
        exit(1);
    }

    unsigned long index = ((unsigned long)addr >> 12) * 8;
    if (lseek(pagemap_fd, index, SEEK_SET) < 0) {
        perror("Failed to seek in pagemap");
        close(pagemap_fd);
        exit(1);
    }

    unsigned long pagemap_entry;
    if (read(pagemap_fd, &pagemap_entry, sizeof(pagemap_entry)) != sizeof(pagemap_entry)) {
        perror("Failed to read pagemap");
        close(pagemap_fd);
        exit(1);
    }
    close(pagemap_fd);

    pfn = (pagemap_entry & ((1ULL << 55) - 1));
    return pfn;
}

int main(int argc, char *argv[]) {
    unsigned char test_value = 24;  // random default val (one of my favorites...)

    if (argc > 1) {
        test_value = (unsigned char)atoi(argv[1]);
    }

    void* buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 4096B for one page
    if (buffer == MAP_FAILED) { 
        perror("mmap failed");
        return 1;
    }

    // def is to write to offset 128 (0x80) into the page
    unsigned char* write_addr = (unsigned char*)buffer + 128;
    *write_addr = test_value;

    unsigned long base_gfn = get_gfn_with_offset(write_addr);
    // full guest physical address by bit-or-ing the offset (GFN << 12 | offset)
    unsigned long full_addr = (base_gfn << 12) | ((unsigned long)write_addr & 0xFFF);

    printf("Virtual address: 0x%lx\n", (unsigned long)write_addr);
    printf("Base guest frame: 0x%lx\n", base_gfn);
    printf("Full guest address: 0x%lx\n", full_addr);
    printf("echo \"0x%lx\" > /proc/gfn_to_pfn\n", full_addr);

    printf("\nPress Enter to exit...\n");
    getchar(); // pause to preserve the virtual address space.

    munmap(buffer, 4096);
    return 0;
}
