//
// Created by glavak on 14.09.17.
//

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void * deathrattle(void * arg)
{
    printf("Terminated\n");
}

void * routine(void * arg)
{
    pthread_cleanup_push(deathrattle, NULL) ;

            for (int i = 0; i < 10; ++i)
            {
                printf("Child %d\n", i + 1);
                sleep(1);
            }

    pthread_cleanup_pop(0);
    pthread_exit(0);
}

int main()
{
    pthread_t thread;
    void * status;

    pthread_create(&thread, NULL, routine, NULL);

    sleep(2);

    if (pthread_cancel(thread))
    {
        printf("error\n");
    }

    sleep(2);
}
