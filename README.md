# ChatRoom - 基于 C/S 架构的多人聊天室

一个基于 TCP 协议的多人实时聊天室应用，包含 Qt 客户端和 C 语言服务端，支持用户注册登录、消息转发、上下线通知等功能。

## 功能特性

- **用户注册/登录** — 密码使用 Salt + SHA-256 加密存储，安全可靠
- **实时聊天** — 消息即时转发给所有在线用户
- **上下线通知** — 用户加入/离开聊天室时广播系统消息
- **多线程并发** — 服务端为每个客户端分配独立线程处理通信
- **粘包处理** — 客户端正确处理 TCP 字节流的粘包/拆包问题
- **SQLite 持久化** — 用户数据存储在 SQLite 数据库中，无需额外安装数据库服务

## 项目结构

```
ChatRoomClient/
├── ChatRoomClient.pro      # Qt 项目配置文件
├── main.cpp                # 程序入口
├── logindialog.h/cpp/ui    # 登录/注册界面
├── chatdialog.h/cpp/ui     # 聊天主界面
├── ChatServer.cpp          # 服务端主程序（部署在 Linux 服务器上）
├── sha256.h/c              # SHA-256 哈希算法纯 C 实现
└── favicon.ico             # 应用图标
```

## 系统架构

```
┌──────────────┐       TCP        ┌──────────────┐
│  Qt 客户端 A  │◄──────────────►│              │
│  (Windows)   │                  │              │
└──────────────┘                  │   C 服务端    │
                                  │  (Linux)     │
┌──────────────┐       TCP        │              │
│  Qt 客户端 B  │◄──────────────►│  SQLite DB   │
│  (Windows)   │                  │              │
└──────────────┘                  └──────────────┘
```

- **客户端**：Qt 5/6（C++），使用 `QTcpSocket` 进行 TCP 通信
- **服务端**：原生 C + POSIX Threads，使用 Linux Socket API
- **数据库**：SQLite 3，轻量级嵌入式数据库

## 通信协议

所有消息以 `\0`（空字符）作为分隔符，解决 TCP 粘包/拆包问题。

### 客户端 → 服务端

| 消息格式 | 说明 |
|---|---|
| `REG:用户名:密码\0` | 注册请求 |
| `LOGIN:用户名:密码\0` | 登录请求 |
| `CHAT:消息内容\0` | 发送聊天消息 |

### 服务端 → 客户端

| 消息格式 | 说明 |
|---|---|
| `REG_OK\0` | 注册成功 |
| `REG_FAIL:原因\0` | 注册失败 |
| `LOGIN_OK\0` | 登录成功 |
| `LOGIN_FAIL:原因\0` | 登录失败 |
| `NEW_MSG:发送者:内容\0` | 转发的聊天消息 |
| `SYSTEM:消息\0` | 系统通知 |

### 交互流程示例

```
客户端                     服务端                      SQLite
  │                         │                           │
  │── REG:alice:123456 ────►│                           │
  │                         │── INSERT (salt+SHA-256) ─►│
  │◄── REG_OK ─────────────│                           │
  │                         │                           │
  │── LOGIN:alice:123456 ──►│                           │
  │                         │── SELECT (验证密码) ──────►│
  │◄── LOGIN_OK ───────────│                           │
  │                         │── 广播 SYSTEM:alice加入 ──│──► 其他客户端
  │                         │                           │
  │── CHAT:大家好 ─────────►│                           │
  │◄── NEW_MSG:alice:大家好 │ (回显给自己，可选)         │
  │                         │── NEW_MSG:alice:大家好 ──►│──► 其他客户端
```

## 密码安全

```
注册流程：
  1. 生成 32 位随机盐值 (Salt)
  2. 计算 Hash = SHA-256(Salt + Password)
  3. 数据库存储：username | hash | salt

登录验证：
  1. 根据用户名查询 salt 和 hash
  2. 计算 SHA-256(salt + input_password)
  3. 比较计算结果与存储的 hash 是否一致
```

- 不存储明文密码
- 每个用户使用独立盐值，防止彩虹表攻击
- SHA-256 算法为纯 C 实现，无外部依赖

## 编译与部署

### 客户端（Windows / Qt Creator）

1. 安装 Qt 5 或 Qt 6（需要 `core gui network widgets` 模块）
2. 使用 Qt Creator 打开 `ChatRoomClient.pro`
3. 点击运行即可

### 服务端（Linux）

**1. 下载 SQLite 源码**

```bash
# 到 https://www.sqlite.org/download.html 查看最新版本
wget https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
unzip sqlite-amalgamation-3450100.zip
cp sqlite-amalgamation-3450100/sqlite3.h .
cp sqlite-amalgamation-3450100/sqlite3.c .
```

**2. 编译**

```bash
# C 文件用 gcc 编译
gcc -c sqlite3.c -o sqlite3.o
gcc -c sha256.c -o sha256.o

# C++ 文件用 g++ 编译
g++ -c ChatServer.cpp -o ChatServer.o

# 用 g++ 链接所有目标文件
g++ -o ChatServer ChatServer.o sha256.o sqlite3.o -lpthread -ldl
```

**3. 运行**

```bash
# 前台运行（调试用）
./ChatServer

# 后台运行
nohup ./ChatServer > server.log 2>&1 &
```

**4. 防火墙配置**

确保服务器开放 9413 端口：

```bash
# firewalld
firewall-cmd --zone=public --add-port=9413/tcp --permanent
firewall-cmd --reload

# 或 iptables
iptables -A INPUT -p tcp --dport 9413 -j ACCEPT
```

如果使用云服务器（阿里云、腾讯云等），还需在安全组中放行 9413 端口。

## 界面展示

### 登录/注册界面

- 用户名和密码输入框
- 登录与注册按钮
- 状态提示信息（红色文字显示错误）
- 连接服务器后按钮才可点击

### 聊天主界面

- 聊天记录显示区（只读）
  - 🔵 蓝色：自己发送的消息
  - 🟢 绿色：其他用户发送的消息
  - 🔴 红色：系统消息（上下线通知等）
- 消息输入区
- 发送按钮

## 技术要点

| 要点 | 实现方式 |
|---|---|
| TCP 粘包/拆包 | 使用 `\0` 作为消息分隔符，接收端缓存 + 循环提取完整消息 |
| 多线程并发 | 每个客户端连接创建独立 pthread，使用读写锁保护共享数据 |
| 线程安全 | `pthread_rwlock_t` 读写锁，读多写少场景下性能优于互斥锁 |
| 密码加密 | Salt + SHA-256，纯 C 实现无外部依赖 |
| SQL 注入防护 | 使用 `sqlite3_bind_text()` 参数绑定，不拼接 SQL |
| SIGPIPE 处理 | 忽略 SIGPIPE 信号，防止向已关闭套接字写入时进程崩溃 |
| 连接超时 | 30 分钟接收超时，自动断开僵尸连接 |
| C/C++ 混合编译 | C 头文件使用 `extern "C"` 声明，确保链接兼容 |

## 依赖

### 客户端
- Qt 5 或 Qt 6（模块：core、gui、network、widgets）
- C++11 或更高版本

### 服务端
- GCC / G++ 编译器
- POSIX Threads（pthread）
- SQLite 3（amalgamation 源码编译，无需安装）
- Linux 操作系统

## License

MIT
