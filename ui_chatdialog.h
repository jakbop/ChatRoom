/********************************************************************************
** Form generated from reading UI file 'chatdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CHATDIALOG_H
#define UI_CHATDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ChatDialog
{
public:
    QHBoxLayout *outerLayout;
    QWidget *chatArea;
    QVBoxLayout *chatLayout;
    QLabel *lblHeader;
    QTextBrowser *edtRecord;
    QHBoxLayout *inputLayout;
    QComboBox *cmbMode;
    QPlainTextEdit *edtMsg;
    QPushButton *btnSendFile;
    QPushButton *btnSend;
    QWidget *userArea;
    QVBoxLayout *userLayout;
    QLabel *lblUsers;
    QListWidget *lstUsers;

    void setupUi(QDialog *ChatDialog)
    {
        if (ChatDialog->objectName().isEmpty())
            ChatDialog->setObjectName(QString::fromUtf8("ChatDialog"));
        ChatDialog->resize(1100, 800);
        ChatDialog->setMinimumSize(QSize(800, 600));
        ChatDialog->setStyleSheet(QString::fromUtf8("QDialog {\n"
"    background-color: #f5f6fa;\n"
"}\n"
"QLabel#lblHeader {\n"
"    color: #2c3e50;\n"
"    font-size: 26px;\n"
"    font-weight: bold;\n"
"    padding: 14px 20px;\n"
"    background-color: #ffffff;\n"
"    border-bottom: 1px solid #e0e0e0;\n"
"}\n"
"QTextEdit#edtRecord {\n"
"    border: none;\n"
"    background-color: #ffffff;\n"
"    font-size: 24px;\n"
"    padding: 14px 20px;\n"
"}\n"
"QPlainTextEdit#edtMsg {\n"
"    border: 2px solid #e0e0e0;\n"
"    border-radius: 10px;\n"
"    background-color: #ffffff;\n"
"    font-size: 24px;\n"
"    padding: 10px 14px;\n"
"    selection-background-color: #4a90d9;\n"
"}\n"
"QPlainTextEdit#edtMsg:focus {\n"
"    border: 2px solid #4a90d9;\n"
"}\n"
"QPushButton#btnSend {\n"
"    background-color: #4a90d9;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"    border-radius: 10px;\n"
"    font-size: 22px;\n"
"    font-weight: bold;\n"
"    padding: 10px 24px;\n"
"}\n"
"QPushButton#btnSend:hover {\n"
"    background-color: #3a7bc8;\n"
"}\n"
"QPushButton#btnSe"
                        "nd:pressed {\n"
"    background-color: #2e6ab5;\n"
"}\n"
"QPushButton#btnSendFile {\n"
"    background-color: #27ae60;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"    border-radius: 10px;\n"
"    font-size: 18px;\n"
"    font-weight: bold;\n"
"    padding: 10px 18px;\n"
"}\n"
"QPushButton#btnSendFile:hover {\n"
"    background-color: #219a52;\n"
"}\n"
"QPushButton#btnSendFile:pressed {\n"
"    background-color: #1e8449;\n"
"}\n"
"QComboBox#cmbMode {\n"
"    border: 2px solid #e0e0e0;\n"
"    border-radius: 10px;\n"
"    background-color: #ffffff;\n"
"    font-size: 18px;\n"
"    padding: 8px 14px;\n"
"    min-width: 140px;\n"
"}\n"
"QComboBox#cmbMode::drop-down {\n"
"    border: none;\n"
"    width: 30px;\n"
"}\n"
"QComboBox#cmbMode QAbstractItemView {\n"
"    border: 2px solid #e0e0e0;\n"
"    border-radius: 6px;\n"
"    background-color: #ffffff;\n"
"    font-size: 18px;\n"
"    padding: 4px;\n"
"    selection-background-color: #4a90d9;\n"
"    selection-color: #ffffff;\n"
"}\n"
"QLabel#lblUsers {\n"
""
                        "    color: #2c3e50;\n"
"    font-size: 18px;\n"
"    font-weight: bold;\n"
"    padding: 10px 14px;\n"
"    background-color: #ffffff;\n"
"    border-bottom: 1px solid #e0e0e0;\n"
"}\n"
"QListWidget#lstUsers {\n"
"    border: none;\n"
"    background-color: #ffffff;\n"
"    font-size: 18px;\n"
"    padding: 8px;\n"
"    outline: none;\n"
"}\n"
"QListWidget#lstUsers::item {\n"
"    padding: 10px 14px;\n"
"    border-bottom: 1px solid #f0f0f0;\n"
"    border-radius: 6px;\n"
"}\n"
"QListWidget#lstUsers::item:hover {\n"
"    background-color: #eaf4ff;\n"
"}\n"
"QListWidget#lstUsers::item:selected {\n"
"    background-color: #4a90d9;\n"
"    color: #ffffff;\n"
"}"));
        outerLayout = new QHBoxLayout(ChatDialog);
        outerLayout->setSpacing(0);
        outerLayout->setObjectName(QString::fromUtf8("outerLayout"));
        outerLayout->setContentsMargins(0, 0, 0, 0);
        chatArea = new QWidget(ChatDialog);
        chatArea->setObjectName(QString::fromUtf8("chatArea"));
        chatLayout = new QVBoxLayout(chatArea);
        chatLayout->setSpacing(0);
        chatLayout->setObjectName(QString::fromUtf8("chatLayout"));
        chatLayout->setContentsMargins(0, 0, 0, 0);
        lblHeader = new QLabel(chatArea);
        lblHeader->setObjectName(QString::fromUtf8("lblHeader"));

        chatLayout->addWidget(lblHeader);

        edtRecord = new QTextBrowser(chatArea);
        edtRecord->setObjectName(QString::fromUtf8("edtRecord"));
        edtRecord->setReadOnly(true);

        chatLayout->addWidget(edtRecord);

        inputLayout = new QHBoxLayout();
        inputLayout->setSpacing(12);
        inputLayout->setObjectName(QString::fromUtf8("inputLayout"));
        inputLayout->setContentsMargins(14, 12, 14, 14);
        cmbMode = new QComboBox(chatArea);
        cmbMode->addItem(QString());
        cmbMode->addItem(QString());
        cmbMode->addItem(QString());
        cmbMode->setObjectName(QString::fromUtf8("cmbMode"));
        cmbMode->setCursor(QCursor(Qt::PointingHandCursor));

        inputLayout->addWidget(cmbMode);

        edtMsg = new QPlainTextEdit(chatArea);
        edtMsg->setObjectName(QString::fromUtf8("edtMsg"));
        edtMsg->setMaximumSize(QSize(16777215, 90));

        inputLayout->addWidget(edtMsg);

        btnSendFile = new QPushButton(chatArea);
        btnSendFile->setObjectName(QString::fromUtf8("btnSendFile"));
        btnSendFile->setMinimumSize(QSize(80, 65));
        btnSendFile->setMaximumSize(QSize(100, 90));
        btnSendFile->setCursor(QCursor(Qt::PointingHandCursor));

        inputLayout->addWidget(btnSendFile);

        btnSend = new QPushButton(chatArea);
        btnSend->setObjectName(QString::fromUtf8("btnSend"));
        btnSend->setMinimumSize(QSize(110, 65));
        btnSend->setMaximumSize(QSize(130, 90));
        btnSend->setCursor(QCursor(Qt::PointingHandCursor));

        inputLayout->addWidget(btnSend);


        chatLayout->addLayout(inputLayout);


        outerLayout->addWidget(chatArea);

        userArea = new QWidget(ChatDialog);
        userArea->setObjectName(QString::fromUtf8("userArea"));
        userArea->setMinimumSize(QSize(180, 0));
        userArea->setMaximumSize(QSize(220, 16777215));
        userLayout = new QVBoxLayout(userArea);
        userLayout->setSpacing(0);
        userLayout->setObjectName(QString::fromUtf8("userLayout"));
        userLayout->setContentsMargins(0, 0, 0, 0);
        lblUsers = new QLabel(userArea);
        lblUsers->setObjectName(QString::fromUtf8("lblUsers"));

        userLayout->addWidget(lblUsers);

        lstUsers = new QListWidget(userArea);
        lstUsers->setObjectName(QString::fromUtf8("lstUsers"));
        lstUsers->setCursor(QCursor(Qt::PointingHandCursor));

        userLayout->addWidget(lstUsers);


        outerLayout->addWidget(userArea);


        retranslateUi(ChatDialog);

        QMetaObject::connectSlotsByName(ChatDialog);
    } // setupUi

    void retranslateUi(QDialog *ChatDialog)
    {
        ChatDialog->setWindowTitle(QCoreApplication::translate("ChatDialog", "\350\201\212\345\244\251\345\256\244", nullptr));
        lblHeader->setText(QCoreApplication::translate("ChatDialog", "\360\237\222\254 \350\201\212\345\244\251\345\256\244", nullptr));
        cmbMode->setItemText(0, QCoreApplication::translate("ChatDialog", "\360\237\222\254 \347\276\244\350\201\212", nullptr));
        cmbMode->setItemText(1, QCoreApplication::translate("ChatDialog", "\360\237\244\226 \347\247\201\350\201\212AI", nullptr));
        cmbMode->setItemText(2, QCoreApplication::translate("ChatDialog", "\360\237\221\244 \347\247\201\350\201\212", nullptr));

        btnSendFile->setText(QCoreApplication::translate("ChatDialog", "\360\237\223\216 \346\226\207\344\273\266", nullptr));
        btnSend->setText(QCoreApplication::translate("ChatDialog", "\345\217\221 \351\200\201", nullptr));
        lblUsers->setText(QCoreApplication::translate("ChatDialog", "\360\237\221\245 \345\234\250\347\272\277\347\224\250\346\210\267", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ChatDialog: public Ui_ChatDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CHATDIALOG_H
