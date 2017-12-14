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
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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
    // Remove host:
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

    // Remove Connection header
    char * connectionHeader = "Connection:";
    size_t connectionHeaderStrlen = strlen(connectionHeader);
    int connectionHeaderStart = -1;

    int j = 0;
    while (j + connectionHeaderStrlen < connection->buffer_size)
    {
        int eq = 1;
        for (int i = 0; i < connectionHeaderStrlen; ++i)
        {
            if (connection->buffer[i + j] != connectionHeader[i])
            {
                eq = 0;
                break;
            }
        }

        if (eq)
        {
            connectionHeaderStart = j;
            break;
        }

        j++;
    }

    if (connectionHeaderStart > -1)
    {
        int connectionHeaderEnd = connectionHeaderStart;
        while (connection->buffer[connectionHeaderEnd] != '\n')
        {
            connectionHeaderEnd++;
        }


        memmove(connection->buffer + connectionHeaderStart, connection->buffer + connectionHeaderEnd,
                connection->buffer_size - connectionHeaderEnd);
        connection->buffer_size -= connectionHeaderEnd - connectionHeaderStart;
    }

    char * newHeader = "Connection:close\n\r";
    size_t newHeaderStrlen = strlen(newHeader);

    char * newBuffer = malloc(connection->buffer_size + newHeaderStrlen * sizeof(char));
    memcpy(newBuffer, connection->buffer, (size_t) connectionHeaderStart);
    memcpy(newBuffer + (size_t) connectionHeaderStart, newHeader, newHeaderStrlen);
    memcpy(newBuffer + (size_t) connectionHeaderStart + newHeaderStrlen, connection->buffer + connectionHeaderStart,
           connection->buffer_size - connectionHeaderStart);
    free(connection->buffer);
    connection->buffer = newBuffer;
    connection->buffer_size += newHeaderStrlen * sizeof(char);
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

static struct connection activeConnections[1024];
static size_t connectionsCount = 0;
static SOCKET listenSocket;

static void exit_handler(void)
{
    close(listenSocket);
}

int main(int argc, const char * argv[])
{
    struct connection c;
    c.buffer = malloc(10000);
    memcpy(c.buffer,
           "GET http://ads.ru/fds HTTP\r\nAdfasdf: fdsa\r\nConnection: keep-alive\r\nContent-Length: 123\r\n\r\n", 101);
    c.buffer_size = strlen(c.buffer);
    //editRequestToSendToServer(&c);

    uint16_t localPort;
    readArgs(argc, argv, &localPort);

    atexit(exit_handler);
    srand((unsigned int) time(NULL));

    listenSocket = initializeListenSocket(getListenAddress(localPort));
    logg(LL_INFO, NULL, "Server started");

    while (1)
    {
        struct pollfd fds[200];
        for (int i = 0; i < connectionsCount; ++i)
        {
            fds[i * 2].fd = activeConnections[i].clientSocket;
            fds[i * 2 + 1].fd = activeConnections[i].serverSocket;
            switch (activeConnections[i].connectionStatus)
            {
                case CS_GETTING_REQUEST:
                    fds[i * 2].events = POLLIN;
                    fds[i * 2 + 1].events = 0;
                    break;

                case CS_WRITING_REQUEST:
                    fds[i * 2].events = 0;
                    fds[i * 2 + 1].events = POLLOUT;
                    break;
                case CS_FORWARDING_REQUEST:
                    fds[i * 2].events = POLLIN;
                    fds[i * 2 + 1].events = POLLOUT;
                    break;
                case CS_FORWARDING_RESPONSE:
                    fds[i * 2].events = POLLOUT;
                    fds[i * 2 + 1].events = POLLIN;
                    break;
            }
        }

        fds[connectionsCount * 2].fd = listenSocket;
        fds[connectionsCount * 2].events = POLLIN;

        int polled = poll(fds, connectionsCount * 2 + 1, 1000 * 2);
        if (polled < 0)
        {
            logg(LL_ERROR, "eventLoop", "polling error");
        }

        if (polled == 0)
        {
            continue;
        }

        char buff[BUFFER_SIZE];
        for (int i = 0; i < connectionsCount; ++i)
        {
            switch (activeConnections[i].connectionStatus)
            {
                case CS_GETTING_REQUEST:
                    if (fds[i * 2].revents & POLLHUP)
                    {
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Client-side socket closed");
                        if (activeConnections[i].buffer_size > 0)
                        {
                            free(activeConnections[i].buffer);
                        }
                        close(activeConnections[i].clientSocket);
                        activeConnections[i] = activeConnections[--connectionsCount];

                        break;
                    }
                    else if (fds[i*2].revents & POLLIN)
                    {
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Getting request");
                        ssize_t readCount = read(activeConnections[i].clientSocket, buff, BUFFER_SIZE);
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Got request");

                        if (readCount == 0)
                        {
                            logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Client-side socket closed");
                            if (activeConnections[i].buffer_size > 0)
                            {
                                free(activeConnections[i].buffer);
                            }
                            close(activeConnections[i].clientSocket);
                            activeConnections[i] = activeConnections[--connectionsCount];

                            break;
                        }
                        else if (readCount < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                logg_track(LL_WARNING, activeConnections[i].trackingId, "Poll just broke POSIX!");
                                break;
                            }
                            perror("read");
                        }

                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Got %zi bytes of request", readCount);
                        if (activeConnections[i].buffer_size == 0)
                        {
                            activeConnections[i].buffer_size = (size_t) readCount;
                            activeConnections[i].buffer = malloc(readCount * sizeof(char));
                        }
                        else
                        {
                            activeConnections[i].buffer_size += (size_t) readCount;
                            activeConnections[i].buffer = realloc(activeConnections[i].buffer,
                                                                  activeConnections[i].buffer_size);
                        }
                        char * dest = activeConnections[i].buffer + activeConnections[i].buffer_size - readCount;
                        memcpy(dest, buff, (size_t) readCount);

                        if (activeConnections[i].buffer_size > 3)
                        {
                            char * url = getUrlFromData(activeConnections[i].buffer, activeConnections[i].buffer_size);
                            if (url != NULL &&
                                isDataHasAllHeaders(activeConnections[i].buffer, activeConnections[i].buffer_size))
                            {
                                logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Editing request");
                                activeConnections[i].connectionStatus = CS_WRITING_REQUEST;
                                activeConnections[i].serverSocket = initServerSocketToUrl(url);
                                editRequestToSendToServer(&activeConnections[i]);
                                logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Edited request");
                            }
                        }
                    }
                    break;

                case CS_WRITING_REQUEST:
                    if (fds[i * 2 + 1].revents & POLLOUT)
                    {
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Writing %zu bytes", activeConnections[i].buffer_size);
                        write(activeConnections[i].serverSocket, activeConnections[i].buffer,
                              activeConnections[i].buffer_size);
                        write(1, activeConnections[i].buffer, activeConnections[i].buffer_size);
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Wrote %zu bytes", activeConnections[i].buffer_size);

                        if (activeConnections[i].buffer[activeConnections[i].buffer_size - 3] == '\n' &&
                            activeConnections[i].buffer[activeConnections[i].buffer_size - 1] == '\n')
                        {
                            activeConnections[i].buffer_size = 0;
                            activeConnections[i].left_to_download = -1;
                            activeConnections[i].connectionStatus = CS_FORWARDING_RESPONSE;
                        }
                        else
                        {
                            activeConnections[i].connectionStatus = CS_FORWARDING_REQUEST;
                        }
                    }
                    break;
                case CS_FORWARDING_REQUEST:
                    if (fds[i * 2].revents & POLLIN && fds[i * 2 + 1].revents & POLLOUT)
                    {
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Reading request");
                        ssize_t readCount = read(activeConnections[i].clientSocket, buff, BUFFER_SIZE);
                        if (readCount > 0)
                        {
                            logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                       "Forwarding %zi  bytes of request", readCount);
                            write(activeConnections[i].serverSocket, buff, (size_t) readCount);
                            logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                       "Forwarded %zi bytes of request", readCount);
                        }
                        else if (readCount < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                logg_track(LL_WARNING, activeConnections[i].trackingId, "Poll just broke POSIX!");
                                break;
                            }
                            perror("read");
                        }

                        if (buff[readCount - 3] == '\n' &&
                            buff[readCount - 1] == '\n')
                        {
                            activeConnections[i].buffer_size = 0;
                            activeConnections[i].left_to_download = -1;
                            activeConnections[i].connectionStatus = CS_FORWARDING_RESPONSE;
                        }
                    }
                    break;
                case CS_FORWARDING_RESPONSE:
//                logg_track(LL_VERBOSE, activeConnections[i].trackingId, "%d;%d", fds[0].revents, fds[ 1].revents);
                    if (fds[i * 2].revents & POLLOUT && fds[i * 2 + 1].revents & POLLIN)
                    {
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Reading response");
                        ssize_t readCount = read(activeConnections[i].serverSocket, buff, BUFFER_SIZE);

                        if (readCount == 0)
                        {
                            logg_track(LL_INFO, activeConnections[i].trackingId, "Transmission over, socket closed");

                            free(activeConnections[i].buffer);
                            close(activeConnections[i].clientSocket);
                            close(activeConnections[i].serverSocket);
                            activeConnections[i] = activeConnections[--connectionsCount];

                            break;
                        }
                        else if (readCount < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                logg_track(LL_WARNING, activeConnections[i].trackingId, "Poll just broke POSIX!");
                                break;
                            }
                            perror("read");
                        }

                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Forwarding %zi  bytes of response", readCount);
                        write(activeConnections[i].clientSocket, buff, (size_t) readCount);
                        write(1, buff, (size_t) readCount);
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Forwarded %zi bytes of response\n", readCount);

                        if (activeConnections[i].left_to_download == -1)
                        {
                            activeConnections[i].buffer_size += (size_t) readCount;
                            activeConnections[i].buffer = realloc(activeConnections[i].buffer,
                                                                  activeConnections[i].buffer_size);

                            char * dest = activeConnections[i].buffer + activeConnections[i].buffer_size - readCount;

                            memcpy(dest, buff, (size_t) readCount);

                            for (int j = 0; j < activeConnections[i].buffer_size - 2; ++j)
                            {
                                if (activeConnections[i].buffer[j] == '\n' &&
                                    activeConnections[i].buffer[j + 2] == '\n')
                                {
                                    int statusCode = getResponseCodeFromData(activeConnections[i].buffer);
                                    long contentLength = getContentLengthFromData(activeConnections[i].buffer,
                                                                                  activeConnections[i].buffer_size);

                                    logg_track(LL_INFO, activeConnections[i].trackingId,
                                               "Response headers acquired, response code: %d, Content-Length: %li",
                                               statusCode, contentLength);

                                    if (contentLength != -1)
                                    {
                                        activeConnections[i].left_to_download =
                                                contentLength - activeConnections[i].buffer_size + (j + 3);

                                        free(activeConnections[i].buffer);
                                        activeConnections[i].buffer_size = 0;

                                        if (activeConnections[i].left_to_download <= 0)
                                        {
                                            logg_track(LL_INFO, activeConnections[i].trackingId,
                                                       "Transmission over, Content-Length reached");

                                            close(activeConnections[i].clientSocket);
                                            close(activeConnections[i].serverSocket);
                                            activeConnections[i] = activeConnections[--connectionsCount];

                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if (!isResponseHasPayload(statusCode))
                                        {
                                            logg_track(LL_INFO, activeConnections[i].trackingId,
                                                       "Transmission over, no payload for %d response", statusCode);
                                            free(activeConnections[i].buffer);
                                            close(activeConnections[i].clientSocket);
                                            close(activeConnections[i].serverSocket);
                                            activeConnections[i] = activeConnections[--connectionsCount];

                                            break;
                                        }
                                    }

                                    break;
                                }
                            }
                        }
                        else
                        {
                            activeConnections[i].left_to_download -= readCount;
                            if (activeConnections[i].left_to_download <= 0)
                            {
                                logg_track(LL_INFO, activeConnections[i].trackingId,
                                           "Transmission over, Content-Length reached");

                                close(activeConnections[i].clientSocket);
                                close(activeConnections[i].serverSocket);
                                activeConnections[i] = activeConnections[--connectionsCount];

                                break;
                            }
                        }
                    }
                    break;
            }
        }

        if ((fds[connectionsCount * 2].revents & POLLIN) && !(fds[connectionsCount * 2].revents & POLLHUP))
        {
            logg(LL_VERBOSE, "accept", "Accepting request %d", (int) fds[connectionsCount * 2].revents);
            SOCKET connectedSocket = accept(listenSocket, (struct sockaddr *) NULL, NULL);

            if (connectedSocket != NULL)
            {
                logg(LL_VERBOSE, "accept ", "Accepted", (int) fds[connectionsCount * 2].revents);
                activeConnections[connectionsCount].clientSocket = connectedSocket;
                activeConnections[connectionsCount].buffer_size = 0;
                activeConnections[connectionsCount].connectionStatus = CS_GETTING_REQUEST;
                activeConnections[connectionsCount].trackingId = rand() % 9000 + 1000;
                connectionsCount++;

                printf("[I] [%d] Accepted incoming connection\n", activeConnections[connectionsCount - 1].trackingId);
            }
        }
    }
}