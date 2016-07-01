/*   D LICENSE
 *
 *   Copyright(c) 2010-2014 Netdp Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Netdp Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <termios.h>
#include <sys/epoll.h>

#ifndef __linux__
  #ifdef __FreeBSD__
    #include <sys/socket.h>
  #else
    #include <net/socket.h>
  #endif
#endif

#include <sys/time.h>

#include "anssock_intf.h"
#include "ans_errno.h"


#define TCP_CLIENT_SEND_LEN 2000

struct epoll_event events[10];

void tcp_send_thread(int fd)  
{  
    int data_num = 0;
    int data_len = 0;
    char send_data[5000];
    memset(send_data, 0, sizeof(send_data));
    int send_len = 0;

    while(1)
    {
        if(fd > 0)
        {
            data_num++;
            sprintf(send_data, "Hello, linux tcp server, num:%d !", data_num);
            send_len = 0;
            
            send_len = anssock_send(fd, send_data, 2000, 0);
            data_len += send_len;

            printf("send len %d, data num %d, data len:%d \n", send_len, data_num, data_len);
        }
        usleep(20000);
    }
    
}

int client_thread()
{
	
	int ret;
    int i = 0 ;
    int epfd;
    int data_num =0;
    struct sockaddr_in addr_in;  
    struct sockaddr_in remote_addr;  
    struct epoll_event event;
    char recv_buf[5000];
    int recv_len; 
    pthread_t id;
    int fd;
    fd = anssock_socket(AF_INET, SOCK_STREAM, 0);	
 
    if(fd < 0)
    {
        printf("create socket failed \n");
        anssock_close(epfd);
        return -1;
    }

    memset(&remote_addr, 0, sizeof(remote_addr));      
    remote_addr.sin_family = AF_INET;  
    remote_addr.sin_port   = htons(8000);  
    remote_addr.sin_addr.s_addr = htonl(0x0A000003); 
//    remote_addr.sin_addr.s_addr = htonl(0x03030303); 

    if(anssock_connect(fd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)     
    {     
		event.data.fd = fd;  
		event.events = EPOLLIN | EPOLLET;  

		ret = anssock_epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
		if(ret != 0)
		{
			printf("epoll ctl failed \n");
			anssock_close(fd);
			anssock_close(epfd);
			return -1;
		}

		printf("start dpdk tcp client application \n");

		ret=pthread_create(&id, NULL, (void *) tcp_send_thread, (void*)fd);  
		if(ret!=0)  
		{  
			printf ("Create pthread error!\n");  
			return 0;  
		}  

		int event_num = 0;
    
		while(1)
		{
			event_num = anssock_epoll_wait (epfd, events, 20, -1);
			if(event_num <= 0)
			{
				printf("epoll_wait failed \n");
				continue;
			}
            
			for(i = 0; i < event_num; i++)
			{
				if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))  
				{  
					printf("dpdk socket(%d) error\n", events[i].data.fd);
					anssock_close (events[i].data.fd);  
					fd = -1;
					continue;  
				}   

				if (events[i].events & EPOLLIN)
				{
					while(1)
					{
						recv_len = anssock_recvfrom(events[i].data.fd, recv_buf, 5000, 0, NULL, NULL);
						if((recv_len < 0) && (errno == ANS_EAGAIN))
						{
							// printf("no data in socket \n");

							break;
						}
						else if(recv_len < 0)
						{
							// socket error
							//anssock_close(fd);
							break;

						}

						printf("Recv data,  len: %d \n", recv_len);

						data_num++;
					}
            
				}
				else
				{
					printf("unknow event %x, fd:%d \n", events[i].events, events[i].data.fd);
				}
            
			}
    
		}



		anssock_close(fd);
		anssock_close(epfd);
	
		return 0;
	}
	else
	{
		printf("Connect to server failed \n");
		anssock_close(fd);
		anssock_close(epfd);
        return -1;
	}
}




int main(void)
{
    int ret;
    int i = 0 ;
    int epfd;
    struct epoll_event event;
    pthread_t id;  

    ret = anssock_init(NULL);
    if(ret != 0)
        printf("init sock ring failed \n");

    /* create epoll socket */
    epfd = anssock_epoll_create(0);
    if(epfd < 0)
    {
        printf("create epoll socket failed \n");
        return -1;
    }
	int concurent = 3;
	while(concurent--)
	{
		ret=pthread_create(&id, NULL, (void *) client_thread, NULL);  
		
    	}
	return 0;
}
