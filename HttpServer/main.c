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

    if (listen(listenSocket, 10))
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

static struct connection activeConnections[1024];
static struct cacheEntry cache[1024];
static size_t connectionsCount = 0;
static size_t cacheSize = 0;
static SOCKET listenSocket;

static void exit_handler(void)
{
    close(listenSocket);
    // TODO close activeConnections sockets
}

int main(int argc, const char * argv[])
{
    uint16_t localPort;
    readArgs(argc, argv, &localPort);

    atexit(exit_handler);
    srand((unsigned int) time(NULL));

    listenSocket = initializeListenSocket(getListenAddress(localPort));
    logg(LL_INFO, NULL, "Server started\n");

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

                case CS_CONNECTING_TO_SERVER:
                    fds[i * 2].events = 0;
                    fds[i * 2 + 1].events = POLLOUT;
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

                case CS_RESPONDING_FROM_CACHE:
                    fds[i * 2].events = POLLOUT;
                    fds[i * 2 + 1].events = 0;
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

        char buff[BUFFER_SIZE];
        for (int i = 0; i < connectionsCount; ++i)
        {
            switch (activeConnections[i].connectionStatus)
            {
                case CS_GETTING_REQUEST:
                    if (fds[i * 2].revents == POLLIN)
                    {
                        ssize_t readCount = read(activeConnections[i].clientSocket, buff, BUFFER_SIZE);

                        if (readCount == 0)
                        {
                            logg_track(LL_VERBOSE, activeConnections[i].trackingId, "Client-side socket closed");
                            if (activeConnections[i].buffer_size > 0)
                            {
                                free(activeConnections[i].buffer);
                            }
                            close(activeConnections[i].clientSocket);
                            activeConnections[i] = activeConnections[connectionsCount-1];
                            connectionsCount--;
                            break;
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
                            char * url = getUrlFromData(activeConnections[i].buffer,
                                                        activeConnections[i].buffer_size);
                            if (url != NULL)
                            {
                                if (!isMethodGet(activeConnections[i].buffer))
                                {
                                    logg_track(LL_WARNING, activeConnections[i].trackingId,
                                               "Only GET methods allowed, connection dropped");

                                    close(activeConnections[i].clientSocket);
                                    free(activeConnections[i].buffer);
                                    free(url);
                                    activeConnections[i] = activeConnections[connectionsCount - 1];
                                    connectionsCount--;
                                }
                                else
                                {
                                    int foundInCache = 0;
                                    for (int j = 0; j < cacheSize; ++j)
                                    {
                                        if (strcmp(cache[j].url, url) == 0)
                                        {
                                            if (cache[j].entryStatus == ES_VALID)
                                            {
                                                logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                                           "Found cache entry for %s, responding from cache", url);

                                                free(activeConnections[i].buffer);

                                                activeConnections[i].cacheEntryIndex = j;
                                                activeConnections[i].cacheBytesWritten = 0;
                                                activeConnections[i].connectionStatus = CS_RESPONDING_FROM_CACHE;
                                            }
                                            else if (cache[j].entryStatus == ES_DOWNLOADING)
                                            {
                                                logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                                           "Found not finished cache entry for %s, responding from server",
                                                           url);

                                                activeConnections[i].cacheEntryIndex = -1; // Do not save to cache, as other client already working on it
                                                activeConnections[i].connectionStatus = CS_CONNECTING_TO_SERVER;
                                                activeConnections[i].serverSocket = initServerSocketToUrl(url);
                                                editRequestToSendToServer(&activeConnections[i]);
                                            }
                                            else
                                            {
                                                continue;
                                            }

                                            foundInCache = 1;
                                            free(url);
                                            break;
                                        }
                                    }

                                    if (!foundInCache)
                                    {
                                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                                   "Cache entry for %s not found, responding from server", url);

                                        cache[cacheSize].url = url;
                                        cache[cacheSize].entryStatus = ES_DOWNLOADING;
                                        cache[cacheSize].dataCount = 0;
                                        cacheSize++;

                                        activeConnections[i].cacheEntryIndex = (int) (cacheSize - 1);
                                        activeConnections[i].connectionStatus = CS_CONNECTING_TO_SERVER;
                                        activeConnections[i].serverSocket = initServerSocketToUrl(url);
                                        editRequestToSendToServer(&activeConnections[i]);
                                    }
                                }
                            }
                        }
                    }
                    break;

                case CS_CONNECTING_TO_SERVER:
                    if (fds[i * 2 + 1].revents == POLLOUT)
                    {
                        activeConnections[i].connectionStatus = CS_WRITING_REQUEST;
                    }
                    break;
                case CS_WRITING_REQUEST:
                    if (fds[i * 2 + 1].revents == POLLOUT)
                    {
                        write(activeConnections[i].serverSocket, activeConnections[i].buffer,
                              activeConnections[i].buffer_size);
                        //write(1, activeConnections[i].buffer, activeConnections[i].buffer_size);
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
                case CS_FORWARDING_REQUEST:
                    if (fds[i * 2].revents == POLLIN && fds[i * 2 + 1].revents == POLLOUT)
                    {
                        ssize_t readCount = read(activeConnections[i].clientSocket, buff, BUFFER_SIZE);
                        if (readCount > 0)
                        {
                            write(activeConnections[i].serverSocket, buff, (size_t) readCount);
                            logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                       "Forwarded %zi bytes of request", readCount);
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
                    if (fds[i * 2].revents == POLLOUT && fds[i * 2 + 1].revents == POLLIN)
                    {
                        ssize_t readCount = read(activeConnections[i].serverSocket, buff, BUFFER_SIZE);
                        if (readCount == 0)
                        {
                            logg_track(LL_INFO, activeConnections[i].trackingId, "Transmission over, socket closed");
                            if (activeConnections[i].cacheEntryIndex != -1)
                            {
                                cache[activeConnections[i].cacheEntryIndex].entryStatus = ES_VALID;
                            }

                            free(activeConnections[i].buffer);
                            close(activeConnections[i].clientSocket);
                            close(activeConnections[i].serverSocket);

                            activeConnections[i] = activeConnections[connectionsCount - 1];
                            connectionsCount--;
                            continue;
                        }

                        write(activeConnections[i].clientSocket, buff, (size_t) readCount);
                        //write(1, buff, (size_t) readCount);
                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Forwarded %zi bytes of response\n", readCount);

                        if (activeConnections[i].cacheEntryIndex != -1)
                        {
                            struct cacheEntry * entry = &cache[activeConnections[i].cacheEntryIndex];

                            entry->dataCount += (size_t) readCount;
                            entry->data = realloc(entry->data,
                                                  entry->dataCount);

                            char * dest = entry->data + entry->dataCount - readCount;

                            memcpy(dest, buff, (size_t) readCount);
                        }

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
                                        if (statusCode != 200)
                                        {
                                            free(cache[activeConnections[i].cacheEntryIndex].data);
                                            cache[activeConnections[i].cacheEntryIndex].entryStatus = ES_INVALID;
                                            activeConnections[i].cacheEntryIndex = -1;
                                        }

                                        activeConnections[i].left_to_download =
                                                contentLength - activeConnections[i].buffer_size + (j + 3);

                                        free(activeConnections[i].buffer);
                                        activeConnections[i].buffer_size = 0;

                                        if (activeConnections[i].left_to_download <= 0)
                                        {
                                            logg_track(LL_INFO, activeConnections[i].trackingId,
                                                       "Transmission over, Content-Length reached");
                                            if (activeConnections[i].cacheEntryIndex != -1)
                                            {
                                                cache[activeConnections[i].cacheEntryIndex].entryStatus = ES_VALID;
                                            }
                                            close(activeConnections[i].clientSocket);
                                            close(activeConnections[i].serverSocket);

                                            activeConnections[i] = activeConnections[connectionsCount - 1];
                                            connectionsCount--;
                                        }
                                    }
                                    else
                                    {
                                        free(cache[activeConnections[i].cacheEntryIndex].data);
                                        cache[activeConnections[i].cacheEntryIndex].entryStatus = ES_INVALID;
                                        activeConnections[i].cacheEntryIndex = -1;

                                        if (!isResponseHasPayload(statusCode))
                                        {
                                            logg_track(LL_INFO, activeConnections[i].trackingId,
                                                       "Transmission over, no payload for %d response", statusCode);
                                            free(activeConnections[i].buffer);
                                            close(activeConnections[i].clientSocket);
                                            close(activeConnections[i].serverSocket);

                                            activeConnections[i] = activeConnections[connectionsCount - 1];
                                            connectionsCount--;
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
                                if (activeConnections[i].cacheEntryIndex != -1)
                                {
                                    cache[activeConnections[i].cacheEntryIndex].entryStatus = ES_VALID;
                                }

                                close(activeConnections[i].clientSocket);
                                close(activeConnections[i].serverSocket);

                                activeConnections[i] = activeConnections[connectionsCount - 1];
                                connectionsCount--;
                            }
                        }
                    }
                    break;

                case CS_RESPONDING_FROM_CACHE:
                    if (fds[i * 2].revents == POLLOUT)
                    {
                        struct cacheEntry entry = cache[activeConnections[i].cacheEntryIndex];

                        char * sendData = entry.data + activeConnections[i].cacheBytesWritten;
                        size_t bytesToWrite = entry.dataCount - activeConnections[i].cacheBytesWritten;
                        if (bytesToWrite > WRITE_BY)
                        {
                            bytesToWrite = WRITE_BY;
                        }

                        ssize_t bytesWritten = write(activeConnections[i].clientSocket, sendData, bytesToWrite);
                        //write(1, sendData, bytesToWrite);

                        logg_track(LL_VERBOSE, activeConnections[i].trackingId,
                                   "Wrote %zi bytes from cache", bytesWritten);

                        activeConnections[i].cacheBytesWritten += bytesWritten;
                        if (activeConnections[i].cacheBytesWritten >= entry.dataCount)
                        {
                            logg_track(LL_INFO, activeConnections[i].trackingId,
                                       "Transmission over, responded from cache");
                            close(activeConnections[i].clientSocket);
                            close(activeConnections[i].serverSocket);

                            activeConnections[i] = activeConnections[connectionsCount - 1];
                            connectionsCount--;
                        }
                    }
            }
        }

        if (fds[connectionsCount * 2].revents == POLLIN)
        {
            SOCKET connectedSocket = accept(listenSocket, (struct sockaddr *) NULL, NULL);
            activeConnections[connectionsCount].clientSocket = connectedSocket;
            activeConnections[connectionsCount].buffer_size = 0;
            activeConnections[connectionsCount].connectionStatus = CS_GETTING_REQUEST;
            activeConnections[connectionsCount].trackingId = rand() % 9000 + 1000;
            connectionsCount++;

            printf("[I] [%d] Accepted incoming connection\n", activeConnections[connectionsCount - 1].trackingId);
        }
    }
}