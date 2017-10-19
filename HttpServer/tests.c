//
// Created by glavak on 19.10.17.
//

#ifndef HTTPSERVER_TESTS_H
#define HTTPSERVER_TESTS_H

#include <stdlib.h>
#include <stdio.h>
#include "httpUtils.h"

void myAssert(int value, char * message)
{
    if(!value)
    {
        printf("Error: %s\n", message);
        exit(1);
    }
}

int main(int argc, const char * argv[])
{
    myAssert(getContentLengthFromData("GET asdsdaf HTTP\r\nAdfasdf: fdsa\r\nContent-Length: 123\r\n\r\n", 56) == 123, "getContentLengthFromData() 1");
    myAssert(getContentLengthFromData("GET asdsdaf HTTP\r\nAdfasdf: fdsa\r\nContent-L", 56) == -1, "getContentLengthFromData() 2");
    myAssert(getContentLengthFromData("GET asdsdaf HTTP\r\nAdfasdf: fdsa\r\n\r\n", 56) == -1, "getContentLengthFromData() 2");

    return 0;
}

#endif //HTTPSERVER_TESTS_H
