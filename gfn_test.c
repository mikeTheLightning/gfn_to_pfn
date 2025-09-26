// gfn_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PROC_PATH "/proc/gfn_to_pfn"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <gfn_hex> [vm_pid]\n", argv[0]);
        return 1;
    }

    char query[128];
    if (argc == 2) {
        snprintf(query, sizeof(query), "%s\n", argv[1]);
    } else {
        snprintf(query, sizeof(query), "%s %s\n", argv[1], argv[2]);
    }

    int fd = open(PROC_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    ssize_t w = write(fd, query, strlen(query));
    if (w < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    char buf[512];
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    if (r < 0) {
        perror("read");
        close(fd);
        return 1;
    }

    buf[r] = '\0';
    printf("Kernel reply: %s", buf);

    close(fd);
    return 0;
}

