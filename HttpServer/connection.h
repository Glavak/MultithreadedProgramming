//
// Created by glavak on 04.09.17.
//

#ifndef HTTPSERVER_CONNECTION_H
#define HTTPSERVER_CONNECTION_H

#include "common.h"

enum connectionStatus
{
    CS_CONNECTING_TO_SERVER = 0,
    CS_TRANSMITTING_REQUEST = 1,
    CS_TRANSMITTING_RESPONSE = 2
};

struct connection
{
    SOCKET clientSocket;
    SOCKET serverSocket;
    enum connectionStatus connectionStatus;
};

#endif //HTTPSERVER_CONNECTION_H
