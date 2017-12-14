//
// Created by glavak on 16.10.17.
//

#ifndef HTTPSERVER_HTTPUTILS_H
#define HTTPSERVER_HTTPUTILS_H

#include "memory.h"

int isMethodGet(char * httpData);

// Example: POST http://ex.ru/dsa HTTP/1.0\n... --> http://ex.ru/dsa
// Must free returned result after usage
char * getUrlFromData(char * httpData, size_t  dataLength);

// Example: http://ex.ru/dsa --> ex.ru
// Must free returned result after usage
char * getHostFromUrl(char * url);

// Example: HTTP/1.1 200 OK\r\n --> 200
int getResponseCodeFromData(char * httpData);

// -1 if no Content-Length header found (yet?)
long getContentLengthFromData(char * httpData, size_t dataLength);

enum ConnectionCloseType
{
    CCT_Close,
    CCT_KeepAlive,
    CCT_UnknownYet
};

enum ConnectionCloseType isConnectionClose(char * httpData, size_t dataLength);

int isDataHasAllHeaders(char * httpData, size_t dataLength);

int isMethodHasPayload(char * method);

int isResponseHasPayload(int statusCode);

#endif //HTTPSERVER_HTTPUTILS_H
