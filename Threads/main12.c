#include <stdio.h>
#include <pthread.h>

#define PRINTS 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void * routine(void * arg)
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_cond_signal(&cond);
        printf("Child %d\n", i + 1);
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    return 0;
}

int main()
{
    pthread_t thread;

    pthread_create(&thread, NULL, routine, NULL);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_cond_signal(&cond);
        printf("Parent %d\n", i + 1);
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    pthread_cond_signal(&cond);

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&mutex);
}
