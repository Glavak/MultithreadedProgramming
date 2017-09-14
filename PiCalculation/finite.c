#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct job
{
    unsigned int firstIteration;
    unsigned int lastIteration;
};

void * routine(struct job * job)
{
    double result = 0;

    unsigned int first = job->firstIteration;
    unsigned int last = job->lastIteration;
    free(job);

    printf("Calc [%d; %d]\n", first, last);

    for (int i = first; i <= last; ++i)
    {
        result += 1.0 / (i * 4.0 + 1.0);
        result -= 1.0 / (i * 4.0 + 3.0);
    }

    double * resultPtr = malloc(sizeof(double));
    *resultPtr = result;
    pthread_exit(resultPtr);
}

double getPi(unsigned int iterations, unsigned int threadsCount)
{
    pthread_t * threads = malloc(sizeof(pthread_t) * threadsCount);

    for (unsigned int i = 0; i < threadsCount; ++i)
    {
        struct job * job = malloc(sizeof(struct job));
        job->firstIteration = i * (iterations / threadsCount);
        job->lastIteration = (i + 1) * (iterations / threadsCount) - 1;

        pthread_create(threads + i, NULL, routine, job);
    }

    double pi = 0;

    for (int i = 0; i < threadsCount; ++i)
    {
        double * threadResult;
        pthread_join(threads[i], &threadResult);

        pi += *threadResult;
        free(threadResult);
    }

    free(threads);

    pi *= 4;

    return pi;
}

int main(int argc, char ** argv)
{
    unsigned int iterations = 20000;
    unsigned int threadsCount = 4;

    if (argc >= 2)
    {
        char * end;
        iterations = (unsigned int) strtol(argv[1], &end, 10);
        if (end == argv[1])
        {
            printf("Incorrect iterations count format\n");
            return 1;
        }
    }

    if (argc >= 3)
    {
        char * end;
        threadsCount = (unsigned int) strtol(argv[2], &end, 10);
        if (end == argv[2])
        {
            printf("Incorrect thread count format\n");
            return 1;
        }
    }

    double pi = getPi(iterations, threadsCount);

    printf("Pi ~ %.15g\n", pi);

    return 0;
}