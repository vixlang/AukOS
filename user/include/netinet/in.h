#ifndef AUKOS_NETINET_IN_H
#define AUKOS_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#define INADDR_ANY ((in_addr_t)0u)

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    unsigned short sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    unsigned short sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

#endif
