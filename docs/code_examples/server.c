#include "socket/socket.h"
#include <stdio.h>

int main(int argc , char **argv) {
    if (argc != 3) {
        printf("Usage: ./server <host> <port>\n");
        return 1;
    }

    const SockInfo sockinfo = {
        .host = argv[1],
        .service = argv[2],
        .socktype = SOCK_STREAM,
    };

    Socket *listen_sock = sock_init(sockinfo);
    sock_listen(listen_sock, 1);

    Socket *client_sock = sock_accept(listen_sock);
    if (client_sock == NULL) {
        return 1;
    }


    while (1) {
        // Do stuff :)
    }

    sock_close(listen_sock);
    return 0;
}
