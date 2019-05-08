#include <stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include<pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include"tl_tcpserver.h"
#include"tl_study.h"

#define ERROR   -1
#define OK      0  

static char g_connect_num = 0;

int create_server_socket()
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0)
    {
        printf("sock_fd socket error\n");
        return ERROR;
    }
    struct sockaddr_in server_socket;
    bzero(&server_socket, sizeof(server_socket));
    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = htonl(INADDR_ANY);
    server_socket.sin_port = htons(TL_TCPSERVER_PORT);
		
		int opt = 1;
		setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(bind(sock_fd, (struct sockaddr*)&server_socket, sizeof(struct sockaddr_in)) < 0)
    {
        printf("sock_fd bind error\n");
        return ERROR;
    }

    if(listen(sock_fd, 5) < 0)
    {
        printf("sock_fd listen error\n");
        return ERROR;
    }

    return sock_fd;
}

void *tl_recv_thread(void *arg)
{
		int newsock = (int)arg;
		char buf[200];
		char send_buf[200];
		while(1)
		{
				int recv_num = read(newsock, buf, sizeof(buf) - 1);
				if(recv_num > 0)
				{
						buf[recv_num] = '\0';
						if(buf[0] == 0xaa)
						{
							send_trafficlight_info(send_buf, g_online_num);
							write(newsock, send_buf, g_online_num * sizeof(LightInfo));
							printf("------------trafficlight data------------\n");
						}
				}
				else if(recv_num == 0)
				{
						printf("client quit\n");
						break;
				}
				else
				{
						break;
				}
		}
		g_connect_num--;
		printf("connect num %d\n", g_connect_num);
		close(newsock);	
		pthread_exit(0);
}

void *tl_accept_thread(void *arg)
{
    int client_sock;
    struct sockaddr_in client_socket;
    int server_sock = (int)arg;
    socklen_t len = sizeof(client_socket);
    pthread_t thread_recv_id;
    pthread_attr_t pproprety;
    //printf("server_sock %d %d\n", server_sock, len);
    while(1)
    {
    		if(g_connect_num < 5)
    				client_sock = accept(server_sock, (struct sockaddr*)&client_socket, &len);
    		else
    		{
    				usleep(100*1000);
    				continue;
    		}
        //printf("accept ok\n");
        if(client_sock < 0)
        {
            printf("accept error\n");
            continue;
        }
        
        g_connect_num++;
        printf("get new client[%s:%d], connect num %d\n",\
               inet_ntoa(client_socket.sin_addr),\
                         ntohs(client_socket.sin_port), g_connect_num);
        pthread_create(&thread_recv_id, NULL, (void *)tl_recv_thread, (void *)client_sock);  
        pthread_detach(thread_recv_id); 

				
    }
    close(server_sock);
    pthread_exit("tl_accept_thread exit");
}

pthread_t tl_tcpserver_init()
{
    int server_sock;
    int client_sock;
    pthread_t tid;

    server_sock = create_server_socket();
		//printf("server_sock %d\n", server_sock);
    if(server_sock < 0)
    {
        printf("create_server_socket error\n");
        return ERROR;
    }
		
		pthread_create(&tid,NULL,tl_accept_thread,(void*)server_sock);
    return tid;
}
