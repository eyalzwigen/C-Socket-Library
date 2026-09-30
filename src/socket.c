#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <errno.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

#include "CSocket/socket.h"

/**
 * This is a thread-local(one copy per-thread) SockError variable
 * that tracks the latest errors related to this socket API.
 * see more details about the SockError struct in socket.h
 */
static _Thread_local SockError SOCK_ERROR = {};

/**
 * This variable tracks the number of sockets
 * that are open in the current running process.
 * It's used to determine when to use WSACleanup() or WSAStartup()
 * and also it frees the latest error message in SOCK_ERROR if the
 * last socket got closed
 */
static atomic_int SOCK_CNT = 0;

/**
 * A macro that is used to update the SOCK_ERROR variable with a new error
 *
 * @param error_code - A SockErrorCode variable with the error code
 * @param error_message - The error message
 */
#define SET_SOCK_ERROR(error_code, error_message)                           \
    do {                                                                    \
        SOCK_ERROR.code = (error_code);                                     \
        strcpy(SOCK_ERROR.message, error_message);                          \
        strcpy(SOCK_ERROR.file, __FILE__);                                  \
        SOCK_ERROR.line = __LINE__;                                         \
    } while (0)

/**
 * Turns a SockErrCode into an appropriate start for an error message
 *
 * @param code - The error code
 * @return a string with the appropriate start of the error message
 */
const static char *mapErrorCodeToMessage(const SockErrorCode code) {
    switch (code) {
        case WSA_STARTUP:
            return WSA_STARTUP_FAILED;
        case WINSOCK_STARTUP:
            return WINSOCK_ERROR;
        case GETADDRINFO:
            return GETADDRINFO_ERR;
        case SOCK_CREATE:
            return CANT_CREATE_SOCKET;
        case SOCK_BIND:
            return CANT_BIND_SOCKET;
        case SOCK_CONN:
            return CANT_CONNECT_SOCKET;
        case SOCK_LISTEN:
            return SOCK_LISTEN_ERR;
        case SOCK_ACCEPT:
            return SOCK_ACCEPT_ERR;
        case SOCK_SEND:
            return SOCK_SEND_ERR;
        case SOCK_RECV:
            return SOCK_RECV_ERR;
        case SOCK_CLOSE:
            return SOCK_CLOSE_ERROR;
        case MEMORY_ALLOCATION:
            return CANT_ALLOCATE_MEMORY;
            break;
        default:
            return UNKNOWN_ERROR;
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
    SockError error = {};
    error.code = SOCK_ERROR.code;
    error.line = SOCK_ERROR.line;
    strcpy(error.file, SOCK_ERROR.file);
    strcpy(error.message, SOCK_ERROR.message);

    return error;
}

const char *str_sock_error(void) {
    const char *start = mapErrorCodeToMessage(SOCK_ERROR.code);
    const char *fileAndLine = turnFileAndLineToMessage(SOCK_ERROR.file, SOCK_ERROR.line);

    static _Thread_local char full_message[MAX_ERROR_MESSAGE_LENGTH];

    snprintf(full_message, sizeof(full_message), "%s %s.\n %s", start ? start : "", fileAndLine ? fileAndLine : "", SOCK_ERROR.message);

    return full_message;
}

typedef struct Socket {
    int sockfd;
    char *host;
    char *service;
    int socktype;
    struct sockaddr_storage *_sockaddr; //! Private
    struct addrinfo *_info_list; //! Extra Private!!!!
} Socket;

typedef enum {
    OK = 0,
    ERR = -1,
    CONN_CLOSED = 1

} SockResult;

static const int SUPPORTED_SOCKET_TYPES[] = {SOCKET_STREAM};

static int mapType(const SocketType socktype) {
    switch (socktype) {
        case SOCKET_STREAM:
            return SOCK_STREAM;

        default:
            return -1;
    }
}

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


static void free_sock(Socket *sock) {
    if (sock == NULL)
        return;

    if (sock->_sockaddr != NULL) free(sock->_sockaddr);
    if (sock->_info_list != NULL) freeaddrinfo(sock->_info_list);
    if (sock->host != NULL) free(sock->host);
    if (sock->service != NULL) free(sock->service);
    free(sock);
}

static atomic_int NEEDS_CLEANUP = 0;

int sock_close(Socket *sock) {
    if (sock == NULL) {
        SET_SOCK_ERROR(SOCK_CLOSE, SOCKET_IS_NULL);
        return 1;
    }

    #ifdef _WIN32
        if (sock->sockfd != -1) {
            closesocket(sock->sockfd);
            atomic_fetch_sub(&SOCK_CNT, 1);
        }

        free_sock(sock);
        if (atomic_load(&SOCK_CNT) == 0 && atomic_load(&NEEDS_CLEANUP)) {
            WSACleanup();
            atomic_store(&NEEDS_CLEANUP, 0);
        }
    #else
        if (sock->sockfd != -1) {
            close(sock->sockfd);
            atomic_fetch_sub(&SOCK_CNT, 1);
        }

        free_sock(sock);
    #endif

    return 0;
}

Socket *sock_new(const char *host, const char *service, const SocketType socktype) {
    if (!isSupported(socktype)) {
        SET_SOCK_ERROR(SOCK_CREATE, SOCK_TYPE_NOT_SUPPORTED);
        return NULL;
    }

    const int mapped_type = mapType(socktype);

    #ifdef _WIN32
        if (atomic_load(&SOCK_CNT) == 0) {
            WSADATA wsaData;

            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                SET_SOCK_ERROR(WSA_STARTUP, WSA_STARTUP_FAILED);
                return NULL;
            }

            if (LOBYTE(wsaData.wVersion) != 2 ||
                HIBYTE(wsaData.wVersion) != 2)
            {
                SET_SOCK_ERROR(WINSOCK_STARTUP, WINSOCK_MISSING);
                 WSACleanup();
                return NULL;
            }

            atomic_store(&NEEDS_CLEANUP, 1);
        }
    #endif

    //* Initialize server info
    int status; // getaddrinfo returns -1 in case of an error. This is used o track it
    struct addrinfo hints; // The base data for the addrinfo
    struct addrinfo *servinfo; // Where getaddrinfo puts the data

    Socket *sock = calloc(1, sizeof(Socket));
    if (sock == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_FOR_SOCKET);
       goto win32_cleanup;
    }
    sock->_sockaddr = NULL;
    sock->_info_list = NULL;
    sock->socktype = mapped_type;

    char *sock_host = calloc(strlen(host) + 1, 1);
    if (sock_host == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
        goto win32_cleanup;
    }
    strcpy(sock_host, host);
    sock->host = sock_host;

    char *sock_service = calloc(strlen(service) + 1, 1);
    if (sock_service == NULL) {
        SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
        goto win32_cleanup;
    }
    strcpy(sock_service, service);
    sock->service = sock_service;

    memset(&hints, 0, sizeof(hints)); // Set all bytes in the hints struct to 0
    hints.ai_family = AF_UNSPEC; // Accepts both IPv4 and IPv6
    hints.ai_socktype = mapped_type;

    if ((status = getaddrinfo(host, service, &hints, &servinfo)) != 0) {
        SET_SOCK_ERROR(GETADDRINFO, (char *) gai_strerror(status));
        free_sock(sock);
        goto win32_cleanup;
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
        goto win32_cleanup;
    }

    atomic_fetch_add(&SOCK_CNT, 1);
    return sock;

    win32_cleanup:
        #ifdef _WIN32
            if (atomic_load(&SOCK_CNT) == 0) {
                WSACleanup();
                atomic_store(&NEEDS_CLEANUP, 0);
            }
            return NULL;
        #else
            return NULL;
        #endif
}

int sock_bind(Socket *sock) {
    if (sock == NULL) {
        SET_SOCK_ERROR(SOCK_BIND, SOCKET_IS_NULL);
        return ERR;
    }

    int status = 0;

    if (sock->_sockaddr == NULL) {
        sock->_sockaddr = calloc(1, sizeof(struct sockaddr_storage));
        if (sock->_sockaddr == NULL) {
            SET_SOCK_ERROR(MEMORY_ALLOCATION, CANT_ALLOCATE_MEMORY);
            goto on_error;
        }
    }

    if ((status = bind(sock->sockfd, sock->_info_list->ai_addr, sock->_info_list->ai_addrlen)) == -1) {
        #ifdef _WIN32
                closesocket(sock->sockfd);
        #else
                close(sock->sockfd);
        #endif

        //* Get a socket file-descriptor and bind it
        for (const struct addrinfo *p = sock->_info_list->ai_next; p != NULL; p = p->ai_next) {
            sock->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock->sockfd < 0) continue;

            if ((status = bind(sock->sockfd, p->ai_addr, p->ai_addrlen)) == -1)
                #ifdef _WIN32
                    closesocket(sock->sockfd);
                #else
                    close(sock->sockfd);
                #endif

            else {
                memcpy(sock->_sockaddr, (struct sockaddr_storage *) p->ai_addr, p->ai_addrlen);
                break;
            }
        }
    }
    else {
        memcpy(sock->_sockaddr, (struct sockaddr_storage *) sock->_info_list->ai_addr, sock->_info_list->ai_addrlen);
    }

    if (sock->sockfd == -1) {
        SET_SOCK_ERROR(SOCK_CREATE, strerror(errno));
        goto on_error;
    }
    else if (status == -1) {
        SET_SOCK_ERROR(SOCK_CREATE, strerror(errno));
        goto on_error;
    }

    return OK;

    on_error:
        sock_close(sock);
        return ERR;
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
        goto on_error;
    }

    socklen_t addr_len = sizeof *new_sock->_sockaddr;
    new_sock->sockfd = accept(sock->sockfd, (struct sockaddr *) new_sock->_sockaddr, &addr_len);
    if (new_sock->sockfd < 0) {
        SET_SOCK_ERROR(SOCK_ACCEPT, strerror(errno));
        goto on_error;
    }

    return new_sock;

    on_error:
        free_sock(new_sock);
        return NULL;
}

int sock_send(const Socket *sock, const Bytes *data) {
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

        bytes_sent = send(sock->sockfd, (const char *) full_data.buffer, full_data.length, 0);

        if (bytes_sent < 0) {
            SET_SOCK_ERROR(SOCK_SEND, strerror(errno));
            free(full_buffer);
            code = ERR;
            break;
        }

        else if (bytes_sent == 0) {
            free(full_buffer);
            code = CONN_CLOSED;
            break;
        }

        if (remove_prefix(&full_data, bytes_sent) == 1) {
            SET_SOCK_ERROR(SOCK_SEND, CANT_ALLOCATE_MEMORY);
            code = ERR;
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
            code = ERR;
            break;
        }

        bytes_received = recv(sock->sockfd, (char *) buffer, bytes_left, 0);

        // Error with receiving data
        if (bytes_received < 0) {
            SET_SOCK_ERROR(SOCK_RECV, strerror(errno));
            free(buffer);
            code = ERR;
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
            code = ERR;
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

// void print_ip(struct sockaddr_storage *addr) {
//     if (addr->ss_family == AF_INET) {
//         const struct sockaddr_in *ipv4 = (struct sockaddr_in *) addr;
//         char ip[INET_ADDRSTRLEN] = {0};
//         printf("%s\n", inet_ntop(AF_INET, &ipv4->sin_addr, ip, INET_ADDRSTRLEN));
//     }
//     else if (addr->ss_family == AF_INET6) {
//         const struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) addr;
//         char ip[INET6_ADDRSTRLEN] = {0};
//         printf("%s\n", inet_ntop(AF_INET6, &ipv6->sin6_addr, ip, INET6_ADDRSTRLEN));
//     }
// }

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
    if (bytes == NULL) return;
    if (bytes != NULL) free(bytes->buffer);
    bytes->buffer = NULL;
    free(bytes);
}
