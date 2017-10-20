//
// Created by glavak on 16.10.17.
//

#ifndef HTTPSERVER_CACHEENTRY_H
#define HTTPSERVER_CACHEENTRY_H

#include "stdlib.h"

enum entryStatus
{
    ES_DOWNLOADING,
    ES_VALID,
    ES_INVALID
};

struct cacheEntry
{
    char * url;
    char * data;
    size_t dataCount;
    enum entryStatus entryStatus;
};

#endif //HTTPSERVER_CACHEENTRY_H
