/*
 * chatdialog.h - 聊天对话框头文件
 *
 * 功能概述：
 *   聊天对话框是用户登录后的主界面，负责：
 *   1. 发送聊天消息到服务器
 *   2. 接收并显示其他用户的消息（NEW_MSG）和系统通知（SYSTEM）
 *   3. 处理连接断开和错误事件
 *
 * 关键设计：
 *   - m_socket 由 LoginDialog 通过 takeSocket() 转移而来，本对象拥有所有权
 *   - 使用 m_buffer 进行 TCP 粘包处理，以 '\0' 作为消息分隔符
 *   - m_connected 标记连接状态，断开后禁用发送功能
 *   - 消息使用 HTML 富文本格式显示，区分自己/他人/系统消息
 */

#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class ChatDialog; }
QT_END_NAMESPACE

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    /*
     * 构造函数 - 接收 Socket 和用户名
     *
     * 参数：
     *   socket   - 从 LoginDialog 转移的 QTcpSocket，本对象拥有所有权
     *   username - 当前登录的用户名，用于消息显示
     *   parent   - 父窗口
     */
    ChatDialog(QTcpSocket *socket, const QString &username, QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    /*
     * 发送按钮点击事件：
     *   构造 CHAT:消息内容\0 协议消息发送到服务器，
     *   同时在本地聊天记录中显示自己的消息
     */
    void on_btnSend_clicked();

    /*
     * 数据到达回调：处理服务器转发的消息
     * - NEW_MSG:发送者:内容 - 其他用户发送的消息
     * - SYSTEM:系统消息     - 系统通知（用户上线/下线等）
     * - 其他                - 未知格式，原样显示
     */
    void onReadyRead();

    /*
     * 连接断开回调：
     *   当服务器主动关闭连接或网络中断时触发，
     *   标记 m_connected = false，禁用发送按钮和输入框
     */
    void onDisconnected();

    /*
     * 网络错误回调：
     *   当 Socket 发生错误时触发（如连接重置、超时等），
     *   显示错误信息并禁用输入控件
     */
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    /*
     * showSystemMsg() - 在聊天记录中显示居中的系统消息
     *
     * 用于显示欢迎消息、上下线通知、连接断开提示等，
     * 以灰色居中样式显示，与普通聊天消息区分
     */
    void showSystemMsg(const QString &msg);

    /*
     * showAiMsg() - 在聊天记录中显示 AI 回复消息
     *
     * 用于显示 DeepSeek AI 的回复，
     * 以紫色(#9b59b6)用户名 + 黑色内容样式显示
     *
     * 参数：
     *   aiName  - AI 名称（如 "🤖 AI助手"）
     *   content - AI 回复内容
     */
    void showAiMsg(const QString &aiName, const QString &content);

    /*
     * onModeChanged() - 聊天模式切换回调
     *
     * 当用户切换 cmbMode 下拉框时触发：
     *   - "💬 群聊"    → 普通群聊模式，placeholder 提示输入消息
     *   - "🤖 私聊AI"  → AI 私聊模式，placeholder 提示输入问题
     */
    void onModeChanged(int index);

    Ui::ChatDialog *ui;       /* UI 界面对象指针，由 Qt Designer 生成 */
    QTcpSocket *m_socket;     /* TCP Socket（本对象拥有所有权） */
    QString m_username;       /* 当前登录的用户名 */
    QByteArray m_buffer;      /* 接收缓冲区，用于粘包处理（以 '\0' 分隔消息） */
    bool m_connected;         /* 连接状态标记，false 时禁用发送功能 */
};
#endif
