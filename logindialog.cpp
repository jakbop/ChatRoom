#include "logindialog.h"
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
    ui->btnRegister->setEnabled(false);

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
    sendRequest("LOGIN");
}

void LoginDialog::on_btnRegister_clicked()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    if (username.isEmpty() || password.isEmpty())
    {
        setStatus("用户名和密码不能为空");
        return;
    }

    if (password.length() < 4)
    {
        setStatus("密码长度不能少于4位");
        return;
    }

    m_username = username;
    sendRequest("REG");
}

void LoginDialog::onConnected()
{
    m_connected = true;
    ui->btnLogin->setEnabled(true);
    ui->btnRegister->setEnabled(true);
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
            accept();
            return;
        }
        else if (response.startsWith("LOGIN_FAIL:"))
        {
            setStatus("登录失败：" + response.mid(11));
            ui->btnLogin->setEnabled(true);
            ui->btnRegister->setEnabled(true);
        }
        else if (response == "REG_OK")
        {
            setStatus("注册成功，请登录");
            ui->edtPassword->clear();
            ui->btnLogin->setEnabled(true);
            ui->btnRegister->setEnabled(true);
        }
        else if (response.startsWith("REG_FAIL:"))
        {
            setStatus("注册失败：" + response.mid(9));
            ui->btnLogin->setEnabled(true);
            ui->btnRegister->setEnabled(true);
        }
    }
}

void LoginDialog::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    setStatus("连接失败：" + m_socket->errorString());
    ui->btnLogin->setEnabled(false);
    ui->btnRegister->setEnabled(false);
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

void LoginDialog::sendRequest(const QString &type)
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    QString msg = type + ":" + username + ":" + password;
    m_socket->write(msg.toUtf8() + '\0');

    setStatus(type == "LOGIN" ? "正在登录..." : "正在注册...");

    ui->btnLogin->setEnabled(false);
    ui->btnRegister->setEnabled(false);
}
