#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

#include <stdint.h>

typedef enum {
    SOCKET_STREAM = 1,
} SocketType;

#define MAX_ERROR_TEXT_LENGTH (1024 + 1)
#define MAX_ERROR_CODE_MESSAGE_LENGTH (128 + 1)
#define ERROR_MESSAGE_FORMAT_OVERHEAD 5

#define MAX_FILE_NAME_LENGTH (128 + 1)

#define MAX_FILE_AND_LINE_LENGTH \
    (MAX_FILE_NAME_LENGTH + 256 + 1)

#define MAX_ERROR_MESSAGE_LENGTH \
    (MAX_ERROR_CODE_MESSAGE_LENGTH + MAX_FILE_AND_LINE_LENGTH + MAX_ERROR_TEXT_LENGTH + ERROR_MESSAGE_FORMAT_OVERHEAD)

// Error message templates
#define WSA_STARTUP_FAILED "WSAStartup failed"
#define WINSOCK_ERROR "An error with the Winsock startup occurred"
#define WINSOCK_MISSING "Version 2.2 of Winsock not available"
#define CANT_ALLOCATE_FOR_SOCKET "Could not allocate memory for new CSocket"
#define CANT_CREATE_SOCKET "Could not create CSocket"
#define CANT_BIND_SOCKET "Could not bind CSocket"
#define CANT_ALLOCATE_SOCKADDR "Could not allocate memory for _sockaddr in new CSocket"
#define CANT_ALLOCATE_BYTES "Could not allocate memory for a new Bytes variable"
#define CANT_ALLOCATE_MEMORY "Failed allocating memory"
#define GETADDRINFO_ERR "An error with getaddrinfo() occurred"
#define CANT_CONNECT_SOCKET "Could not connect to CSocket"
#define SOCK_LISTEN_ERR "An error occurred when tried to listen on CSocket"
#define SOCK_ACCEPT_ERR "An error occurred when tried to accept a new CSocket"
#define SOCK_SEND_ERR "An error occured when tried to send data to CSocket"
#define SOCK_RECV_ERR "An error occurred when tried to receive from CSocket"
#define UNKNOWN_ERROR "An unknown error occurred"
#define ENCODE_ERR "An error with encoding data"
#define ENCODE_NULL "Cannot serialize NULL pointer"
#define CANT_CONNECT_DGRAM_SOCKET "Cannot \"connect\" a datagram CSocket"
#define SOCK_TYPE_NOT_SUPPORTED "This type of CSocket is not yet supported by this library."
#define SOCK_CLOSE_ERROR "Couldn't close the CSocket"
#define SOCKET_IS_NULL "The pointer provided for the CSocket is NULL"
#define SOCKET_FILE_DESCRIPTOR_INVALID "The file-descriptor of the CSocket is invalid"

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
} SockErrorCode;

typedef struct {
    SockErrorCode code;
    char message[MAX_ERROR_TEXT_LENGTH];
    char file[MAX_FILE_NAME_LENGTH];
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
 * new CSocket with the first valid set of values
 *
 * @param sockinfo - Contains the host, port, and type of the CSocket
 * @return A new 'Socket' struct with the CSocket's file-descriptor, and all.
 *  - On error, it returns NULL
 */

/**
 *  Creates a new CSocket
 *
 * @param host - The host of the CSocket (for example: 127.0.0.1, 0.0.0.0, 192.0.2.67)
 * @param service - The service/port of the CSocket (for example: 8080, http, 443)
 * @param socktype - The type of the CSocket you want to create (SOCK_STREAM, SOCK_DGRAM, etc...)
 * @return  A new 'Socket' struct with the CSocket's file-descriptor, and all.
 *  - On error, it returns NULL
 */
Socket *sock_new(const char *host, const char *service, SocketType socktype);

/**
 * Binds a CSocket
 *
 * @param sock - The CSocket to bind
 * @return 0 if no errors, else 1
 */
int sock_bind(Socket *sock);

/**
 * Closes a CSocket and frees all memory.
 * @param sock - the CSocket to close. After the call, the sock will be NULL
 * @returns 0 if no errors, else 1
 */
int sock_close(Socket *sock);

/**
 * Connects a stream CSocket to a designated host and port
 * @param sock - Pointer to the CSocket to connect through
 * @return o if no errors else 1
 */
int sock_connect(const Socket *sock);

/**
 * Listens for incoming connections
 *
 * @param sock - The CSocket to listen to
 * @param backlog - How many incoming connections are allowed to wait until being accepted
 * @return 0 if there are no errors, else 1
 */
int sock_listen(const Socket *sock, int backlog);

/**
 * Accepts an incoming CSocket connection
 *
 * @param sock - The listening CSocket
 * @return A new CSocket that you can use to communicate with the client
 */
Socket *sock_accept(const Socket *sock);

//-------------------------------------------------------------------

/** IO **/

/**
 * Sends a message
 *
 * @param sock - A pointer to the CSocket to send through
 * @param data - The data to send
 * @return 0 if all data was sent, -1 if there were errors, and 1 if the CSocket closed the connection
 */
int sock_send(const Socket *sock, const Bytes *data);


/**
 * Receive a
 *
 * @param sock - The soket to receive from
 * @param dest - The destination Bytes object to put the data in
 * @return 0 if all data received, -1 if there was an error, and 1 if the CSocket closed the connection
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
 * @return a SockError struct with the data of the latest error
 */
SockError sock_error();

/**
 * Get data of the latest error in the form of an error message
 *
 * @return
 */
const char *str_sock_error();

#endif //SOCKET_TYPES_H