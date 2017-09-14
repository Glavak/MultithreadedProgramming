#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void * fetch(void * arg)
{
    for (int i = 0; i < 10; ++i)
    {
        printf("Child %d\n", i+1);
        sleep(1);
    }
    pthread_exit(0);
}

int main()
{
    pthread_t thread;
    void * status;

    pthread_create(&thread, NULL, fetch, NULL);

    pthread_join(thread, &status);

    for (int i = 0; i < 10; ++i)
    {
        printf("Parent %d\n", i+1);
        sleep(1);
    }
}
