//
// Created by glavak on 14.09.17.
//

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

int counter = 0;

void * deathrattle(void * arg)
{
    printf("Terminated\n");
}

void * routine(void * arg)
{
    pthread_cleanup_push(deathrattle, NULL) ;

            while (1)
            {
                int a = 0;
                for (int j = 0; j < 10000; ++j)
                {
                    a += sin(j);
                }
                counter++;
                printf("Child %d\n", counter);
            }

    pthread_cleanup_pop(0);
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

    for (int i = 0; i < 10; ++i)
    {
        //printf("Parent %d\n", counter);
        sleep(1);
    }

    pthread_join(thread, NULL);
}
