#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class ChatDialog; }
QT_END_NAMESPACE

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    ChatDialog(QTcpSocket *socket, const QString &username, QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    void on_btnSend_clicked();
    void onReadyRead();

private:
    Ui::ChatDialog *ui;
    QTcpSocket *m_socket;
    QString m_username;
    QByteArray m_buffer;
};
#endif // CHATDIALOG_H
