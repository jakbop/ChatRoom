#include "chatdialog.h"
#include "ui_chatdialog.h"

ChatDialog::ChatDialog(QTcpSocket *socket, const QString &username, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog)
    , m_socket(socket)
    , m_username(username)
{
    ui->setupUi(this);

    setWindowTitle("聊天室 - " + m_username);
    ui->lblHeader->setText("💬 聊天室 - " + m_username);

    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &ChatDialog::onReadyRead);

    ui->edtRecord->append(
        "<div style='text-align:center; color:#999; font-size:15px; padding:10px;'>"
        "—— 欢迎进入聊天室 ——</div>"
    );
}

ChatDialog::~ChatDialog()
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
    }
    delete ui;
}

void ChatDialog::on_btnSend_clicked()
{
    QString msg = ui->edtMsg->toPlainText().trimmed();

    if (!msg.isEmpty())
    {
        QString fullMsg = "CHAT:" + msg;
        m_socket->write(fullMsg.toUtf8() + '\0');

        ui->edtRecord->append(
            "<div style='margin:6px 0; font-size:17px;'>"
            "<span style='color:#4a90d9; font-weight:bold;'>[" + m_username + "]</span> "
            "<span style='color:#333333;'>" + msg.toHtmlEscaped() + "</span>"
            "</div>"
        );
    }

    ui->edtMsg->clear();
    ui->edtMsg->setFocus();
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
                    "<div style='margin:6px 0; font-size:17px;'>"
                    "<span style='color:#27ae60; font-weight:bold;'>[" + sender + "]</span> "
                    "<span style='color:#333333;'>" + message.toHtmlEscaped() + "</span>"
                    "</div>"
                );
            }
        }
        else if (text.startsWith("SYSTEM:"))
        {
            ui->edtRecord->append(
                "<div style='text-align:center; color:#999; font-size:15px; padding:10px;'>"
                "—— " + text.mid(7) + " ——</div>"
            );
        }
        else
        {
            ui->edtRecord->append(
                "<div style='margin:6px 0; font-size:17px; color:#333333;'>" + text.toHtmlEscaped() + "</div>"
            );
        }
    }
}
