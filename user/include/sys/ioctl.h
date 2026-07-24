#ifndef AUKOS_SYS_IOCTL_H
#define AUKOS_SYS_IOCTL_H

#define TIOCGWINSZ 0x5413
#define TIOCNOTTY 0x5422
#define BLKGETSIZE64 0x80081272
#define BLKROSET 0x125d

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, unsigned long request, ...);

#endif
