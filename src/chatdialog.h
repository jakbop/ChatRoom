#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMap>
#include <QTextBrowser>
#include <QFile>
#include <QProgressBar>
#include <QLabel>
#include <QCryptographicHash>

#define CHUNK_SIZE (64 * 1024)
#define FILE_PORT 9414

QT_BEGIN_NAMESPACE
namespace Ui { class ChatDialog; }
QT_END_NAMESPACE

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    ChatDialog(QTcpSocket *socket, const QString &username, const QString &password,
               const QByteArray &pendingData = QByteArray(), QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    void on_btnSend_clicked();
    void on_btnSendFile_clicked();
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onReconnected();
    void onReconnectTimeout();
    void onUserItemClicked(QListWidgetItem *item);
    void onAnchorClicked(const QUrl &link);
    void onFileReadyRead();
    void onFileConnected();
    void onFileError(QAbstractSocket::SocketError socketError);

private:
    void showSystemMsg(const QString &msg);
    void showAiMsg(const QString &aiName, const QString &content);
    void showWhisperMsg(const QString &sender, const QString &content);
    void showWhisperSent(const QString &target, const QString &content);
    void onModeChanged(int index);
    void sendLoginRequest();
    void updateUserList(const QString &userListStr);

    void startChunkUpload();
    void sendNextChunk();
    void finishUpload();
    void startChunkDownload(const QString &savePath);
    void requestNextChunk();
    void finishDownload();
    void processFilePortMessage(const QByteArray &payload);
    void writeLp(QTcpSocket *sock, const QByteArray &data);
    void writeLpStr(QTcpSocket *sock, const QString &str);
    void updateTransferProgress(int current, int total, const QString &direction);
    void hideTransferProgress();
    void replaceFileNotify(const QString &key, const QString &newHtml);

    Ui::ChatDialog *ui;
    QTcpSocket *m_socket;
    QString m_username;
    QString m_password;
    QByteArray m_buffer;
    bool m_connected;
    QTimer *m_reconnectTimer;
    int m_reconnectCount;
    bool m_reconnecting;
    QString m_whisperTarget;

    QTcpSocket *m_fileSocket;
    QByteArray m_fileBuffer;
    QString m_fileSocketRole;
    QString m_fileSocketTarget;
    QString m_fileSocketFilename;

    QFile m_uploadFile;
    QString m_uploadFilename;
    int m_totalChunks;
    qint64 m_uploadFileSize;
    QByteArray m_fileMd5;
    int m_uploadedChunkCount;

    QFile m_downloadFile;
    QString m_downloadFilename;
    QString m_downloadSender;
    qint64 m_downloadFileSize;
    int m_downloadTotalChunks;
    int m_downloadedChunkCount;
    int m_currentDownloadChunk;

    QProgressBar *m_transferProgress;
    QLabel *m_transferStatus;

    QMap<QString, int> m_fileNotifyBlocks;
};
#endif
