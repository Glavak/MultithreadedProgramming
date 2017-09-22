//
// Created by glavak on 14.09.17.
//

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void * routine(void * arg)
{
    char ** arr = (char **)arg;
    while (**arr != '\0')
    {
        fprintf(stderr, "%s\n", *arr);
        arr++;
        //sleep(1);
    }
    return 0;
}

int main()
{
    pthread_t thread,thread1,thread2,thread3;
    void * status;

    char * strs[20000];
    strs[0] = "0 Some string";
    strs[1] = "0 Another string";
    strs[2] = "0 One more";
    strs[3] = "0 And the last one";
    for (int i = 0; i < 19000; ++i)
    {
        strs[i] = "0 sdfafd";
    }
    strs[19000] = "";

    pthread_create(&thread, NULL, routine, strs);

    char * strs1[20000];
    strs1[0] = "1 Only two";
    strs1[1] = "1 strings here";
    for (int i = 0; i < 19000; ++i)
    {
        strs1[i] = "1 sdfafd";
    }
    strs1[19000] = "";

    pthread_create(&thread1, NULL, routine, strs1);

    char * strs2[20000];
    strs2[0] = "2 The one";
    for (int i = 0; i < 19000; ++i)
    {
        strs2[i] = "2 sdfafd";
    }
    strs2[19000] = "";

    pthread_create(&thread2, NULL, routine, strs2);

    char * strs3[20000];
    strs3[0] = "3 The 42";
    for (int i = 0; i < 19000; ++i)
    {
        strs3[i] = "3 sdfafd";
    }
    strs3[19000] = "";

    pthread_create(&thread3, NULL, routine, strs3);

    pthread_join(thread, &status);
    pthread_join(thread1, &status);
    pthread_join(thread2, &status);
    pthread_join(thread3, &status);
}
