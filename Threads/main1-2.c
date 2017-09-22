#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void * routine(void * arg)
{
    for (int i = 0; i < 10; ++i)
    {
        //printf("Child %d\n", i+1);

        write(2, "Child\n",6);
    }

    return 0;
}

int main()
{
    pthread_t thread;
    void * status;

    pthread_create(&thread, NULL, routine, NULL);

    int a = pthread_join(thread, &status);

    for (int i = 0; i < 10; ++i)
    {
        //printf("Parent %d\n", i+1);
        write(2, "Paren\n",6);
    }

    fprintf(stderr, "%d\n", status);
}
