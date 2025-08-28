#include "AICompletion.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

AICompletion::AICompletion(QObject *parent) : QObject(parent) {}

void AICompletion::request(const QString &content, int cursorOffset, const QString &language, const QString &filePath) {
    if (m_proc) { m_proc->deleteLater(); m_proc=nullptr; }
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, &AICompletion::onFinished);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &AICompletion::onReadyStdout);
    QJsonObject body{{"content", content}, {"cursor", cursorOffset}, {"language", language}, {"path", filePath}};
    QJsonDocument doc(body);
    QString prog = "python3";
    QString script = QString::fromStdString(std::string(PYTHON_DIR) + "/ide_agent.py");
    m_proc->start(prog, {script});
    m_proc->write(doc.toJson(QJsonDocument::Compact));
    m_proc->closeWriteChannel();
}

void AICompletion::onReadyStdout() {
    m_buf += m_proc->readAllStandardOutput();
}

void AICompletion::onFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode); Q_UNUSED(status);
    auto text = QString::fromUtf8(m_buf).trimmed();
    if (text.isEmpty()) emit failed("empty"); else emit suggestionReady(text);
}

