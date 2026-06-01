/********************************************************************************
** Form generated from reading UI file 'logindialog.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINDIALOG_H
#define UI_LOGINDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_LoginDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *lblTitle;
    QSpacerItem *spacerTitleSub;
    QLabel *lblSubtitle;
    QSpacerItem *spacerTop;
    QVBoxLayout *formLayout;
    QLineEdit *edtUsername;
    QLineEdit *edtPassword;
    QSpacerItem *spacerBtn;
    QPushButton *btnLogin;
    QSpacerItem *spacerReg;
    QPushButton *btnGoRegister;
    QSpacerItem *spacerStatus;
    QLabel *lblStatus;
    QSpacerItem *spacerBottom;

    void setupUi(QDialog *LoginDialog)
    {
        if (LoginDialog->objectName().isEmpty())
            LoginDialog->setObjectName(QString::fromUtf8("LoginDialog"));
        LoginDialog->resize(560, 620);
        LoginDialog->setMinimumSize(QSize(440, 500));
        LoginDialog->setStyleSheet(QString::fromUtf8("QDialog {\n"
"    background-color: #ffffff;\n"
"}\n"
"QLabel#lblTitle {\n"
"    color: #1a1a2e;\n"
"    font-size: 36px;\n"
"    font-weight: bold;\n"
"}\n"
"QLabel#lblSubtitle {\n"
"    color: #888888;\n"
"    font-size: 18px;\n"
"}\n"
"QLabel#lblStatus {\n"
"    color: #e74c3c;\n"
"    font-size: 16px;\n"
"}\n"
"QLineEdit {\n"
"    border: 2px solid #e0e0e0;\n"
"    border-radius: 10px;\n"
"    padding: 12px 18px;\n"
"    font-size: 19px;\n"
"    background-color: #f8f9fa;\n"
"    color: #333333;\n"
"    selection-background-color: #4a90d9;\n"
"}\n"
"QLineEdit:focus {\n"
"    border: 2px solid #4a90d9;\n"
"    background-color: #ffffff;\n"
"}\n"
"QLineEdit::placeholder {\n"
"    color: #aaaaaa;\n"
"}\n"
"QPushButton#btnLogin {\n"
"    background-color: #4a90d9;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"    border-radius: 10px;\n"
"    padding: 14px;\n"
"    font-size: 20px;\n"
"    font-weight: bold;\n"
"}\n"
"QPushButton#btnLogin:hover {\n"
"    background-color: #3a7bc8;\n"
"}\n"
"QPushButton#btnL"
                        "ogin:pressed {\n"
"    background-color: #2e6ab5;\n"
"}\n"
"QPushButton#btnLogin:disabled {\n"
"    background-color: #b0c4de;\n"
"    color: #e0e0e0;\n"
"}\n"
"QPushButton#btnGoRegister {\n"
"    border: none;\n"
"    color: #4a90d9;\n"
"    font-size: 17px;\n"
"    background-color: transparent;\n"
"}\n"
"QPushButton#btnGoRegister:hover {\n"
"    color: #3a7bc8;\n"
"    text-decoration: underline;\n"
"}"));
        verticalLayout = new QVBoxLayout(LoginDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(50, 60, 50, 50);
        lblTitle = new QLabel(LoginDialog);
        lblTitle->setObjectName(QString::fromUtf8("lblTitle"));
        lblTitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblTitle);

        spacerTitleSub = new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(spacerTitleSub);

        lblSubtitle = new QLabel(LoginDialog);
        lblSubtitle->setObjectName(QString::fromUtf8("lblSubtitle"));
        lblSubtitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblSubtitle);

        spacerTop = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerTop);

        formLayout = new QVBoxLayout();
        formLayout->setSpacing(20);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        edtUsername = new QLineEdit(LoginDialog);
        edtUsername->setObjectName(QString::fromUtf8("edtUsername"));
        edtUsername->setMaxLength(20);
        edtUsername->setMinimumSize(QSize(0, 52));

        formLayout->addWidget(edtUsername);

        edtPassword = new QLineEdit(LoginDialog);
        edtPassword->setObjectName(QString::fromUtf8("edtPassword"));
        edtPassword->setMaxLength(30);
        edtPassword->setEchoMode(QLineEdit::Password);
        edtPassword->setMinimumSize(QSize(0, 52));

        formLayout->addWidget(edtPassword);


        verticalLayout->addLayout(formLayout);

        spacerBtn = new QSpacerItem(20, 24, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerBtn);

        btnLogin = new QPushButton(LoginDialog);
        btnLogin->setObjectName(QString::fromUtf8("btnLogin"));
        btnLogin->setMinimumSize(QSize(0, 54));
        btnLogin->setCursor(QCursor(Qt::PointingHandCursor));

        verticalLayout->addWidget(btnLogin);

        spacerReg = new QSpacerItem(20, 18, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerReg);

        btnGoRegister = new QPushButton(LoginDialog);
        btnGoRegister->setObjectName(QString::fromUtf8("btnGoRegister"));
        btnGoRegister->setMinimumSize(QSize(0, 30));
        btnGoRegister->setCursor(QCursor(Qt::PointingHandCursor));
        btnGoRegister->setFlat(true);

        verticalLayout->addWidget(btnGoRegister);

        spacerStatus = new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerStatus);

        lblStatus = new QLabel(LoginDialog);
        lblStatus->setObjectName(QString::fromUtf8("lblStatus"));
        lblStatus->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblStatus);

        spacerBottom = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerBottom);


        retranslateUi(LoginDialog);

        QMetaObject::connectSlotsByName(LoginDialog);
    } // setupUi

    void retranslateUi(QDialog *LoginDialog)
    {
        LoginDialog->setWindowTitle(QCoreApplication::translate("LoginDialog", "\350\201\212\345\244\251\345\256\244 - \347\231\273\345\275\225", nullptr));
        lblTitle->setText(QCoreApplication::translate("LoginDialog", "\346\254\242\350\277\216\345\233\236\346\235\245", nullptr));
        lblSubtitle->setText(QCoreApplication::translate("LoginDialog", "\347\231\273\345\275\225\344\275\240\347\232\204\350\201\212\345\244\251\345\256\244\350\264\246\345\217\267", nullptr));
        edtUsername->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\360\237\221\244  \347\224\250\346\210\267\345\220\215", nullptr));
        edtPassword->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\360\237\224\222  \345\257\206\347\240\201", nullptr));
        btnLogin->setText(QCoreApplication::translate("LoginDialog", "\347\231\273 \345\275\225", nullptr));
        btnGoRegister->setText(QCoreApplication::translate("LoginDialog", "\350\277\230\346\262\241\346\234\211\350\264\246\345\217\267\357\274\237\347\253\213\345\215\263\346\263\250\345\206\214", nullptr));
        lblStatus->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class LoginDialog: public Ui_LoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINDIALOG_H
