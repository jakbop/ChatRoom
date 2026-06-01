#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QTextBlock>

ChatDialog::ChatDialog(QTcpSocket *socket, const QString &username, const QString &password,
                       const QByteArray &pendingData, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog)
    , m_socket(socket)
    , m_username(username)
    , m_password(password)
    , m_connected(true)
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectCount(0)
    , m_reconnecting(false)
    , m_fileSocket(nullptr)
    , m_totalChunks(0)
    , m_uploadFileSize(0)
    , m_fileMd5()
    , m_uploadedChunkCount(0)
    , m_downloadFileSize(0)
    , m_downloadTotalChunks(0)
    , m_downloadedChunkCount(0)
    , m_currentDownloadChunk(0)
    , m_transferProgress(nullptr)
    , m_transferStatus(nullptr)
{
    ui->setupUi(this);

    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMinimizeButtonHint);

    setWindowTitle("聊天室 - " + m_username);
    ui->lblHeader->setText("💬 聊天室 - " + m_username);

    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &ChatDialog::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatDialog::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ChatDialog::onSocketError);
    connect(m_socket, &QTcpSocket::connected, this, &ChatDialog::onReconnected);

    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatDialog::onReconnectTimeout);

    connect(ui->cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatDialog::onModeChanged);

    connect(ui->lstUsers, &QListWidget::itemClicked,
            this, &ChatDialog::onUserItemClicked);

    connect(ui->edtRecord, &QTextBrowser::anchorClicked,
            this, &ChatDialog::onAnchorClicked);
    ui->edtRecord->setOpenLinks(false);

    ui->edtMsg->setPlaceholderText("输入消息，@AI 开头向AI提问...");

    showSystemMsg("欢迎进入聊天室！");

    if (!pendingData.isEmpty())
    {
        m_buffer = pendingData;
        onReadyRead();
    }
}

ChatDialog::~ChatDialog()
{
    if (m_uploadFile.isOpen())
        m_uploadFile.close();
    if (m_downloadFile.isOpen())
        m_downloadFile.close();
    if (m_fileSocket)
    {
        m_fileSocket->disconnectFromHost();
        delete m_fileSocket;
        m_fileSocket = nullptr;
    }
    if (m_socket)
    {
        m_reconnectTimer->stop();
        m_socket->disconnectFromHost();
    }
    delete ui;
}

void ChatDialog::on_btnSend_clicked()
{
    if (!m_connected)
    {
        showSystemMsg("连接已断开，正在尝试重连...");
        return;
    }

    QString msg = ui->edtMsg->toPlainText().trimmed();

    if (!msg.isEmpty())
    {
        int mode = ui->cmbMode->currentIndex();

        if (mode == 1)
        {
            QString fullMsg = "CHAT:@AI:PRIVATE:" + msg;
            m_socket->write(fullMsg.toUtf8() + '\0');

            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px;'>"
                "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
                "<span style='color:#333333;'>" + msg.toHtmlEscaped() + "</span>"
                "</div>"
            );

            showSystemMsg("AI 正在思考中...");
        }
        else if (mode == 2)
        {
            if (m_whisperTarget.isEmpty())
            {
                showSystemMsg("请先从右侧在线用户列表中选择私聊对象");
                return;
            }

            QString fullMsg = "WHISPER:" + m_whisperTarget + ":" + msg;
            m_socket->write(fullMsg.toUtf8() + '\0');
        }
        else
        {
            QString fullMsg = "CHAT:" + msg;
            m_socket->write(fullMsg.toUtf8() + '\0');

            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px;'>"
                "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
                "<span style='color:#333333;'>" + msg.toHtmlEscaped() + "</span>"
                "</div>"
            );

            if (msg.startsWith("@AI"))
            {
                showSystemMsg("AI 正在思考中...");
            }
        }
    }

    ui->edtMsg->clear();
    ui->edtMsg->setFocus();
}

void ChatDialog::on_btnSendFile_clicked()
{
    if (!m_connected)
    {
        showSystemMsg("连接已断开，无法发送文件");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件", QString(),
                                                     "所有文件 (*.*)");
    if (filePath.isEmpty())
        return;

    QFileInfo fi(filePath);
    qint64 fileSize = fi.size();

    if (fileSize > 500 * 1024 * 1024)
    {
        showSystemMsg("文件大小超过 500MB 限制");
        return;
    }

    m_uploadFile.setFileName(filePath);
    m_uploadFilename = fi.fileName();
    m_uploadFileSize = fileSize;
    m_totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    m_uploadedChunkCount = 0;

    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        const int bufSize = 1024 * 1024;
        while (!f.atEnd())
            hash.addData(f.read(bufSize));
        m_fileMd5 = hash.result().toHex();
        f.close();
    }

    int mode = ui->cmbMode->currentIndex();
    QString targetUser;
    if (mode == 2 && !m_whisperTarget.isEmpty())
        targetUser = m_whisperTarget;
    else
        targetUser = "ALL";

    double sizeKB = fileSize / 1024.0;
    QString sizeStr;
    if (sizeKB > 1024)
        sizeStr = QString("%1MB").arg(sizeKB / 1024.0, 0, 'f', 1);
    else
        sizeStr = QString("%1KB").arg(sizeKB, 0, 'f', 1);

    QString fullMsg = QString("FILE_UPLOAD_START:%1:%2:%3:%4:%5")
                          .arg(targetUser).arg(m_uploadFilename)
                          .arg(m_uploadFileSize).arg(m_totalChunks).arg(QString::fromUtf8(m_fileMd5));
    m_socket->write(fullMsg.toUtf8() + '\0');

    ui->edtRecord->append(
        "<div style='margin:14px 0; font-size:24px;'>"
        "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
        "<span style='color:#27ae60; font-weight:bold;'>📎 上传文件：" + m_uploadFilename.toHtmlEscaped()
        + " (" + sizeStr + ")，等待服务器准备...</span>"
        "</div>"
    );
}

void ChatDialog::onReadyRead()
{
    m_buffer += m_socket->readAll();

    int index;
    while ((index = m_buffer.indexOf('\0')) != -1)
    {
        QByteArray msg = m_buffer.left(index);
        m_buffer = m_buffer.mid(index + 1);

        QString text = QString::fromUtf8(msg);

        if (text.startsWith("NEW_MSG:"))
        {
            QString content = text.mid(8);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString sender = content.left(sep);
                QString message = content.mid(sep + 1);

                ui->edtRecord->append(
                    "<div style='margin:14px 0; font-size:24px;'>"
                    "<span style='color:#27ae60; font-weight:bold;'>[" + sender + "]</span> "
                    "<span style='color:#333333;'>" + message.toHtmlEscaped() + "</span>"
                    "</div>"
                );
            }
        }
        else if (text.startsWith("WHISPER_MSG:"))
        {
            QString content = text.mid(12);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString sender = content.left(sep);
                QString message = content.mid(sep + 1);
                showWhisperMsg(sender, message);
            }
        }
        else if (text.startsWith("WHISPER_SENT:"))
        {
            QString content = text.mid(13);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString target = content.left(sep);
                QString message = content.mid(sep + 1);
                showWhisperSent(target, message);
            }
        }
        else if (text.startsWith("FILE_NOTIFY:"))
        {
            QString content = text.mid(12);
            int sep1 = content.indexOf(':');
            if (sep1 != -1)
            {
                QString sender = content.left(sep1);
                QString rest = content.mid(sep1 + 1);
                int sep2 = rest.indexOf(':');
                if (sep2 != -1)
                {
                    QString filename = rest.left(sep2);
                    QString rest2 = rest.mid(sep2 + 1);
                    int sep3 = rest2.indexOf(':');
                    QString fileSize;
                    QString fileMd5;
                    if (sep3 != -1)
                    {
                        fileSize = rest2.left(sep3);
                        fileMd5 = rest2.mid(sep3 + 1);
                    }
                    else
                    {
                        fileSize = rest2;
                    }

                    QString key = sender + "|" + filename;

                    ui->edtRecord->append(
                        "<div style='margin:14px 0; font-size:24px; background-color:#e8f5e9; "
                        "border-radius:8px; padding:10px 14px;'>"
                        "<span style='color:#27ae60; font-weight:bold;'>[" + sender + "]</span> "
                        "<span style='color:#27ae60; font-weight:bold;'>📎 发送文件：" + filename.toHtmlEscaped()
                        + " (" + fileSize + ")</span><br/>"
                        "<a href='http://chatroom.download/" + key + "' style='color:#4a90d9; font-weight:bold; font-size:22px; text-decoration:none; "
                        "background-color:#e3f2fd; padding:6px 16px; border-radius:6px; margin-right:10px;'>📥 下载</a>"
                        "<a href='http://chatroom.reject/" + key + "' style='color:#e74c3c; font-weight:bold; font-size:22px; text-decoration:none; "
                        "background-color:#fdeaea; padding:6px 16px; border-radius:6px;'>❌ 拒绝</a>"
                        "</div>"
                    );
                    m_fileNotifyBlocks[key] = ui->edtRecord->document()->blockCount() - 1;
                }
            }
        }
        else if (text.startsWith("FILE_UPLOAD_READY:"))
        {
            m_uploadFilename = text.mid(18);
            m_fileSocketRole = "sender";

            if (m_fileSocket)
            {
                m_fileSocket->disconnectFromHost();
                delete m_fileSocket;
            }

            m_fileSocket = new QTcpSocket(this);
            m_fileBuffer.clear();

            connect(m_fileSocket, &QTcpSocket::connected, this, &ChatDialog::onFileConnected);
            connect(m_fileSocket, &QTcpSocket::readyRead, this, &ChatDialog::onFileReadyRead);
            connect(m_fileSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                    this, &ChatDialog::onFileError);

            m_fileSocket->connectToHost("39.104.71.92", FILE_PORT);

            showSystemMsg("服务器已准备好，正在连接文件端口上传...");
        }
        else if (text.startsWith("FILE_UPLOAD_COMPLETE:"))
        {
            QString content = text.mid(21);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString sender = content.left(sep);
                QString filename = content.mid(sep + 1);
                showSystemMsg("文件上传完成：" + filename);
                hideTransferProgress();
            }
        }
        else if (text.startsWith("FILE_DOWNLOAD_INFO:"))
        {
            QString content = text.mid(19);
            int sep1 = content.indexOf(':');
            if (sep1 != -1)
            {
                QString sizeStr = content.left(sep1);
                QString rest = content.mid(sep1 + 1);
                int sep2 = rest.indexOf(':');
                if (sep2 != -1)
                {
                    m_downloadFileSize = sizeStr.toLongLong();
                    m_downloadTotalChunks = rest.left(sep2).toInt();
                    m_downloadedChunkCount = 0;

                    QString savePath = QFileDialog::getSaveFileName(this, "保存文件",
                                                                     m_downloadFilename, "所有文件 (*.*)");
                    if (savePath.isEmpty())
                    {
                        showSystemMsg("已取消下载");
                        return;
                    }

                    m_downloadFile.setFileName(savePath);
                    if (!m_downloadFile.open(QIODevice::WriteOnly))
                    {
                        showSystemMsg("无法创建文件：" + savePath);
                        return;
                    }

                    m_downloadFile.resize(m_downloadFileSize);

                    m_fileSocketRole = "receiver";
                    m_fileSocketFilename = m_downloadFilename;

                    if (m_fileSocket)
                    {
                        m_fileSocket->disconnectFromHost();
                        delete m_fileSocket;
                    }

                    m_fileSocket = new QTcpSocket(this);
                    m_fileBuffer.clear();

                    connect(m_fileSocket, &QTcpSocket::connected, this, &ChatDialog::onFileConnected);
                    connect(m_fileSocket, &QTcpSocket::readyRead, this, &ChatDialog::onFileReadyRead);
                    connect(m_fileSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                            this, &ChatDialog::onFileError);

                    m_fileSocket->connectToHost("39.104.71.92", FILE_PORT);

                    showSystemMsg("正在连接文件端口下载 " + m_downloadFilename + "...");
                }
            }
        }
        else if (text.startsWith("FILE_REJECT:"))
        {
            QString content = text.mid(12);
            int sep = content.indexOf(':');
            if (sep != -1)
            {
                QString receiver = content.left(sep);
                QString filename = content.mid(sep + 1);
                showSystemMsg(receiver + " 拒绝了文件 " + filename);
            }
        }
        else if (text.startsWith("FILE_ERROR:"))
        {
            showSystemMsg("文件错误：" + text.mid(11));
        }
        else if (text.startsWith("USER_LIST:"))
        {
            updateUserList(text.mid(10));
        }
        else if (text.startsWith("AI_RESP:"))
        {
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
            showSystemMsg(text.mid(7));
        }
        else if (text == "LOGIN_OK")
        {
            m_connected = true;
            m_reconnecting = false;
            ui->btnSend->setEnabled(true);
            ui->edtMsg->setEnabled(true);
            ui->btnSendFile->setEnabled(true);
            showSystemMsg("重新连接服务器成功！");
        }
        else if (text.startsWith("LOGIN_FAIL:"))
        {
            showSystemMsg("重连登录失败：" + text.mid(11) + "，将继续重试...");
        }
        else
        {
            ui->edtRecord->append(
                "<div style='margin:14px 0; font-size:24px; color:#333333;'>" + text.toHtmlEscaped() + "</div>"
            );
        }
    }
}

void ChatDialog::onDisconnected()
{
    m_connected = false;

    ui->btnSend->setEnabled(false);
    ui->edtMsg->setEnabled(false);
    ui->btnSendFile->setEnabled(false);

    if (!m_reconnecting)
    {
        showSystemMsg("与服务器断开连接");
        m_reconnecting = true;
        m_reconnectCount = 0;
    }

    if (!m_reconnectTimer->isActive())
    {
        m_reconnectTimer->start(3000);
    }
}

void ChatDialog::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    m_connected = false;

    ui->btnSend->setEnabled(false);
    ui->edtMsg->setEnabled(false);
    ui->btnSendFile->setEnabled(false);

    if (!m_reconnecting)
    {
        showSystemMsg("与服务器断开连接");
        m_reconnecting = true;
        m_reconnectCount = 0;
    }

    if (!m_reconnectTimer->isActive())
    {
        m_reconnectTimer->start(3000);
    }
}

void ChatDialog::onReconnected()
{
    m_reconnectTimer->stop();
    m_reconnectCount = 0;

    sendLoginRequest();
}

void ChatDialog::onReconnectTimeout()
{
    m_reconnectCount++;

    m_socket->abort();
    m_socket->connectToHost("39.104.71.92", 9413);
}

void ChatDialog::onUserItemClicked(QListWidgetItem *item)
{
    QString displayText = item->text();

    displayText.remove("👤 ");

    if (displayText.endsWith("（我）"))
        displayText.chop(4);

    if (displayText == m_username)
    {
        showSystemMsg("不能和自己私聊");
        return;
    }

    m_whisperTarget = displayText;
    ui->cmbMode->setCurrentIndex(2);
    ui->edtMsg->setPlaceholderText("正在私聊 " + displayText + "，输入消息...");
    ui->edtMsg->setFocus();
}

void ChatDialog::onAnchorClicked(const QUrl &link)
{
    QString host = link.host();
    QString key = link.path();

    if (key.startsWith("/"))
        key = key.mid(1);

    if (host == "chatroom.download")
    {
        int sep = key.indexOf('|');
        if (sep != -1)
        {
            QString sender = key.left(sep);
            QString filename = key.mid(sep + 1);

            m_downloadSender = sender;
            m_downloadFilename = filename;

            QString fullMsg = "FILE_DOWNLOAD_REQ:" + sender + ":" + filename;
            m_socket->write(fullMsg.toUtf8() + '\0');

            showSystemMsg("请求下载文件 " + filename + "...");
        }
    }
    else if (host == "chatroom.reject")
    {
        int sep = key.indexOf('|');
        if (sep != -1)
        {
            QString sender = key.left(sep);
            QString filename = key.mid(sep + 1);

            QString fullMsg = "FILE_REJECT:" + sender + ":" + filename;
            m_socket->write(fullMsg.toUtf8() + '\0');

            replaceFileNotify(key,
                "<div style='margin:14px 0; font-size:24px; background-color:#f5f5f5; "
                "border-radius:8px; padding:10px 14px;'>"
                "<span style='color:#999; font-weight:bold;'>[" + sender + "]</span> "
                "<span style='color:#999; font-weight:bold;'>📎 发送文件：" + filename.toHtmlEscaped()
                + "</span><br/>"
                "<span style='color:#e74c3c; font-weight:bold; font-size:22px;'>❌ 已拒绝</span>"
                "</div>"
            );
        }
    }
}

void ChatDialog::onFileConnected()
{
    QString authMsg = "AUTH:" + m_username;
    writeLpStr(m_fileSocket, authMsg);
}

void ChatDialog::onFileReadyRead()
{
    m_fileBuffer += m_fileSocket->readAll();

    while (m_fileBuffer.size() >= 4)
    {
        uint32_t msgLen = ((uint32_t)(unsigned char)m_fileBuffer[0] << 24) |
                          ((uint32_t)(unsigned char)m_fileBuffer[1] << 16) |
                          ((uint32_t)(unsigned char)m_fileBuffer[2] << 8) |
                          ((uint32_t)(unsigned char)m_fileBuffer[3]);

        if (msgLen == 0 || msgLen > CHUNK_SIZE + 1024)
        {
            m_fileBuffer.clear();
            break;
        }

        if ((uint32_t)m_fileBuffer.size() < 4 + msgLen)
            break;

        QByteArray payload = m_fileBuffer.mid(4, msgLen);
        m_fileBuffer.remove(0, 4 + msgLen);

        processFilePortMessage(payload);
    }
}

void ChatDialog::processFilePortMessage(const QByteArray &payload)
{
    if (payload.startsWith("AUTH_OK"))
    {
        if (m_fileSocketRole == "sender")
        {
            startChunkUpload();
        }
        else if (m_fileSocketRole == "receiver")
        {
            startChunkDownload(m_downloadFile.fileName());
        }
    }
    else if (payload.startsWith("AUTH_FAIL"))
    {
        showSystemMsg("文件端口认证失败，请重新登录");
        m_fileSocket->disconnectFromHost();
    }
    else if (payload.startsWith("FILE_CHUNK_ACK:"))
    {
        QString ackText = QString::fromUtf8(payload);
        int lastColon = ackText.lastIndexOf(':');
        if (lastColon != -1)
        {
            int chunkIndex = ackText.mid(lastColon + 1).toInt();
            m_uploadedChunkCount = chunkIndex + 1;
            updateTransferProgress(m_uploadedChunkCount, m_totalChunks, "上传");
            sendNextChunk();
        }
    }
    else if (payload.startsWith("FILE_CHUNK_DOWN_RESP:"))
    {
        int headerLen = 0;
        int colonCount = 0;
        for (int i = 0; i < payload.size() && colonCount < 4; i++)
        {
            headerLen = i;
            if (payload[i] == ':') colonCount++;
        }
        headerLen++;

        QString header = QString::fromUtf8(payload.left(headerLen));
        QByteArray chunkData = payload.mid(headerLen);

        QStringList parts = header.split(':');
        if (parts.size() >= 4)
        {
            int chunkIndex = parts[2].toInt();

            m_downloadFile.seek((qint64)chunkIndex * CHUNK_SIZE);
            m_downloadFile.write(chunkData);

            m_downloadedChunkCount = chunkIndex + 1;
            updateTransferProgress(m_downloadedChunkCount, m_downloadTotalChunks, "下载");

            if (chunkIndex + 1 >= m_downloadTotalChunks)
            {
                finishDownload();
            }
            else
            {
                requestNextChunk();
            }
        }
    }
    else if (payload.startsWith("ERROR:"))
    {
        QString errText = QString::fromUtf8(payload);
        showSystemMsg("文件传输错误：" + errText.mid(6));
        m_fileSocket->disconnectFromHost();
        hideTransferProgress();
    }
}

void ChatDialog::onFileError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    showSystemMsg("文件传输连接错误：" + m_fileSocket->errorString());

    if (m_fileSocket)
    {
        m_fileSocket->disconnectFromHost();
    }
    hideTransferProgress();
}

void ChatDialog::startChunkUpload()
{
    if (!m_uploadFile.open(QIODevice::ReadOnly))
    {
        showSystemMsg("无法打开文件进行上传");
        m_fileSocket->disconnectFromHost();
        return;
    }
    m_uploadedChunkCount = 0;
    updateTransferProgress(0, m_totalChunks, "上传");
    sendNextChunk();
}

void ChatDialog::sendNextChunk()
{
    if (m_uploadedChunkCount >= m_totalChunks)
    {
        finishUpload();
        return;
    }

    QByteArray chunkData = m_uploadFile.read(CHUNK_SIZE);

    QString header = QString("FILE_CHUNK_UP:%1:%2:%3:")
                         .arg(m_uploadFilename)
                         .arg(m_uploadedChunkCount)
                         .arg(m_totalChunks);

    QByteArray headerBytes = header.toUtf8();
    QByteArray message = headerBytes + chunkData;

    writeLp(m_fileSocket, message);
}

void ChatDialog::finishUpload()
{
    m_uploadFile.close();

    writeLpStr(m_fileSocket, "FILE_UPLOAD_FINISH:" + m_uploadFilename);

    QString msg = "FILE_UPLOAD_DONE:" + m_uploadFilename;
    m_socket->write(msg.toUtf8() + '\0');

    showSystemMsg("文件 " + m_uploadFilename + " 上传完毕，等待服务器确认...");

    m_fileSocket->disconnectFromHost();
}

void ChatDialog::startChunkDownload(const QString &savePath)
{
    Q_UNUSED(savePath);

    m_downloadedChunkCount = 0;
    m_currentDownloadChunk = 0;
    updateTransferProgress(0, m_downloadTotalChunks, "下载");

    requestNextChunk();
}

void ChatDialog::requestNextChunk()
{
    QString req = QString("FILE_CHUNK_DOWN_REQ:%1:%2:%3")
                      .arg(m_downloadSender).arg(m_downloadFilename).arg(m_downloadedChunkCount);
    writeLpStr(m_fileSocket, req);
}

void ChatDialog::finishDownload()
{
    m_downloadFile.close();

    QString msg = "FILE_DOWNLOAD_ACK:" + m_downloadSender + ":" + m_downloadFilename;
    m_socket->write(msg.toUtf8() + '\0');

    showSystemMsg("文件下载完成：" + m_downloadFilename);
    hideTransferProgress();

    m_fileSocket->disconnectFromHost();
}

void ChatDialog::writeLp(QTcpSocket *sock, const QByteArray &data)
{
    uint32_t len = data.size();
    char prefix[4];
    prefix[0] = (char)((len >> 24) & 0xFF);
    prefix[1] = (char)((len >> 16) & 0xFF);
    prefix[2] = (char)((len >> 8) & 0xFF);
    prefix[3] = (char)(len & 0xFF);
    sock->write(prefix, 4);
    sock->write(data);
}

void ChatDialog::writeLpStr(QTcpSocket *sock, const QString &str)
{
    writeLp(sock, str.toUtf8());
}

void ChatDialog::updateTransferProgress(int current, int total, const QString &direction)
{
    if (!m_transferProgress)
    {
        m_transferProgress = new QProgressBar(this);
        m_transferProgress->setMinimum(0);
        m_transferProgress->setMaximum(100);
        m_transferProgress->setFixedHeight(20);
        m_transferProgress->setStyleSheet(
            "QProgressBar { border: 1px solid #e0e0e0; border-radius: 4px; background: #f0f0f0; }"
            "QProgressBar::chunk { background: #4a90d9; border-radius: 3px; }"
        );

        m_transferStatus = new QLabel(this);
        m_transferStatus->setStyleSheet("color:#4a90d9; font-size:14px;");

        QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(ui->chatArea->layout());
        if (mainLayout)
        {
            QHBoxLayout *progressLayout = new QHBoxLayout();
            progressLayout->addWidget(m_transferProgress, 3);
            progressLayout->addWidget(m_transferStatus, 1);
            mainLayout->insertLayout(mainLayout->count() - 1, progressLayout);
        }
    }

    int percent = (total > 0) ? (current * 100 / total) : 0;
    m_transferProgress->setValue(percent);
    m_transferStatus->setText(QString("%1中... %2% (%3/%4)").arg(direction).arg(percent).arg(current).arg(total));
    m_transferProgress->show();
    m_transferStatus->show();
}

void ChatDialog::hideTransferProgress()
{
    if (m_transferProgress)
    {
        m_transferProgress->hide();
    }
    if (m_transferStatus)
    {
        m_transferStatus->hide();
    }
}

void ChatDialog::replaceFileNotify(const QString &key, const QString &newHtml)
{
    if (!m_fileNotifyBlocks.contains(key))
        return;

    int blockNum = m_fileNotifyBlocks.take(key);
    QTextDocument *doc = ui->edtRecord->document();
    QTextBlock block = doc->findBlockByNumber(blockNum);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertHtml(newHtml);
}

void ChatDialog::showSystemMsg(const QString &msg)
{
    ui->edtRecord->append(
        "<div style='text-align:center; color:#999; font-size:19px; padding:14px;'>"
        "—— " + msg + " ——</div>"
    );
}

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

void ChatDialog::showWhisperMsg(const QString &sender, const QString &content)
{
    ui->edtRecord->append(
        "<div style='margin:14px 0; font-size:24px; background-color:#fff8e1; "
        "border-radius:8px; padding:10px 14px;'>"
        "<span style='color:#e67e22; font-weight:bold;'>[" + sender + "]</span> "
        "<span style='color:#e67e22; font-weight:bold;'>（私聊）</span> "
        "<span style='color:#333333;'>" + content.toHtmlEscaped() + "</span>"
        "</div>"
    );
}

void ChatDialog::showWhisperSent(const QString &target, const QString &content)
{
    ui->edtRecord->append(
        "<div style='margin:14px 0; font-size:24px; background-color:#fff8e1; "
        "border-radius:8px; padding:10px 14px;'>"
        "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
        "<span style='color:#e67e22; font-weight:bold;'>（私聊 → " + target.toHtmlEscaped() + "）</span> "
        "<span style='color:#333333;'>" + content.toHtmlEscaped() + "</span>"
        "</div>"
    );
}

void ChatDialog::onModeChanged(int index)
{
    if (index == 0)
    {
        ui->edtMsg->setPlaceholderText("输入消息，@AI 开头向AI提问...");
        m_whisperTarget.clear();
    }
    else if (index == 1)
    {
        ui->edtMsg->setPlaceholderText("输入问题，直接向AI提问（仅你可见）...");
        m_whisperTarget.clear();
    }
    else if (index == 2)
    {
        if (m_whisperTarget.isEmpty())
        {
            ui->edtMsg->setPlaceholderText("请从右侧在线用户列表中选择私聊对象...");
        }
        else
        {
            ui->edtMsg->setPlaceholderText("正在私聊 " + m_whisperTarget + "，输入消息...");
        }
    }
}

void ChatDialog::sendLoginRequest()
{
    QString msg = "LOGIN:" + m_username + ":" + m_password;
    m_socket->write(msg.toUtf8() + '\0');
}

void ChatDialog::updateUserList(const QString &userListStr)
{
    ui->lstUsers->clear();

    QStringList users = userListStr.split(',', QString::SkipEmptyParts);

    int onlineCount = users.size();
    ui->lblUsers->setText(QString("👥 在线用户 (%1)").arg(onlineCount));

    for (const QString &user : users)
    {
        QString displayText = user;
        if (user == m_username)
        {
            displayText = user + "（我）";
        }

        QListWidgetItem *item = new QListWidgetItem("👤 " + displayText, ui->lstUsers);
        if (user == m_username)
        {
            item->setForeground(QColor("#4a90d9"));
        }
    }
}
