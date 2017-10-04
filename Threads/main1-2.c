#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define PRINTS 10

static pthread_mutex_t parent_mutex[PRINTS];
static pthread_mutex_t child_mutex[PRINTS];

static void * routine(void * arg)
{
    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_mutex_lock(&child_mutex[i]);
    }

    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_mutex_lock(&parent_mutex[i]);
        pthread_mutex_unlock(&parent_mutex[i]);

        printf("Child %d\n", i + 1);
        pthread_mutex_unlock(&child_mutex[i]);
    }

    return 0;
}

int main()
{
    pthread_t thread;

    pthread_mutexattr_t attrs;
    pthread_mutexattr_init(&attrs);
    pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_ERRORCHECK);

    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_mutex_init(&parent_mutex[i], &attrs);
        pthread_mutex_init(&child_mutex[i], &attrs);

        pthread_mutex_lock(&parent_mutex[i]);
    }

    pthread_create(&thread, NULL, routine, NULL);

    sleep(1);

    for (int i = 0; i < PRINTS; ++i)
    {
        if (i > 0)
        {
            // Wait for child to print "Child {i-1}"
            pthread_mutex_lock(&child_mutex[i - 1]);
            pthread_mutex_unlock(&child_mutex[i - 1]);
        }

        printf("Parent %d\n", i + 1);
        pthread_mutex_unlock(&parent_mutex[i]);
    }

    pthread_join(thread, NULL);

    for (int i = 0; i < PRINTS; ++i)
    {
        pthread_mutex_destroy(&parent_mutex[i]);
        pthread_mutex_destroy(&child_mutex[i]);
    }
}
