#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>

sem_t detailsA;
sem_t detailsB;
sem_t detailsAB;
sem_t detailsC;

void * workerA(void * arg)
{
    while (1)
    {
        sleep(1);
        sem_post(&detailsA);
        printf("A produced\n");
    }
}

void * workerB(void * arg)
{
    while (1)
    {
        sleep(2);
        sem_post(&detailsB);
        printf("B produced\n");
    }
}

void * workerAB(void * arg)
{
    while (1)
    {
        sem_wait(&detailsA);
        sem_wait(&detailsB);
        sem_post(&detailsAB);
        printf("AB produced\n");
    }
}

void * workerC(void * arg)
{
    while (1)
    {
        sleep(3);
        sem_post(&detailsC);
        printf("C produced\n");
    }
}

void * workerWidget(void * arg)
{
    int i = 10;
    while (--i)
    {
        sem_wait(&detailsAB);
        sem_wait(&detailsC);
        printf("Widget produced\n");
    }
}

int main()
{
    pthread_t threadA;
    pthread_t threadB;
    pthread_t threadAB;
    pthread_t threadC;
    pthread_t threadWidgets;


    pthread_create(&threadA, NULL, workerA, NULL);
    pthread_create(&threadB, NULL, workerB, NULL);
    pthread_create(&threadAB, NULL, workerAB, NULL);
    pthread_create(&threadC, NULL, workerC, NULL);
    pthread_create(&threadWidgets, NULL, workerWidget, NULL);

    pthread_join(threadWidgets, NULL);

    return 0;
}