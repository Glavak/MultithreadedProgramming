//
// Created by glavak on 03.09.17.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "logging.h"

void log_error(char * module, char * message)
{
    logg(LL_ERROR, module, message);
}

static char * getLevelName(enum loggingLevels level)
{
    switch (level)
    {
        case LL_ERROR:
            return "E";
        case LL_WARNING:
            return "W";
        case LL_INFO:
            return "I";
        case LL_VERBOSE:
            return "V";
    }
}

void logg(enum loggingLevels level, char * module, char * format, ...)
{
    if (level > LOGGING_LEVEL)
    {
        return;
    }

    va_list args;
    FILE * stream = stdout;
    if (level == LL_ERROR)
    {
        stream = stderr;
    }

    fprintf(stream, "[%s] ", getLevelName(level));
    if (module != NULL)
    {
        fprintf(stream, "[%s] ", module);
    }

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    fprintf(stream, "\n");

    if (level == LL_ERROR)
    {
        perror(module);
        exit(1);
    }
}

void logg_track(enum loggingLevels level, int trackingId, char * format, ...)
{
    if (level > LOGGING_LEVEL)
    {
        return;
    }

    va_list args;
    FILE * stream = stdout;
    if (level == LL_ERROR)
    {
        stream = stderr;
    }

    fprintf(stream, "[%s] [%d] ", getLevelName(level), trackingId);

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    fprintf(stream, "\n");

    if (level == LL_ERROR)
    {
        char s[50];
        sprintf(s, "request [%d]", trackingId);
        perror(s);
        exit(1);
    }
}