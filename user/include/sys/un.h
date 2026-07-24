#ifndef AUKOS_SYS_UN_H
#define AUKOS_SYS_UN_H

#include <sys/socket.h>

struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

#endif
