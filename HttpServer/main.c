#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "logging.h"
#include "addressUtils.h"
#include "common.h"
#include "connection.h"
#include "httpUtils.h"
#include "cacheEntry.h"

SOCKET initializeListenSocket(struct sockaddr_in listenAddress)
{
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listenSocket < 0)
    {
        logg(LL_ERROR, "initializeListenSocket", "can't create socket");
    }

    if (bind(listenSocket, (struct sockaddr *) &listenAddress, sizeof(listenAddress)))
    {
        logg(LL_ERROR, "initializeListenSocket", "can't bind socket");
    }

    if (listen(listenSocket, 100))
    {
        logg(LL_ERROR, "initializeListenSocket", "can't start listening");
    }

    return listenSocket;
}

SOCKET initializeServerSocket(struct sockaddr_in address)
{
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (clientSocket < 0)
    {
        logg(LL_ERROR, "initializeServerSocket", "can't create socket");
    }

    if (connect(clientSocket, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            logg(LL_ERROR, "initializeServerSocket", "can't connect");
        }
    }

    return clientSocket;
}

SOCKET initServerSocketToUrl(char * fullUrl)
{
    char * host = getHostFromUrl(fullUrl);
    char ip[20];
    getIpByHostname(host, ip);
    free(host);
    struct sockaddr_in address = getServerAddress(ip, 80);
    SOCKET serverSocket = initializeServerSocket(address);
    return serverSocket;
}

// Edit request to proxy server to make it valid to be send to target server
void editRequestToSendToServer(struct connection * connection)
{
    int firstSpace = 0;
    while (connection->buffer[firstSpace] != ' ')
    {
        firstSpace++;
    }

    int firstSlash = firstSpace + 8; // +8 to skip http://
    while (connection->buffer[firstSlash] != '/')
    {
        firstSlash++;
    }

    memmove(connection->buffer + firstSpace + 1, connection->buffer + firstSlash,
            connection->buffer_size - firstSlash);
    connection->buffer_size -= firstSlash - (firstSpace + 1);
}

void readArgs(int argc, const char * argv[], uint16_t * outLocalPort)
{
    if (argc < 2)
    {
        logg(LL_ERROR, "readArgs", "Usage: %s localPort", argv[0]);
    }

    char * end;

    long localPort = strtol(argv[1], &end, 10);
    if (*end != '\0' || localPort <= 0 || localPort >= UINT16_MAX)
    {
        logg(LL_ERROR, "readArgs", "Incorrect localPort format");
    }
    *outLocalPort = (uint16_t) localPort;
}

static SOCKET listenSocket;

static void exit_handler(void)
{
    close(listenSocket);
}

void * thread_exit_handler(struct connection * connection)
{
    printf("Terminated\n");
}

void * handleConnection(struct connection * connection)
{
    logg_track(LL_INFO, connection->trackingId, "Thread started");


    struct pollfd fds[2];
    char buff[BUFFER_SIZE];

    fds[0].fd = connection->clientSocket;
    fds[1].fd = connection->serverSocket;

    while (1)
    {
        switch (connection->connectionStatus)
        {
            case CS_GETTING_REQUEST:
                fds[0].events = POLLIN;
                fds[1].events = 0;
                break;

            case CS_WRITING_REQUEST:
                fds[0].events = 0;
                fds[1].events = POLLOUT;
                break;
            case CS_FORWARDING_REQUEST:
                fds[0].events = POLLIN;
                fds[1].events = POLLOUT;
                break;
            case CS_FORWARDING_RESPONSE:
                fds[0].events = POLLOUT;
                fds[1].events = POLLIN;
                break;
        }

        int polled = poll(fds, 2, 1000 * 2);

        if (polled < 0)
        {
            logg(LL_ERROR, "eventLoop", "polling error");
        }

        if (polled == 0)
        {
            continue;
        }

        switch (connection->connectionStatus)
        {
            case CS_GETTING_REQUEST:
                if (fds[0].revents & POLLHUP)
                {
                    logg_track(LL_VERBOSE, connection->trackingId, "Client-side socket closed");
                    if (connection->buffer_size > 0)
                    {
                        free(connection->buffer);
                    }
                    close(connection->clientSocket);

                    return NULL;
                }
                else if (fds[0].revents & POLLIN)
                {
                    logg_track(LL_VERBOSE, connection->trackingId, "Getting request");
                    ssize_t readCount = read(connection->clientSocket, buff, BUFFER_SIZE);
                    logg_track(LL_VERBOSE, connection->trackingId, "Got request");

                    if (readCount == 0)
                    {
                        logg_track(LL_VERBOSE, connection->trackingId, "Client-side socket closed");
                        if (connection->buffer_size > 0)
                        {
                            free(connection->buffer);
                        }
                        close(connection->clientSocket);

                        return NULL;
                    }

                    logg_track(LL_VERBOSE, connection->trackingId, "Got %zi bytes of request", readCount);
                    if (connection->buffer_size == 0)
                    {
                        connection->buffer_size = (size_t) readCount;
                        connection->buffer = malloc(readCount * sizeof(char));
                    }
                    else
                    {
                        connection->buffer_size += (size_t) readCount;
                        connection->buffer = realloc(connection->buffer,
                                                     connection->buffer_size);
                    }
                    char * dest = connection->buffer + connection->buffer_size - readCount;
                    memcpy(dest, buff, (size_t) readCount);

                    if (connection->buffer_size > 3)
                    {
                        char * url = getUrlFromData(connection->buffer,
                                                    connection->buffer_size);
                        if (url != NULL)
                        {
                            connection->connectionStatus = CS_WRITING_REQUEST;
                            connection->serverSocket = initServerSocketToUrl(url);
                            editRequestToSendToServer(connection);
                        }
                    }
                }
                break;

            case CS_WRITING_REQUEST:
                if (fds[1].revents & POLLOUT)
                {
                    logg_track(LL_VERBOSE, connection->trackingId,
                               "Writing %zu bytes", connection->buffer_size);
                    write(connection->serverSocket, connection->buffer,
                          connection->buffer_size);
                    write(1, connection->buffer, connection->buffer_size);
                    logg_track(LL_VERBOSE, connection->trackingId,
                               "Wrote %zu bytes", connection->buffer_size);

                    if (connection->buffer[connection->buffer_size - 3] == '\n' &&
                        connection->buffer[connection->buffer_size - 1] == '\n')
                    {
                        connection->buffer_size = 0;
                        connection->left_to_download = -1;
                        connection->connectionStatus = CS_FORWARDING_RESPONSE;
                    }
                    else
                    {
                        connection->connectionStatus = CS_FORWARDING_REQUEST;
                    }
                }
                break;
            case CS_FORWARDING_REQUEST:
                if (fds[0].revents & POLLIN && fds[1].revents & POLLOUT)
                {
                    logg_track(LL_VERBOSE, connection->trackingId, "Reading request");
                    ssize_t readCount = read(connection->clientSocket, buff, BUFFER_SIZE);
                    if (readCount > 0)
                    {
                        logg_track(LL_VERBOSE, connection->trackingId,
                                   "Forwarding %zi  bytes of request", readCount);
                        write(connection->serverSocket, buff, (size_t) readCount);
                        logg_track(LL_VERBOSE, connection->trackingId,
                                   "Forwarded %zi bytes of request", readCount);
                    }

                    if (buff[readCount - 3] == '\n' &&
                        buff[readCount - 1] == '\n')
                    {
                        connection->buffer_size = 0;
                        connection->left_to_download = -1;
                        connection->connectionStatus = CS_FORWARDING_RESPONSE;
                    }
                }
                break;
            case CS_FORWARDING_RESPONSE:
//                logg_track(LL_VERBOSE, connection->trackingId, "%d;%d", fds[0].revents, fds[ 1].revents);
                if (fds[0].revents & POLLOUT)
                {
                    logg_track(LL_VERBOSE, connection->trackingId, "Reading response");
                    ssize_t readCount = read(connection->serverSocket, buff, BUFFER_SIZE);
                    if (readCount == 0)
                    {
                        logg_track(LL_INFO, connection->trackingId, "Transmission over, socket closed");

                        free(connection->buffer);
                        close(connection->clientSocket);
                        close(connection->serverSocket);

                        return NULL;
                    }

                    logg_track(LL_VERBOSE, connection->trackingId,
                               "Forwarding %zi  bytes of response", readCount);
                    write(connection->clientSocket, buff, (size_t) readCount);
                    write(1, buff, (size_t) readCount);
                    logg_track(LL_VERBOSE, connection->trackingId,
                               "Forwarded %zi bytes of response\n", readCount);

                    if (connection->left_to_download == -1)
                    {
                        connection->buffer_size += (size_t) readCount;
                        connection->buffer = realloc(connection->buffer,
                                                     connection->buffer_size);

                        char * dest = connection->buffer + connection->buffer_size - readCount;

                        memcpy(dest, buff, (size_t) readCount);

                        for (int j = 0; j < connection->buffer_size - 2; ++j)
                        {
                            if (connection->buffer[j] == '\n' &&
                                connection->buffer[j + 2] == '\n')
                            {
                                int statusCode = getResponseCodeFromData(connection->buffer);
                                long contentLength = getContentLengthFromData(connection->buffer,
                                                                              connection->buffer_size);

                                logg_track(LL_INFO, connection->trackingId,
                                           "Response headers acquired, response code: %d, Content-Length: %li",
                                           statusCode, contentLength);

                                if (contentLength != -1)
                                {
                                    connection->left_to_download =
                                            contentLength - connection->buffer_size + (j + 3);

                                    free(connection->buffer);
                                    connection->buffer_size = 0;

                                    if (connection->left_to_download <= 0)
                                    {
                                        logg_track(LL_INFO, connection->trackingId,
                                                   "Transmission over, Content-Length reached");

                                        close(connection->clientSocket);
                                        close(connection->serverSocket);

                                        return NULL;
                                    }
                                }
                                else
                                {
                                    if (!isResponseHasPayload(statusCode))
                                    {
                                        logg_track(LL_INFO, connection->trackingId,
                                                   "Transmission over, no payload for %d response", statusCode);
                                        free(connection->buffer);
                                        close(connection->clientSocket);
                                        close(connection->serverSocket);

                                        return NULL;
                                    }
                                }

                                break;
                            }
                        }
                    }
                    else
                    {
                        connection->left_to_download -= readCount;
                        if (connection->left_to_download <= 0)
                        {
                            logg_track(LL_INFO, connection->trackingId,
                                       "Transmission over, Content-Length reached");

                            close(connection->clientSocket);
                            close(connection->serverSocket);

                            return NULL;
                        }
                    }
                }
                break;
        }
    }
}

int main(int argc, const char * argv[])
{
    uint16_t localPort;
    readArgs(argc, argv, &localPort);

    atexit(exit_handler);
    srand((unsigned int) time(NULL));

    listenSocket = initializeListenSocket(getListenAddress(localPort));
    logg(LL_INFO, NULL, "Server started");

    while (1)
    {
        logg(LL_VERBOSE, "accept", "Accepting request..");
        SOCKET connectedSocket = accept(listenSocket, (struct sockaddr *) NULL, NULL);

        if (connectedSocket != NULL)
        {
            logg(LL_VERBOSE, "accept", "Accepted");

            struct connection * connection = malloc(sizeof(struct connection));

            connection->clientSocket = connectedSocket;
            connection->buffer_size = 0;
            connection->connectionStatus = CS_GETTING_REQUEST;
            connection->trackingId = rand() % 9000 + 1000;

            logg_track(LL_INFO, connection->trackingId, "Accepted incoming connection");


            pthread_t thread;
            pthread_create(&thread, NULL, (void * (*)(void *)) handleConnection, connection);
        }
    }
}