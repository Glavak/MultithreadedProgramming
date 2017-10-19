//
// Created by glavak on 04.09.17.
//

#ifndef HTTPSERVER_CONNECTION_H
#define HTTPSERVER_CONNECTION_H

#include <glob.h>
#include "common.h"

enum connectionStatus
{
    CS_GETTING_REQUEST = 0,

    CS_CONNECTING_TO_SERVER = 10,
    CS_WRITING_REQUEST = 11,
    CS_FORWARDING_REQUEST = 12,
    CS_FORWARDING_RESPONSE = 13,

    CS_RESPONDING_FROM_CACHE = 20
};

struct connection
{
    SOCKET clientSocket;
    SOCKET serverSocket;
    enum connectionStatus connectionStatus;

    char * buffer;
    size_t buffer_size;

    long left_to_download;

    int trackingId;
};

#endif //HTTPSERVER_CONNECTION_H
