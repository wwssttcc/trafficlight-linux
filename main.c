#include<stdio.h>
#include"tl_tcpserver.h"
#include"tl_study.h"

int main(int argc, char **argv)
{
    pthread_t tcp_tid, tl_tid;
    printf("trafficlight v1.0\n");

    tcp_tid = tl_tcpserver_init();
    tl_tid = trafficLightInit();

    pthread_join(tcp_tid, NULL);
    pthread_join(tl_tid, NULL);
    return 0;
}
