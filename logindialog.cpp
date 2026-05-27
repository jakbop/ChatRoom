/*
 * logindialog.cpp - 登录对话框实现文件
 *
 * 本文件实现了登录对话框的全部逻辑，包括：
 *   - TCP 连接的建立与管理
 *   - 登录请求的发送与响应处理
 *   - 注册界面的跳转（信号断开/重连机制）
 *   - Socket 所有权的转移（takeSocket）
 *
 * 通信协议：
 *   发送：LOGIN:用户名:密码\0
 *   接收：LOGIN_OK\0 或 LOGIN_FAIL:原因\0
 */

#include "logindialog.h"
#include "registerdialog.h"
#include "ui_logindialog.h"

/* ==========================================================================
 * 构造函数 - 初始化 UI、创建 Socket、建立信号槽连接、发起服务器连接
 * ========================================================================== */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_socket(new QTcpSocket(this))   /* 创建 TCP Socket，本对象作为父级 */
    , m_connected(false)               /* 初始状态：未连接 */
{
    ui->setupUi(this);

    /* 移除窗口标题栏上的 "?" 帮助按钮（Windows 风格） */
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    /* 连接未建立前，禁用登录和注册按钮，防止用户在未连接时操作 */
    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);

    /*
     * 绑定 Socket 的三个关键信号：
     *   connected  - TCP 三次握手完成，连接成功
     *   readyRead  - 有数据到达，需要读取并解析
     *   error      - 连接或通信过程中发生错误
     */
    connect(m_socket, &QTcpSocket::connected, this, &LoginDialog::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &LoginDialog::onError);

    /* 构造时立即发起连接 */
    connectToServer();
}

/* ==========================================================================
 * 析构函数 - 释放 UI 资源
 *
 * 注意：m_socket 的生命周期由 takeSocket() 决定：
 *   - 如果调用了 takeSocket()，m_socket 为 nullptr，不会在此释放
 *   - 如果未调用（用户取消登录），m_socket 作为子对象会被 Qt 自动释放
 * ========================================================================== */
LoginDialog::~LoginDialog()
{
    delete ui;
}

/* ==========================================================================
 * takeSocket() - 转移 Socket 所有权
 *
 * 登录成功后由 main.cpp 调用，将 Socket 从 LoginDialog 转移到 ChatDialog。
 *
 * 关键步骤：
 *   1. 保存原始指针到局部变量
 *   2. 将 m_socket 置为 nullptr（防止析构时误操作）
 *   3. 断开 socket 与本对象的所有信号-槽连接
 *      这一步极其重要！如果不断开，当 ChatDialog 也连接了同一个
 *      socket 的 readyRead 信号时，两个槽函数都会被调用，
 *      而 LoginDialog 的槽函数访问 m_socket（nullptr）会导致崩溃
 *
 * 返回值：QTcpSocket 指针，调用方（ChatDialog）接管其生命周期
 * ========================================================================== */
QTcpSocket* LoginDialog::takeSocket()
{
    QTcpSocket *sock = m_socket;
    m_socket = nullptr;
    /* 断开 sock 发出的所有信号与本对象(this)的所有槽函数的连接 */
    disconnect(sock, nullptr, this, nullptr);
    return sock;
}

/* ==========================================================================
 * getUsername() - 返回当前登录的用户名
 * ========================================================================== */
QString LoginDialog::getUsername() const
{
    return m_username;
}

/* ==========================================================================
 * on_btnLogin_clicked() - 登录按钮点击事件
 *
 * 流程：
 *   1. 获取并验证用户输入（用户名和密码不能为空）
 *   2. 保存用户名到成员变量（登录成功后需要传递给 ChatDialog）
 *   3. 调用 sendLoginRequest() 发送登录协议消息
 * ========================================================================== */
void LoginDialog::on_btnLogin_clicked()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    if (username.isEmpty() || password.isEmpty())
    {
        setStatus("用户名和密码不能为空");
        return;
    }

    m_username = username;
    sendLoginRequest();
}

/* ==========================================================================
 * on_btnGoRegister_clicked() - 跳转注册按钮点击事件
 *
 * 关键机制：信号断开/重连
 *
 *   打开注册对话框前，必须断开 LoginDialog 的 readyRead 信号连接，
 *   因为 RegisterDialog 也会连接同一个 socket 的 readyRead 信号。
 *   如果不断开，当服务器返回注册响应时，LoginDialog 的 onReadyRead()
 *   也会被触发，导致消息被错误处理。
 *
 *   注册对话框关闭后（无论成功与否），重新连接 readyRead 信号，
 *   恢复 LoginDialog 的消息接收能力。
 *
 *   注册对话框使用 exec() 以模态方式运行，阻塞直到关闭。
 * ========================================================================== */
void LoginDialog::on_btnGoRegister_clicked()
{
    /* 断开 readyRead 信号，避免 LoginDialog 和 RegisterDialog 同时响应 */
    disconnect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);

    /* 以模态方式打开注册对话框 */
    RegisterDialog regDlg(m_socket, this);
    regDlg.exec();

    /* 注册对话框关闭后，重新连接 readyRead 信号 */
    connect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);

    ui->edtUsername->setFocus();
}

/* ==========================================================================
 * onConnected() - TCP 连接成功回调
 *
 * 连接建立后启用登录和注册按钮，清除状态栏提示
 * ========================================================================== */
void LoginDialog::onConnected()
{
    m_connected = true;
    ui->btnLogin->setEnabled(true);
    ui->btnGoRegister->setEnabled(true);
    setStatus("");
}

/* ==========================================================================
 * onReadyRead() - 数据到达回调
 *
 * 处理 TCP 粘包问题：
 *   TCP 是字节流协议，不保证消息边界。服务器发送的多条消息可能
 *   合并在一个 TCP 包中到达，也可能一条消息被拆分成多个包。
 *   因此使用 m_buffer 缓冲区累积接收数据，以 '\0' 作为消息分隔符
 *   逐条提取完整消息进行处理。
 *
 * 消息处理：
 *   - LOGIN_OK：登录成功
 *       1. 先断开 socket 与本对象的所有信号连接（防止崩溃）
 *       2. 调用 accept() 关闭对话框，返回 QDialog::Accepted
 *   - LOGIN_FAIL:原因：登录失败，显示原因并重新启用按钮
 * ========================================================================== */
void LoginDialog::onReadyRead()
{
    /* 将新到达的数据追加到缓冲区 */
    m_buffer += m_socket->readAll();

    int index;
    /* 循环提取所有完整消息（以 '\0' 分隔） */
    while ((index = m_buffer.indexOf('\0')) != -1)
    {
        /* 提取一条完整消息（不含 '\0'） */
        QByteArray msg = m_buffer.left(index);
        /* 从缓冲区中移除已处理的消息和分隔符 */
        m_buffer = m_buffer.mid(index + 1);

        QString response = QString::fromUtf8(msg);

        if (response == "LOGIN_OK")
        {
            /*
             * 登录成功！
             * 必须在 accept() 之前断开所有信号连接。
             * accept() 会使对话框关闭并返回，但 LoginDialog 对象
             * 仍在栈上（main.cpp 中），如果不断开信号，后续
             * ChatDialog 连接的 readyRead 信号触发时，
             * LoginDialog::onReadyRead() 也会被调用，
             * 此时 m_socket 已被 takeSocket() 置为 nullptr，
             * 访问会导致空指针崩溃。
             */
            disconnect(m_socket, nullptr, this, nullptr);
            accept();  /* 关闭对话框，返回值 = QDialog::Accepted */
            return;
        }
        else if (response.startsWith("LOGIN_FAIL:"))
        {
            /* 登录失败，提取失败原因并显示，重新启用按钮 */
            setStatus("登录失败：" + response.mid(11));
            ui->btnLogin->setEnabled(true);
            ui->btnGoRegister->setEnabled(true);
        }
    }
}

/* ==========================================================================
 * onError() - 网络错误回调
 *
 * 当 TCP 连接失败或通信出错时触发。
 * 包含 m_socket 空指针检查：如果 takeSocket() 已将 m_socket 置为
 * nullptr，但 error 信号仍在事件队列中，此时直接返回避免崩溃。
 * ========================================================================== */
void LoginDialog::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    /* 防御性检查：takeSocket() 后 m_socket 可能为 nullptr */
    if (!m_socket) return;

    setStatus("连接失败：" + m_socket->errorString());
    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);
}

/* ==========================================================================
 * setStatus() - 更新状态栏文本
 * ========================================================================== */
void LoginDialog::setStatus(const QString &text)
{
    ui->lblStatus->setText(text);
}

/* ==========================================================================
 * connectToServer() - 发起 TCP 连接到聊天服务器
 *
 * 服务器地址和端口硬编码在此处，可根据实际部署修改。
 * ========================================================================== */
void LoginDialog::connectToServer()
{
    setStatus("正在连接服务器...");
    m_socket->connectToHost("39.104.71.92", 9413);
}

/* ==========================================================================
 * sendLoginRequest() - 构造并发送登录请求
 *
 * 协议格式：LOGIN:用户名:密码\0
 *
 * 发送后禁用登录和注册按钮，防止重复提交。
 * 按钮会在收到服务器响应后重新启用（onReadyRead 中处理）。
 * ========================================================================== */
void LoginDialog::sendLoginRequest()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    /* 构造协议消息，以 '\0' 作为分隔符 */
    QString msg = "LOGIN:" + username + ":" + password;
    m_socket->write(msg.toUtf8() + '\0');

    setStatus("正在登录...");

    /* 禁用按钮，等待服务器响应 */
    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);
}
