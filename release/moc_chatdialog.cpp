/****************************************************************************
** Meta object code from reading C++ file 'chatdialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/chatdialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'chatdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ChatDialog_t {
    QByteArrayData data[19];
    char stringdata0[274];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ChatDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ChatDialog_t qt_meta_stringdata_ChatDialog = {
    {
QT_MOC_LITERAL(0, 0, 10), // "ChatDialog"
QT_MOC_LITERAL(1, 11, 18), // "on_btnSend_clicked"
QT_MOC_LITERAL(2, 30, 0), // ""
QT_MOC_LITERAL(3, 31, 22), // "on_btnSendFile_clicked"
QT_MOC_LITERAL(4, 54, 11), // "onReadyRead"
QT_MOC_LITERAL(5, 66, 14), // "onDisconnected"
QT_MOC_LITERAL(6, 81, 13), // "onSocketError"
QT_MOC_LITERAL(7, 95, 28), // "QAbstractSocket::SocketError"
QT_MOC_LITERAL(8, 124, 11), // "socketError"
QT_MOC_LITERAL(9, 136, 13), // "onReconnected"
QT_MOC_LITERAL(10, 150, 18), // "onReconnectTimeout"
QT_MOC_LITERAL(11, 169, 17), // "onUserItemClicked"
QT_MOC_LITERAL(12, 187, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(13, 204, 4), // "item"
QT_MOC_LITERAL(14, 209, 15), // "onAnchorClicked"
QT_MOC_LITERAL(15, 225, 4), // "link"
QT_MOC_LITERAL(16, 230, 15), // "onFileReadyRead"
QT_MOC_LITERAL(17, 246, 15), // "onFileConnected"
QT_MOC_LITERAL(18, 262, 11) // "onFileError"

    },
    "ChatDialog\0on_btnSend_clicked\0\0"
    "on_btnSendFile_clicked\0onReadyRead\0"
    "onDisconnected\0onSocketError\0"
    "QAbstractSocket::SocketError\0socketError\0"
    "onReconnected\0onReconnectTimeout\0"
    "onUserItemClicked\0QListWidgetItem*\0"
    "item\0onAnchorClicked\0link\0onFileReadyRead\0"
    "onFileConnected\0onFileError"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ChatDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x08 /* Private */,
       3,    0,   75,    2, 0x08 /* Private */,
       4,    0,   76,    2, 0x08 /* Private */,
       5,    0,   77,    2, 0x08 /* Private */,
       6,    1,   78,    2, 0x08 /* Private */,
       9,    0,   81,    2, 0x08 /* Private */,
      10,    0,   82,    2, 0x08 /* Private */,
      11,    1,   83,    2, 0x08 /* Private */,
      14,    1,   86,    2, 0x08 /* Private */,
      16,    0,   89,    2, 0x08 /* Private */,
      17,    0,   90,    2, 0x08 /* Private */,
      18,    1,   91,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, QMetaType::QUrl,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7,    8,

       0        // eod
};

void ChatDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ChatDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->on_btnSend_clicked(); break;
        case 1: _t->on_btnSendFile_clicked(); break;
        case 2: _t->onReadyRead(); break;
        case 3: _t->onDisconnected(); break;
        case 4: _t->onSocketError((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        case 5: _t->onReconnected(); break;
        case 6: _t->onReconnectTimeout(); break;
        case 7: _t->onUserItemClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 8: _t->onAnchorClicked((*reinterpret_cast< const QUrl(*)>(_a[1]))); break;
        case 9: _t->onFileReadyRead(); break;
        case 10: _t->onFileConnected(); break;
        case 11: _t->onFileError((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractSocket::SocketError >(); break;
            }
            break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ChatDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_ChatDialog.data,
    qt_meta_data_ChatDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ChatDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ChatDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ChatDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int ChatDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
