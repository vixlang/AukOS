#ifndef AUKOS_TERMIOS_H
#define AUKOS_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCGETS 0x5401u
#define TCSETS 0x5402u

#define ICANON 0x0002u
#define ECHO 0x0008u
#define ISIG 0x0001u
#define ECHOE 0x0010u
#define ECHOK 0x0020u
#define ECHOCTL 0x0200u
#define ECHOKE 0x0400u
#define IEXTEN 0x8000u

#define ICRNL 0x0100u
#define IUTF8 0x4000u
#define IXANY 0x0800u

#define OPOST 0x0001u
#define ONLCR 0x0004u

#define CS8 0x0030u
#define CREAD 0x0080u

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
};

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);

#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

int cfsetspeed(struct termios *termios_p, speed_t speed);
void cfmakeraw(struct termios *termios_p);

#endif
