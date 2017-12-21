#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

int main(int argc, const char * argv[])
{
    int listenSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    struct sockaddr_in result;

    result.sin_family = AF_INET;
    result.sin_port = htons(4242);

    bind(listenSocket, (struct sockaddr *) &result, sizeof(result));

    listen(listenSocket, 10);

    int connectedSocket = accept(listenSocket, (struct sockaddr *) NULL, NULL);

    if (connectedSocket < 0)
    {
        perror("accept");
    }

    shutdown(connectedSocket, SHUT_RD);
    
    int a;
    int c = read(connectedSocket, &a, 1);
    
    printf("%d\n", c);

    getchar();

    close(connectedSocket);
}