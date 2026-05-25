/*
 * ChatServer.cpp - 聊天室服务端主程序
 *
 * 功能概述：
 *   1. 基于 TCP 的多线程并发聊天服务器
 *   2. 支持用户注册、登录（密码使用 salt + SHA-256 加密存储）
 *   3. 登录后可发送聊天消息，消息转发给所有其他在线用户
 *   4. 使用 SQLite 数据库持久化存储用户信息
 *
 * 通信协议（所有消息以 '\0' 作为分隔符）：
 *   客户端 → 服务端：
 *     REG:用户名:密码\0       注册请求
 *     LOGIN:用户名:密码\0     登录请求
 *     CHAT:消息内容\0         聊天消息
 *
 *   服务端 → 客户端：
 *     REG_OK\0                注册成功
 *     REG_FAIL:原因\0         注册失败
 *     LOGIN_OK\0              登录成功
 *     LOGIN_FAIL:原因\0       登录失败
 *     NEW_MSG:发送者:内容\0   转发的聊天消息
 *     SYSTEM:系统消息\0       系统通知（用户上线/下线等）
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

/* ========================= 宏定义 ========================= */
#define PORT 9413        // 服务器监听端口号
#define MAX_MSG 4096     // 单条消息最大长度（字节）
#define SALT_LEN 32      // 密码盐值长度（字符数）
#define HASH_LEN 65      // SHA-256 哈希值长度（64个十六进制字符 + 1个'\0'）

/* ========================= 客户端信息结构体 ========================= */
// 每个连接到服务器的客户端都会分配一个此结构体，
// 用于记录该客户端的网络信息和登录状态
typedef struct
{
	int sock_conn;            // 与该客户端通信的连接套接字描述符
	char ip[16];              // 客户端的 IP 地址（点分十进制，最多15字符 + '\0'）
	unsigned short port;      // 客户端的端口号（网络字节序已转换为主机字节序）
	time_t online_time;       // 客户端连接服务器的时间戳
	char username[50];        // 登录后的用户名（未登录时为空字符串）
	int logged_in;            // 登录状态标志：0=未登录，1=已登录
} client_info_t;

/* ========================= 全局变量 ========================= */

// 在线客户端链表：存储当前所有已连接（无论是否登录）的客户端信息指针
// 当有客户端连接时 push_back，断开时 remove
std::list<client_info_t*> client_list;

// 读写锁：保护 client_list 的并发访问
// 多个线程可能同时读取（转发消息时）或写入（客户端上线下线时）
// 使用读写锁而非互斥锁，是因为读多写少，读写锁允许并发读，提高性能
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;

// SQLite 数据库连接句柄（全局唯一，整个进程共享）
// SQLite 在串行化模式下是线程安全的，多线程可安全使用同一个连接
sqlite3 *g_db = NULL;

/* ========================= 函数声明 ========================= */

// 通信线程函数：每个客户端连接对应一个此线程
void* comm_thr(void* arg);

// 数据库初始化：打开数据库文件并创建 users 表
int db_init();

// 用户注册：将新用户信息写入数据库（密码使用 salt+SHA-256 存储）
int db_register(const char *username, const char *password);

// 用户登录：验证用户名和密码是否匹配
int db_login(const char *username, const char *password);

// 生成随机盐值：用于密码加密，防止彩虹表攻击
void generate_salt(char *salt, int len);

// SHA-256 哈希计算：将输入字符串计算为64字符的十六进制哈希值
void sha256_hash(const char *input, char *output);

// 向指定客户端发送消息（自动追加 '\0' 分隔符）
void send_to_client(int sock, const char *msg);

// 向所有已登录的在线客户端广播系统消息（可排除指定客户端）
void broadcast_system(const char *msg, client_info_t *exclude);


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
	// 忽略 SIGPIPE 信号
	// 当一个客户端异常断开后，服务端如果继续向该套接字 write，
	// 操作系统会发送 SIGPIPE 信号，默认行为是终止进程。
	// 忽略此信号后，write 会返回 -1 并设置 errno 为 EPIPE，
	// 我们可以在代码中检测到这个错误并做清理处理，而不是让进程崩溃。
	signal(SIGPIPE, SIG_IGN);

	// 初始化数据库（打开 chatroom.db，创建 users 表）
	if (db_init() != 0)
	{
		fprintf(stderr, "数据库初始化失败，服务器退出\n");
		return 1;
	}

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

	// ===== 第 4 步：循环接受客户端连接 =====
	int sock_conn;                        // 连接套接字（与每个客户端通信）
	pthread_t tid;                        // 线程 ID
	client_info_t* pci = NULL;            // 客户端信息指针
	struct sockaddr_in client_addr;        // 客户端地址信息
	socklen_t addr_len = sizeof(client_addr);

	// 设置接收超时时间：30 分钟
	// 如果一个客户端连接后 30 分钟内没有发送任何数据，read() 将返回 -1
	// 并设置 errno 为 EWOULDBLOCK/EAGAIN，我们可以据此断开该客户端
	struct timeval tv;
	tv.tv_sec = 30 * 60;
	tv.tv_usec = 0;

	while (1)
	{
		// accept() 会阻塞等待新的客户端连接
		// 返回一个新的套接字 sock_conn，专门用于与该客户端通信
		// client_addr 会被填充为客户端的地址信息
		sock_conn = accept(sock_listen, (struct sockaddr*)&client_addr, &addr_len);
		if (-1 == sock_conn)
		{
			perror("accept fail");
			continue;  // accept 失败不退出，继续等待下一个连接
		}

		// 使用 calloc 分配内存并自动初始化为 0
		// 这样 username[0] 和 logged_in 等字段默认就是 0/空
		pci = (client_info_t*)calloc(1, sizeof(client_info_t));
		if (NULL == pci)
		{
			perror("calloc fail");
			close(sock_conn);  // 内存不足，关闭该连接，继续服务其他客户端
			continue;
		}

		// 填充客户端信息
		pci->sock_conn = sock_conn;
		strncpy(pci->ip, inet_ntoa(client_addr.sin_addr), sizeof(pci->ip) - 1);
		// inet_ntoa() 将网络字节序的 IP 地址转为点分十进制字符串
		// ntohs() 将网络字节序的端口号转为主机字节序
		pci->port = ntohs(client_addr.sin_port);
		pci->online_time = time(NULL);  // 记录连接时间
		pci->username[0] = '\0';        // 初始未登录，用户名为空
		pci->logged_in = 0;             // 初始未登录

		// 为该客户端创建通信线程
		// comm_thr 是线程函数，pci 是传递给线程的参数
		if (pthread_create(&tid, NULL, comm_thr, pci))
		{
			perror("pthread_create fail");
			free(pci);         // 释放已分配的内存
			close(sock_conn);  // 关闭连接
			continue;
		}

		// 为该客户端的连接套接字设置接收超时
		// 这样在 comm_thr 中的 read() 如果超过 30 分钟没有数据会返回错误
		setsockopt(sock_conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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
				// 调用数据库登录验证函数
				else if (db_login(username, password) == 0)
				{
					// 登录成功：更新客户端信息结构体
					strncpy(pci->username, username, sizeof(pci->username) - 1);
					pci->logged_in = 1;

					// 向该客户端发送登录成功响应
					send_to_client(pci->sock_conn, "LOGIN_OK");

					// 向所有其他已登录的客户端广播系统消息，通知有新用户加入
					char sys_msg[256];
					snprintf(sys_msg, sizeof(sys_msg), "%s 加入了聊天室", username);
					broadcast_system(sys_msg, pci);  // pci 是排除对象，不给自己发

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

				// 构造转发消息：NEW_MSG:发送者用户名:消息内容
				char forward_msg[MAX_MSG];
				snprintf(forward_msg, sizeof(forward_msg), "NEW_MSG:%s:%s", pci->username, chat_msg);

				// 使用读锁遍历在线客户端列表，将消息转发给所有其他已登录用户
				pthread_rwlock_rdlock(&client_list_lock);
				for (it = client_list.begin(); it != client_list.end(); ++it)
				{
					// *it != pci: 不转发给自己（客户端本地已经显示了）
					// (*it)->logged_in: 只转发给已登录的用户
					if (*it != pci && (*it)->logged_in)
					{
						send_to_client((*it)->sock_conn, forward_msg);
					}
				}
				pthread_rwlock_unlock(&client_list_lock);
			}
			// ===== 处理未知格式的消息（兼容旧客户端） =====
			// 如果消息没有 REG:/LOGIN:/CHAT: 前缀，且用户已登录，
			// 则当作普通聊天消息转发
			else
			{
				if (pci->logged_in)
				{
					char forward_msg[MAX_MSG];
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
		broadcast_system(sys_msg, pci);
		printf("用户 [%s] 下线\n", pci->username);
	}

	// 关闭与该客户端的连接套接字
	close(pci->sock_conn);

	// 从在线客户端列表中移除该客户端
	// 使用写锁，因为要修改链表
	pthread_rwlock_wrlock(&client_list_lock);
	client_list.remove(pci);
	pthread_rwlock_unlock(&client_list_lock);

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
