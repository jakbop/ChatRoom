#include "registerdialog.h"
#include "ui_registerdialog.h"

RegisterDialog::RegisterDialog(QTcpSocket *socket, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , m_socket(socket)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(m_socket, &QTcpSocket::readyRead, this, &RegisterDialog::onReadyRead);
}

RegisterDialog::~RegisterDialog()
{
    disconnect(m_socket, &QTcpSocket::readyRead, this, &RegisterDialog::onReadyRead);
    delete ui;
}

void RegisterDialog::on_btnRegister_clicked()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();
    QString confirm = ui->edtConfirm->text();

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

    if (password != confirm)
    {
        setStatus("两次输入的密码不一致");
        return;
    }

    sendRegisterRequest();
}

void RegisterDialog::on_btnBack_clicked()
{
    reject();
}

void RegisterDialog::onReadyRead()
{
    m_buffer += m_socket->readAll();

    int index;
    while ((index = m_buffer.indexOf('\0')) != -1)
    {
        QByteArray msg = m_buffer.left(index);
        m_buffer = m_buffer.mid(index + 1);

        QString response = QString::fromUtf8(msg);

        if (response == "REG_OK")
        {
            setStatus("注册成功！请返回登录");
            ui->edtPassword->clear();
            ui->edtConfirm->clear();
            ui->btnRegister->setEnabled(true);
        }
        else if (response.startsWith("REG_FAIL:"))
        {
            setStatus("注册失败：" + response.mid(9));
            ui->btnRegister->setEnabled(true);
        }
    }
}

void RegisterDialog::setStatus(const QString &text)
{
    ui->lblStatus->setText(text);
}

void RegisterDialog::sendRegisterRequest()
{
    QString username = ui->edtUsername->text().trimmed();
    QString password = ui->edtPassword->text();

    QString msg = "REG:" + username + ":" + password;
    m_socket->write(msg.toUtf8() + '\0');

    setStatus("正在注册...");
    ui->btnRegister->setEnabled(false);
}
