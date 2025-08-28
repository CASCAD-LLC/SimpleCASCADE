// src/ui/CodeEditor.cpp
#include "CodeEditor.hpp"
#include <QPainter>
#include <QTextBlock>

Highlighter::Highlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    const QString keywordPatterns[] = {
        "\\bchar\\b", "\\bclass\\b", "\\bconst\\b", "\\bdouble\\b",
        "\\benum\\b", "\\bexplicit\\b", "\\bfriend\\b", "\\binline\\b",
        "\\bint\\b", "\\blong\\b", "\\bnamespace\\b", "\\boperator\\b",
        "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b", "\\bshort\\b",
        "\\bsignals\\b", "\\bsigned\\b", "\\bslots\\b", "\\bstatic\\b",
        "\\bstruct\\b", "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b",
        "\\bunion\\b", "\\bunsigned\\b", "\\bvirtual\\b", "\\bvoid\\b", "\\bvolatile\\b"
    };
    for (const QString &pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    classFormat.setForeground(Qt::darkMagenta);
    classFormat.setFontWeight(QFont::Bold);
    HighlightingRule classRule;
    classRule.pattern = QRegularExpression("\\bQ[A-Za-z]+\\b");
    classRule.format = classFormat;
    highlightingRules.append(classRule);

    commentFormat.setForeground(Qt::darkGreen);
    HighlightingRule singleLineCommentRule;
    singleLineCommentRule.pattern = QRegularExpression("//[^\n]*");
    singleLineCommentRule.format = commentFormat;
    highlightingRules.append(singleLineCommentRule);

    stringFormat.setForeground(Qt::darkYellow);
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\".*\"");
    stringRule.format = stringFormat;
    highlightingRules.append(stringRule);

    numberFormat.setForeground(Qt::darkCyan);
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b\\d+\\.?\\d*\\b");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    commentStart = QRegularExpression("/\\*");
    commentEnd = QRegularExpression("\\*/");
}

void Highlighter::highlightBlock(const QString &text) {
    for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStart);

    while (startIndex >= 0) {
        QRegularExpressionMatch match = commentEnd.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, commentFormat);
        startIndex = text.indexOf(commentStart, startIndex + commentLength);
    }
}

// === Реализация CodeEditor ===

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    lineNumberArea = new QWidget(this);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::updateLineNumberAreaWidth(int) {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    setViewportMargins(space, 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#3c3c3c"));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor("#2d2d2d"));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::lightGray);
            painter.drawText(0, top, lineNumberArea->width() - 5, fontMetrics().height(),
                           Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}