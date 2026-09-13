#include <arpa/inet.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
    #define IS_WINDOWS 1

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #define IS_WINDOWS 0

    #include <sys/socket.h>
    #include <unistd.h>
#endif

#include <errno.h>
#include "socket/socket.h"
#include <netdb.h>
#include <stdio.h>
#include <string.h>

struct Socket {
    SockInfo sockinfo;
    int sockfd;
    struct sockaddr_storage *_sockaddr; //! Private
};

/**
 * Receives an exact amount of bytes from a socket
 *
 * @param sock - A pointer to the socket to receive from
 * @param max_bytes - Maximum number of bytes to receive
 * @return A bytes object on success. On failure, returns:
 *                   { .buffer = NULL, .length = 0 }.
 */
static Bytes recv_exact(Socket *sock, size_t max_bytes);


static int sock_cnt = 0;

void sock_close(Socket *sock) {
    if (sock->sockfd < 0) return;

    #ifdef _WIN32
        closesocket(sock->sockfd);
        free(sock->_sockaddr);
        free(sock);
        sock_cnt--;
        if (sock_cnt == 0) WSACleanup();
    #else
        close(sock->sockfd);
        free(sock->_sockaddr);
        free(sock);
        sock_cnt--;
    #endif
}

Socket *sock_init(const SockInfo sockinfo) {
    #ifdef _WIN32
        if (sock_cnt == 0) {
            WSADATA wsaData;

            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                fprintf(stderr, "WSAStartup failed.\n");
                return NULL;
            }

            if (LOBYTE(wsaData.wVersion) != 2 ||
                HIBYTE(wsaData.wVersion) != 2)
            {
                fprintf(stderr,"Version 2.2 of Winsock not available.\n");
                if ()
                    WSACleanup();
                return NULL;
            }
        }
    #endif

    //* Initialize server info
    int status; // getaddrinfo returns -1 in case of an error. This is used o track it
    struct addrinfo hints; // The base data for the addrinfo
    struct addrinfo *servinfo; // Where getaddrinfo puts the data

    Socket *sock = calloc(1, sizeof(Socket));
    if (sock == NULL) {
        // Add an error to a custom error tracker (sock_error(), etc...)
        return NULL;
    }
    sock->_sockaddr = NULL;

    memset(&hints, 0, sizeof(hints)); // Set all bytes in the hints struct to 0
    hints.ai_family = AF_UNSPEC; // Accepts both IPv4 and IPv6
    hints.ai_socktype = sockinfo.socktype; // A TCP Stream socket

    if ((status = getaddrinfo(sockinfo.host, sockinfo.service, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status)); // If there was an error, print it to the stderr pipe
        return NULL;
    }

    //* Get a socket file-descriptor and bind it
    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        sock->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock->sockfd < 0) continue;

        if ((status = bind(sock->sockfd, p->ai_addr, p->ai_addrlen)) == -1) sock_close(sock);
        sock->_sockaddr = (struct sockaddr_storage *) p->ai_addr;
    }

    if (sock->sockfd == -1) {
        fprintf (stderr, "Could not create a socket.\nError from the latest system call:\n%s\n", strerror(errno));
        return NULL;
    }
    else if (status == -1) {
        fprintf(stderr, "Could not bind the created socket.\nError from the latest system call: %s\n", strerror(errno));
        return NULL;
    }

    sock_cnt++;
    freeaddrinfo(servinfo);
    servinfo = NULL;
    return sock;
}

int sock_connect(const Socket *sock) {
    const int status = connect(sock->sockfd, (struct sockaddr *) sock->_sockaddr, sizeof(struct sockaddr_storage));
    if (status < 0) {
        fprintf(stderr, "Could not connect to the socket.\nError: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int sock_listen(const Socket *sock, int backlog) {
    const int status = listen(sock->sockfd, backlog);
    if (status < 0) {
        fprintf(stderr, "Could not listen on socket.\nError: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

Socket *sock_accept(const Socket *sock) {
    Socket *new_sock = calloc(1, sizeof(Socket));
    if (new_sock == NULL) {
        fprintf(stderr, "Could not allocate memory for new socket.\n");
        return NULL;
    }
    new_sock->_sockaddr = calloc(1, sizeof(struct sockaddr_storage));
    if (new_sock->_sockaddr == NULL) {
        fprintf(stderr, "Could not allocate memory for _sockaddr in new socket.\n");
        return NULL;
    }

    socklen_t addr_len = sizeof *new_sock->_sockaddr;
    new_sock->sockfd = accept(sock->sockfd, (struct sockaddr *) new_sock->_sockaddr, &addr_len);
    if (new_sock->sockfd < 0) {
        fprintf(stderr, "Could not accept new socket.\nError: %s\n", strerror(errno));
        return NULL;
    }

    return new_sock;
}

int sock_sendall(const Socket *sock, Bytes data) {
    uint32_t buffer_len = htonl(sizeof data.buffer);

    unsigned char *full_buffer = calloc(sizeof buffer_len + sizeof data, 1);
    memcpy(full_buffer, &buffer_len, sizeof buffer_len);
    memcpy(full_buffer, data.buffer, sizeof *data.buffer);

    int bytes_left = sizeof full_buffer;
    while (bytes_left > 0) {
        int bytes_sent = send(sock->sockfd, full_buffer, sizeof bytes_left, 0);

        if (bytes_sent < 0) {
            fprintf(stderr, "Could not send data to socket.\nError: %s\n", strerror(errno));
            return 1;
        }

        bytes_left -= bytes_sent;
    }
}

int recv_exact(Socket *sock, Bytes buffer, int max_bytes) {

}

void print_ip(struct sockaddr_storage *addr) {
    const struct sockaddr_in *ipv4;
    const struct sockaddr_in6 *ipv6;

    if (addr->ss_family == AF_INET) {
        ipv4 = (struct sockaddr_in *) addr;
        char ip[INET_ADDRSTRLEN] = {0};
        printf("%s\n", inet_ntop(AF_INET, &ipv4->sin_addr, ip, INET_ADDRSTRLEN));
    }
    else if (addr->ss_family == AF_INET6) {
        ipv6 = (struct sockaddr_in6 *) addr;
        char ip[INET6_ADDRSTRLEN] = {0};
        printf("%s\n", inet_ntop(AF_INET6, &ipv6->sin6_addr, ip, INET6_ADDRSTRLEN));
    }
}

