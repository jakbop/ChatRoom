/*
 * registerdialog.cpp - 注册对话框实现文件
 *
 * 本文件实现了注册对话框的全部逻辑，包括：
 *   - 用户输入验证（非空、密码长度、密码一致性）
 *   - 注册请求的发送与响应处理
 *   - 信号-槽的自动清理（析构时断开 readyRead）
 *
 * 通信协议：
 *   发送：REG:用户名:密码\0
 *   接收：REG_OK\0 或 REG_FAIL:原因\0
 */

#include "registerdialog.h"
#include "ui_registerdialog.h"

/* ==========================================================================
 * 构造函数 - 初始化 UI、绑定 Socket 的 readyRead 信号
 *
 * 参数：
 *   socket - LoginDialog 传入的共享 Socket，已建立 TCP 连接
 *   parent - 父窗口（LoginDialog）
 *
 * 注意：LoginDialog 在打开注册对话框前已断开了自己的 readyRead 信号，
 *       因此只有 RegisterDialog 会响应服务器消息，避免信号冲突。
 * ========================================================================== */
RegisterDialog::RegisterDialog(QTcpSocket *socket, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , m_socket(socket)              /* 保存外部传入的 Socket，不拥有所有权 */
{
    ui->setupUi(this);

    /* 移除窗口标题栏上的 "?" 帮助按钮 */
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMinimizeButtonHint);

    /* 连接 readyRead 信号，接收服务器返回的注册结果 */
    connect(m_socket, &QTcpSocket::readyRead, this, &RegisterDialog::onReadyRead);
}

/* ==========================================================================
 * 析构函数 - 断开信号连接并释放 UI 资源
 *
 * 关键：必须在析构时断开 readyRead 信号连接！
 *   因为 m_socket 的生命周期由 LoginDialog 管理，如果 RegisterDialog
 *   已销毁但信号仍连接，当 socket 有数据到达时会尝试调用已销毁对象
 *   的槽函数，导致未定义行为（崩溃）。
 * ========================================================================== */
RegisterDialog::~RegisterDialog()
{
    /* 断开 readyRead 信号，防止槽函数在对象销毁后被调用 */
    disconnect(m_socket, &QTcpSocket::readyRead, this, &RegisterDialog::onReadyRead);
    delete ui;
}

/* ==========================================================================
 * on_btnRegister_clicked() - 注册按钮点击事件
 *
 * 验证流程：
 *   1. 用户名和密码不能为空
 *   2. 密码长度不少于 4 位（基本安全要求）
 *   3. 两次输入的密码必须一致
 *
 * 验证通过后调用 sendRegisterRequest() 发送注册请求
 * ========================================================================== */
void RegisterDialog::on_btnRegister_clicked()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();
    QString confirm = ui->edtConfirm->text();

    /* 验证：用户名和密码不能为空 */
    if (username.isEmpty() || password.isEmpty())
    {
        setStatus("用户名和密码不能为空");
        return;
    }

    /* 验证：密码长度不少于 4 位 */
    if (password.length() < 4)
    {
        setStatus("密码长度不能少于4位");
        return;
    }

    /* 验证：两次输入的密码必须一致 */
    if (password != confirm)
    {
        setStatus("两次输入的密码不一致");
        return;
    }

    sendRegisterRequest();
}

/* ==========================================================================
 * on_btnBack_clicked() - 返回按钮点击事件
 *
 * 调用 reject() 关闭注册对话框，返回 QDialog::Rejected。
 * LoginDialog 的 on_btnGoRegister_clicked() 中会重新连接 readyRead 信号。
 * ========================================================================== */
void RegisterDialog::on_btnBack_clicked()
{
    reject();
}

/* ==========================================================================
 * onReadyRead() - 数据到达回调
 *
 * 处理 TCP 粘包问题：
 *   与 LoginDialog 和 ChatDialog 相同，使用 m_buffer 缓冲区
 *   累积接收数据，以 '\0' 作为消息分隔符逐条提取。
 *
 * 消息处理：
 *   - REG_OK：注册成功
 *       1. 显示"注册成功！请返回登录"
 *       2. 清空密码和确认密码输入框（安全考虑）
 *       3. 重新启用注册按钮（允许注册其他账号）
 *       注意：不自动关闭对话框，让用户手动点击"返回"
 *   - REG_FAIL:原因：注册失败，显示原因并重新启用按钮
 * ========================================================================== */
void RegisterDialog::onReadyRead()
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

        if (response == "REG_OK")
        {
            /* 注册成功，提示用户返回登录 */
            setStatus("注册成功！请返回登录");
            /* 清空密码输入框，防止密码泄露 */
            ui->edtPassword->clear();
            ui->edtConfirm->clear();
            /* 重新启用注册按钮 */
            ui->btnRegister->setEnabled(true);
        }
        else if (response.startsWith("REG_FAIL:"))
        {
            /* 注册失败，提取并显示失败原因 */
            setStatus("注册失败：" + response.mid(9));
            /* 重新启用注册按钮，允许用户修改后重试 */
            ui->btnRegister->setEnabled(true);
        }
    }
}

/* ==========================================================================
 * setStatus() - 更新状态栏文本
 * ========================================================================== */
void RegisterDialog::setStatus(const QString &text)
{
    ui->lblStatus->setText(text);
}

/* ==========================================================================
 * sendRegisterRequest() - 构造并发送注册请求
 *
 * 协议格式：REG:用户名:密码\0
 *
 * 发送后禁用注册按钮，防止重复提交。
 * 按钮会在收到服务器响应后重新启用（onReadyRead 中处理）。
 * ========================================================================== */
void RegisterDialog::sendRegisterRequest()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    /* 构造协议消息，以 '\0' 作为分隔符 */
    QString msg = "REG:" + username + ":" + password;
    m_socket->write(msg.toUtf8() + '\0');

    setStatus("正在注册...");

    /* 禁用注册按钮，等待服务器响应 */
    ui->btnRegister->setEnabled(false);
}
