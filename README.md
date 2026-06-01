# ChatRoom - 基于 C/S 架构的多人聊天室

一个基于 TCP 协议的多人实时聊天室应用，包含 Qt 客户端和 C 语言服务端，支持用户注册登录、群聊、私聊、文件传输、AI 助手等功能。

## 功能特性

- **用户注册/登录** — 密码使用 Salt + SHA-256 加密存储，安全可靠
- **实时群聊** — 消息即时转发给所有在线用户
- **私聊功能** — 支持点对点私密聊天
- **文件传输** — 支持大文件分块传输（最大 500MB），断点续传
- **AI 助手** — 集成 DeepSeek AI，支持公聊和私聊模式
- **上下线通知** — 用户加入/离开聊天室时广播系统消息
- **在线用户列表** — 实时显示当前在线用户
- **断线重连** — 网络异常后自动重连并恢复登录状态
- **多线程并发** — 服务端为每个客户端分配独立线程处理通信
- **粘包处理** — 客户端正确处理 TCP 字节流的粘包/拆包问题
- **SQLite 持久化** — 用户数据存储在 SQLite 数据库中，无需额外安装数据库服务

## 项目结构

```
ChatRoomClient/
├── ChatRoomClient.pro      # Qt 项目配置文件
├── README.md              # 项目说明文档
├── release/               # 编译输出目录
├── resources/             # 资源文件
│   └── favicon.ico        # 应用图标
├── server/                # 服务端代码（部署在 Linux 服务器上）
│   ├── ChatServer.cpp     # 服务端主程序
│   ├── sha256.c/h         # SHA-256 哈希算法纯 C 实现
│   └── deepseek.c/h       # DeepSeek AI API 调用模块
└── src/                   # 客户端代码
    ├── main.cpp           # 程序入口
    ├── chatdialog.h/cpp/ui    # 聊天主界面
    ├── logindialog.h/cpp/ui   # 登录界面
    └── registerdialog.h/cpp/ui # 注册界面
```

## 系统架构

```
┌──────────────┐       TCP (9413)    ┌──────────────┐
│  Qt 客户端 A  │◄─────────────────►│              │
│  (Windows)   │                     │              │
└──────────────┘                     │   C 服务端    │
                                    │  (Linux)     │
┌──────────────┐       TCP (9413)    │              │
│  Qt 客户端 B  │◄─────────────────►│  SQLite DB   │
│  (Windows)   │                     │  + 文件存储   │
└──────────────┘                     └──────────────┘
       │                                    │
       └───────── TCP (9414) ───────────────┘
                 (文件传输端口)
```

- **客户端**：Qt 5/6（C++），使用 `QTcpSocket` 进行 TCP 通信
- **服务端**：原生 C + POSIX Threads，使用 Linux Socket API
- **数据库**：SQLite 3，轻量级嵌入式数据库
- **端口**：主端口 9413（消息通信），文件端口 9414（文件传输）

## 通信协议

### 主端口 (9413) - 文本协议

所有消息以 `\0`（空字符）作为分隔符，解决 TCP 粘包/拆包问题。

#### 客户端 → 服务端

| 消息格式 | 说明 |
|---|---|
| `REG:用户名:密码\0` | 注册请求 |
| `LOGIN:用户名:密码\0` | 登录请求 |
| `CHAT:消息内容\0` | 发送群聊消息 |
| `WHISPER:目标用户:消息内容\0` | 发送私聊消息 |
| `@AI 问题\0` | 公聊模式向 AI 提问（所有人可见） |
| `@AI:PRIVATE:问题\0` | 私聊模式向 AI 提问（仅自己可见） |
| `FILE_UPLOAD_START:目标用户:文件名:文件大小:总块数:MD5\0` | 文件上传开始 |
| `FILE_UPLOAD_DONE:文件名\0` | 文件上传完成 |
| `FILE_DOWNLOAD_REQ:发送者:文件名\0` | 请求下载文件 |
| `FILE_DOWNLOAD_ACK:发送者:文件名\0` | 下载完成确认 |
| `FILE_REJECT:发送者:文件名\0` | 拒绝接收文件 |

#### 服务端 → 客户端

| 消息格式 | 说明 |
|---|---|
| `REG_OK\0` | 注册成功 |
| `REG_FAIL:原因\0` | 注册失败 |
| `LOGIN_OK\0` | 登录成功 |
| `LOGIN_FAIL:原因\0` | 登录失败 |
| `NEW_MSG:发送者:内容\0` | 转发的群聊消息 |
| `WHISPER_MSG:发送者:内容\0` | 收到的私聊消息 |
| `WHISPER_SENT:目标用户:内容\0` | 私聊发送确认 |
| `AI_RESP:AI名:内容\0` | AI 公聊回复（所有人可见） |
| `AI_PRIV_RESP:AI名:内容\0` | AI 私聊回复（仅自己可见） |
| `FILE_UPLOAD_READY:文件名\0` | 服务端准备好接收上传 |
| `FILE_UPLOAD_COMPLETE:发送者:文件名\0` | 上传完成通知 |
| `FILE_NOTIFY:发送者:文件名:大小:MD5\0` | 文件到达通知 |
| `FILE_DOWNLOAD_INFO:文件大小:总块数:块位图\0` | 下载信息 |
| `FILE_ERROR:原因\0` | 文件传输错误 |
| `SYSTEM:消息\0` | 系统通知 |
| `USER_LIST:用户1,用户2,...\0` | 在线用户列表 |

### 文件端口 (9414) - 二进制协议

使用长度前缀协议（二进制安全）：`[4字节大端序消息体长度][消息体]`

| 消息体内容 | 说明 |
|---|---|
| `AUTH:用户名` | 认证 |
| `FILE_CHUNK_UP:文件名:块序号:总块数:[二进制数据]` | 上传一个块 |
| `FILE_CHUNK_DOWN_REQ:发送者:文件名` | 请求下载 |
| `FILE_CHUNK_DOWN_RESP:文件名:块序号:总块数:[二进制]` | 响应下载 |
| `FILE_UPLOAD_FINISH:文件名` | 上传结束通知 |
| `AUTH_OK\0` | 认证成功 |
| `AUTH_FAIL\0` | 认证失败 |
| `FILE_CHUNK_ACK:文件名:块序号` | 块接收确认 |

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
  │◄── USER_LIST:bob,carol │                           │
  │                         │── 广播 SYSTEM:alice加入 ──│──► 其他客户端
  │                         │                           │
  │── CHAT:大家好 ─────────►│                           │
  │◄── NEW_MSG:alice:大家好 │                           │
  │                         │── NEW_MSG:alice:大家好 ──►│──► 其他客户端
  │                         │                           │
  │── WHISPER:bob:私聊 ────►│                           │
  │                         │── WHISPER_MSG:alice:私聊 ─►│──► bob
  │◄── WHISPER_SENT:bob:私聊│                           │
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

## AI 助手功能

### 使用方式

**公聊模式**（所有人可见提问和回复）：
```
@AI 你好，介绍一下自己
```

**私聊模式**（仅自己可见）：
```
@AI:PRIVATE:帮我写一段代码
```

### 配置方法

在 `server/ChatServer.cpp` 中配置 DeepSeek API Key：

```cpp
#define DEEPSEEK_API_KEY "YOUR_API_KEY_HERE"
```

从 [DeepSeek 平台](https://platform.deepseek.com/) 获取 API Key。

## 文件传输

### 特性

- 分块传输：64KB/块
- 支持大文件：最大 500MB
- 断点续传：基于块位图实现
- MD5 校验：确保文件完整性

### 流程

```
上传：
  1. 客户端发送 FILE_UPLOAD_START
  2. 服务端回复 FILE_UPLOAD_READY
  3. 客户端通过文件端口分块发送数据
  4. 服务端发送 FILE_NOTIFY 通知接收方
  5. 接收方选择保存路径后开始下载

下载：
  1. 客户端发送 FILE_DOWNLOAD_REQ
  2. 服务端回复 FILE_DOWNLOAD_INFO（包含块位图）
  3. 客户端请求缺失的块
  4. 服务端通过文件端口发送数据块
  5. 完成后发送 FILE_DOWNLOAD_ACK
```

## 编译与部署

### 客户端（Windows / Qt Creator）

**环境要求**：
- Qt 5 或 Qt 6（需要 `core gui network widgets` 模块）
- C++11 或更高版本

**编译步骤**：
1. 使用 Qt Creator 打开 `ChatRoomClient.pro`
2. 配置构建套件（MSVC 或 MinGW）
3. 点击构建按钮编译
4. 点击运行即可

### 服务端（Linux）

**环境要求**：
- GCC / G++ 编译器
- POSIX Threads（pthread）
- SQLite 3（amalgamation 源码编译，无需安装）
- Linux 操作系统

**编译步骤**：

```bash
# 1. 下载 SQLite 源码（到 https://www.sqlite.org/download.html 查看最新版本）
wget https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
unzip sqlite-amalgamation-3450100.zip
cp sqlite-amalgamation-3450100/sqlite3.h server/
cp sqlite-amalgamation-3450100/sqlite3.c server/

# 2. 进入 server 目录
cd server

# 3. 编译
gcc -c sqlite3.c -o sqlite3.o
gcc -c sha256.c -o sha256.o
gcc -c deepseek.c -o deepseek.o
g++ -c ChatServer.cpp -o ChatServer.o
g++ -o ChatServer ChatServer.o sha256.o sqlite3.o deepseek.o -lpthread -ldl -lcurl

# 4. 创建文件存储目录
mkdir -p file_storage
```

**运行**：

```bash
# 前台运行（调试用）
./ChatServer

# 后台运行
nohup ./ChatServer > server.log 2>&1 &
```

**防火墙配置**：

确保服务器开放 9413 和 9414 端口：

```bash
# firewalld
firewall-cmd --zone=public --add-port=9413/tcp --permanent
firewall-cmd --zone=public --add-port=9414/tcp --permanent
firewall-cmd --reload

# 或 iptables
iptables -A INPUT -p tcp --dport 9413 -j ACCEPT
iptables -A INPUT -p tcp --dport 9414 -j ACCEPT
```

如果使用云服务器（阿里云、腾讯云等），还需在安全组中放行这两个端口。

## 界面展示

### 登录/注册界面

- 用户名和密码输入框
- 登录与注册按钮
- 状态提示信息（红色文字显示错误）
- 连接服务器后按钮才可点击

### 聊天主界面

- **聊天记录显示区**（只读）
  - 🔵 蓝色：自己发送的消息
  - 🟢 绿色：其他用户发送的消息
  - 🟣 紫色：私聊消息
  - 🤖 灰色：AI 回复
  - 🔴 红色：系统消息（上下线通知等）
- **在线用户列表**（点击可发起私聊）
- **消息输入区**
- **发送按钮**
- **发送文件按钮**
- **模式切换**（公聊/私聊）

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
| 断线重连 | 客户端检测断开后自动重连，重试次数递增间隔 |
| 文件分块传输 | 64KB/块，支持断点续传 |
| HTTP 请求（AI）| 使用原生 socket 实现 HTTP/HTTPS 请求 |

## 依赖

### 客户端
- Qt 5 或 Qt 6（模块：core、gui、network、widgets）
- C++11 或更高版本

### 服务端
- GCC / G++ 编译器
- POSIX Threads（pthread）
- SQLite 3（amalgamation 源码编译）
- Linux 操作系统
- DeepSeek API Key（可选，用于 AI 功能）

## License

MIT