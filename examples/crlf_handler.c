/**
 * This example creates a TCP listener on address 0.0.0.0:8080 that waits for
 * an incoming connection. Once a connection is received, the listener reads
 * messages from the connection in a blocking manner until EOF is reached or
 * the connection is closed. The listener expects the client (the connection
 * initiator) to close the connection after sending all messages. The listener
 * itself never writes any data back to the connection.
 *
 * The message and EOF format used in this example are defined by the
 * following BNF:
 *
 * CHAR     = <any US-ASCII character (octets 0 - 127)>
 * CR       = <US-ASCII CR, carriage return (13)>
 * LF       = <US-ASCII LF, linefeed (10)>
 * SP       = <US-ASCII SP, space (32)>
 * HT       = <US-ASCII HT, horizontal-tab (9)>
 * WS       = SP | HT
 * CRLF     = CR LF
 * EOF      = WS (CR | LF | CRLF)
 * elem     = <any CHAR except CR or LF>
 * msg-line = <any sequence of elem>
 * message  = msg-line (CR | LF | CRLF)
 *
 * NOTE: This is a toy protocol, but it serves for this simple example.
 * WARNING: This toy protocol may cause the listener to hang indefinitely if
 *          the client does not close the connection immediately after sending
 *          all messages, particularly when the last message (or EOF) ends
 *          with a lone CR.
 */

#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#define IO_IMPL
#include "../io.h"

int newlistener() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0 && "Failed to create socket");

    int _true = 1;
    assert(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(_true)) != -1 && "Failed to set socket option");

    struct sockaddr_in saddr = {0};
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(8080);
    saddr.sin_addr.s_addr=INADDR_ANY;
    assert(bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0 && "Failed to bind address");

    assert(listen(sockfd, 10) != -1 && "Failed to start listening on address");

    return sockfd;
}

int getconn(int sockfd) {
    int connfd = accept(sockfd, NULL, NULL);
    assert(connfd >= 0 && "Failed to accept a connection");
    printf("<Received connection>\n");
    return connfd;
}

#define MSG_MAX_LEN 1 << 8
#define MAX_READ 64 * 1 << 10

size_t read_msg(IO_Reader *r, char *msg, size_t maxlen) {
    // Read a bunch of data into the msg
    IO_Err err = io_reader_prefetch(r, maxlen);
    if (err == IO_ERR_EOF) {
        printf("Reached EOF\n");
        return 0;
    }
    assert((err == IO_ERR_OK || err == IO_ERR_PARTIAL) && "Failed to read message");

    // Iterate over msg until met CR or LF
    size_t n = 0;
    size_t buffered = io_reader_buffered(r);
    while (n < maxlen && n < buffered) {
        char c = io_buffer_at(r->b, n++);
        if (c == '\r' || c == '\n') {
            // When met CR or LF consume n elements
            assert(io_reader_nconsume(r, msg, n) == IO_ERR_OK && "Failed to consume reader");
            // If it's \r peek for 1 byte to check if it's \n
            if (c == '\r') {
                err = io_reader_npeek(r, &c, 1);
                // If EOF: noop
                if (err == IO_ERR_EOF) {
                    printf("Reached EOF\n");
                    break;
                }
                assert((err == IO_ERR_OK || err == IO_ERR_PARTIAL) && "Failed to read message");
            }
            // If the peeked byte is \n: consume 1 byte.
            if (c == '\n') {
                char *dest = (n > maxlen) ? NULL : msg + n;
                assert(io_reader_nconsume(r, dest, 1) == IO_ERR_OK && "Failed to consume reader");
                n++;
            }
            break;
        }
    }
    return n;
}

int isws(char c) {
    return c == ' ' || c == '\t';
}

int is_end_of_stream(char *msg) {
    size_t len = strlen(msg);
    size_t pos = 0;
    while (pos < len) {
        if (!isws(msg[pos])) break;
        pos++;
    };
    return msg[pos] == '\r' || msg[pos] == '\n';
}

int main(void) {
    IO_Err err;
    IO_Buffer b = {0};
    IO_Reader r = {0};

    int sockfd = newlistener();
    int connfd = getconn(sockfd);

    err = io_buffer_init(&b, MSG_MAX_LEN);
    assert(err == IO_ERR_OK && "Failed to initialize buffer");

    err = io_reader_init(&r, &b, connfd);
    assert(err == IO_ERR_OK && "Failed to initialize reader");

    while (r.nread < MAX_READ) {
        char msg[MSG_MAX_LEN] = {0};
        size_t nread = read_msg(&r, msg, MSG_MAX_LEN);
        if (nread == 0) break;
        if (is_end_of_stream(msg)) {
            printf("<Connection closed gracefully>\n");
            break;
        }
        printf("<Read message (%zu bytes)>\n%s", nread, msg);
    }

    io_buffer_free(&b);
    close(connfd);
    close(sockfd);
    return 0;
}
