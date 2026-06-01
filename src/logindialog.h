/*
 * logindialog.h - 登录对话框头文件
 *
 * 功能概述：
 *   登录对话框是应用程序的入口界面，负责：
 *   1. 建立与服务器的 TCP 连接
 *   2. 发送登录请求并处理服务器响应
 *   3. 提供跳转到注册界面的入口
 *   4. 登录成功后将 TCP Socket 转交给聊天窗口
 *
 * 关键设计：
 *   - m_socket 是本对话框创建的 QTcpSocket，登录成功后通过 takeSocket()
 *     转移给 ChatDialog，转移时必须断开所有信号连接，防止信号槽冲突
 *   - m_buffer 用于 TCP 粘包处理，以 '\0' 作为消息分隔符
 *   - 打开注册界面时需要临时断开 readyRead 信号，避免 LoginDialog 和
 *     RegisterDialog 同时响应服务器消息
 *   - 连接失败时自动重连（m_reconnectTimer），每 3 秒重试一次
 */

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    /*
     * takeSocket() - 取出 TCP Socket 的所有权
     *
     * 登录成功后调用此方法将 socket 从 LoginDialog 转移到 ChatDialog。
     * 转移时会断开 socket 与本对象的所有信号-槽连接，
     * 并将 m_socket 置为 nullptr，防止后续误用。
     *
     * 返回值：原始的 QTcpSocket 指针，调用方负责管理其生命周期
     */
    QTcpSocket* takeSocket();

    /*
     * getUsername() - 获取当前登录的用户名
     *
     * 登录成功后由 main.cpp 调用，传递给 ChatDialog 用于消息显示。
     */
    QString getUsername() const;

    /*
     * getPassword() - 获取当前登录的密码
     *
     * 登录成功后由 main.cpp 调用，传递给 ChatDialog 用于断线重连后重新登录。
     */
    QString getPassword() const;

    /*
     * takePendingData() - 取出 Socket 缓冲区中的未处理数据
     *
     * 登录成功后，服务器可能在 LOGIN_OK 之后紧接着发送了 USER_LIST 等消息，
     * 这些消息可能还在 m_buffer 中尚未处理。调用此方法取出这些数据，
     * 传递给 ChatDialog 继续处理，避免消息丢失。
     */
    QByteArray takePendingData();

private slots:
    /* 登录按钮点击事件：验证输入并发送登录请求 */
    void on_btnLogin_clicked();

    /* 跳转注册按钮点击事件：断开信号后打开注册对话框 */
    void on_btnGoRegister_clicked();

    /* TCP 连接成功回调：启用登录和注册按钮 */
    void onConnected();

    /*
     * 数据到达回调：处理服务器返回的登录响应
     * - LOGIN_OK：登录成功，断开信号后关闭对话框
     * - LOGIN_FAIL:原因：登录失败，显示原因并重新启用按钮
     */
    void onReadyRead();

    /*
     * 网络错误回调：显示错误信息并启动自动重连定时器
     * 包含 m_socket 空指针检查，防止 takeSocket() 后误触发
     */
    void onError(QAbstractSocket::SocketError socketError);

    /*
     * 自动重连定时器回调：
     * 每 3 秒触发一次，尝试重新连接服务器
     * 连接成功后自动停止定时器
     */
    void onReconnectTimeout();

private:
    /* 更新状态栏文本 */
    void setStatus(const QString &text);

    /* 发起 TCP 连接到服务器 */
    void connectToServer();

    /* 构造并发送 LOGIN:用户名:密码\0 协议消息 */
    void sendLoginRequest();

    Ui::LoginDialog *ui;       /* UI 界面对象指针，由 Qt Designer 生成 */
    QTcpSocket *m_socket;      /* TCP 客户端 Socket，登录成功后转移给 ChatDialog */
    QString m_username;        /* 当前登录的用户名 */
    QString m_password;        /* 当前登录的密码（用于传递给 ChatDialog 断线重连） */
    QByteArray m_buffer;       /* 接收缓冲区，用于粘包处理（以 '\0' 分隔消息） */
    QByteArray m_pendingData;  /* 登录成功后缓冲区中的剩余数据，传递给 ChatDialog */
    bool m_connected;          /* TCP 是否已连接到服务器 */
    QTimer *m_reconnectTimer;  /* 自动重连定时器，连接失败时每 3 秒重试 */
    int m_reconnectCount;      /* 重连次数计数，用于显示状态信息 */
};

#endif
