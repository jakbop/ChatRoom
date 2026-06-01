/*
 * registerdialog.h - 注册对话框头文件
 *
 * 功能概述：
 *   注册对话框由 LoginDialog 打开（模态方式），负责：
 *   1. 收集用户注册信息（用户名、密码、确认密码）
 *   2. 发送注册请求到服务器并处理响应
 *   3. 提供返回登录界面的功能
 *
 * 关键设计：
 *   - m_socket 由 LoginDialog 传入（不拥有所有权），注册期间共享使用
 *   - 构造时连接 readyRead 信号，析构时断开，确保信号不会泄漏
 *   - 使用 m_buffer 进行粘包处理，与 LoginDialog/ChatDialog 机制一致
 *   - 注册成功后不会自动关闭对话框，用户需手动点击"返回"按钮
 */

#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class RegisterDialog; }
QT_END_NAMESPACE

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    /*
     * 构造函数 - 接收外部传入的 Socket 指针
     *
     * 参数：
     *   socket - 由 LoginDialog 创建的 QTcpSocket，注册期间共享使用
     *   parent - 父窗口（LoginDialog）
     *
     * 注意：本对象不拥有 socket 的所有权，不负责其生命周期管理
     */
    explicit RegisterDialog(QTcpSocket *socket, QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    /* 注册按钮点击事件：验证输入并发送注册请求 */
    void on_btnRegister_clicked();

    /* 返回按钮点击事件：关闭注册对话框，返回登录界面 */
    void on_btnBack_clicked();

    /*
     * 数据到达回调：处理服务器返回的注册响应
     * - REG_OK：注册成功，提示用户返回登录
     * - REG_FAIL:原因：注册失败，显示原因并重新启用按钮
     */
    void onReadyRead();

private:
    /* 更新状态栏文本 */
    void setStatus(const QString &text);

    /* 构造并发送 REG:用户名:密码\0 协议消息 */
    void sendRegisterRequest();

    Ui::RegisterDialog *ui;    /* UI 界面对象指针，由 Qt Designer 生成 */
    QTcpSocket *m_socket;      /* TCP Socket（外部传入，不拥有所有权） */
    QByteArray m_buffer;       /* 接收缓冲区，用于粘包处理（以 '\0' 分隔消息） */
};

#endif
