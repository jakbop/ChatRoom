#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <list>



typedef struct
{
	int sock_conn;
	char ip[16];
	unsigned short port;
	time_t online_time;     // 上线时间
	// char user_name[50];  // 用户名
	//......

} client_info_t;


std::list<client_info_t*> client_list;  // 存储当前所有在线的客户端信息
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;  // 读写锁，保护在线客户端列表


void* comm_thr(void* arg);



int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	// 第 1 步：创建监听套接字
	int sock_listen = socket(AF_INET, SOCK_STREAM, 0);

	if(-1 == sock_listen)
	{
		perror("socket fail");
		exit(1);
	}


	// 开启地址复用，以允许服务器快速重启
	int val = 1;
	setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));


	// 第 2 步：绑定地址

	// 指定地址
	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;          // 指定地址家族(AF)为 Internet 地址家族
	myaddr.sin_addr.s_addr = INADDR_ANY;  // 指定 IP 地址为本机任意地址
	//myaddr.sin_addr.s_addr = inet_addr("172.16.251.96");  // 指定 IP 地址为本机的某个具体 IP 地址
	myaddr.sin_port = htons(9413);        // 指定端口号为 9413
	
	//printf("%hu\n", htons(6666));  // 2586

	// 绑定
	if(-1 == bind(sock_listen, (struct sockaddr*)&myaddr, sizeof(myaddr)))
	{
		perror("bind fail");
		exit(1);
	}


	// 第 3 步：监听
	if(-1 == listen(sock_listen, 5))
	{
		perror("listen fail");
		exit(1);
	}

	// 第 4 步：接收客户端连接请求
	
	int sock_conn;
	pthread_t tid;
	client_info_t* pci = NULL;
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	struct timeval tv;
	tv.tv_sec = 30 * 60;  // 设置接收超时时间为 30 分钟
	tv.tv_usec = 0;


	while(1)
	{	
		sock_conn = accept(sock_listen, (struct sockaddr*)&client_addr, &addr_len);

		if(-1 == sock_conn)
		{
			perror("accept fail");
			exit(1);
		}

		pci = (client_info_t*)malloc(sizeof(client_info_t));

		if(NULL == pci)
		{
			perror("malloc fail");
			close(sock_conn);
			continue;
		}

		// 获取当前上线的客户端信息
		pci->sock_conn = sock_conn;
		strcpy(pci->ip, inet_ntoa(client_addr.sin_addr));
		pci->port = ntohs(client_addr.sin_port);
		pci->online_time = time(NULL);
		
		if(pthread_create(&tid, NULL, comm_thr, pci))
		{
			perror("pthread_create fail");

			free(pci);
			close(sock_conn);
			continue;
		}

		// 设置接收超时
		setsockopt(sock_conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	}

	// 第 7 步：关闭监听套接字
	close(sock_listen);

	return 0;
}



// 定义通信线程函数
void* comm_thr(void* arg)
{
	client_info_t* pci = (client_info_t*)arg;
	int ret;
	char msg[1025] = "";  // 单条消息不超过 1024 字节
	std::list<client_info_t*>::iterator it;

	pthread_detach(pthread_self());

	printf("\n客户端(%s:%hu)上线...\n", pci->ip, pci->port);

	pthread_rwlock_wrlock(&client_list_lock);
	client_list.push_back(pci);
	pthread_rwlock_unlock(&client_list_lock);

	// 第 5 步：收发数据
	while(1)
	{
		ret = read(pci->sock_conn, msg, sizeof(msg) - 1);

		if(ret > 0)
		{
			msg[ret] = '\0';

			printf("\n收到客户端(%s:%hu)消息：%s\n", pci->ip, pci->port, msg);

			// 将消息转发给其他在线的客户端
			pthread_rwlock_rdlock(&client_list_lock);

			for(it = client_list.begin(); it != client_list.end(); ++it)
			{
				if(*it != pci)  // 不向消息发送者转发消息
				{
					write((*it)->sock_conn, msg, ret+1);
				}
			}

			pthread_rwlock_unlock(&client_list_lock);
		}
		else if(ret == 0)
		{
			printf("\n客户端(%s:%hu)已关闭连接！\n", pci->ip, pci->port);
			break;
		}
		else
		{
			if(errno == EWOULDBLOCK || errno == EAGAIN)
			{
				printf("\n客户端(%s:%hu)接收超时！\n", pci->ip, pci->port);
			}
			else
			{
				perror("recv fail");
				printf("\n与客户端(%s:%hu)通信发生错误！\n", pci->ip, pci->port);
			}

			break;
		}
	}


	// 第 6 步：断开连接（关闭连接套接字）
	close(pci->sock_conn);

	printf("\n客户端(%s:%hu)下线！\n", pci->ip, pci->port);

	pthread_rwlock_wrlock(&client_list_lock);
	client_list.remove(pci);
	pthread_rwlock_unlock(&client_list_lock);

	free(pci);

	return NULL;
}
