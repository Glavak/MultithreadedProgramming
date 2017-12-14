//
// Created by glavak on 16.10.17.
//

#include "httpUtils.h"

#include <stdlib.h>
#include <stdio.h>

int isMethodGet(char * httpData)
{
    return httpData[0] == 'G' &&
           httpData[1] == 'E' &&
           httpData[2] == 'T';
}

char * getUrlFromData(char * httpData, size_t dataLength)
{
    if (dataLength < 6)
    {
        return NULL;
    }

    int startUrl = 0;
    for (; httpData[startUrl] != ' '; ++startUrl)
    {
        if (startUrl + 1 >= dataLength)
        {
            return NULL;
        }
    }
    startUrl++;

    int endUrl = startUrl;
    for (; httpData[endUrl] != ' '; ++endUrl)
    {
        if (endUrl + 1 >= dataLength)
        {
            return NULL;
        }
    }

    char * result = malloc(sizeof(char) * (endUrl - startUrl + 1));

    memcpy(result, httpData + startUrl, (size_t) (endUrl - startUrl));
    result[endUrl - startUrl] = '\0';

    return result;
}

char * getHostFromUrl(char * url)
{
    url += 7; // Skip http://

    int endHost = 0;
    for (; url[endHost] != '/'; ++endHost)
    {
    }

    char * result = malloc(sizeof(char) * (endHost + 1));

    memcpy(result, url, (size_t) (endHost));
    result[endHost] = '\0';

    return result;
}

int getResponseCodeFromData(char * httpData)
{
    httpData += 9;

    char * end;

    int response = (int) strtol(httpData, &end, 10);
    if (end == httpData || response <= 0)
    {
        // Something went wrong
        return -1;
    }
    return response;
}

long getContentLengthFromData(char * httpData, size_t dataLength)
{
    char * headerStart = "Content-Length:";
    size_t headerStartLen = strlen(headerStart);

    int i = 0;
    while (1)
    {
        while (httpData[i] != '\n')
        {
            if (i + 1 >= dataLength)
            {
                return -1;
            }
            i++;
        }

        i++;

        int matches = 1;
        for (int j = 0; j < headerStartLen; ++j)
        {
            if (httpData[i + j] != headerStart[j])
            {
                matches = 0;
                break;
            }
        }

        if (matches)
        {
            i += headerStartLen;

            char * end;

            long contentLength = strtol(httpData + i, &end, 10);
            if (end == httpData + i || contentLength <= 0)
            {
                // Something went wrong
                return -1;
            }
            return contentLength;
        }
    }
}

enum ConnectionCloseType isConnectionClose(char * httpData, size_t dataLength)
{
    char * headerStart = "Connection:";
    size_t headerStartLen = strlen(headerStart);

    int i = 0;
    while (1)
    {
        while (httpData[i] != '\n')
        {
            if (i + 1 >= dataLength)
            {
                return CCT_UnknownYet;
            }
            i++;
        }

        i++;

        int matches = 1;
        for (int j = 0; j < headerStartLen; ++j)
        {
            if (httpData[i + j] != headerStart[j])
            {
                matches = 0;
                break;
            }
        }

        if (matches)
        {
            i += headerStartLen;

            char * end;

            long contentLength = strtol(httpData + i, &end, 10);
            if (end == httpData + i || contentLength <= 0)
            {
                // Something went wrong
                return -1;
            }
            return contentLength;
        }
    }
}

int isDataHasAllHeaders(char * httpData, size_t dataLength)
{
    for (int i = 0; i < dataLength - 2; ++i)
    {
        if (httpData[i] == '\n' && httpData[i + 2] == '\n')
        {
            return 1;
        }
    }
    return 0;
}

int isMethodHasPayload(char * method)
{
    if (method[0] == 'G' &&
        method[1] == 'E' &&
        method[2] == 'T')
    {
        return 0;
    }

    if (method[0] == 'H' &&
        method[1] == 'E' &&
        method[2] == 'A' &&
        method[2] == 'D')
    {
        return 0;
    }

    return 1;
}

int isResponseHasPayload(int statusCode)
{
    if (statusCode == 204)
    {
        return 0;
    }

    if (statusCode == 304)
    {
        return 0;
    }

    if (statusCode >= 100 && statusCode < 200)
    {
        return 0;
    }

    return 1;
}