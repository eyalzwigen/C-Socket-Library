#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

#include <netdb.h>

typedef enum {
    WSA_STARTUP = 1,
    WINSOCK_STARTUP,
    GETADDRINFO,
    SOCK_CREATE,
    SOCK_BIND,
    SOCK_CONN,
    SOCK_LISTEN,
    SOCK_ACCEPT,
    SOCK_SEND,
    SOCK_RECV,
    SOCK_CLOSE,
    MEMORY_ALLOCATION,
    ENCODE
} SockErrCode;

typedef struct {
    SockErrCode code;
    char *message;
    char *file;
    int line;
} SockError;

typedef struct {
    unsigned char *buffer;
    size_t length;
} Bytes;

typedef struct Socket Socket;


/** Sockets **/

/*
 * Enumerates all addrinfo items and creates a
 * new socket with the first valid set of values
 *
 * @param sockinfo - Contains the host, port, and type of the socket
 * @return A new 'Socket' struct with the socket's file-descriptor, and all.
 *  - On error, it returns NULL
 */

/**
 *  Creates a new socket
 *
 * @param host - The host of the socket (for example: 127.0.0.1, 0.0.0.0, 192.0.2.67)
 * @param service - The service/port of the socket (for example: 8080, http, 443)
 * @param socktype - The type of the socket you want to create (SOCK_STREAM, SOCK_DGRAM, etc...)
 * @return  A new 'Socket' struct with the socket's file-descriptor, and all.
 *  - On error, it returns NULL
 */
Socket *sock_new(const char *host, const char *service, int socktype);

/**
 * Binds a socket
 *
 * @param sock - The socket to bind
 * @return 0 if no errors, else 1
 */
int sock_bind(Socket *sock);

/**
 * Closes a socket and frees all memory.
 * @param sock - the socket to close. After the call, the sock will be NULL
 * @returns 0 if no errors, else 1
 */
int sock_close(Socket *sock);

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
 * @return 0 if all data was sent, -1 if there were errors, and 1 if the socket closed the connection
 */
int sock_sendall(const Socket *sock, const Bytes *data);


/**
 * Receive a
 *
 * @param sock - The soket to receive from
 * @param dest - The destination Bytes object to put the data in
 * @return 0 if all data received, -1 if there was an error, and 1 if the socket closed the connection
 */
int sock_recv(const Socket *sock, Bytes *dest);

//-------------------------------------------------------------------

/** Helpers **/

/**
 * Turn a value into an Byte struct
 *
 * @param data - The pointer to the data
 * @param length - The size of the data
 * @returns
 */
Bytes *encode(const void *data, size_t length);

/**
 * Removes a prefix from a bytearray
 *
 * @param bytes - The pointer to the bytes
 * @param prefix_length - The length of the prefix
 * @return 0 if no errors, else 1
 */
int remove_prefix(Bytes *bytes, size_t prefix_length);

/**
 * Removes a suffix from a bytearray
 *
 * @param bytes - The pointer to the bytes
 * @param suffix_length - The length of the prefix
 * @return
 */
int remove_suffix(Bytes *bytes, size_t suffix_length);

/**
 * Extract a uint32_t from a Bytes variable
 *
 * @param bytes - The bytes to convert
 * @return the value
 */
uint32_t bytes_to_u32(const Bytes *bytes);

/**
 * Extract an int from a Bytes variable
 *
 * @param bytes - The bytes to convert
 * @return the value
 */
int bytes_to_int(const Bytes *bytes);

/**
 * Free a bytearray
 *
 * @param bytes - The pointer to the bytes to free
 */
void free_bytes(Bytes *bytes);


//-------------------------------------------------------------------

/** Error Tracking **/

/**
 * Get data of the latest error in the form of SockError
 *
 * @return a SockError object with the data of the latest error
 */
SockError sock_error();

/**
 * Get data of the latest error in the form of an error message
 *
 * @return
 */
const char *str_sock_error();

#endif //SOCKET_TYPES_H