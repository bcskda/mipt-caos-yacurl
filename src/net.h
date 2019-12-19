#pragma once
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEBUG

typedef struct net_request {
    char *hostname;
    int port;
    char *send_buf;
    size_t send_buf_size;
    char *recv_buf;
    size_t recv_buf_size;
    void (*on_data)(const struct net_request *, size_t);
} net_request_t;

// Should set defaults as:
//   recv_buf <- NULL, recv_buf_size <- 0, on_data <- NULL
net_request_t *net_request_init() {
    net_request_t *net_req_ptr = (net_request_t *)calloc(1, sizeof(net_request_t));

    net_req_ptr->hostname = NULL;
    net_req_ptr->port = 0;
    net_req_ptr->send_buf = NULL;
    net_req_ptr->send_buf_size = 0;
    net_req_ptr->recv_buf = NULL;
    net_req_ptr->recv_buf_size = 0;
    net_req_ptr->on_data = NULL;

    return net_req_ptr;
}

// Shouldn`t free struct fields.
void net_request_free(net_request_t *net_req) {
    free(net_req);
}

// Should establish a TCP connection, send contents of send_buf and
// save the server`s response to recv_buf, calling on_data (if not null)
// each time the buffer gets full.
// If recv_buf is null or recv_buf_size is non-positive, should return
// an error. If recv_buf is full and on_data is null, should return an error.
//
// Return value:
//   zero if no error ocurred, -1 otherwise.
int net_send_receive(net_request_t *request) {
    // Bad moment?????????????????????????????????????????????????????????
    char port_num[10];
    sprintf(port_num, "%d", request->port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;        // It's not necessary: IPv4 or IPv6.
    hints.ai_socktype = SOCK_STREAM;    // TCP stream-sockets.
    struct addrinfo *addrinfo_list = NULL;

    int status = 0;
    if ((status = getaddrinfo(request->hostname, port_num, &hints, &addrinfo_list)) != 0) {
        fprintf(stderr, "getaddrinfo error: %sn", gai_strerror(status));
        return -1;
    }

    // Create socket.
    int sock_fd = socket(addrinfo_list->ai_family, addrinfo_list->ai_socktype, addrinfo_list->ai_protocol);
    if (sock_fd == -1) {
        fprintf(stderr, "Socket creation failed");
        return -1;
    }

    if (connect(sock_fd, addrinfo_list->ai_addr, addrinfo_list->ai_addrlen) != 0) {
        fprintf(stderr, "Connection failed");
        close(sock_fd);
        return -1;
    }



    // Send the request.
    #ifdef DEBUG
        printf("Start sending the request.\n");
        fflush(stdout);
    #endif // DEBUG

    long long bytes_sent = 0;
    size_t remaining_bytes = request->send_buf_size;
    while ((bytes_sent = send(sock_fd, request->send_buf + request->send_buf_size - remaining_bytes, remaining_bytes, 0)) > 0) {
        remaining_bytes -= bytes_sent;
    }

    if (remaining_bytes > 0) {
        if (bytes_sent < 0) {
            fprintf(stderr, "Error occured during the transmition (e. g. server closed the connection)");
        } else {
            fprintf(stderr, "The transmition is strangely interrupted");
        }
        close(sock_fd);
        return -1;
    }


    // Read the response.
    #ifdef DEBUG
        printf("Start reading the response.\n");
        fflush(stdout);
    #endif // DEBUG

    long long was_read = 0;
    remaining_bytes = request->recv_buf_size;
    while ((was_read = recv(sock_fd, request->recv_buf + request->recv_buf_size - remaining_bytes, remaining_bytes, 0)) > 0) {
        remaining_bytes -= was_read;

        if (remaining_bytes == 0) {
            if (request->on_data == NULL) {
                fprintf(stderr, "recv_buf is full, but there is no function to free it");
                shutdown(sock_fd, SHUT_RDWR);
                close(sock_fd);
                return -1;
            } else {
                request->on_data(request, request->recv_buf_size);
            }
        }
        #ifdef DEBUG
            printf("%s\n", request->recv_buf);
            fflush(stdout);
        #endif // DEBUG
    }

    if (was_read == -1) {
        fprintf(stderr, "recv failed");
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        return -1;
    }

    #ifdef DEBUG
        printf("Finished.\n");
        fflush(stdout);
    #endif // DEBUG

    close(sock_fd);
    free(addrinfo_list);

    return 0;
}