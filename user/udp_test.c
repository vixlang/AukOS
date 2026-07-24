#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define UDP_TEST_LOCAL_PORT 49152u
#define UDP_TEST_ECHO_PORT 45454u

static void write_message(const char *message)
{
    write(1, message, strlen(message));
}

static int fail(int socket_fd, const char *message)
{
    write_message("[udp_test] FAIL: ");
    write_message(message);
    write_message("\n");
    if (socket_fd >= 0) {
        close(socket_fd);
    }
    return 1;
}

int main(int argc, char **argv, char **envp)
{
    static const char payload[] = "AukOS UDP echo!";
    struct sockaddr_in local = {0};
    struct sockaddr_in destination = {0};
    struct sockaddr_in source = {0};
    socklen_t source_length = sizeof(source);
    char reply[sizeof(payload)];
    ssize_t result;
    int socket_fd;

    (void)argc;
    (void)argv;
    (void)envp;
    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
        return fail(-1, "socket");
    }

    local.sin_family = AF_INET;
    local.sin_port = htons(UDP_TEST_LOCAL_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_fd, (const struct sockaddr *)&local, sizeof(local)) != 0) {
        return fail(socket_fd, "bind");
    }

    destination.sin_family = AF_INET;
    destination.sin_port = htons(UDP_TEST_ECHO_PORT);
    destination.sin_addr.s_addr = htonl(0x0a000202u);
    result = sendto(socket_fd, payload, sizeof(payload) - 1u, 0,
                    (const struct sockaddr *)&destination,
                    sizeof(destination));
    if (result != (ssize_t)(sizeof(payload) - 1u)) {
        return fail(socket_fd, "sendto");
    }

    result = recvfrom(socket_fd, reply, sizeof(reply), 0,
                      (struct sockaddr *)&source, &source_length);
    if (result != (ssize_t)(sizeof(payload) - 1u)) {
        return fail(socket_fd, "recvfrom timeout or length");
    }
    if (memcmp(reply, payload, sizeof(payload) - 1u) != 0) {
        return fail(socket_fd, "payload mismatch");
    }
    if (source_length != sizeof(source) || source.sin_family != AF_INET ||
        source.sin_port != htons(UDP_TEST_ECHO_PORT) ||
        source.sin_addr.s_addr != htonl(0x0a000202u)) {
        return fail(socket_fd, "source mismatch");
    }
    if (close(socket_fd) != 0) {
        return fail(-1, "close");
    }
    write_message("[udp_test] PASS\n");
    return 0;
}
