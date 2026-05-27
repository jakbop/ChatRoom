/*
 * chatdialog.cpp - 聊天对话框实现文件
 *
 * 本文件实现了聊天对话框的全部逻辑，包括：
 *   - 聊天消息的发送与接收
 *   - 服务器转发消息的解析与显示
 *   - 连接断开和错误的处理
 *   - HTML 富文本格式的消息渲染
 *
 * 通信协议：
 *   发送：CHAT:消息内容\0
 *   接收：NEW_MSG:发送者:内容\0  或  SYSTEM:系统消息\0
 *
 * 消息显示样式：
 *   - 自己的消息：蓝色用户名 + 黑色内容
 *   - 他人消息：  绿色用户名 + 黑色内容
 *   - 系统消息：  灰色居中显示
 */

#include "chatdialog.h"
#include "ui_chatdialog.h"

/* ==========================================================================
 * 构造函数 - 初始化 UI、设置 Socket 父对象、绑定信号槽
 *
 * 参数：
 *   socket   - 从 LoginDialog::takeSocket() 获取的 Socket
 *   username - 登录成功后保存的用户名
 *   parent   - 父窗口
 *
 * Socket 所有权转移：
 *   调用 m_socket->setParent(this) 将 Socket 的 Qt 父对象设为 ChatDialog，
 *   当 ChatDialog 销毁时，Socket 也会被自动释放。
 * ========================================================================== */
ChatDialog::ChatDialog(QTcpSocket *socket, const QString &username, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog)
    , m_socket(socket)
    , m_username(username)
    , m_connected(true)               /* 初始状态：已连接（登录时已建立连接） */
{
    ui->setupUi(this);

    /* 设置窗口标题和头部标签 */
    setWindowTitle("聊天室 - " + m_username);
    ui->lblHeader->setText("💬 聊天室 - " + m_username);

    /*
     * 将 Socket 的 Qt 父对象设为 ChatDialog
     * 这样当 ChatDialog 销毁时，Socket 也会被自动 delete
     */
    m_socket->setParent(this);

    /*
     * 绑定三个关键信号：
     *   readyRead   - 服务器发来消息，需要读取并解析
     *   disconnected - 连接断开（服务器关闭或网络中断）
     *   error       - Socket 发生错误
     */
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatDialog::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatDialog::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ChatDialog::onSocketError);

    /* 绑定聊天模式切换信号 */
    connect(ui->cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatDialog::onModeChanged);

    /* 设置输入框默认提示 */
    ui->edtMsg->setPlaceholderText("输入消息，@AI 开头向AI提问...");

    /* 显示欢迎消息 */
    showSystemMsg("欢迎进入聊天室！");
}

/* ==========================================================================
 * 析构函数 - 断开连接并释放资源
 *
 * 当用户关闭聊天窗口时调用，主动断开与服务器的 TCP 连接。
 * Socket 会由 Qt 的父子对象机制自动释放（setParent 已设置）。
 * ========================================================================== */
ChatDialog::~ChatDialog()
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
    }
    delete ui;
}

/* ==========================================================================
 * on_btnSend_clicked() - 发送按钮点击事件
 *
 * 流程：
 *   1. 检查连接状态，断开时提示用户
 *   2. 获取输入框文本并去除首尾空白
 *   3. 构造 CHAT:消息内容\0 协议消息发送到服务器
 *   4. 在本地聊天记录中显示自己发送的消息（蓝色用户名）
 *   5. 清空输入框并重新聚焦，方便连续输入
 * ========================================================================== */
void ChatDialog::on_btnSend_clicked()
{
    /* 连接已断开时，禁止发送并提示 */
    if (!m_connected)
    {
        showSystemMsg("连接已断开，无法发送消息");
        return;
    }

    QString msg = ui->edtMsg->toPlainText().trimmed();

    if (!msg.isEmpty())
    {
        int mode = ui->cmbMode->currentIndex();

        if (mode == 1)
        {
            /* ===== 私聊 AI 模式 =====
             * 协议：CHAT:@AI:PRIVATE:问题内容\0
             * 服务端识别 @AI:PRIVATE: 前缀，AI 回复仅发送给提问者
             */
            QString fullMsg = "CHAT:@AI:PRIVATE:" + msg;
            m_socket->write(fullMsg.toUtf8() + '\0');

            /* 本地显示自己的提问（蓝色用户名） */
            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px;'>"
                "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
                "<span style='color:#333333;'>" + msg.toHtmlEscaped() + "</span>"
                "</div>"
            );

            /* 显示 AI 正在思考的提示 */
            showSystemMsg("AI 正在思考中...");
        }
        else
        {
            /* ===== 群聊模式 =====
             * 如果消息以 @AI 开头，服务端会识别为公聊 AI 提问
             * 否则作为普通聊天消息转发
             */
            QString fullMsg = "CHAT:" + msg;
            m_socket->write(fullMsg.toUtf8() + '\0');

            /* 本地显示自己发送的消息 */
            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px;'>"
                "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
                "<span style='color:#333333;'>" + msg.toHtmlEscaped() + "</span>"
                "</div>"
            );

            /* 群聊模式下 @AI 提问也显示思考提示 */
            if (msg.startsWith("@AI"))
            {
                showSystemMsg("AI 正在思考中...");
            }
        }
    }

    /* 清空输入框并聚焦，方便用户继续输入 */
    ui->edtMsg->clear();
    ui->edtMsg->setFocus();
}

/* ==========================================================================
 * onReadyRead() - 数据到达回调
 *
 * 处理 TCP 粘包问题：
 *   与 LoginDialog 和 RegisterDialog 相同，使用 m_buffer 缓冲区
 *   累积接收数据，以 '\0' 作为消息分隔符逐条提取完整消息。
 *
 * 消息类型处理：
 *   1. NEW_MSG:发送者:内容
 *      - 服务器转发的其他用户消息
 *      - 绿色(#27ae60)加粗发送者用户名 + 黑色消息内容
 *      - 使用 toHtmlEscaped() 防止 HTML 注入
 *
 *   2. SYSTEM:系统消息
 *      - 服务器发送的系统通知（用户上线/下线等）
 *      - 灰色居中显示，与聊天消息区分
 *
 *   3. 其他
 *      - 未知格式的消息，原样显示
 * ========================================================================== */
void ChatDialog::onReadyRead()
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

        QString text = QString::fromUtf8(msg);

        if (text.startsWith("NEW_MSG:"))
        {
            /* 解析 NEW_MSG:发送者:内容 格式 */
            QString content = text.mid(8);  /* 去掉 "NEW_MSG:" 前缀 */
            int sep = content.indexOf(':');  /* 找到发送者和内容的分隔符 */
            if (sep != -1)
            {
                QString sender = content.left(sep);       /* 发送者用户名 */
                QString message = content.mid(sep + 1);   /* 消息内容 */

                ui->edtRecord->append(
                    "<div style='margin:14px 0; font-size:24px;'>"
                    "<span style='color:#27ae60; font-weight:bold;'>[" + sender + "]</span> "
                    "<span style='color:#333333;'>" + message.toHtmlEscaped() + "</span>"
                    "</div>"
                );
            }
        }
        else if (text.startsWith("AI_RESP:"))
        {
            /* AI 公聊回复：广播给所有人的 AI 回复
             * 格式：AI_RESP:🤖 AI助手:回复内容
             * 紫色(#9b59b6)用户名 + 黑色内容
             */
            QString content = text.mid(8);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString aiName = content.left(sep);
                QString aiMsg = content.mid(sep + 1);
                showAiMsg(aiName, aiMsg);
            }
        }
        else if (text.startsWith("AI_PRIV_RESP:"))
        {
            /* AI 私聊回复：仅提问者可见的 AI 回复
             * 格式：AI_PRIV_RESP:🤖 AI助手:回复内容
             * 紫色(#9b59b6)用户名 + 黑色内容 + "（私聊）"标记
             */
            QString content = text.mid(13);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString aiName = content.left(sep);
                QString aiMsg = content.mid(sep + 1);
                showAiMsg(aiName + "（私聊）", aiMsg);
            }
        }
        else if (text.startsWith("SYSTEM:"))
        {
            /* 系统消息：灰色居中显示 */
            showSystemMsg(text.mid(7));
        }
        else
        {
            /* 未知格式消息：原样显示（黑色文本） */
            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px; color:#333333;'>" + text.toHtmlEscaped() + "</div>"
            );
        }
    }
}

/* ==========================================================================
 * onDisconnected() - 连接断开回调
 *
 * 触发场景：
 *   - 服务器主动关闭连接（如超时踢出、服务器关闭）
 *   - 网络中断
 *
 * 处理：
 *   1. 标记 m_connected = false，防止发送操作
 *   2. 显示断开提示
 *   3. 禁用发送按钮和输入框，避免用户在断开状态下操作
 * ========================================================================== */
void ChatDialog::onDisconnected()
{
    m_connected = false;
    showSystemMsg("与服务器断开连接");

    /* 禁用输入控件，防止断开后继续发送 */
    ui->btnSend->setEnabled(false);
    ui->edtMsg->setEnabled(false);
}

/* ==========================================================================
 * onSocketError() - 网络错误回调
 *
 * 触发场景：
 *   - 连接重置（ECONNRESET）
 *   - 网络不可达
 *   - DNS 解析失败等
 *
 * 处理：
 *   1. 标记 m_connected = false
 *   2. 显示具体错误信息（来自 Socket 的 errorString）
 *   3. 禁用输入控件
 * ========================================================================== */
void ChatDialog::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    m_connected = false;
    showSystemMsg("连接出错：" + m_socket->errorString());

    /* 禁用输入控件 */
    ui->btnSend->setEnabled(false);
    ui->edtMsg->setEnabled(false);
}

/* ==========================================================================
 * showSystemMsg() - 在聊天记录中显示居中的系统消息
 *
 * 样式：灰色(#999)文字，居中对齐，上下有内边距
 * 用于：欢迎消息、上下线通知、连接状态提示等
 * ========================================================================== */
void ChatDialog::showSystemMsg(const QString &msg)
{
    ui->edtRecord->append(
        "<div style='text-align:center; color:#999; font-size:19px; padding:14px;'>"
        "—— " + msg + " ——</div>"
    );
}

/* ==========================================================================
 * showAiMsg() - 在聊天记录中显示 AI 回复消息
 *
 * 样式：紫色(#9b59b6)加粗 AI 名称 + 黑色消息内容
 * 与普通用户消息区分（蓝色=自己，绿色=他人，紫色=AI）
 *
 * 参数：
 *   aiName  - AI 名称（如 "🤖 AI助手" 或 "🤖 AI助手（私聊）"）
 *   content - AI 回复内容
 * ========================================================================== */
void ChatDialog::showAiMsg(const QString &aiName, const QString &content)
{
    ui->edtRecord->append(
        "<div style='margin:14px 0; font-size:24px; background-color:#f0e6ff; "
        "border-radius:8px; padding:10px 14px;'>"
        "<span style='color:#9b59b6; font-weight:bold;'>[" + aiName + "]</span> "
        "<span style='color:#333333;'>" + content.toHtmlEscaped() + "</span>"
        "</div>"
    );
}

/* ==========================================================================
 * onModeChanged() - 聊天模式切换回调
 *
 * 当用户切换 cmbMode 下拉框时触发：
 *   - index 0: "💬 群聊"    → 普通群聊模式
 *   - index 1: "🤖 私聊AI"  → AI 私聊模式
 *
 * 切换模式时会更新输入框的 placeholder 提示文字，
 * 帮助用户了解当前模式的输入方式
 * ========================================================================== */
void ChatDialog::onModeChanged(int index)
{
    if (index == 0)
    {
        ui->edtMsg->setPlaceholderText("输入消息，@AI 开头向AI提问...");
    }
    else if (index == 1)
    {
        ui->edtMsg->setPlaceholderText("输入问题，直接向AI提问（仅你可见）...");
    }
}
