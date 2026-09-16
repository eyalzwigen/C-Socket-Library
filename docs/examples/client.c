#include "socket/socket.h"
#include <stdio.h>

int main(int argc , char **argv) {
    if (argc != 3) {
        printf("Usage: ./client <host> <port>\n");
        return 1;
    }

    Socket *sock = sock_new(argv[1], argv[2], SOCK_STREAM);
    if (sock == NULL || sock_bind(sock) == 1) {
        return 1;
    }

    if (sock_bind(sock) == 1 || sock_connect(sock) == 1) {
        sock_close(sock);
        fprintf(stderr, "%s\n", str_sock_error());
        return 1;
    }


    while (1) {
        // Do stuff :)
    }

    sock_close(sock);
    return 0;
}
