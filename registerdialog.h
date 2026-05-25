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
    explicit RegisterDialog(QTcpSocket *socket, QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void on_btnRegister_clicked();
    void on_btnBack_clicked();
    void onReadyRead();

private:
    void setStatus(const QString &text);
    void sendRegisterRequest();

    Ui::RegisterDialog *ui;
    QTcpSocket *m_socket;
    QByteArray m_buffer;
};

#endif
