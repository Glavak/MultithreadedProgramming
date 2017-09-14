#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include "logging.h"
#include "addressUtils.h"
#include "common.h"
#include "connection.h"

SOCKET initializeListenSocket(struct sockaddr_in listenAddress)
{
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listenSocket < 0)
    {
        log_error("initializeListenSocket", "can't create socket");
    }

    if (bind(listenSocket, (struct sockaddr *) &listenAddress, sizeof(listenAddress)))
    {
        log_error("initializeListenSocket", "can't bind socket");
    }

    printf("Bound, starting listening...\n");
    if (listen(listenSocket, 10))
    {
        log_error("initializeListenSocket", "can't start listening");
    }

    return listenSocket;
}

SOCKET initializeServerSocket(struct sockaddr_in address)
{
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (clientSocket < 0)
    {
        log_error("initializeServerSocket", "can't create socket");
    }

    if (connect(clientSocket, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            printf("Connecting to server started\n");
            return clientSocket;
        }

        log_error("initializeServerSocket", "can't connect");
    }

    printf("Connected to server\n");

    return clientSocket;
}

void readArgs(int argc, const char *argv[], uint16_t *outLocalPort, char **outServerIp, uint16_t *outServerPort)
{
    if (argc < 4)
    {
        char message[255];
        sprintf(message, "Usage: %s localPort serverIp serverPort\n", argv[0]);
        log_error("main", message);
    }

    char *end;

    long localPort = strtol(argv[1], &end, 10);
    if (*end != '\0' || localPort <= 0 || localPort >= UINT16_MAX)
    {
        log_error("main", "Incorrect localPort format\n");
    }
    *outLocalPort = (uint16_t) localPort;

    *outServerIp = (char *) argv[2];

    long serverPort = strtol(argv[3], &end, 10);
    if (*end != '\0' || serverPort <= 0 || serverPort >= UINT16_MAX)
    {
        log_error("main", "Incorrect serverPort format\n");
    }
    *outServerPort = (uint16_t) serverPort;
}

int main(int argc, const char *argv[])
{
    uint16_t localPort, serverPort;
    char *serverIp;

    readArgs(argc, argv, &localPort, &serverIp, &serverPort);

    struct connection activeConnections[1024];
    size_t connectionsCount = 0;
    SOCKET listenSocket = initializeListenSocket(getListenAddress(localPort));
    struct sockaddr_in address = getServerAddress(serverIp, serverPort);

    while (1)
    {
        struct pollfd fds[200];
        for (int i = 0; i < connectionsCount; ++i)
        {
            fds[i * 2].fd = activeConnections[i].clientSocket;
            fds[i * 2 + 1].fd = activeConnections[i].serverSocket;
            switch (activeConnections[i].connectionStatus)
            {
                case CS_CONNECTING_TO_SERVER:
                    fds[i * 2].events = 0;
                    fds[i * 2 + 1].events = POLLOUT;
                    break;
                case CS_TRANSMITTING_REQUEST:
                    fds[i * 2].events = POLLIN;
                    fds[i * 2 + 1].events = POLLOUT;
                    break;
                case CS_TRANSMITTING_RESPONSE:
                    fds[i * 2].events = POLLOUT | POLLIN;
                    fds[i * 2 + 1].events = POLLIN;
                    break;
            }
        }

        fds[connectionsCount * 2].fd = listenSocket;
        fds[connectionsCount * 2].events = POLLIN;

        int polled = poll(fds, connectionsCount * 2 + 1, 1000 * 2);
        if (polled < 0)
        {
            log_error("eventLoop", "polling error");
        }

        char buff[BUFFER_SIZE];
        for (int i = 0; i < connectionsCount; ++i)
        {
            switch (activeConnections[i].connectionStatus)
            {
                case CS_CONNECTING_TO_SERVER:
                    if (fds[i * 2 + 1].revents == POLLOUT)
                    {
                        activeConnections[i].connectionStatus = CS_TRANSMITTING_REQUEST;
                    }
                    break;
                case CS_TRANSMITTING_REQUEST:
                    if (fds[i * 2].revents == POLLIN && fds[i * 2 + 1].revents == POLLOUT)
                    {
                        ssize_t readCount = read(activeConnections[i].clientSocket, buff, BUFFER_SIZE);
                        printf("Transmitting request, %d bytes...\n", (int) readCount);
                        if (readCount > 0)
                        {
                            write(activeConnections[i].serverSocket, buff, (size_t) readCount);
                            printf("Wrote %d bytes\n", (int) readCount);
                        }

                        if (readCount < BUFFER_SIZE)
                        {
                            activeConnections[i].connectionStatus = CS_TRANSMITTING_RESPONSE;
                        }
                    }
                    break;
                case CS_TRANSMITTING_RESPONSE:
                    if (fds[i * 2].revents == POLLIN && read(activeConnections[i].serverSocket, buff, 1) == 0)
                    {
                        printf("Connection closed \n");
                        close(activeConnections[i].clientSocket);
                        close(activeConnections[i].serverSocket);

                        activeConnections[i] = activeConnections[connectionsCount - 1];
                        connectionsCount--;
                    }
                    else if (fds[i * 2].revents == POLLOUT && fds[i * 2 + 1].revents == POLLIN)
                    {
                        ssize_t readCount = read(activeConnections[i].serverSocket, buff, BUFFER_SIZE);
                        printf("Transmitting response, %d bytes...\n", (int) readCount);
                        if (readCount > 0)
                        {
                            write(activeConnections[i].clientSocket, buff, (size_t) readCount);
                            printf("Wrote %d bytes\n", (int) readCount);
                        }
                    }
                    break;
            }
        }

        if (fds[connectionsCount * 2].revents == POLLIN)
        {
            printf("Accepted!\n");
            SOCKET serverSocket = initializeServerSocket(address);

            SOCKET connectedSocket = accept(listenSocket, (struct sockaddr *) NULL, NULL);
            activeConnections[connectionsCount].clientSocket = connectedSocket;
            activeConnections[connectionsCount].serverSocket = serverSocket;
            activeConnections[connectionsCount].connectionStatus = CS_CONNECTING_TO_SERVER;
            connectionsCount++;
        }
    }
}