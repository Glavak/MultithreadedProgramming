//
// Created by glavak on 03.09.17.
//

#ifndef HTTPSERVER_LOGGING_H
#define HTTPSERVER_LOGGING_H

#define LOGGING_LEVEL LL_INFO

enum loggingLevels
{
    LL_ERROR,
    LL_WARNING,
    LL_INFO,
    LL_VERBOSE,
};

void log_error(char *module, char *message);

void logg(enum loggingLevels level, char * module, char * format, ...);

void logg_track(enum loggingLevels level, int trackingId, char * format, ...);

#endif //HTTPSERVER_LOGGING_H
