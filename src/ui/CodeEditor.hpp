// src/ui/CodeEditor.hpp
#pragma once
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    Highlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QRegularExpression commentStart;
    QRegularExpression commentEnd;
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &, int);

private:
    QWidget *lineNumberArea = nullptr;
};