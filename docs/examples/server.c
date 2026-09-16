#include <socket/socket.h>
#include <stdio.h>

int main(int argc , char **argv) {
    if (argc != 3) {
        printf("Usage: ./server <host> <port>\n");
        return 1;
    }

    Socket *listen_sock = sock_new(argv[1], argv[2], SOCK_STREAM);
    if (listen_sock == NULL) {
        fprintf(stderr, "%s\n", str_sock_error());
        return 1;
    }

    if (sock_bind(listen_sock) == 1 || sock_listen(listen_sock, 1) == 1) {
        fprintf(stderr, "%s\n", str_sock_error());
        sock_close(listen_sock);
        return 1;
    }

    Socket *client_sock = sock_accept(listen_sock);
    if (client_sock == NULL) {
        fprintf(stderr, "%s\n", str_sock_error());
        sock_close(client_sock);
        sock_close(listen_sock);
        return 1;
    }


    while (1) {
        // Do stuff :)
    }

    sock_close(listen_sock);
    return 0;
}
