#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)

// Translate guest virtual address to guest physical address
uint64_t virt_to_phys(uintptr_t va) {
    int pagemap = open("/proc/self/pagemap", O_RDONLY);
    if (pagemap < 0) {
        perror("open pagemap");
        return 0;
    }

    uint64_t offset = (va / PAGE_SIZE) * sizeof(uint64_t);
    if (lseek(pagemap, offset, SEEK_SET) == -1) {
        perror("lseek");
        close(pagemap);
        return 0;
    }

    uint64_t entry;
    if (read(pagemap, &entry, sizeof(entry)) != sizeof(entry)) {
        perror("read");
        close(pagemap);
        return 0;
    }
    close(pagemap);

    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "Page not present\n");
        return 0;
    }

    uint64_t pfn = entry & ((1ULL << 55) - 1);
    uint64_t phys = (pfn << PAGE_SHIFT) | (va & (PAGE_SIZE - 1));
    return phys;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <host_ip>\n", argv[0]);
        return 1;
    }

    const char *host_ip = argv[1];

    // mmap one page of anonymous memory, read/write
    void *addr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("Allocated mmaped memory at VA: %p\n", addr);

    // Write something to memory to ensure page is allocated
    strcpy(addr, "Hello from guest mmap!");

    // Translate mmaped VA to GPA
    uint64_t gpa = virt_to_phys((uintptr_t)addr);
    if (gpa == 0) {
        fprintf(stderr, "Failed to translate VA to GPA\n");
        munmap(addr, PAGE_SIZE);
        return 1;
    }
    printf("Guest Physical Address (GPA) for mmaped memory: 0x%llx\n", gpa);

    // TCP connect/send/receive same as before
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); munmap(addr, PAGE_SIZE); return 1; }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(12345);
    if (inet_pton(AF_INET, host_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        munmap(addr, PAGE_SIZE);
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        munmap(addr, PAGE_SIZE);
        close(sockfd);
        return 1;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llx\n", gpa);
    if (write(sockfd, buf, strlen(buf)) < 0) {
        perror("write");
        munmap(addr, PAGE_SIZE);
        close(sockfd);
        return 1;
    }

    ssize_t n = read(sockfd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        perror("read");
        munmap(addr, PAGE_SIZE);
        close(sockfd);
        return 1;
    }
    buf[n] = '\0';

    printf("Host Physical Address (HPA): %s\n", buf);

    munmap(addr, PAGE_SIZE);
    close(sockfd);
    return 0;
}
