/*
 * main.cpp - 应用程序入口
 *
 * 程序流程：
 *   1. 创建 QApplication 实例（Qt 事件循环）
 *   2. 显示登录对话框（LoginDialog）
 *      - 用户输入用户名和密码
 *      - 连接服务器并发送登录请求
 *      - 登录成功：LoginDialog::accept() 返回 QDialog::Accepted
 *      - 登录失败/取消：返回其他值，程序退出
 *   3. 登录成功后，从 LoginDialog 取出 Socket 和用户名
 *   4. 创建聊天对话框（ChatDialog）并显示
 *   5. 进入主事件循环（a.exec()），直到聊天窗口关闭
 *
 * Socket 生命周期：
 *   LoginDialog 创建 Socket → takeSocket() 转移给 ChatDialog →
 *   ChatDialog 析构时断开连接并释放
 */

#include "logindialog.h"
#include "chatdialog.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    /* 创建 Qt 应用程序实例，管理 GUI 事件循环和资源 */
    QApplication a(argc, argv);

    /* 第一步：显示登录对话框（模态方式） */
    LoginDialog login;

    /*
     * exec() 以模态方式运行登录对话框：
     *   - 阻塞直到对话框关闭（accept 或 reject）
     *   - 返回 QDialog::Accepted 表示登录成功
     *   - 返回 QDialog::Rejected 表示用户取消或关闭窗口
     */
    if (login.exec() == QDialog::Accepted)
    {
        /*
         * 登录成功：
         *   - takeSocket() 取出 Socket（所有权从 LoginDialog 转移到 ChatDialog）
         *   - getUsername() 获取登录的用户名
         */
        ChatDialog chat(login.takeSocket(), login.getUsername(), login.getPassword(),
                        login.takePendingData());

        /* 显示聊天窗口 */
        chat.show();

        /*
         * 进入 Qt 主事件循环：
         *   处理用户输入、网络事件、定时器等
         *   直到 chat.show() 的窗口被关闭才返回
         */
        return a.exec();
    }

    /* 用户取消登录，程序退出 */
    return 0;
}
