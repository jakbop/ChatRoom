#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    QTcpSocket* takeSocket();
    QString getUsername() const;

private slots:
    void on_btnLogin_clicked();
    void on_btnRegister_clicked();
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    void setStatus(const QString &text);
    void connectToServer();
    void sendRequest(const QString &type);

    Ui::LoginDialog *ui;
    QTcpSocket *m_socket;
    QString m_username;
    QByteArray m_buffer;
    bool m_connected;
};

#endif
