#include "logindialog.h"
#include "chatdialog.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LoginDialog login;
    if (login.exec() == QDialog::Accepted)
    {
        ChatDialog chat(login.takeSocket(), login.getUsername());
        chat.show();
        return a.exec();
    }

    return 0;
}
