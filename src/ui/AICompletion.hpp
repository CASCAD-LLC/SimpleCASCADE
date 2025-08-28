#pragma once
#include <QObject>
#include <QProcess>

class AICompletion : public QObject {
    Q_OBJECT
public:
    explicit AICompletion(QObject *parent=nullptr);
    void request(const QString &content,
                 int cursorOffset,
                 const QString &language,
                 const QString &filePath);
signals:
    void suggestionReady(const QString &text);
    void failed(const QString &error);
private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onReadyStdout();
private:
    QProcess *m_proc=nullptr;
    QByteArray m_buf;
};

