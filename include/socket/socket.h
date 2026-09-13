#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

#include <netdb.h>

typedef struct Bytes {
    unsigned char *data;
    size_t length;
} Bytes;

typedef struct SockInfo {
    const char *host;
    const char *service;
    int socktype;
} SockInfo;

typedef struct Socket Socket;

/** Sockets **/

/**
 * Enumerates all addrinfo items and initializes a
 * socket with the first valid set of values
 *
 * @param sockinfo - Contains the host, port, and type of the socket
 * @return A new 'Socket' struct with a bound socket, and all.
 *  - On error, it returns NULL
 */
Socket *sock_init(SockInfo sockinfo);

/**
 * Closes a socket
 * @param sock - the socket to close
 */
static void sock_close(Socket *sock);

/**
 * Connects a stream socket to a designated host and port
 * @param sock - Pointer to the socket to connect through
 * @return o if no errors else 1
 */
int sock_connect(const Socket *sock);

/**
 * Listens for incoming connections
 *
 * @param sock - The socket to listen to
 * @param backlog - How many incoming connections are allowed to wait until being accepted
 * @return 0 if there are no errors, else 1
 */
int sock_listen(const Socket *sock, int backlog);

/**
 * Accepts an incoming socket connection
 *
 * @param sock - The listening socket
 * @return A new socket that you can use to communicate with the client
 */
Socket *sock_accept(const Socket *sock);

//-------------------------------------------------------------------

/** IO **/

/**
 * Prints an IP Address
 *
 * @param addr - The address to print
 */
void print_ip(struct sockaddr_storage *addr);

/**
 * Sends a message
 *
 * @param sock - A pointer to the socket to send through
 * @param data - The data to send
 * @return 0 if no errors, else 1
 */
int sock_sendall(const Socket *sock, Bytes data);


/**
 * Receive a
 *
 * @param sock - The soket to receive from
 * @return A bytes object on success. On failure, returns:
 *                   { .buffer = NULL, .length = 0 }.
 */
Bytes sock_recv(Socket *sock);

//-------------------------------------------------------------------

/** Helpers **/

/**
 * Free a bytearray
 *
 *@param bytes - The bytes to free
 */
void free_bytes(Bytes bytes);

#endif //SOCKET_TYPES_H