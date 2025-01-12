#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <physical_address>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned long physical_address = strtoul(argv[1], NULL, 0);

    int fd = open("/dev/mem", O_RDONLY);
    if (fd == -1) {
        perror("Failed to open /dev/mem");
        return EXIT_FAILURE;
    }

    if (lseek(fd, physical_address, SEEK_SET) == (off_t)-1) {
        perror("Failed to seek to physical address");
        close(fd);
        return EXIT_FAILURE;
    }

    unsigned char value;
    if (read(fd, &value, sizeof(value)) != sizeof(value)) {
        perror("Failed to read value at physical address");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("%u\n", value);

    close(fd);
    return 0;
}
