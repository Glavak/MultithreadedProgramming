#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

static volatile int keepRunning = 1;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t mybarrier;
static unsigned int maxIteration = 0;

void intHandler(int _)
{
    keepRunning = 0;
}

struct job
{
    unsigned int threadNumber;
    unsigned int threadsCount;
};

void * routine(struct job * job)
{
    double result = 0;

    unsigned int number = job->threadNumber;
    unsigned int count = job->threadsCount;
    free(job);

    unsigned int i;
    for (i = number; keepRunning; i += count)
    {
        result += 1.0 / (i * 4.0 + 1.0);
        result -= 1.0 / (i * 4.0 + 3.0);
    }

    pthread_mutex_lock(&mutex);
    if (i > maxIteration)
    {
        maxIteration = i;
    }
    pthread_mutex_unlock(&mutex);

    pthread_barrier_wait(&mybarrier);
    int ourMaxIteration = (maxIteration / 4) * 4 + number;

    for (; i <= ourMaxIteration; i += count)
    {
        result += 1.0 / (i * 4.0 + 1.0);
        result -= 1.0 / (i * 4.0 + 3.0);
    }

    double * resultPtr = malloc(sizeof(double));
    *resultPtr = result;
    pthread_exit(resultPtr);
}

double getPi(unsigned int threadsCount)
{
    pthread_t * threads = malloc(sizeof(pthread_t) * threadsCount);

    for (unsigned int i = 0; i < threadsCount; ++i)
    {
        struct job * job = malloc(sizeof(struct job));
        job->threadNumber = i;
        job->threadsCount = threadsCount;

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
    unsigned int threadsCount = 4;

    if (argc >= 2)
    {
        char * end;
        threadsCount = (unsigned int) strtol(argv[1], &end, 10);
        if (end == argv[1])
        {
            printf("Incorrect thread count format\n");
            return 1;
        }
    }

    pthread_barrier_init(&mybarrier, NULL, threadsCount);

    signal(SIGINT, intHandler);

    printf("Press Ctrl+C to stop counting and print result\n");

    double pi = getPi(threadsCount);

    printf("Pi ~ %.15g\n", pi);

    return 0;
}