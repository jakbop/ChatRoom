#include "logindialog.h"
#include "registerdialog.h"
#include "ui_logindialog.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_socket(new QTcpSocket(this))
    , m_connected(false)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);

    connect(m_socket, &QTcpSocket::connected, this, &LoginDialog::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &LoginDialog::onError);

    connectToServer();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QTcpSocket* LoginDialog::takeSocket()
{
    QTcpSocket *sock = m_socket;
    m_socket = nullptr;
    disconnect(sock, nullptr, this, nullptr);
    return sock;
}

QString LoginDialog::getUsername() const
{
    return m_username;
}

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

void LoginDialog::on_btnGoRegister_clicked()
{
    disconnect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);

    RegisterDialog regDlg(m_socket, this);
    regDlg.exec();

    connect(m_socket, &QTcpSocket::readyRead, this, &LoginDialog::onReadyRead);

    ui->edtUsername->setFocus();
}

void LoginDialog::onConnected()
{
    m_connected = true;
    ui->btnLogin->setEnabled(true);
    ui->btnGoRegister->setEnabled(true);
    setStatus("");
}

void LoginDialog::onReadyRead()
{
    m_buffer += m_socket->readAll();

    int index;
    while ((index = m_buffer.indexOf('\0')) != -1)
    {
        QByteArray msg = m_buffer.left(index);
        m_buffer = m_buffer.mid(index + 1);

        QString response = QString::fromUtf8(msg);

        if (response == "LOGIN_OK")
        {
            disconnect(m_socket, nullptr, this, nullptr);
            accept();
            return;
        }
        else if (response.startsWith("LOGIN_FAIL:"))
        {
            setStatus("登录失败：" + response.mid(11));
            ui->btnLogin->setEnabled(true);
            ui->btnGoRegister->setEnabled(true);
        }
    }
}

void LoginDialog::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (!m_socket) return;
    setStatus("连接失败：" + m_socket->errorString());
    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);
}

void LoginDialog::setStatus(const QString &text)
{
    ui->lblStatus->setText(text);
}

void LoginDialog::connectToServer()
{
    setStatus("正在连接服务器...");
    m_socket->connectToHost("39.104.71.92", 9413);
}

void LoginDialog::sendLoginRequest()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    QString msg = "LOGIN:" + username + ":" + password;
    m_socket->write(msg.toUtf8() + '\0');

    setStatus("正在登录...");

    ui->btnLogin->setEnabled(false);
    ui->btnGoRegister->setEnabled(false);
}
