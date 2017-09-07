//
// Created by glavak on 03.09.17.
//

#include <netinet/in.h>
#include <arpa/inet.h>
#include "addressUtils.h"
#include "logging.h"

struct sockaddr_in getListenAddress(uint16_t port)
{
    struct sockaddr_in result;

    result.sin_addr.s_addr = htonl(INADDR_ANY);
    result.sin_family = AF_INET;
    result.sin_port = htons(port);

    return result;
}

struct sockaddr_in getServerAddress(char *ip, uint16_t port)
{
    struct sockaddr_in result;

    result.sin_family = AF_INET;
    result.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &result.sin_addr) <= 0)
    {
        log_error("getServerAddress", "Inet_pton error occurred");
    }

    return result;
}