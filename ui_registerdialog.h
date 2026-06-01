/********************************************************************************
** Form generated from reading UI file 'registerdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REGISTERDIALOG_H
#define UI_REGISTERDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_RegisterDialog
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
    QLineEdit *edtConfirm;
    QSpacerItem *spacerBtn;
    QPushButton *btnRegister;
    QSpacerItem *spacerBack;
    QPushButton *btnBack;
    QSpacerItem *spacerStatus;
    QLabel *lblStatus;
    QSpacerItem *spacerBottom;

    void setupUi(QDialog *RegisterDialog)
    {
        if (RegisterDialog->objectName().isEmpty())
            RegisterDialog->setObjectName(QString::fromUtf8("RegisterDialog"));
        RegisterDialog->resize(560, 680);
        RegisterDialog->setMinimumSize(QSize(440, 560));
        RegisterDialog->setStyleSheet(QString::fromUtf8("QDialog {\n"
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
"QPushButton#btnRegister {\n"
"    background-color: #4a90d9;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"    border-radius: 10px;\n"
"    padding: 14px;\n"
"    font-size: 20px;\n"
"    font-weight: bold;\n"
"}\n"
"QPushButton#btnRegister:hover {\n"
"    background-color: #3a7bc8;\n"
"}\n"
"QPushButto"
                        "n#btnRegister:pressed {\n"
"    background-color: #2e6ab5;\n"
"}\n"
"QPushButton#btnRegister:disabled {\n"
"    background-color: #b0c4de;\n"
"    color: #e0e0e0;\n"
"}\n"
"QPushButton#btnBack {\n"
"    background-color: #f0f0f0;\n"
"    color: #666666;\n"
"    border: 2px solid #e0e0e0;\n"
"    border-radius: 10px;\n"
"    padding: 12px;\n"
"    font-size: 18px;\n"
"}\n"
"QPushButton#btnBack:hover {\n"
"    background-color: #e4e4e4;\n"
"    border: 2px solid #cccccc;\n"
"}"));
        verticalLayout = new QVBoxLayout(RegisterDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(50, 60, 50, 50);
        lblTitle = new QLabel(RegisterDialog);
        lblTitle->setObjectName(QString::fromUtf8("lblTitle"));
        lblTitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblTitle);

        spacerTitleSub = new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(spacerTitleSub);

        lblSubtitle = new QLabel(RegisterDialog);
        lblSubtitle->setObjectName(QString::fromUtf8("lblSubtitle"));
        lblSubtitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblSubtitle);

        spacerTop = new QSpacerItem(20, 36, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerTop);

        formLayout = new QVBoxLayout();
        formLayout->setSpacing(18);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        edtUsername = new QLineEdit(RegisterDialog);
        edtUsername->setObjectName(QString::fromUtf8("edtUsername"));
        edtUsername->setMaxLength(20);
        edtUsername->setMinimumSize(QSize(0, 52));

        formLayout->addWidget(edtUsername);

        edtPassword = new QLineEdit(RegisterDialog);
        edtPassword->setObjectName(QString::fromUtf8("edtPassword"));
        edtPassword->setMaxLength(30);
        edtPassword->setEchoMode(QLineEdit::Password);
        edtPassword->setMinimumSize(QSize(0, 52));

        formLayout->addWidget(edtPassword);

        edtConfirm = new QLineEdit(RegisterDialog);
        edtConfirm->setObjectName(QString::fromUtf8("edtConfirm"));
        edtConfirm->setMaxLength(30);
        edtConfirm->setEchoMode(QLineEdit::Password);
        edtConfirm->setMinimumSize(QSize(0, 52));

        formLayout->addWidget(edtConfirm);


        verticalLayout->addLayout(formLayout);

        spacerBtn = new QSpacerItem(20, 24, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerBtn);

        btnRegister = new QPushButton(RegisterDialog);
        btnRegister->setObjectName(QString::fromUtf8("btnRegister"));
        btnRegister->setMinimumSize(QSize(0, 54));
        btnRegister->setCursor(QCursor(Qt::PointingHandCursor));

        verticalLayout->addWidget(btnRegister);

        spacerBack = new QSpacerItem(20, 12, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerBack);

        btnBack = new QPushButton(RegisterDialog);
        btnBack->setObjectName(QString::fromUtf8("btnBack"));
        btnBack->setMinimumSize(QSize(0, 48));
        btnBack->setCursor(QCursor(Qt::PointingHandCursor));

        verticalLayout->addWidget(btnBack);

        spacerStatus = new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerStatus);

        lblStatus = new QLabel(RegisterDialog);
        lblStatus->setObjectName(QString::fromUtf8("lblStatus"));
        lblStatus->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(lblStatus);

        spacerBottom = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(spacerBottom);


        retranslateUi(RegisterDialog);

        QMetaObject::connectSlotsByName(RegisterDialog);
    } // setupUi

    void retranslateUi(QDialog *RegisterDialog)
    {
        RegisterDialog->setWindowTitle(QCoreApplication::translate("RegisterDialog", "\350\201\212\345\244\251\345\256\244 - \346\263\250\345\206\214", nullptr));
        lblTitle->setText(QCoreApplication::translate("RegisterDialog", "\345\210\233\345\273\272\350\264\246\345\217\267", nullptr));
        lblSubtitle->setText(QCoreApplication::translate("RegisterDialog", "\346\263\250\345\206\214\344\270\200\344\270\252\346\226\260\347\232\204\350\201\212\345\244\251\345\256\244\350\264\246\345\217\267", nullptr));
        edtUsername->setPlaceholderText(QCoreApplication::translate("RegisterDialog", "\360\237\221\244  \347\224\250\346\210\267\345\220\215", nullptr));
        edtPassword->setPlaceholderText(QCoreApplication::translate("RegisterDialog", "\360\237\224\222  \345\257\206\347\240\201\357\274\210\350\207\263\345\260\2214\344\275\215\357\274\211", nullptr));
        edtConfirm->setPlaceholderText(QCoreApplication::translate("RegisterDialog", "\360\237\224\222  \347\241\256\350\256\244\345\257\206\347\240\201", nullptr));
        btnRegister->setText(QCoreApplication::translate("RegisterDialog", "\346\263\250 \345\206\214", nullptr));
        btnBack->setText(QCoreApplication::translate("RegisterDialog", "\342\206\220 \350\277\224\345\233\236\347\231\273\345\275\225", nullptr));
        lblStatus->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class RegisterDialog: public Ui_RegisterDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REGISTERDIALOG_H
