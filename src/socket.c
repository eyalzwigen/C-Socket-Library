#include <arpa/inet.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif

#include <errno.h>
#include "socket/socket.h"
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#define MAX_FILE_AND_LINE_LENGTH 256
#define MAX_ERROR_MESSAGE_LENGTH (MAX_FILE_AND_LINE_LENGTH + 1024)

// Error message templates
#define WSA_STARTUP_FAILED "WSAStartup failed"
#define WINSOCK_ERROR "An error with the Winsock startup occurred"
#define WINSOCK_MISSING "Version 2.2 of Winsock not available"
#define CANT_ALLOCATE_FOR_SOCKET "Could not allocate memory for new socket"
#define CANT_CREATE_SOCKET "Could not create socket"
#define CANT_BIND_SOCKET "Could not bind socket"
#define CANT_ALLOCATE_SOCKADDR "Could not allocate memory for _sockaddr in new socket"
#define CANT_ALLOCATE_BYTES "Could not allocate memory for a new Bytes variable"
#define CANT_ALLOCATE_MEMORY "Failed allocating memory"
#define GETADDRINFO_ERR "An error with getaddrinfo() occurred"
#define CANT_CONNECT_SOCKET "Could not connect to socket"
#define SOCK_LISTEN_ERR "An error occurred when tried to listen on socket"
#define SOCK_ACCEPT_ERR "An error occurred when tried to accept a new socket"
#define SOCK_SEND_ERR "An error occured when tried to send data to socket"
#define SOCK_RECV_ERR "An error occurred when tried to receive from socket"
#define UNKNOWN_ERROR "An unknown error occurred"
#define ENCODE_ERR "An error with encoding data"
#define ENCODE_NULL "Cannot serialize NULL pointer"
#define CANT_CONNECT_DGRAM_SOCKET "Cannot \"connect\" a datagram socket"
#define SOCK_TYPE_NOT_SUPPORTED "This type of socket is not yet supported by this library."

static _Thread_local SockError SOCK_ERROR = {};

#define SET_SOCK_ERROR(error_code, error_message)                           \
    do {                                                                    \
        SOCK_ERROR.code = (error_code);                                     \
                                                                            \
        if (SOCK_ERROR.message != NULL) free(SOCK_ERROR.message);           \
        char *new_message = calloc(strlen(error_message), sizeof(char));    \
        if (new_message == NULL) SOCK_ERROR.message = NULL;                 \
        else {                                                              \
            strcpy(new_message, error_message);                             \
            SOCK_ERROR.message = new_message;                               \
        }                                                                   \
                                                                            \
        SOCK_ERROR.file = __FILE__;                                         \
        SOCK_ERROR.line = __LINE__;                                         \
    } while (0)

/**
 * Turns a SockErrCode into an appropriate start for an error message
 *
 * @param code - The error code
 * @return a string with the appropriate start of the error message
 */
const static char *mapErrorCodeToMessage(const SockErrCode code) {
    switch (code) {
        case WSA_STARTUP:
            return WSA_STARTUP_FAILED;
            break;
        case WINSOCK_STARTUP:
            return WINSOCK_ERROR;
            break;
        case GETADDRINFO:
            return GETADDRINFO_ERR;
            break;
        case SOCK_CREATE:
            return CANT_CREATE_SOCKET;
            break;
        case SOCK_BIND:
            return CANT_BIND_SOCKET;
            break;
        case SOCK_CONN:
            return CANT_CONNECT_SOCKET;
            break;
        case SOCK_LISTEN:
            return SOCK_LISTEN_ERR;
            break;
        case SOCK_ACCEPT:
            return SOCK_ACCEPT_ERR;
            break;
        case SOCK_SEND:
            return SOCK_SEND_ERR;
            break;
        case SOCK_RECV:
            return SOCK_RECV_ERR;
            break;
        case MEMORY_ALLOCATION:
            return CANT_ALLOCATE_MEMORY;
            break;
        default:
            return UNKNOWN_ERROR;
            break;
    }
}

/**
 * Makes a file and line message for an error
 *
 * @param file - The file's name
 * @param line -line number
 * @return the file and line message
 */
const static char *turnFileAndLineToMessage(const char *file, const int line) {
    // Thread-local static buffer: exists for the thread lifetime, no free() needed
    static _Thread_local char buf[MAX_FILE_AND_LINE_LENGTH];

    // Formats the values into the buffer safely
    snprintf(buf, sizeof(buf), "in %s at line %d", file, line);

    return buf;
}

SockError sock_error(void) {
    return SOCK_ERROR;
}

const char *str_sock_error(void) {
    const char *start = mapErrorCodeToMessage(SOCK_ERROR.code);
    const char *fileAndLine = turnFileAndLineToMessage(SOCK_ERROR.file, SOCK_ERROR.line);

    static _Thread_local char full_message[MAX_ERROR_MESSAGE_LENGTH];

    snprintf(full_message, sizeof(full_message), "%s %s.\n %s", start, fileAndLine, SOCK_ERROR.message);

    free((void *) start);
    free((void *) fileAndLine);
    return full_message;
}

typedef struct Socket {
    int sockfd;
    const char *host;
    const char *service;
    int socktype;
    struct sockaddr_storage *_sockaddr; //! Private
    struct addrinfo *_info_list; //! Extra Private!!!!
} Socket;

typedef enum {
    OK = 0,
    ERROR = -1,
    CONN_CLOSED = 1

} SockResult;

static const int SUPPORTED_SOCKET_TYPES[] = {SOCK_STREAM};

/**
 * Checks whether a socket type is compatible with the library
 *
 * @param type - The type of the socket
 * @return 1 if yes, 0 if not
 */
static int isSupported(const int type) {
    const int len = sizeof SUPPORTED_SOCKET_TYPES / sizeof SUPPORTED_SOCKET_TYPES[0];
    for (int i = 0; i < len; i++) {
        if (type == SUPPORTED_SOCKET_TYPES[i]) return 1;
    }

    return 0;
}

/**
 * Receives an exact amount of bytes from a socket
 *
 * @param sock - A pointer to the socket to receive from
 * @param dest - A pointer to the Bytes variable to put the data in
 * @param max_bytes - Maximum number of bytes to receive
 * @return 0 if no errors, else 1
 */
static int recv_exact(const Socket *sock, Bytes *dest, size_t max_bytes);

static int sock_cnt = 0;

static void free_sock(Socket *sock) {
    free(sock->_sockaddr);
    if (sock->_info_list != NULL) freeaddrinfo(sock->_info_list);
    if (sock->host != NULL) free((void *) sock->host);
    if (sock->service != NULL) free((void *) sock->service);
    free(sock);
}

void sock_close(Socket *sock) {
    if (sock->sockfd < 0) return;

    #ifdef _WIN32
        if (sock->sockfd != -1) closesocket(sock->sockfd);
        free_sock(sock);
        sock_cnt--;
        if (sock_cnt == 0) WSACleanup();
    #else
        if (sock->sockfd != -1) close(sock->sockfd);
        free_sock(sock);
        sock_cnt--;
    #endif

    if (sock_cnt == 0) {
        free(SOCK_ERROR.message);
        SOCK_ERROR.message = NULL;
    }
}

Socket *sock_new(const char *host, const char *service, const int socktype) {
    if (!isSupported(socktype)) {
        SET_SOCK_ERROR(SOCK_CREATE, SOCK_TYPE_NOT_SUPPORTED);
        return NULL;
    }

    #ifdef _WIN32
        if (sock_cnt == 0) {
            WSADATA wsaData;

            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                SET_SOCK_ERROR(WSA_STARTUP, WSA_STARTUP_FAILED);
                return NULL;
            }

            if (LOBYTE(wsaData.wVersion) != 2 ||
                HIBYTE(wsaData.wVersion) != 2)
            {
                SET_SOCK_ERROR(WINSOCK_STARTUP, WINSOCK_MISSING);
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
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_FOR_SOCKET);
        return NULL;
    }
    sock->_sockaddr = NULL;
    sock->_info_list = NULL;
    sock->socktype = socktype;

    char *sock_host = calloc(strlen(host), 1);
    if (sock_host == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
        return NULL;
    }
    strcpy(sock_host, host);
    sock->host = sock_host;

    char *sock_service = calloc(strlen(service), 1);
    if (sock_service == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
        return NULL;
    }
    strcpy(sock_service, service);
    sock->service = sock_service;

    memset(&hints, 0, sizeof(hints)); // Set all bytes in the hints struct to 0
    hints.ai_family = AF_UNSPEC; // Accepts both IPv4 and IPv6
    hints.ai_socktype = socktype; // A TCP Stream socket

    if ((status = getaddrinfo(host, service, &hints, &servinfo)) != 0) {
        SET_SOCK_ERROR(GETADDRINFO, (char *) gai_strerror(status));
        free_sock(sock);
        return NULL;
    }

    //* Get a socket file-descriptor
    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        sock->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock->sockfd < 0) continue;

        sock->_info_list = p;
        for (struct addrinfo *ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
            if (ptr->ai_next == p) {
                ptr->ai_next = NULL;
                freeaddrinfo(servinfo);
            }
        }
    }

    if (sock->sockfd == -1) {

        SET_SOCK_ERROR(SOCK_CREATE, strerror(errno));
        free_sock(sock);
        freeaddrinfo(servinfo);
        return NULL;
    }

    sock_cnt++;
    return sock;
}

int sock_bind(Socket *sock) {
    int status = 0;

    if ((status = bind(sock->sockfd, sock->_info_list->ai_addr, sock->_info_list->ai_addrlen)) == -1) {
        close(sock->sockfd);

        //* Get a socket file-descriptor and bind it
        for (const struct addrinfo *p = sock->_info_list->ai_next; p != NULL; p = p->ai_next) {
            sock->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock->sockfd < 0) continue;

            if ((status = bind(sock->sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
                close(sock->sockfd);
                continue;
            }
            sock->_sockaddr = (struct sockaddr_storage *) p->ai_addr;
            freeaddrinfo(sock->_info_list);
        }
    }
    else {
        sock->_sockaddr = (struct sockaddr_storage *) sock->_info_list->ai_addr;
        freeaddrinfo(sock->_info_list);
    }

    if (sock->sockfd == -1) {
        SET_SOCK_ERROR(SOCK_CREATE, strerror(errno));
        sock_close(sock);
        return 1;
    }
    else if (status == -1) {
        SET_SOCK_ERROR(SOCK_CREATE, strerror(errno));
        sock_close(sock);
        return 1;
    }

    return 0;
}

int sock_connect(const Socket *sock) {
    if (sock->socktype == SOCK_DGRAM) {
        SET_SOCK_ERROR(SOCK_CONN, CANT_CONNECT_DGRAM_SOCKET);
        return 1;
    }

    const int status = connect(sock->sockfd, (struct sockaddr *) sock->_sockaddr, sizeof(struct sockaddr_storage));
    if (status < 0) {
        SET_SOCK_ERROR(SOCK_CONN, strerror(errno));
        return 1;
    }
    return 0;
}

int sock_listen(const Socket *sock, int backlog) {
    const int status = listen(sock->sockfd, backlog);
    if (status < 0) {
        SET_SOCK_ERROR(SOCK_LISTEN, strerror(errno));
        return 1;
    }

    return 0;
}

Socket *sock_accept(const Socket *sock) {
    Socket *new_sock = calloc(1, sizeof(Socket));
    if (new_sock == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_FOR_SOCKET);
        return NULL;
    }
    new_sock->_sockaddr = calloc(1, sizeof(struct sockaddr_storage));
    if (new_sock->_sockaddr == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_SOCKADDR);
        free_sock(new_sock);
        return NULL;
    }

    socklen_t addr_len = sizeof *new_sock->_sockaddr;
    new_sock->sockfd = accept(sock->sockfd, (struct sockaddr *) new_sock->_sockaddr, &addr_len);
    if (new_sock->sockfd < 0) {
        SET_SOCK_ERROR(SOCK_ACCEPT, strerror(errno));
        free_sock(new_sock);
        return NULL;
    }

    return new_sock;
}

int sock_sendall(const Socket *sock, const Bytes *data) {
    SockResult code = OK;

    const uint32_t buffer_len = htonl(sizeof data->buffer);

    unsigned char *full_buffer = calloc(sizeof buffer_len + sizeof data->buffer, 1);
    memcpy(full_buffer, &buffer_len, sizeof buffer_len);
    memcpy(full_buffer + sizeof buffer_len, data->buffer, data->length);

    Bytes full_data = {
        .buffer = full_buffer,
        .length = sizeof full_buffer,
    };

    while (full_data.length > 0) {
        ssize_t bytes_sent = 0;

        bytes_sent = send(sock->sockfd, full_data.buffer, full_data.length, 0);

        if (bytes_sent < 0) {
            SET_SOCK_ERROR(SOCK_SEND, strerror(errno));
            free_bytes(&full_data);
            code = ERROR;
            break;
        }

        else if (bytes_sent == 0) {
            free(full_buffer);
            code = CONN_CLOSED;
            break;
        }

        if (remove_prefix(&full_data, bytes_sent) == 1) {
            SET_SOCK_ERROR(SOCK_SEND, CANT_ALLOCATE_MEMORY);
            free(full_buffer);
            code = ERROR;
            break;
        }

    }

    free(full_buffer);
    return code;
}

static int recv_exact(const Socket *sock, Bytes *dest, const size_t max_bytes) {
    SockResult code = 0;

    unsigned char *full_buffer = calloc(1, max_bytes);
    if (full_buffer == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
    }

    size_t bytes_left = max_bytes;
    while (bytes_left < max_bytes) {
        ssize_t bytes_received = 0;
        unsigned char *buffer = calloc(1, max_bytes);
        if (buffer == NULL) {
            SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
            free(full_buffer);
            code = ERROR;
            break;
        }

        bytes_received = recv(sock->sockfd, buffer, bytes_left, 0);

        // Error with receiving data
        if (bytes_received < 0) {
            SET_SOCK_ERROR(SOCK_RECV, strerror(errno));
            free(buffer);
            code = ERROR;
            break;
        }

        // The socket closed connection
        if (bytes_received == 0) {
            free(buffer);
            return code = CONN_CLOSED;
            break;
        }

        bytes_left -= bytes_received;

        // Resize the current buffer to the amount of bytes received
        unsigned char *new_buffer = realloc(buffer, bytes_received);
        if (new_buffer == NULL) {
            SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
            free(full_buffer);
            free(buffer);
            code = ERROR;
            break;
        }
        free(buffer);
        buffer = new_buffer;

        // Append the received buffer to the full buffer
        memcpy(full_buffer + (max_bytes - bytes_left), buffer, bytes_received);
        free(buffer);

    }
    if (code == OK) {
        if (dest->buffer != NULL) {
            free(dest->buffer);
            dest->buffer = NULL;
            dest->length = 0;
        }

        dest->buffer = full_buffer;
        dest->length = sizeof full_buffer;
    }
    return code;
}

int sock_recv(const Socket *sock, Bytes *dest) {
    SockResult code = OK;

    // First, receive the 'header' with the size of the data sent
    Bytes header = {
        .buffer = NULL,
        .length = 0,
    };

    code = recv_exact(sock, &header, sizeof(uint32_t));
    if (code != OK ) {
        free(header.buffer);
        return code;
    }

    const uint32_t buffer_len = bytes_to_u32(&header);
    free(header.buffer);
    header.buffer = NULL;

    code = recv_exact(sock, dest, (size_t) buffer_len);

    return code;
}

void print_ip(struct sockaddr_storage *addr) {
    if (addr->ss_family == AF_INET) {
        const struct sockaddr_in *ipv4 = (struct sockaddr_in *) addr;
        char ip[INET_ADDRSTRLEN] = {0};
        printf("%s\n", inet_ntop(AF_INET, &ipv4->sin_addr, ip, INET_ADDRSTRLEN));
    }
    else if (addr->ss_family == AF_INET6) {
        const struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) addr;
        char ip[INET6_ADDRSTRLEN] = {0};
        printf("%s\n", inet_ntop(AF_INET6, &ipv6->sin6_addr, ip, INET6_ADDRSTRLEN));
    }
}

Bytes *encode(const void *data, const size_t length) {
    if (data == NULL) {
        SET_SOCK_ERROR(ENCODE, ENCODE_NULL);
        return NULL;
    }

    Bytes *bytes = calloc(1, sizeof(Bytes));
    if (bytes == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_BYTES);
        return NULL;
    }

    bytes->buffer = (unsigned char *) data;
    bytes->length = length;
    return bytes;
}

int remove_prefix(Bytes *bytes, const size_t prefix_length) {
    unsigned char *new_data = calloc(1, bytes->length - prefix_length);
    if (new_data == NULL) {
        return 1;
    }

    for (size_t i = 0; i < bytes->length - prefix_length; i++) {
        new_data[i] = bytes->buffer[i + prefix_length];
    }

    free(bytes->buffer);
    bytes->buffer = new_data;
    bytes->length -= prefix_length;
    return 0;
}

int remove_suffix(Bytes *bytes, const size_t suffix_length) {
    unsigned char *new_data = calloc(1, bytes->length - suffix_length);
    if (new_data == NULL) {
        return 1;
    }

    for (size_t i = 0; i < bytes->length - suffix_length; i++) {
        new_data[i] = bytes->buffer[i];
    }

    free(bytes->buffer);
    bytes->buffer = new_data;
    bytes->length -= suffix_length;
    return 0;
}

uint32_t bytes_to_u32(const Bytes *bytes) {
    uint32_t n;
    memcpy(&n, bytes->buffer, sizeof(uint32_t));
    return n;
}

int bytes_to_int(const Bytes *bytes) {
    int n;
    memcpy(&n, bytes->buffer, sizeof(int));
    return n;
}

void free_bytes(Bytes *bytes) {
    free(bytes->buffer);
    bytes->buffer = NULL;
    free(bytes);
}
