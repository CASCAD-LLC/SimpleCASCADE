// src/ui/CodeEditor.hpp
#pragma once
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QWidget>
#include <QPaintEvent>
#include <QSize>

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
    class LineNumberArea : public QWidget {
    public:
        explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), m_editor(editor) {}
        QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }
    protected:
        void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }
    private:
        CodeEditor *m_editor;
    };

    LineNumberArea *lineNumberArea = nullptr;
    Highlighter *m_highlighter = nullptr;
};