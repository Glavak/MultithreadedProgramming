#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>

#define PRINTS 10

sem_t s1;
sem_t s2;

static void * routine(void * arg)
{
    for (int i = 0; i < PRINTS; ++i)
    {
        sem_wait(&s2);
        printf("Child %d\n", i + 1);
        sem_post(&s1);
    }

    return 0;
}

int main()
{
    pthread_t thread;

    sem_init(&s1, 0, 1);
    sem_init(&s2, 0, 0);

    pthread_create(&thread, NULL, routine, NULL);

    for (int i = 0; i < PRINTS; ++i)
    {
        sem_wait(&s1);
        printf("Parent %d\n", i + 1);
        sem_post(&s2);
    }

    pthread_join(thread, NULL);

    sem_destroy(&s1);
    sem_destroy(&s2);
}
