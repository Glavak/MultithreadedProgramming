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

    CS_WRITING_REQUEST = 11,
    CS_FORWARDING_REQUEST = 12,
    CS_FORWARDING_RESPONSE = 13
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
