//
// Created by glavak on 03.09.17.
//

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "logging.h"

void log_error(char *module, char *message)
{
    printf("%s: %s\n", module, message);
    printf("errno=%d\n", errno);
    perror(module);
    exit(1);
}