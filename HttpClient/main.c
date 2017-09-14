#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define SOCKET int

struct sockaddr_in getServerAddress(char *ip, uint16_t port)
{
    struct sockaddr_in result;

    result.sin_family = AF_INET;
    result.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &result.sin_addr) <= 0)
    {
        perror("getServerAddress");
        exit(1);
    }

    return result;
}

int main(int argc, const char *argv[])
{
    char *host = (char *) argv[1];
    char *path = host;

    while (*path != '/')
    {
        path++;
    }
    *path = '\0';
    path++;

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("main");
    }

    struct sockaddr_in address = getServerAddress(host, 80);
    if (connect(serverSocket, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        perror("main");
        return 1;
    }

    char recievedText[65500];
    int totalTextStored = 0;

    int totalNewLinesPrinted = 0;
    int lineStartPosition = 0;

    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    FD_ZERO(&efds);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while (1)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(serverSocket, &wfds);

        select(serverSocket + 1, &rfds, &wfds, &efds, &tv);

        if (FD_ISSET(serverSocket, &wfds))
        {
            printf("Sending request..\n");
            char text[1024];
            sprintf(text,
                    "GET /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nAccept-Encoding: utf8\r\nAccept-Language: en-US,en;q=0.8,ru;q=0.6\r\n\r\n",
                    path, host);
            write(serverSocket, text, strlen(text));
            break;
        }
    }

    printf("Reading response..\n");

    int promptPrinted = 0;

    while (1)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(fileno(stdin), &rfds);
        FD_SET(serverSocket, &rfds);

        select(serverSocket + 1, &rfds, &wfds, &efds, &tv);

        if (FD_ISSET(serverSocket, &rfds))
        {
            ssize_t readBytes = read(serverSocket, recievedText + totalTextStored, 1024);
            totalTextStored += readBytes;

            for (int i = lineStartPosition; i < totalTextStored; ++i)
            {
                if (totalNewLinesPrinted >= 25)
                {
                    if (!promptPrinted)
                    {
                        printf("=== press enter to see another page of result ===\n");
                        promptPrinted = 1;
                    }
                    break;
                }

                if (recievedText[i] == '\n')
                {
                    write(fileno(stdout), recievedText + lineStartPosition, (size_t) (i - lineStartPosition) + 1);
                    lineStartPosition = i + 1;
                    totalNewLinesPrinted++;
                }
            }
        }

        if (FD_ISSET(fileno(stdin), &rfds))
        {
            char buff;
            read(fileno(stdin), &buff, 1);

            totalNewLinesPrinted = 0;

            for (int i = lineStartPosition; i < totalTextStored; ++i)
            {
                if (totalNewLinesPrinted >= 25)
                {
                    break;
                }

                if (recievedText[i] == '\n')
                {
                    write(fileno(stdout), recievedText + lineStartPosition, (size_t) (i - lineStartPosition) + 1);
                    lineStartPosition = i + 1;
                    totalNewLinesPrinted++;
                }
            }

            if (lineStartPosition == totalTextStored)
            {
                printf("=== end of data. press enter to quit ===\n");
                break;
            }
            else
            {
                printf("=== press enter to see another page of result ===\n");
            }
        }
    }

    char buff;
    read(fileno(stdin), &buff, 1);

    close(serverSocket);

    return 0;
}