//
// Created by glavak on 03.09.17.
//

#ifndef HTTPSERVER_ADDRESSUTILS_H
#define HTTPSERVER_ADDRESSUTILS_H

struct sockaddr_in getListenAddress(uint16_t port);

struct sockaddr_in getServerAddress(char *ip, uint16_t port);

int getIpByHostname(char * hostname, char * ip);

#endif //HTTPSERVER_ADDRESSUTILS_H
