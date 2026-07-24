#ifndef AUKOS_SYS_SOCKET_H
#define AUKOS_SYS_SOCKET_H

#include <stddef.h>
#include <sys/types.h>

#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2

typedef unsigned int socklen_t;

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int shutdown(int sockfd, int how);

#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#endif
