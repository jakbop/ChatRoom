#include "chatdialog.h"
#include "ui_chatdialog.h"

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog)
{
    ui->setupUi(this);

    ui->btnSend->setEnabled(false);

    connect(&sock, &QTcpSocket::connected, this, &ChatDialog::onConnected); 
    connect(&sock, &QTcpSocket::readyRead, this, &ChatDialog::onReadyRead);

    sock.connectToHost("39.104.71.92", 9413);

    // 红色字体显示系统消息
    ui->edtRecord->append("<font color='red'>[系统消息] 正在连接服务器...</font><br>");
}

ChatDialog::~ChatDialog()
{
    delete ui;
}


void ChatDialog::on_btnSend_clicked()
{
    QString msg = ui->edtMsg->toPlainText().trimmed();

    if (!msg.isEmpty())
    {
        // 发送消息
        sock.write(msg.toUtf8());

        // 显示自己发送的消息
        ui->edtRecord->append("<font color='blue'>[我] " + msg + "</font><br>");    
    }

    ui->edtMsg->clear();
    ui->edtMsg->setFocus();
}

void ChatDialog::onConnected()
{
    ui->edtRecord->append("<font color='red'>[系统消息] 连接服务器成功！</font><br>");

    ui->btnSend->setEnabled(true);
}

void ChatDialog::onReadyRead()
{
    // 每一条消息都是以 \0 字符结尾的，所以我们可以通过 readAll() 方法一次性读取所有数据，然后按 \0 分割成多条消息
    // 注意：TCP 协议是面向字节流的协议，可能会出现粘包和拆包的情况，可能出现一条消息被分成多次接收，或者多条消息被合并成一次接收的情况，所以我们需要在接收数据时进行处理，确保每条消息都是完整的。
    // 要注意一种特殊情况：一条消息分成多次接收，末尾没有 \0 字符，这时我们需要将这部分数据保存起来，等下一次接收时再与新接收的数据合并起来处理。
    static QByteArray buffer; // 用于保存上一次接收时未处理完的数据

    buffer += sock.readAll(); // 将新接收的数据与上一次未处理完的数据合并起来

    // 将 buffer 中最后一个 \0 字符之前的部分作为完整的消息进行处理，剩余的部分继续保存在 buffer 中等待下一次接收
    int index;
    while ((index = buffer.indexOf('\0')) != -1) // 查找 \0 字符的位置
    {
        QByteArray msg = buffer.left(index); // 获取完整的消息
        buffer = buffer.mid(index + 1); // 将剩余的部分保存在 buffer 中

        // 显示接收到的消息
        ui->edtRecord->append("<font color='green'>[匿名网友] " + QString::fromUtf8(msg) + "</font><br>");
    }
}
