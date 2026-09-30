#include <assert.h>
#include <stdlib.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CSocket/socket.h>
#include <string.h>

#define TYPE_SOCK 0
#define TYPE_BYTES 1

typedef struct node {
    void *value;
    int type;
    struct node *next;
} node;

static node *HEAD = NULL;

static int add_node(node *head, void *value, const int type) {
    if (head == NULL) {
        fprintf(stderr, "The head is NULL");
        return 1;
    }

    node *new_node = calloc(1, sizeof(node));
    if (new_node == NULL) {
        fprintf(stderr, "Can't allocate memory :(");
        return 1;
    }
    new_node->value = value;
    new_node->type = type;

    node *p = head;
    while (p->next != NULL) {
        p = p->next;
    }

    p->next = new_node;
    return 0;
}

static int init_suite(void) {
    HEAD = calloc(1, sizeof(node));
    if (HEAD == NULL) {
        fprintf(stderr, "Can't allocate memory :(");
        return 1;
    }

    return 0;
}

static int clean_suite(void) {
    return 0;
}

static int clean_nodes() {
    node *p = HEAD->next;

    while (p != NULL) {
        node *next = p->next;

        switch (p->type) {
            case TYPE_SOCK:
                if (p->value != NULL)
                    sock_close((Socket *) p->value);
                break;
            case TYPE_BYTES:
                if (p->value != NULL)
                    free_bytes((Bytes *) p->value);
                break;

            default:
                break;
        }

        free(p);
        p = next;
    }

    HEAD->next = NULL;
    return 0;
}


// static void print_nodes() {
//     node *p = HEAD->next;
//     while (p != NULL) {
//         printf("Node type: %d\n", p->type);
//         printf("Address: %p\n", p->value);
//         p = p->next;
//     }
// }

static void test_sock_new() {
    // Create a valid socket
    Socket *valid_sock = sock_new("127.0.0.1", "8080", SOCKET_STREAM);
    add_node(HEAD, valid_sock, TYPE_SOCK);
    CU_ASSERT(valid_sock != NULL);

    // Create a socket with valid host & service but an unsupported type
    Socket *invalid_type = sock_new("127.0.0.1", "8080", 2);
    add_node(HEAD, invalid_type, TYPE_SOCK);
    CU_ASSERT(invalid_type == NULL);
    CU_ASSERT(sock_error().code == SOCK_CREATE);
    CU_ASSERT(strcmp(sock_error().message, SOCK_TYPE_NOT_SUPPORTED) == 0);

    // Create a socket with valid type & service but invalid host
    Socket *invalid_host = sock_new("Invalid Host", "8080", SOCKET_STREAM);
    add_node(HEAD, invalid_host, TYPE_SOCK);
    CU_ASSERT(invalid_host == NULL);
    CU_ASSERT(sock_error().code == GETADDRINFO);

    //Create a socket with valid type & host but invalid service
    Socket *invalid_service = sock_new("127.0.0.1", "Invalid Service", SOCKET_STREAM);
    add_node(HEAD, invalid_service, TYPE_SOCK);
    CU_ASSERT(invalid_service == NULL);
    CU_ASSERT(sock_error().code == GETADDRINFO);

    clean_nodes();
}

static void test_sock_bind(void) {
    // Create a valid socket
    Socket *sock = sock_new("127.0.0.1", "8080", SOCKET_STREAM);
    add_node(HEAD, sock, TYPE_SOCK);
    CU_ASSERT(sock != NULL);

    // Bind the socket
    CU_ASSERT(sock_bind(sock) == 0);

    clean_nodes();
}

static void test_sock_connect(void) {

}

static void test_sock_listen(void) {

}

static void test_sock_accept(void) {

}

static void test_sock_send(void) {

}

static void test_sock_recv(void) {

}

static void test_sock_close(void) {

}

int main() {
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite pSuite = CU_add_suite("Test Socket Library", init_suite, clean_suite);
    if (pSuite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    unsigned int failures = 0;

    // Add Tests
    if (CU_add_test(pSuite, "Test Creating a New Socket", test_sock_new) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Binding a Socket", test_sock_bind) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Connecting a Socket", test_sock_connect) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Listening for a Socket", test_sock_listen) == NULL) goto cleanup;
    if (CU_add_test(pSuite,"Test accepting a Socket", test_sock_accept) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Sending Data To a Socket", test_sock_send) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Receiving Data From a Socket", test_sock_recv) == NULL) goto cleanup;
    if (CU_add_test(pSuite, "Test Closing a Socket", test_sock_close) == NULL) goto cleanup;

    // Run Tests
    CU_basic_run_tests();
    failures = CU_get_number_of_failures();

    CU_cleanup_registry();
    return failures == 0 ? 0 : 1;

    cleanup:
        CU_cleanup_registry();
        return 1;
}
