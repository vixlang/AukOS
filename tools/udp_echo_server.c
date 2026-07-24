#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct sockaddr_in address;
    struct sockaddr_in peer;
    socklen_t peer_length = sizeof(peer);
    uint8_t buffer[2048];
    long exchanges = 1;
    long port;
    int enabled = 1;
    int fd;
    ssize_t length;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: %s PORT [EXCHANGES]\n", argv[0]);
        return 2;
    }
    errno = 0;
    port = strtol(argv[1], 0, 10);
    if (errno != 0 || port <= 0 || port > 65535) {
        fprintf(stderr, "invalid UDP port\n");
        return 2;
    }
    if (argc == 3) {
        errno = 0;
        exchanges = strtol(argv[2], 0, 10);
        if (errno != 0 || exchanges <= 0) {
            fprintf(stderr, "invalid UDP exchange count\n");
            return 2;
        }
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0 || setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled,
                             sizeof(enabled)) != 0) {
        perror("udp echo socket");
        return 1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        perror("udp echo bind");
        close(fd);
        return 1;
    }
    printf("[udp_echo] READY %ld\n", port);
    fflush(stdout);
    for (long exchange = 0; exchange < exchanges; exchange++) {
        peer_length = sizeof(peer);
        length = recvfrom(fd, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&peer, &peer_length);
        if (length < 0 || sendto(fd, buffer, (size_t)length, 0,
                                 (const struct sockaddr *)&peer,
                                 peer_length) != length) {
            perror("udp echo exchange");
            close(fd);
            return 1;
        }
        printf("[udp_echo] ECHO %zd\n", length);
        fflush(stdout);
    }
    close(fd);
    return 0;
}
