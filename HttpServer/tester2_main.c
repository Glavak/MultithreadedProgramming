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
    struct sockaddr_in result;

    result.sin_family = AF_INET;
    result.sin_port = htons(4242);

    inet_pton(AF_INET, "127.0.0.1", &result.sin_addr);

    int connectedSocket= socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    connect(connectedSocket, (struct sockaddr *) &result, sizeof(result));

    sleep(1);

    shutdown(connectedSocket, SHUT_WR);

    //ssize_t a = read(connectedSocket, NULL, 100);
    int a = write(connectedSocket, &connectedSocket, 1);

    printf("%d\n", a);
    perror("asd");

    getchar();
}