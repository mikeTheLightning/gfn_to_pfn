#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

#define PORT 12345
#define PROC_PATH "/proc/gfn_to_pfn"

void read_dmesg_for_hpa(char *hpa_buf, size_t size, const char *gpa_str) {
    if (!hpa_buf || !gpa_str || size == 0) {
        fprintf(stderr, "Invalid arguments\n");
        return;
    }

    FILE *fp = popen("sudo dmesg | tail -n 50", "r");
    if (!fp) {
        perror("popen failed");
        return;
    }

    char line[512];
    char cleaned_gpa[64];

    // Clean newline characters from gpa_str
    strncpy(cleaned_gpa, gpa_str, sizeof(cleaned_gpa) - 1);
    cleaned_gpa[sizeof(cleaned_gpa) - 1] = '\0';
    char *newline = strpbrk(cleaned_gpa, "\r\n");
    if (newline) *newline = '\0';

    char match_prefix[128];
    snprintf(match_prefix, sizeof(match_prefix), "exact phys addr for gpa %s:", cleaned_gpa);

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, match_prefix)) {
            char *colon = strchr(line, ':');
            if (colon) {
                while (*colon == ':' || *colon == ' ') colon++;
                strncpy(hpa_buf, colon, size - 1);
                hpa_buf[size - 1] = '\0';
                // Strip trailing newline
                char *nl = strchr(hpa_buf, '\n');
                if (nl) *nl = '\0';
                break;
            }
        }
    }

    pclose(fp);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Host server listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        char buffer[64] = {0};
        ssize_t valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            close(new_socket);
            continue;
        }
        buffer[valread] = '\0';

        printf("Received GPA from guest: %s", buffer);

        // Write GPA to /proc/gfn_to_pfn
        int f = open(PROC_PATH, O_WRONLY);
        if (f < 0) {
            perror("open /proc/gfn_to_pfn");
            const char *err = "Error: cannot open /proc/gfn_to_pfn\n";
            write(new_socket, err, strlen(err));
            close(new_socket);
            continue;
        }
        if (write(f, buffer, strlen(buffer)) < 0) {
            perror("write /proc/gfn_to_pfn");
            const char *err = "Error: write failed\n";
            write(new_socket, err, strlen(err));
            close(f);
            close(new_socket);
            continue;
        }
        close(f);

        // Read dmesg for HPA info
        char hpa_msg[256] = {0};
        read_dmesg_for_hpa(hpa_msg, sizeof(hpa_msg), buffer);
        printf("HPA: %s\n", hpa_msg);

        // Send HPA back to guest
        write(new_socket, hpa_msg, strlen(hpa_msg));

        close(new_socket);
    }

    close(server_fd);
    return 0;
}

