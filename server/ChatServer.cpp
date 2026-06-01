/*
 * ChatServer.cpp - 聊天室服务端主程序
 *
 * 功能概述：
 *   1. 基于 TCP 的多线程并发聊天服务器
 *   2. 支持用户注册、登录（密码使用 salt + SHA-256 加密存储）
 *   3. 登录后可发送聊天消息，消息转发给所有其他在线用户
 *   4. 使用 SQLite 数据库持久化存储用户信息
 *
 * 通信协议：
 *   主端口 9413 使用 '\0' 作为消息分隔符（文本协议）：
 *     客户端 → 服务端：
 *       REG:用户名:密码\0              注册请求
 *       LOGIN:用户名:密码\0            登录请求
 *       CHAT:消息内容\0                聊天消息（群聊）
 *       WHISPER:目标用户:消息内容\0     私聊消息
 *       FILE_UPLOAD_START:目标用户:文件名:文件大小:总块数:MD5\0  文件上传开始
 *       FILE_UPLOAD_DONE:文件名\0                                文件上传完成
 *       FILE_DOWNLOAD_REQ:发送者:文件名\0                        请求下载文件
 *       FILE_DOWNLOAD_ACK:发送者:文件名\0                        下载完成确认
 *       FILE_REJECT:发送者:文件名\0                              拒绝接收文件
 *
 *     服务端 → 客户端：
 *       REG_OK\0                        注册成功
 *       REG_FAIL:原因\0                 注册失败
 *       LOGIN_OK\0                      登录成功
 *       LOGIN_FAIL:原因\0               登录失败
 *       NEW_MSG:发送者:内容\0           转发的聊天消息
 *       WHISPER_MSG:发送者:内容\0       收到的私聊消息
 *       WHISPER_SENT:目标用户:内容\0    私聊发送确认
 *       FILE_UPLOAD_READY:文件名\0      服务端准备好接收上传
 *       FILE_UPLOAD_COMPLETE:发送者:文件名\0  上传完成通知
 *       FILE_NOTIFY:发送者:文件名:大小:MD5\0  文件到达通知
 *       FILE_DOWNLOAD_INFO:文件大小:总块数:块位图\0  下载信息
 *       FILE_ERROR:原因\0              错误信息
 *       SYSTEM:系统消息\0               系统通知
 *       USER_LIST:用户1,用户2,...\0     在线用户列表
 *       AI_RESP:AI名:内容\0            AI公聊回复
 *       AI_PRIV_RESP:AI名:内容\0       AI私聊回复
 *
 *   文件端口 9414 使用长度前缀协议（二进制安全）：
 *     [4字节大端序消息体长度][消息体]
 *     消息体内容：
 *       AUTH:用户名                                      认证
 *       FILE_CHUNK_UP:文件名:块序号:总块数:[二进制数据]   上传一个块
 *       FILE_CHUNK_DOWN_REQ:发送者:文件名                 请求下载
 *       FILE_CHUNK_DOWN_RESP:文件名:块序号:总块数:[二进制] 响应下载
 *       FILE_UPLOAD_FINISH:文件名                         上传结束通知
 *       AUTH_OK\0                                        认证成功
 *       AUTH_FAIL\0                                      认证失败
 *       FILE_CHUNK_ACK:文件名:块序号                      块接收确认
 *
 * 编译命令（Linux）：
 *   g++ -o ChatServer ChatServer.cpp sha256.c sqlite3.c -lpthread -ldl
 */

/* ========================= 标准库头文件 ========================= */
#include <stdio.h>       // printf, fprintf, perror, snprintf
#include <string.h>      // strlen, strcmp, strncmp, strncpy, strchr, memset
#include <stdlib.h>      // malloc, calloc, free, rand, srand
#include <time.h>        // time()
#include <errno.h>       // errno, EWOULDBLOCK, EAGAIN
#include <signal.h>      // signal, SIGPIPE, SIG_IGN

/* ========================= POSIX 网络与线程头文件 ========================= */
#include <sys/types.h>   // 基本系统数据类型
#include <sys/stat.h>    // 文件状态
#include <sys/socket.h>  // socket, bind, listen, accept, read, write, setsockopt
#include <arpa/inet.h>   // inet_ntoa, htons, ntohs, struct sockaddr_in
#include <fcntl.h>       // 文件控制（未使用，保留备用）
#include <unistd.h>      // close()
#include <pthread.h>     // pthread_create, pthread_detach, pthread_self, 读写锁

/* ========================= C++ STL 头文件 ========================= */
#include <list>          // std::list，用于管理在线客户端列表
#include <string>        // std::string（保留备用）

/* ========================= 第三方库头文件 ========================= */
#include "sqlite3.h"     // SQLite3 数据库 C API
#include "sha256.h"      // SHA-256 哈希算法（本项目自行实现）
#include "deepseek.h"    // DeepSeek AI API 调用模块

/* ========================= 宏定义 ========================= */
#define PORT 9413
#define FILE_PORT 9414
#define MAX_MSG 4096
#define CHUNK_SIZE (64 * 1024)
#define MAX_FILE_SIZE (500 * 1024 * 1024)
#define FILE_STORAGE_DIR "./file_storage/"
#define FILE_EXPIRE_DAYS 3
#define MAX_CONCURRENT_UPLOADS 10
#define SALT_LEN 32
#define HASH_LEN 65
#define AI_RESP_MAX 2048

/*
 * DeepSeek API Key
 * 请替换为你自己的 API Key（从 https://platform.deepseek.com/ 获取）
 * 安全建议：生产环境应从环境变量或配置文件读取，不要硬编码
 */
#define DEEPSEEK_API_KEY "your key"

/* ========================= 客户端信息结构体 ========================= */
// 每个连接到服务器的客户端都会分配一个此结构体，
// 用于记录该客户端的网络信息和登录状态
typedef struct
{
	int sock_conn;
	char ip[16];
	unsigned short port;
	time_t online_time;
	char username[50];
	int logged_in;
} client_info_t;

typedef struct
{
	int sock_conn;
	char username[50];
	int authenticated;
} file_conn_info_t;

typedef struct
{
	char filename[256];
	char sender[50];
	char target[50];
	long long file_size;
	int total_chunks;
	char md5[33];
	time_t upload_time;
	int upload_complete;
	unsigned char *chunk_map;
} file_meta_t;

typedef struct
{
	char filename[256];
	char uploader[50];
	FILE *fp;
	int uploaded_chunks;
} upload_session_t;

/* ========================= 全局变量 ========================= */

std::list<client_info_t*> client_list;
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;

std::list<file_conn_info_t*> file_conn_list;
pthread_rwlock_t file_conn_list_lock = PTHREAD_RWLOCK_INITIALIZER;

std::list<file_meta_t*> g_file_store;
pthread_rwlock_t g_file_store_lock = PTHREAD_RWLOCK_INITIALIZER;

std::list<upload_session_t*> g_upload_sessions;
pthread_rwlock_t g_upload_sessions_lock = PTHREAD_RWLOCK_INITIALIZER;

sqlite3 *g_db = NULL;

/* ========================= 函数声明 ========================= */

void* comm_thr(void* arg);
int db_init();
int db_register(const char *username, const char *password);
int db_login(const char *username, const char *password);
void generate_salt(char *salt, int len);
void sha256_hash(const char *input, char *output);
void send_to_client(int sock, const char *msg);
void broadcast_system(const char *msg, client_info_t *exclude);
void send_user_list(int sock);
client_info_t* find_client_by_username(const char *username);
file_conn_info_t* find_file_conn_by_username(const char *username);
void* file_comm_thr(void* arg);
int init_file_storage();
file_meta_t* find_file_meta(const char *sender, const char *filename);
upload_session_t* find_upload_session(const char *uploader, const char *filename);
int read_length_prefixed(int sock, char **out_buf, int *out_len);
void write_length_prefixed(int sock, const char *data, int len);
void write_lp_str(int sock, const char *str);
void cleanup_expired_files();
void* cleanup_thr(void* arg);

typedef struct {
    char question[MAX_MSG];
    char username[50];
    int  sock_conn;
    int  is_private;
} ai_request_t;

void* ai_thr(void* arg);


/* ==========================================================================
 * main() - 服务器主函数
 *
 * 执行流程：
 *   1. 忽略 SIGPIPE 信号（防止向已关闭的套接字写入时进程崩溃）
 *   2. 初始化数据库
 *   3. 创建监听套接字 → 绑定地址 → 开始监听
 *   4. 循环 accept 接受客户端连接，为每个连接创建通信线程
 * ========================================================================== */
int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	if (db_init() != 0)
	{
		fprintf(stderr, "数据库初始化失败，服务器退出\n");
		return 1;
	}

	if (init_file_storage() != 0)
	{
		fprintf(stderr, "文件存储目录初始化失败\n");
		return 1;
	}

	pthread_t cleanup_tid;
	pthread_create(&cleanup_tid, NULL, cleanup_thr, NULL);

	// ===== 第 1 步：创建监听套接字 =====
	// AF_INET: IPv4 地址族
	// SOCK_STREAM: 面向连接的 TCP 协议
	// 0: 自动选择协议（TCP）
	int sock_listen = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sock_listen)
	{
		perror("socket fail");
		return 1;
	}

	// 开启地址复用选项（SO_REUSEADDR）
	// 作用：服务器重启时可以立即重新绑定之前使用的地址和端口，
	// 否则需要等待 TIME_WAIT 状态超时（通常2分钟）才能重新绑定
	int val = 1;
	setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	// ===== 第 2 步：绑定地址 =====
	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;          // IPv4 地址族
	myaddr.sin_addr.s_addr = INADDR_ANY;  // 绑定本机所有网络接口（0.0.0.0）
	                                      // 这样服务器可以通过任意网卡接收连接
	myaddr.sin_port = htons(PORT);        // 端口号，htons() 将主机字节序转为网络字节序

	if (-1 == bind(sock_listen, (struct sockaddr*)&myaddr, sizeof(myaddr)))
	{
		perror("bind fail");
		return 1;
	}

	// ===== 第 3 步：开始监听 =====
	// 第二个参数 10 是等待连接队列的最大长度
	// 当已有 10 个连接等待 accept 时，新的连接将被拒绝
	if (-1 == listen(sock_listen, 10))
	{
		perror("listen fail");
		return 1;
	}

	printf("聊天服务器启动，监听端口 %d...\n", PORT);

	// ===== 第 5 步：创建文件传输监听套接字 =====
	int sock_file_listen = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sock_file_listen)
	{
		perror("file socket fail");
		return 1;
	}

	setsockopt(sock_file_listen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	struct sockaddr_in file_addr;
	file_addr.sin_family = AF_INET;
	file_addr.sin_addr.s_addr = INADDR_ANY;
	file_addr.sin_port = htons(FILE_PORT);

	if (-1 == bind(sock_file_listen, (struct sockaddr*)&file_addr, sizeof(file_addr)))
	{
		perror("file bind fail");
		return 1;
	}

	if (-1 == listen(sock_file_listen, 10))
	{
		perror("file listen fail");
		return 1;
	}

	printf("文件传输服务启动，监听端口 %d...\n", FILE_PORT);

	// ===== 第 6 步：循环接受客户端连接（主端口 + 文件端口） =====
	pthread_t tid;                        // 线程 ID
	struct sockaddr_in client_addr;        // 客户端地址信息
	socklen_t addr_len = sizeof(client_addr);

	// 设置接收超时时间：30 分钟
	struct timeval tv;
	tv.tv_sec = 30 * 60;
	tv.tv_usec = 0;

	// 使用 select 同时监听主端口和文件端口
	fd_set read_fds;
	int max_fd = (sock_listen > sock_file_listen ? sock_listen : sock_file_listen) + 1;

	while (1)
	{
		FD_ZERO(&read_fds);
		FD_SET(sock_listen, &read_fds);
		FD_SET(sock_file_listen, &read_fds);

		// select 阻塞等待任一端口有新连接
		int sel_ret = select(max_fd, &read_fds, NULL, NULL, NULL);
		if (sel_ret < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select fail");
			continue;
		}

		// ===== 主端口有新连接 =====
		if (FD_ISSET(sock_listen, &read_fds))
		{
			int sock_conn = accept(sock_listen, (struct sockaddr*)&client_addr, &addr_len);
			if (-1 == sock_conn)
			{
				perror("accept fail");
			}
			else
			{
				client_info_t* pci = (client_info_t*)calloc(1, sizeof(client_info_t));
				if (NULL == pci)
				{
					perror("calloc fail");
					close(sock_conn);
				}
				else
				{
					pci->sock_conn = sock_conn;
					strncpy(pci->ip, inet_ntoa(client_addr.sin_addr), sizeof(pci->ip) - 1);
					pci->port = ntohs(client_addr.sin_port);
					pci->online_time = time(NULL);
					pci->username[0] = '\0';
					pci->logged_in = 0;

					if (pthread_create(&tid, NULL, comm_thr, pci))
					{
						perror("pthread_create fail");
						free(pci);
						close(sock_conn);
					}
					else
					{
						setsockopt(sock_conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
					}
				}
			}
		}

		// ===== 文件端口有新连接 =====
		if (FD_ISSET(sock_file_listen, &read_fds))
		{
			int sock_file_conn = accept(sock_file_listen, (struct sockaddr*)&client_addr, &addr_len);
			if (-1 == sock_file_conn)
			{
				perror("file accept fail");
			}
			else
			{
				file_conn_info_t* pfci = (file_conn_info_t*)calloc(1, sizeof(file_conn_info_t));
				if (NULL == pfci)
				{
					perror("calloc fail");
					close(sock_file_conn);
				}
				else
				{
					pfci->sock_conn = sock_file_conn;
					pfci->username[0] = '\0';
					pfci->authenticated = 0;

					if (pthread_create(&tid, NULL, file_comm_thr, pfci))
					{
						perror("pthread_create fail");
						free(pfci);
						close(sock_file_conn);
					}
					else
					{
						setsockopt(sock_file_conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
					}
				}
			}
		}
	}

	// 以下代码在正常情况下不会执行（while(1) 是无限循环）
	// 仅在服务器需要优雅关闭时执行
	close(sock_listen);
	sqlite3_close(g_db);
	return 0;
}


/* ==========================================================================
 * comm_thr() - 通信线程函数
 *
 * 每个客户端连接对应一个独立的通信线程，负责：
 *   1. 接收该客户端发送的消息
 *   2. 根据消息类型（REG/LOGIN/CHAT）分别处理
 *   3. 客户端断开时清理资源
 *
 * 参数 arg: 指向 client_info_t 的指针，包含该客户端的所有信息
 * 返回值:   NULL
 * ========================================================================== */
void* comm_thr(void* arg)
{
	client_info_t* pci = (client_info_t*)arg;
	int ret;
	char msg[MAX_MSG];  // 接收消息的缓冲区
	std::list<client_info_t*>::iterator it;  // 链表迭代器，用于遍历在线客户端

	// 将线程设置为 detach 状态
	// detach 后线程结束时其资源会自动回收，不需要其他线程调用 pthread_join
	pthread_detach(pthread_self());

	printf("\n客户端(%s:%hu)连接...\n", pci->ip, pci->port);

	// 将新客户端加入在线客户端列表
	// 使用写锁，因为要修改链表
	pthread_rwlock_wrlock(&client_list_lock);
	client_list.push_back(pci);
	pthread_rwlock_unlock(&client_list_lock);

	// ===== 主循环：持续接收并处理客户端消息 =====
	while (1)
	{
		// 从客户端读取数据
		// read() 在以下情况返回：
		//   ret > 0:  成功读取了 ret 个字节
		//   ret == 0: 客户端正常关闭了连接（发送了 FIN）
		//   ret < 0:  发生错误（超时、连接重置等）
		ret = read(pci->sock_conn, msg, sizeof(msg) - 1);

		if (ret > 0)
		{
			// 手动添加字符串结束符，方便后续用字符串函数处理
			msg[ret] = '\0';

			printf("\n收到(%s:%hu)消息：%s\n", pci->ip, pci->port, msg);

			// ===== 处理注册请求 =====
			// 消息格式：REG:用户名:密码\0
			if (strncmp(msg, "REG:", 4) == 0)
			{
				char username[50] = "";
				char password[50] = "";

				// 解析用户名和密码（以冒号分隔）
				char *rest = msg + 4;  // 跳过 "REG:" 前缀
				char *colon = strchr(rest, ':');  // 查找用户名和密码之间的冒号
				if (colon)
				{
					*colon = '\0';  // 将冒号位置替换为字符串结束符，截断用户名
					strncpy(username, rest, sizeof(username) - 1);
					strncpy(password, colon + 1, sizeof(password) - 1);
				}
				else
				{
					// 没有冒号，说明消息格式不正确，只有用户名没有密码
					strncpy(username, rest, sizeof(username) - 1);
				}

				// 校验用户名和密码是否为空
				if (strlen(username) == 0 || strlen(password) == 0)
				{
					send_to_client(pci->sock_conn, "REG_FAIL:用户名和密码不能为空");
				}
				// 调用数据库注册函数
				else if (db_register(username, password) == 0)
				{
					send_to_client(pci->sock_conn, "REG_OK");
					printf("用户 [%s] 注册成功\n", username);
				}
				else
				{
					send_to_client(pci->sock_conn, "REG_FAIL:用户名已存在");
					printf("用户 [%s] 注册失败（用户名已存在）\n", username);
				}
			}
			// ===== 处理登录请求 =====
			// 消息格式：LOGIN:用户名:密码\0
			else if (strncmp(msg, "LOGIN:", 6) == 0)
			{
				char username[50] = "";
				char password[50] = "";

				// 解析用户名和密码（与注册相同的解析逻辑）
				char *rest = msg + 6;  // 跳过 "LOGIN:" 前缀
				char *colon = strchr(rest, ':');
				if (colon)
				{
					*colon = '\0';
					strncpy(username, rest, sizeof(username) - 1);
					strncpy(password, colon + 1, sizeof(password) - 1);
				}
				else
				{
					strncpy(username, rest, sizeof(username) - 1);
				}

				// 校验用户名和密码是否为空
				if (strlen(username) == 0 || strlen(password) == 0)
				{
					send_to_client(pci->sock_conn, "LOGIN_FAIL:用户名和密码不能为空");
				}
				// 检查该用户名是否已在线（防止重复登录）
				else if (find_client_by_username(username) != NULL)
				{
					send_to_client(pci->sock_conn, "LOGIN_FAIL:该账号已在其他设备登录");
					printf("用户 [%s] 重复登录被拒绝\n", username);
				}
				// 调用数据库登录验证函数
				else if (db_login(username, password) == 0)
				{
					// 登录成功：更新客户端信息结构体
					strncpy(pci->username, username, sizeof(pci->username) - 1);
					pci->logged_in = 1;

					// 向该客户端发送登录成功响应
					send_to_client(pci->sock_conn, "LOGIN_OK");

					// 向该客户端发送当前在线用户列表
					send_user_list(pci->sock_conn);

					// 向所有其他已登录的客户端广播系统消息，通知有新用户加入
					char sys_msg[256];
					snprintf(sys_msg, sizeof(sys_msg), "%s 加入了聊天室", username);
					broadcast_system(sys_msg, pci);

					// 向所有其他已登录客户端更新在线用户列表
					pthread_rwlock_rdlock(&client_list_lock);
					for (it = client_list.begin(); it != client_list.end(); ++it)
					{
						if (*it != pci && (*it)->logged_in)
							send_user_list((*it)->sock_conn);
					}
					pthread_rwlock_unlock(&client_list_lock);

					printf("用户 [%s] 登录成功\n", username);
				}
				else
				{
					send_to_client(pci->sock_conn, "LOGIN_FAIL:用户名或密码错误");
					printf("用户 [%s] 登录失败\n", username);
				}
			}
			// ===== 处理聊天消息 =====
			// 消息格式：CHAT:消息内容\0
			// 支持两种 AI 交互模式：
			//   - 公聊模式：CHAT:@AI 问题\0          → 广播提问 + 广播回复
			//   - 私聊模式：CHAT:@AI:PRIVATE:问题\0   → 仅提问者可见回复
			else if (strncmp(msg, "CHAT:", 5) == 0)
			{
				// 必须先登录才能发送聊天消息
				if (!pci->logged_in)
				{
					send_to_client(pci->sock_conn, "SYSTEM:请先登录");
					continue;
				}

				// 提取聊天内容（跳过 "CHAT:" 前缀）
				char *chat_msg = msg + 5;

				/* ===== 检测是否为 AI 提问（@AI 开头）===== */
				if (strncmp(chat_msg, "@AI", 3) == 0)
				{
					char *ai_question = NULL;
					int is_private = 0;

					if (strncmp(chat_msg, "@AI:PRIVATE:", 12) == 0)
					{
						/* 私聊 AI 模式：仅提问者可见 AI 回复 */
						ai_question = chat_msg + 12;
						is_private = 1;
					}
					else
					{
						/* 公聊 AI 模式：跳过 @AI 后面的空格、冒号等分隔符 */
						ai_question = chat_msg + 3;
						while (*ai_question == ' ' || *ai_question == ':' || *ai_question == '\t')
							ai_question++;
						is_private = 0;
					}

					if (ai_question && strlen(ai_question) > 0)
				{
					/* 检查 API Key 是否已配置 */
					if (strcmp(DEEPSEEK_API_KEY, "YOUR_API_KEY_HERE") == 0)
					{
						if (is_private)
							send_to_client(pci->sock_conn, "AI_PRIV_RESP:🤖 AI助手:AI 功能未启用，请联系管理员配置 API Key");
						else
							broadcast_system("AI 功能未启用，请联系管理员配置 DeepSeek API Key", NULL);
						continue;
					}

					/* 1. 先广播用户的提问（仅公聊模式下所有人可见） */
					if (!is_private)
					{
						char forward_msg[MAX_MSG];
						snprintf(forward_msg, sizeof(forward_msg),
								 "NEW_MSG:%s:%s", pci->username, chat_msg);

						pthread_rwlock_rdlock(&client_list_lock);
						for (it = client_list.begin(); it != client_list.end(); ++it)
						{
							if (*it != pci && (*it)->logged_in)
								send_to_client((*it)->sock_conn, forward_msg);
						}
						pthread_rwlock_unlock(&client_list_lock);
					}

						/* 2. 分配 AI 请求参数，创建独立线程调用 API */
						ai_request_t *req = (ai_request_t*)calloc(1, sizeof(ai_request_t));
						if (req)
						{
							strncpy(req->question, ai_question, sizeof(req->question) - 1);
							strncpy(req->username, pci->username, sizeof(req->username) - 1);
							req->sock_conn = pci->sock_conn;
							req->is_private = is_private;

							pthread_t ai_tid;
							if (pthread_create(&ai_tid, NULL, ai_thr, req) == 0)
							{
								pthread_detach(ai_tid);
							}
							else
							{
								free(req);
								broadcast_system("AI 服务暂时不可用", NULL);
							}
						}

						continue; /* AI 提问已处理，跳过普通转发 */
					}
				}

				/* ===== 普通聊天消息（非 AI 提问）===== */
				char forward_msg[MAX_MSG];
				snprintf(forward_msg, sizeof(forward_msg), "NEW_MSG:%s:%s", pci->username, chat_msg);

				pthread_rwlock_rdlock(&client_list_lock);
				for (it = client_list.begin(); it != client_list.end(); ++it)
				{
					if (*it != pci && (*it)->logged_in)
					{
						send_to_client((*it)->sock_conn, forward_msg);
					}
				}
				pthread_rwlock_unlock(&client_list_lock);
			}
			// ===== 处理私聊消息 =====
			// 消息格式：WHISPER:目标用户:消息内容\0
			else if (strncmp(msg, "WHISPER:", 8) == 0)
			{
				if (!pci->logged_in)
				{
					send_to_client(pci->sock_conn, "SYSTEM:请先登录");
					continue;
				}

				char *rest = msg + 8;
				char *colon = strchr(rest, ':');
				if (!colon)
				{
					send_to_client(pci->sock_conn, "SYSTEM:私聊格式错误");
					continue;
				}

				*colon = '\0';
				char target_user[50] = "";
				strncpy(target_user, rest, sizeof(target_user) - 1);
				char *whisper_msg = colon + 1;

				// 查找目标用户
				client_info_t *target = find_client_by_username(target_user);
				if (target)
				{
					// 向目标用户发送私聊消息
					char recv_msg[MAX_MSG];
					snprintf(recv_msg, sizeof(recv_msg), "WHISPER_MSG:%s:%s", pci->username, whisper_msg);
					send_to_client(target->sock_conn, recv_msg);

					// 向发送者确认私聊已发送
					char sent_msg[MAX_MSG];
					snprintf(sent_msg, sizeof(sent_msg), "WHISPER_SENT:%s:%s", target_user, whisper_msg);
					send_to_client(pci->sock_conn, sent_msg);
				}
				else
				{
					char sys_msg[256];
					snprintf(sys_msg, sizeof(sys_msg), "用户 %s 不在线", target_user);
					send_to_client(pci->sock_conn, sys_msg);
				}
			}
			else if (strncmp(msg, "FILE_UPLOAD_START:", 18) == 0)
			{
				if (!pci->logged_in)
				{
					send_to_client(pci->sock_conn, "FILE_ERROR:请先登录");
					continue;
				}

				char *rest = msg + 18;
				char *colon1 = strchr(rest, ':');
				if (!colon1) { send_to_client(pci->sock_conn, "FILE_ERROR:格式错误"); continue; }
				*colon1 = '\0';
				char target_user[50] = "";
				strncpy(target_user, rest, sizeof(target_user) - 1);

				char *p2 = colon1 + 1;
				char *colon2 = strchr(p2, ':');
				if (!colon2) { send_to_client(pci->sock_conn, "FILE_ERROR:格式错误"); continue; }
				*colon2 = '\0';
				char filename[256] = "";
				strncpy(filename, p2, sizeof(filename) - 1);

				char *p3 = colon2 + 1;
				char *colon3 = strchr(p3, ':');
				if (!colon3) { send_to_client(pci->sock_conn, "FILE_ERROR:格式错误"); continue; }
				*colon3 = '\0';
				long long file_size = atoll(p3);

				char *p4 = colon3 + 1;
				char *colon4 = strchr(p4, ':');
				if (!colon4) { send_to_client(pci->sock_conn, "FILE_ERROR:格式错误"); continue; }
				*colon4 = '\0';
				int total_chunks = atoi(p4);

				char *md5_str = colon4 + 1;

				if (file_size > MAX_FILE_SIZE)
				{
					char err[256];
					snprintf(err, sizeof(err), "FILE_ERROR:文件大小超过限制(最大%dMB)", MAX_FILE_SIZE / (1024 * 1024));
					send_to_client(pci->sock_conn, err);
					continue;
				}

				pthread_rwlock_wrlock(&g_upload_sessions_lock);
				int active_uploads = 0;
				std::list<upload_session_t*>::iterator usit;
				for (usit = g_upload_sessions.begin(); usit != g_upload_sessions.end(); ++usit)
					active_uploads++;
				pthread_rwlock_unlock(&g_upload_sessions_lock);

				if (active_uploads >= MAX_CONCURRENT_UPLOADS)
				{
					send_to_client(pci->sock_conn, "FILE_ERROR:服务器繁忙，请稍后重试");
					continue;
				}

				file_meta_t *meta = (file_meta_t*)calloc(1, sizeof(file_meta_t));
				strncpy(meta->filename, filename, sizeof(meta->filename) - 1);
				strncpy(meta->sender, pci->username, sizeof(meta->sender) - 1);
				strncpy(meta->target, target_user, sizeof(meta->target) - 1);
				meta->file_size = file_size;
				meta->total_chunks = total_chunks;
				strncpy(meta->md5, md5_str, sizeof(meta->md5) - 1);
				meta->upload_time = time(NULL);
				meta->upload_complete = 0;
				int map_bytes = (total_chunks + 7) / 8;
				meta->chunk_map = (unsigned char*)calloc(map_bytes, 1);

				pthread_rwlock_wrlock(&g_file_store_lock);
				g_file_store.push_back(meta);
				pthread_rwlock_unlock(&g_file_store_lock);

				char filepath[512];
				snprintf(filepath, sizeof(filepath), "%s%s__%s.tmp", FILE_STORAGE_DIR, pci->username, filename);
				FILE *fp = fopen(filepath, "wb");
				if (!fp)
				{
					send_to_client(pci->sock_conn, "FILE_ERROR:服务器存储失败");
					pthread_rwlock_wrlock(&g_file_store_lock);
					g_file_store.remove(meta);
					pthread_rwlock_unlock(&g_file_store_lock);
					free(meta->chunk_map);
					free(meta);
					continue;
				}
				fclose(fp);

				upload_session_t *session = (upload_session_t*)calloc(1, sizeof(upload_session_t));
				strncpy(session->filename, filename, sizeof(session->filename) - 1);
				strncpy(session->uploader, pci->username, sizeof(session->uploader) - 1);
				session->fp = fopen(filepath, "r+b");
				session->uploaded_chunks = 0;

				pthread_rwlock_wrlock(&g_upload_sessions_lock);
				g_upload_sessions.push_back(session);
				pthread_rwlock_unlock(&g_upload_sessions_lock);

				char ready_msg[MAX_MSG];
				snprintf(ready_msg, sizeof(ready_msg), "FILE_UPLOAD_READY:%s", filename);
				send_to_client(pci->sock_conn, ready_msg);

				printf("文件上传开始：%s → %s，文件：%s (%lld字节, %d块)\n",
					   pci->username, target_user, filename, file_size, total_chunks);
			}
			else if (strncmp(msg, "FILE_UPLOAD_DONE:", 17) == 0)
			{
				if (!pci->logged_in)
					continue;

				char *filename = msg + 17;

				pthread_rwlock_wrlock(&g_upload_sessions_lock);
				upload_session_t *session = find_upload_session(pci->username, filename);
				if (session)
				{
					if (session->fp) fclose(session->fp);
					g_upload_sessions.remove(session);
					free(session);
				}
				pthread_rwlock_unlock(&g_upload_sessions_lock);

				pthread_rwlock_wrlock(&g_file_store_lock);
				file_meta_t *meta = find_file_meta(pci->username, filename);
				if (meta)
				{
					meta->upload_complete = 1;

					char old_path[512], new_path[512];
					snprintf(old_path, sizeof(old_path), "%s%s__%s.tmp", FILE_STORAGE_DIR, meta->sender, meta->filename);
					snprintf(new_path, sizeof(new_path), "%s%s__%s.data", FILE_STORAGE_DIR, meta->sender, meta->filename);
					rename(old_path, new_path);

					pthread_rwlock_unlock(&g_file_store_lock);

					char complete_msg[MAX_MSG];
					snprintf(complete_msg, sizeof(complete_msg), "FILE_UPLOAD_COMPLETE:%s:%s", pci->username, filename);
					send_to_client(pci->sock_conn, complete_msg);

					if (strcmp(meta->target, "ALL") == 0)
					{
						char notify_msg[MAX_MSG];
						snprintf(notify_msg, sizeof(notify_msg), "FILE_NOTIFY:%s:%s:%lld:%s",
								 meta->sender, meta->filename, meta->file_size, meta->md5);

						pthread_rwlock_rdlock(&client_list_lock);
						for (it = client_list.begin(); it != client_list.end(); ++it)
						{
							if (*it != pci && (*it)->logged_in)
								send_to_client((*it)->sock_conn, notify_msg);
						}
						pthread_rwlock_unlock(&client_list_lock);
					}
					else
					{
						client_info_t *target = find_client_by_username(meta->target);
						if (target)
						{
							char notify_msg[MAX_MSG];
							snprintf(notify_msg, sizeof(notify_msg), "FILE_NOTIFY:%s:%s:%lld:%s",
									 meta->sender, meta->filename, meta->file_size, meta->md5);
							send_to_client(target->sock_conn, notify_msg);
						}
					}

					printf("文件上传完成：%s，文件：%s\n", pci->username, filename);
				}
				else
				{
					pthread_rwlock_unlock(&g_file_store_lock);
					send_to_client(pci->sock_conn, "FILE_ERROR:文件信息未找到");
				}
			}
			else if (strncmp(msg, "FILE_DOWNLOAD_REQ:", 18) == 0)
			{
				if (!pci->logged_in)
					continue;

				char *rest = msg + 18;
				char *colon = strchr(rest, ':');
				if (!colon) continue;
				*colon = '\0';
				char sender[50] = "";
				strncpy(sender, rest, sizeof(sender) - 1);
				char *filename = colon + 1;

				pthread_rwlock_rdlock(&g_file_store_lock);
				file_meta_t *meta = find_file_meta(sender, filename);
				if (meta && meta->upload_complete)
				{
					int map_bytes = (meta->total_chunks + 7) / 8;
					char *bitmap_str = (char*)malloc(map_bytes * 2 + 1);
					for (int i = 0; i < map_bytes; i++)
						sprintf(bitmap_str + i * 2, "%02x", meta->chunk_map[i]);

					char info_msg[MAX_MSG];
					snprintf(info_msg, sizeof(info_msg), "FILE_DOWNLOAD_INFO:%lld:%d:%s",
							 meta->file_size, meta->total_chunks, bitmap_str);
					send_to_client(pci->sock_conn, info_msg);
					free(bitmap_str);
				}
				else
				{
					send_to_client(pci->sock_conn, "FILE_ERROR:文件不存在或未上传完成");
				}
				pthread_rwlock_unlock(&g_file_store_lock);
			}
			else if (strncmp(msg, "FILE_DOWNLOAD_ACK:", 18) == 0)
			{
				if (!pci->logged_in)
					continue;
				printf("用户 [%s] 下载完成文件：%s\n", pci->username, msg + 18);
			}
			else if (strncmp(msg, "FILE_REJECT:", 12) == 0)
			{
				if (!pci->logged_in)
					continue;

				char *rest = msg + 12;
				char *colon = strchr(rest, ':');
				if (!colon) continue;
				*colon = '\0';
				char sender[50] = "";
				strncpy(sender, rest, sizeof(sender) - 1);
				char *filename = colon + 1;

				pthread_rwlock_rdlock(&g_file_store_lock);
				file_meta_t *meta = find_file_meta(sender, filename);
				int is_private = meta && strcmp(meta->target, "ALL") != 0;
				pthread_rwlock_unlock(&g_file_store_lock);

				if (is_private)
				{
					pthread_rwlock_wrlock(&g_file_store_lock);
					meta = find_file_meta(sender, filename);
					if (meta)
					{
						char filepath[512];
						snprintf(filepath, sizeof(filepath), "%s%s__%s.data", FILE_STORAGE_DIR, meta->sender, meta->filename);
						unlink(filepath);
						snprintf(filepath, sizeof(filepath), "%s%s__%s.tmp", FILE_STORAGE_DIR, meta->sender, meta->filename);
						unlink(filepath);
						g_file_store.remove(meta);
						free(meta->chunk_map);
						free(meta);
					}
					pthread_rwlock_unlock(&g_file_store_lock);
				}

				client_info_t *target = find_client_by_username(sender);
				if (target)
				{
					char reject_msg[MAX_MSG];
					snprintf(reject_msg, sizeof(reject_msg), "FILE_REJECT:%s:%s", pci->username, filename);
					send_to_client(target->sock_conn, reject_msg);
				}
			}
			// ===== 处理未知格式的消息（兼容旧客户端） =====
			// 如果消息没有 REG:/LOGIN:/CHAT: 前缀，且用户已登录，
			// 则当作普通聊天消息转发
			else
			{
				if (pci->logged_in)
				{
					char forward_msg[MAX_MSG + 256];
					snprintf(forward_msg, sizeof(forward_msg), "NEW_MSG:%s:%s", pci->username, msg);

					pthread_rwlock_rdlock(&client_list_lock);
					for (it = client_list.begin(); it != client_list.end(); ++it)
					{
						if (*it != pci && (*it)->logged_in)
						{
							send_to_client((*it)->sock_conn, forward_msg);
						}
					}
					pthread_rwlock_unlock(&client_list_lock);
				}
			}
		}
		// ret == 0：客户端调用了 close() 正常关闭了连接
		else if (ret == 0)
		{
			printf("\n客户端(%s:%hu)已关闭连接！\n", pci->ip, pci->port);
			break;  // 跳出循环，进入清理阶段
		}
		// ret < 0：读取发生错误
		else
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				// 接收超时：客户端长时间没有发送数据（超过30分钟）
				printf("\n客户端(%s:%hu)接收超时！\n", pci->ip, pci->port);
			}
			else
			{
				// 其他错误：如连接重置（ECONNRESET）等
				perror("recv fail");
				printf("\n与客户端(%s:%hu)通信发生错误！\n", pci->ip, pci->port);
			}
			break;  // 跳出循环，进入清理阶段
		}
	}

	// ===== 客户端断开连接后的清理工作 =====

	// 如果该客户端已登录，广播下线通知
	if (pci->logged_in)
	{
		char sys_msg[256];
		snprintf(sys_msg, sizeof(sys_msg), "%s 离开了聊天室", pci->username);
		printf("用户 [%s] 下线\n", pci->username);

		// 先从在线客户端列表中移除该客户端
		pthread_rwlock_wrlock(&client_list_lock);
		client_list.remove(pci);
		pthread_rwlock_unlock(&client_list_lock);

		// 广播下线通知
		broadcast_system(sys_msg, pci);

		// 向所有其他已登录客户端更新在线用户列表
		pthread_rwlock_rdlock(&client_list_lock);
		for (it = client_list.begin(); it != client_list.end(); ++it)
		{
			if (*it != pci && (*it)->logged_in)
				send_user_list((*it)->sock_conn);
		}
		pthread_rwlock_unlock(&client_list_lock);
	}
	else
	{
		// 未登录用户断开，直接从列表移除
		pthread_rwlock_wrlock(&client_list_lock);
		client_list.remove(pci);
		pthread_rwlock_unlock(&client_list_lock);
	}

	// 关闭与该客户端的连接套接字
	close(pci->sock_conn);

	// 释放客户端信息结构体内存
	free(pci);

	return NULL;
}


/* ==========================================================================
 * db_init() - 数据库初始化
 *
 * 功能：
 *   1. 打开（或创建）SQLite 数据库文件 chatroom.db
 *   2. 如果 users 表不存在则创建
 *
 * 返回值：0=成功，-1=失败
 * ========================================================================== */
int db_init()
{
	// 打开数据库文件，如果文件不存在则自动创建
	// 数据库文件名为 "chatroom.db"，会在服务器运行目录下生成
	int rc = sqlite3_open("chatroom.db", &g_db);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "打开数据库失败: %s\n", sqlite3_errmsg(g_db));
		return -1;
	}

	// 创建 users 表的 SQL 语句
	// IF NOT EXISTS: 如果表已存在则不重复创建
	// 各字段说明：
	//   id: 自增主键，唯一标识每个用户
	//   username: 用户名，UNIQUE 约束保证不重复
	//   password_hash: 密码哈希值（salt+SHA-256），不存储明文密码
	//   salt: 每个用户独有的随机盐值，防止彩虹表攻击
	//   register_time: 注册时间，自动填充当前时间
	//   last_login_time: 最后登录时间，每次登录时更新
	const char *sql = "CREATE TABLE IF NOT EXISTS users ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT UNIQUE NOT NULL,"
		"password_hash TEXT NOT NULL,"
		"salt TEXT NOT NULL,"
		"register_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"last_login_time DATETIME)";

	// 执行建表 SQL
	char *errMsg = NULL;
	rc = sqlite3_exec(g_db, sql, NULL, NULL, &errMsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "建表失败: %s\n", errMsg);
		sqlite3_free(errMsg);  // 释放 SQLite 分配的错误信息内存
		return -1;
	}

	printf("数据库初始化成功\n");
	return 0;
}


/* ==========================================================================
 * db_register() - 用户注册
 *
 * 功能：
 *   1. 检查用户名是否已存在
 *   2. 生成随机盐值
 *   3. 计算 salt+password 的 SHA-256 哈希值
 *   4. 将用户名、哈希值、盐值写入数据库
 *
 * 密码安全策略：
 *   不存储明文密码，而是存储 salt+SHA-256(salt+password)
 *   盐值的作用：即使两个用户使用相同密码，由于盐值不同，哈希值也不同，
 *   攻击者无法通过彩虹表反推密码
 *
 * 参数：
 *   username - 用户名
 *   password - 明文密码
 *
 * 返回值：0=注册成功，-1=注册失败（用户名已存在或其他错误）
 * ========================================================================== */
int db_register(const char *username, const char *password)
{
	sqlite3_stmt *stmt;

	// 第一步：查询用户名是否已存在
	// 使用参数绑定（?）而非字符串拼接，防止 SQL 注入攻击
	const char *sql = "SELECT id FROM users WHERE username = ?";

	int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL准备失败: %s\n", sqlite3_errmsg(g_db));
		return -1;
	}

	// 将 username 绑定到第一个 ? 占位符
	// SQLITE_TRANSIENT 表示 SQLite 会在内部复制一份数据，原始数据可以随时释放
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

	// sqlite3_step() 执行查询
	// SQLITE_ROW 表示查询到了一行结果（即用户名已存在）
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		sqlite3_finalize(stmt);  // 释放语句对象
		return -1;               // 用户名已存在，注册失败
	}
	sqlite3_finalize(stmt);

	// 第二步：生成随机盐值
	char salt[SALT_LEN + 1];  // +1 给 '\0'
	generate_salt(salt, SALT_LEN);

	// 第三步：计算 salt+password 的 SHA-256 哈希值
	char salted_input[256];
	snprintf(salted_input, sizeof(salted_input), "%s%s", salt, password);

	char hash[HASH_LEN];  // 64个十六进制字符 + '\0'
	sha256_hash(salted_input, hash);

	// 第四步：将用户信息插入数据库
	sql = "INSERT INTO users (username, password_hash, salt) VALUES (?, ?, ?)";
	rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL准备失败: %s\n", sqlite3_errmsg(g_db));
		return -1;
	}

	// 绑定三个参数
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);  // 用户名
	sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_TRANSIENT);      // 密码哈希值
	sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_TRANSIENT);      // 盐值

	// 执行插入操作
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	// SQLITE_DONE 表示 INSERT 成功执行完毕
	if (rc != SQLITE_DONE)
	{
		return -1;  // 插入失败（可能是用户名冲突等）
	}

	return 0;  // 注册成功
}


/* ==========================================================================
 * db_login() - 用户登录验证
 *
 * 功能：
 *   1. 根据用户名查询数据库中的 password_hash 和 salt
 *   2. 用查询到的 salt 和用户输入的 password 计算 SHA-256 哈希值
 *   3. 比较计算出的哈希值与数据库中存储的哈希值是否一致
 *   4. 如果一致，更新 last_login_time 字段
 *
 * 参数：
 *   username - 用户名
 *   password - 用户输入的明文密码
 *
 * 返回值：0=登录成功，-1=登录失败（用户名不存在或密码错误）
 * ========================================================================== */
int db_login(const char *username, const char *password)
{
	sqlite3_stmt *stmt;

	// 第一步：查询该用户名对应的密码哈希值和盐值
	const char *sql = "SELECT password_hash, salt FROM users WHERE username = ?";

	int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL准备失败: %s\n", sqlite3_errmsg(g_db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

	// 如果查询不到结果，说明用户名不存在
	if (sqlite3_step(stmt) != SQLITE_ROW)
	{
		sqlite3_finalize(stmt);
		return -1;
	}

	// 从查询结果中取出 password_hash 和 salt
	// sqlite3_column_text() 返回的是 SQLite 内部管理的字符串指针，
	// 在 sqlite3_finalize() 之前有效
	const char *stored_hash = (const char*)sqlite3_column_text(stmt, 0);
	const char *salt = (const char*)sqlite3_column_text(stmt, 1);

	// 第二步：用数据库中的 salt 和用户输入的密码计算哈希值
	char salted_input[256];
	snprintf(salted_input, sizeof(salted_input), "%s%s", salt, password);

	char computed_hash[HASH_LEN];
	sha256_hash(salted_input, computed_hash);

	// 第三步：比较计算出的哈希值与存储的哈希值
	// strcmp 返回 0 表示两个字符串完全一致
	int result = (strcmp(stored_hash, computed_hash) == 0) ? 0 : -1;

	// 第四步：如果密码正确，更新最后登录时间
	if (result == 0)
	{
		sqlite3_finalize(stmt);  // 先释放当前的语句对象

		sql = "UPDATE users SET last_login_time = CURRENT_TIMESTAMP WHERE username = ?";
		rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
		if (rc == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
		}
	}

	sqlite3_finalize(stmt);
	return result;
}


/* ==========================================================================
 * generate_salt() - 生成随机盐值
 *
 * 功能：生成指定长度的随机字符串，由大小写字母和数字组成
 *
 * 盐值的作用：
 *   即使两个用户使用了相同的密码，由于盐值不同，
 *   存储在数据库中的哈希值也不同，攻击者无法通过对比哈希值
 *   来判断哪些用户使用了相同密码，也无法使用预计算的彩虹表破解
 *
 * 参数：
 *   salt - 输出缓冲区，用于存放生成的盐值字符串
 *   len  - 盐值长度（字符数，不含末尾的 '\0'）
 * ========================================================================== */
void generate_salt(char *salt, int len)
{
	// 可用字符集：小写字母 + 大写字母 + 数字，共 62 个字符
	static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	// 使用当前时间戳和线程 ID 的异或值作为随机种子
	// 这样不同线程、不同时间生成的盐值会有所不同
	srand((unsigned int)time(NULL) ^ (unsigned int)pthread_self());

	// 逐个随机选取字符
	for (int i = 0; i < len; i++)
	{
		salt[i] = charset[rand() % (sizeof(charset) - 1)];  // sizeof(charset)-1 = 62
	}
	salt[len] = '\0';  // 字符串结束符
}


/* ==========================================================================
 * sha256_hash() - SHA-256 哈希计算
 *
 * 功能：将输入字符串计算 SHA-256 哈希值，输出为 64 字符的十六进制字符串
 *
 * 示例：
 *   输入 "abc" → 输出 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
 *
 * 参数：
 *   input  - 输入字符串（通常是 salt+password 的拼接）
 *   output - 输出缓冲区，至少需要 65 字节（64字符 + '\0'）
 * ========================================================================== */
void sha256_hash(const char *input, char *output)
{
	unsigned char hash[32];  // SHA-256 输出 32 字节（256 位）的原始哈希值
	SHA256_CTX ctx;

	// 初始化 → 填入数据 → 计算最终哈希值
	sha256_init(&ctx);
	sha256_update(&ctx, (const uint8_t*)input, strlen(input));
	sha256_final(&ctx, hash);

	// 将 32 字节的二进制哈希值转为 64 字符的十六进制字符串
	// 每个字节转为 2 个十六进制字符，如 0xFF → "ff"
	for (int i = 0; i < 32; i++)
	{
		sprintf(output + (i * 2), "%02x", hash[i]);
	}
	output[64] = '\0';  // 字符串结束符
}


/* ==========================================================================
 * send_to_client() - 向客户端发送消息
 *
 * 功能：将消息发送给指定客户端，并在末尾追加 '\0' 作为消息分隔符
 *
 * 重要说明：
 *   TCP 是面向字节流的协议，没有消息边界的概念。
 *   我们使用 '\0' 作为消息之间的分隔符，客户端通过查找 '\0'
 *   来分割出完整的消息（处理粘包/拆包问题）。
 *
 * 参数：
 *   sock - 目标客户端的套接字描述符
 *   msg  - 要发送的消息字符串（不含 '\0'）
 * ========================================================================== */
void send_to_client(int sock, const char *msg)
{
	int len = strlen(msg);
	write(sock, msg, len);    // 发送消息内容
	write(sock, "\0", 1);     // 发送 '\0' 分隔符，标识一条完整消息的结束
}


/* ==========================================================================
 * broadcast_system() - 广播系统消息
 *
 * 功能：向所有已登录的在线客户端广播系统消息
 *       可以排除一个指定的客户端（通常是触发该事件的客户端自己）
 *
 * 使用场景：
 *   - 用户登录时广播 "xxx 加入了聊天室"（排除自己）
 *   - 用户下线时广播 "xxx 离开了聊天室"（排除自己，因为自己已经断开）
 *
 * 参数：
 *   msg     - 系统消息内容（不含 "SYSTEM:" 前缀，函数内部会自动添加）
 *   exclude - 要排除的客户端指针（不向其发送），可以为 NULL 表示不排除
 * ========================================================================== */
void broadcast_system(const char *msg, client_info_t *exclude)
{
	// 构造完整的系统消息：SYSTEM:消息内容
	char full_msg[MAX_MSG];
	snprintf(full_msg, sizeof(full_msg), "SYSTEM:%s", msg);

	// 使用读锁遍历在线客户端列表
	// 读锁允许多个线程同时读取，不会互相阻塞
	pthread_rwlock_rdlock(&client_list_lock);
	std::list<client_info_t*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); ++it)
	{
		// 两个条件：
		//   *it != exclude: 不发给被排除的客户端
		//   (*it)->logged_in: 只发给已登录的客户端
		if (*it != exclude && (*it)->logged_in)
		{
			send_to_client((*it)->sock_conn, full_msg);
		}
	}
	pthread_rwlock_unlock(&client_list_lock);
}


/* ==========================================================================
 * send_user_list() - 向指定客户端发送在线用户列表
 *
 * 格式：USER_LIST:用户1,用户2,用户3\0
 * 客户端收到后更新在线用户列表控件
 * ========================================================================== */
void send_user_list(int sock)
{
	char list_msg[MAX_MSG] = "USER_LIST:";
	int offset = strlen(list_msg);
	int first = 1;

	pthread_rwlock_rdlock(&client_list_lock);
	std::list<client_info_t*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); ++it)
	{
		if ((*it)->logged_in)
		{
			if (!first)
			{
				list_msg[offset++] = ',';
				list_msg[offset] = '\0';
			}
			int name_len = strlen((*it)->username);
			if (offset + name_len + 2 < (int)sizeof(list_msg))
			{
				strncpy(list_msg + offset, (*it)->username, name_len);
				offset += name_len;
				list_msg[offset] = '\0';
			}
			first = 0;
		}
	}
	pthread_rwlock_unlock(&client_list_lock);

	send_to_client(sock, list_msg);
}


/* ==========================================================================
 * find_client_by_username() - 根据用户名查找在线客户端
 *
 * 遍历在线客户端列表，查找匹配用户名的客户端。
 * 用于私聊和文件传输时定位目标用户。
 *
 * 返回值：找到则返回客户端信息指针，未找到返回 NULL
 * 注意：返回的指针在锁释放后可能失效（客户端可能断开），
 *       调用方应尽快使用，不要长期持有
 * ========================================================================== */
client_info_t* find_client_by_username(const char *username)
{
	pthread_rwlock_rdlock(&client_list_lock);
	std::list<client_info_t*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); ++it)
	{
		if ((*it)->logged_in && strcmp((*it)->username, username) == 0)
		{
			pthread_rwlock_unlock(&client_list_lock);
			return *it;
		}
	}
	pthread_rwlock_unlock(&client_list_lock);
	return NULL;
}


/* ==========================================================================
 * find_file_conn_by_username() - 根据用户名查找文件传输连接
 *
 * 在文件连接列表中查找已认证的、匹配用户名的文件连接。
 * 用于文件数据转发时定位接收方的文件连接。
 *
 * 返回值：找到则返回文件连接信息指针，未找到返回 NULL
 * ========================================================================== */
file_conn_info_t* find_file_conn_by_username(const char *username)
{
	pthread_rwlock_rdlock(&file_conn_list_lock);
	std::list<file_conn_info_t*>::iterator it;
	for (it = file_conn_list.begin(); it != file_conn_list.end(); ++it)
	{
		if ((*it)->authenticated && strcmp((*it)->username, username) == 0)
		{
			pthread_rwlock_unlock(&file_conn_list_lock);
			return *it;
		}
	}
	pthread_rwlock_unlock(&file_conn_list_lock);
	return NULL;
}

int init_file_storage()
{
	struct stat st = {0};
	if (stat(FILE_STORAGE_DIR, &st) == -1)
	{
		if (mkdir(FILE_STORAGE_DIR, 0755) != 0)
		{
			perror("mkdir file_storage fail");
			return -1;
		}
	}
	printf("文件存储目录就绪：%s\n", FILE_STORAGE_DIR);
	return 0;
}

file_meta_t* find_file_meta(const char *sender, const char *filename)
{
	std::list<file_meta_t*>::iterator it;
	for (it = g_file_store.begin(); it != g_file_store.end(); ++it)
	{
		if (strcmp((*it)->sender, sender) == 0 && strcmp((*it)->filename, filename) == 0)
			return *it;
	}
	return NULL;
}

upload_session_t* find_upload_session(const char *uploader, const char *filename)
{
	std::list<upload_session_t*>::iterator it;
	for (it = g_upload_sessions.begin(); it != g_upload_sessions.end(); ++it)
	{
		if (strcmp((*it)->uploader, uploader) == 0 && strcmp((*it)->filename, filename) == 0)
			return *it;
	}
	return NULL;
}

int read_length_prefixed(int sock, char **out_buf, int *out_len)
{
	uint32_t net_len = 0;
	int n = read(sock, &net_len, 4);
	if (n <= 0) return -1;
	if (n < 4) return -1;

	uint32_t msg_len = ntohl(net_len);
	if (msg_len == 0 || msg_len > CHUNK_SIZE + 1024)
		return -1;

	char *buf = (char*)malloc(msg_len + 1);
	if (!buf) return -1;

	int total = 0;
	while (total < (int)msg_len)
	{
		n = read(sock, buf + total, msg_len - total);
		if (n <= 0)
		{
			free(buf);
			return -1;
		}
		total += n;
	}
	buf[total] = '\0';

	*out_buf = buf;
	*out_len = total;
	return 0;
}

void write_length_prefixed(int sock, const char *data, int len)
{
	uint32_t net_len = htonl(len);
	write(sock, &net_len, 4);
	write(sock, data, len);
}

void write_lp_str(int sock, const char *str)
{
	write_length_prefixed(sock, str, strlen(str));
}

void cleanup_expired_files()
{
	time_t now = time(NULL);
	time_t expire_seconds = FILE_EXPIRE_DAYS * 24 * 3600;

	pthread_rwlock_wrlock(&g_file_store_lock);
	std::list<file_meta_t*>::iterator it = g_file_store.begin();
	while (it != g_file_store.end())
	{
		file_meta_t *meta = *it;
		if (now - meta->upload_time > expire_seconds)
		{
			char filepath[512];
			snprintf(filepath, sizeof(filepath), "%s%s__%s.data", FILE_STORAGE_DIR, meta->sender, meta->filename);
			unlink(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s__%s.tmp", FILE_STORAGE_DIR, meta->sender, meta->filename);
			unlink(filepath);

			printf("清理过期文件：%s/%s (上传于 %ld)\n", meta->sender, meta->filename, meta->upload_time);

			free(meta->chunk_map);
			free(meta);
			it = g_file_store.erase(it);
		}
		else
		{
			++it;
		}
	}
	pthread_rwlock_unlock(&g_file_store_lock);
}

void* cleanup_thr(void* arg)
{
	pthread_detach(pthread_self());
	while (1)
	{
		sleep(3600);
		cleanup_expired_files();
	}
	return NULL;
}

void* file_comm_thr(void* arg)
{
	file_conn_info_t* pfci = (file_conn_info_t*)arg;

	pthread_detach(pthread_self());

	printf("文件传输连接已建立(socket=%d)\n", pfci->sock_conn);

	pthread_rwlock_wrlock(&file_conn_list_lock);
	file_conn_list.push_back(pfci);
	pthread_rwlock_unlock(&file_conn_list_lock);

	while (1)
	{
		char *msg_buf = NULL;
		int msg_len = 0;
		int rc = read_length_prefixed(pfci->sock_conn, &msg_buf, &msg_len);

		if (rc != 0 || msg_buf == NULL)
			break;

		if (strncmp(msg_buf, "AUTH:", 5) == 0)
		{
			char *username = msg_buf + 5;
			strncpy(pfci->username, username, sizeof(pfci->username) - 1);

			client_info_t *main_client = find_client_by_username(pfci->username);
			if (main_client)
			{
				pfci->authenticated = 1;
				write_lp_str(pfci->sock_conn, "AUTH_OK");
				printf("文件连接认证成功：[%s](socket=%d)\n", pfci->username, pfci->sock_conn);
			}
			else
			{
				pfci->authenticated = 0;
				write_lp_str(pfci->sock_conn, "AUTH_FAIL");
				printf("文件连接认证失败：[%s] 未在主端口登录\n", pfci->username);
			}
		}
		else if (strncmp(msg_buf, "FILE_CHUNK_UP:", 14) == 0)
		{
			if (!pfci->authenticated)
			{
				write_lp_str(pfci->sock_conn, "ERROR:未认证");
				free(msg_buf);
				continue;
			}

			char *rest = msg_buf + 14;
			char *colon1 = strchr(rest, ':');
			if (!colon1) { free(msg_buf); continue; }
			*colon1 = '\0';
			char filename[256] = "";
			strncpy(filename, rest, sizeof(filename) - 1);

			char *p2 = colon1 + 1;
			char *colon2 = strchr(p2, ':');
			if (!colon2) { free(msg_buf); continue; }
			*colon2 = '\0';
			int chunk_index = atoi(p2);

			char *p3 = colon2 + 1;
			char *colon3 = strchr(p3, ':');
			if (!colon3) { free(msg_buf); continue; }
			*colon3 = '\0';
			int total_chunks = atoi(p3);

			char *chunk_data = colon3 + 1;
			int chunk_data_len = msg_len - (chunk_data - msg_buf);

			pthread_rwlock_wrlock(&g_upload_sessions_lock);
			upload_session_t *session = find_upload_session(pfci->username, filename);
			if (session && session->fp)
			{
				fseek(session->fp, (long long)chunk_index * CHUNK_SIZE, SEEK_SET);
				fwrite(chunk_data, 1, chunk_data_len, session->fp);
				fflush(session->fp);
				session->uploaded_chunks++;

				pthread_rwlock_wrlock(&g_file_store_lock);
				file_meta_t *meta = find_file_meta(pfci->username, filename);
				if (meta && chunk_index < meta->total_chunks)
				{
					int byte_idx = chunk_index / 8;
					int bit_idx = chunk_index % 8;
					meta->chunk_map[byte_idx] |= (1 << bit_idx);
				}
				pthread_rwlock_unlock(&g_file_store_lock);

				char ack[MAX_MSG];
				snprintf(ack, sizeof(ack), "FILE_CHUNK_ACK:%s:%d", filename, chunk_index);
				write_lp_str(pfci->sock_conn, ack);

				printf("文件块接收：%s，文件：%s，块 %d/%d\n",
					   pfci->username, filename, chunk_index + 1, total_chunks);
			}
			else
			{
				write_lp_str(pfci->sock_conn, "ERROR:上传会话未找到");
			}
			pthread_rwlock_unlock(&g_upload_sessions_lock);
		}
		else if (strncmp(msg_buf, "FILE_CHUNK_DOWN_REQ:", 20) == 0)
		{
			if (!pfci->authenticated)
			{
				write_lp_str(pfci->sock_conn, "ERROR:未认证");
				free(msg_buf);
				continue;
			}

			char *rest = msg_buf + 20;
			char *colon1 = strchr(rest, ':');
			if (!colon1) { free(msg_buf); continue; }
			*colon1 = '\0';
			char sender[50] = "";
			strncpy(sender, rest, sizeof(sender) - 1);

			char *p2 = colon1 + 1;
			char *colon2 = strchr(p2, ':');
			char filename[256] = "";
			int chunk_index = -1;

			if (colon2)
			{
				*colon2 = '\0';
				strncpy(filename, p2, sizeof(filename) - 1);
				chunk_index = atoi(colon2 + 1);
			}
			else
			{
				strncpy(filename, p2, sizeof(filename) - 1);
			}

			pthread_rwlock_rdlock(&g_file_store_lock);
			file_meta_t *meta = find_file_meta(sender, filename);
			if (meta && meta->upload_complete)
			{
				char filepath[512];
				snprintf(filepath, sizeof(filepath), "%s%s__%s.data", FILE_STORAGE_DIR, meta->sender, meta->filename);
				FILE *fp = fopen(filepath, "rb");
				if (fp)
				{
					int start_chunk = (chunk_index >= 0) ? chunk_index : 0;
					int end_chunk = (chunk_index >= 0) ? chunk_index + 1 : meta->total_chunks;

					for (int i = start_chunk; i < end_chunk; i++)
					{
						long long offset = (long long)i * CHUNK_SIZE;
						fseek(fp, offset, SEEK_SET);
						char read_buf[CHUNK_SIZE];
						int bytes_left = meta->file_size - offset;
						int to_read = (bytes_left < CHUNK_SIZE) ? bytes_left : CHUNK_SIZE;
						if (to_read <= 0) break;
						int n = fread(read_buf, 1, to_read, fp);
						if (n <= 0) break;

						char header[512];
						int header_len = snprintf(header, sizeof(header),
							"FILE_CHUNK_DOWN_RESP:%s:%d:%d:", filename, i, meta->total_chunks);

						int total_msg_len = header_len + n;
						char *send_buf = (char*)malloc(4 + total_msg_len);
						if (send_buf)
						{
							uint32_t net_len = htonl(total_msg_len);
							memcpy(send_buf, &net_len, 4);
							memcpy(send_buf + 4, header, header_len);
							memcpy(send_buf + 4 + header_len, read_buf, n);
							write(pfci->sock_conn, send_buf, 4 + total_msg_len);
							free(send_buf);
						}
					}
					fclose(fp);
					if (chunk_index >= 0)
						printf("文件块下载：%s ← %s，文件：%s，块 %d/%d\n", pfci->username, sender, filename, chunk_index + 1, meta->total_chunks);
					else
						printf("文件下载完成：%s ← %s，文件：%s\n", pfci->username, sender, filename);
				}
				else
				{
					write_lp_str(pfci->sock_conn, "ERROR:文件读取失败");
				}
			}
			else
			{
				write_lp_str(pfci->sock_conn, "ERROR:文件不存在或未上传完成");
			}
			pthread_rwlock_unlock(&g_file_store_lock);
		}
		else if (strncmp(msg_buf, "FILE_UPLOAD_FINISH:", 19) == 0)
		{
			printf("文件上传连接关闭：%s\n", msg_buf);
		}

		free(msg_buf);
	}

	printf("文件连接[%s](socket=%d)已关闭\n",
		   pfci->authenticated ? pfci->username : "未认证", pfci->sock_conn);

	close(pfci->sock_conn);

	pthread_rwlock_wrlock(&file_conn_list_lock);
	file_conn_list.remove(pfci);
	pthread_rwlock_unlock(&file_conn_list_lock);

	free(pfci);
	return NULL;
}


/* ==========================================================================
 * ai_thr() - AI 回调线程函数
 *
 * 在独立线程中调用 DeepSeek API，避免阻塞通信线程。
 * API 调用通常耗时 1-5 秒，如果在通信线程中直接调用，
 * 会导致该客户端在此期间无法接收任何消息。
 *
 * 流程：
 *   1. 调用 deepseek_ask() 获取 AI 回复
 *   2. 根据模式（公聊/私聊）发送回复：
 *      - 公聊：广播 AI_RESP:🤖 AI助手:回复内容 给所有人
 *      - 私聊：仅发送 AI_PRIV_RESP:🤖 AI助手:回复内容 给提问者
 *   3. API 调用失败时发送错误提示
 *
 * 参数 arg: 指向 ai_request_t 的指针（堆分配，本函数负责释放）
 * 返回值:   NULL
 * ========================================================================== */
void* ai_thr(void* arg)
{
	ai_request_t *req = (ai_request_t*)arg;
	char ai_response[AI_RESP_MAX] = {0};

	pthread_detach(pthread_self());

	printf("AI 收到来自 [%s] 的问题：%s（%s模式）\n",
		   req->username, req->question,
		   req->is_private ? "私聊" : "公聊");

	/* 调用 DeepSeek API 获取 AI 回复 */
	int ret = deepseek_ask(DEEPSEEK_API_KEY, req->question,
						   ai_response, sizeof(ai_response));

	if (ret == 0 && strlen(ai_response) > 0)
	{
		/* AI 回复成功 */
		char resp_msg[MAX_MSG + AI_RESP_MAX];

		if (req->is_private)
		{
			/* 私聊模式：仅发送给提问者 */
			snprintf(resp_msg, sizeof(resp_msg),
					 "AI_PRIV_RESP:🤖 AI助手:%s", ai_response);
			send_to_client(req->sock_conn, resp_msg);
			printf("AI 私聊回复 [%s]：%s\n", req->username, ai_response);
		}
		else
		{
			/* 公聊模式：广播给所有在线用户 */
			snprintf(resp_msg, sizeof(resp_msg),
					 "AI_RESP:🤖 AI助手:%s", ai_response);

			pthread_rwlock_rdlock(&client_list_lock);
			std::list<client_info_t*>::iterator it;
			for (it = client_list.begin(); it != client_list.end(); ++it)
			{
				if ((*it)->logged_in)
				{
					send_to_client((*it)->sock_conn, resp_msg);
				}
			}
			pthread_rwlock_unlock(&client_list_lock);
			printf("AI 公聊回复：%s\n", ai_response);
		}
	}
	else
	{
		/* AI 调用失败 */
		if (req->is_private)
		{
			send_to_client(req->sock_conn, "AI_PRIV_RESP:🤖 AI助手:抱歉，我暂时无法回答，请稍后再试");
		}
		else
		{
			broadcast_system("AI 助手暂时无法回答，请稍后再试", NULL);
		}
		printf("AI 回复失败\n");
	}

	/* 释放请求参数内存 */
	free(req);
	return NULL;
}
