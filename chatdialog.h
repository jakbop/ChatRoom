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
    ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    void on_btnSend_clicked();
    void onConnected();
    void onReadyRead();

private:
    Ui::ChatDialog *ui;
    QTcpSocket sock;
};
#endif // CHATDIALOG_H
