#include "CodePanel.hpp"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>
#include <QHeaderView>

CodePanel::CodePanel(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    m_toolbar = new QToolBar(this);
    layout->addWidget(m_toolbar);

    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("Фильтр файлов (имя)...");
    m_toolbar->addWidget(m_filterEdit);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &CodePanel::onSearchTextChanged);

    auto newAct = m_toolbar->addAction("Новый файл");
    auto openAct = m_toolbar->addAction("Открыть файл");
    auto saveAct = m_toolbar->addAction("Сохранить");
    auto saveAsAct = m_toolbar->addAction("Сохранить как");
    auto findAct = m_toolbar->addAction("Найти в файле");
    auto aiAct = m_toolbar->addAction("ИИ-подсказка");
    connect(newAct, &QAction::triggered, this, &CodePanel::onNewFile);
    connect(openAct, &QAction::triggered, this, &CodePanel::onOpenFile);
    connect(saveAct, &QAction::triggered, this, &CodePanel::onSaveFile);
    connect(saveAsAct, &QAction::triggered, this, &CodePanel::onSaveAsFile);
    connect(aiAct, &QAction::triggered, this, &CodePanel::onAskAI);
    connect(findAct, &QAction::triggered, this, &CodePanel::onFindInFile);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setRootPath(QDir::currentPath());
    m_fsModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    m_filterModel = new FilenameFilterModel(this);
    m_filterModel->setSourceModel(m_fsModel);

    m_tree = new QTreeView();
    m_tree->setModel(m_filterModel);
    m_tree->setRootIndex(m_filterModel->mapFromSource(m_fsModel->index(QDir::currentPath())));
    m_tree->setColumnWidth(0, 260);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(m_tree);
    connect(m_tree, &QTreeView::doubleClicked, this, &CodePanel::onTreeDoubleClicked);

    m_editor = new CodeEditor();
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
}

void CodePanel::onSearchTextChanged(const QString &text) {
    m_filterModel->setFilterString(text);
}

void CodePanel::onTreeDoubleClicked(const QModelIndex &index) {
    QModelIndex src = m_filterModel->mapToSource(index);
    QString path = m_fsModel->filePath(src);
    if (QFileInfo(path).isFile()) loadFile(path);
}

void CodePanel::onOpenSelected() {
    auto idx = m_tree->currentIndex();
    if (!idx.isValid()) return;
    onTreeDoubleClicked(idx);
}

void CodePanel::onNewFile() {
    QString path = QFileDialog::getSaveFileName(this, "Новый файл", QDir::currentPath());
    if (path.isEmpty()) return;
    QFile f(path); if (!f.open(QIODevice::WriteOnly)) return; f.close();
    loadFile(path);
}

void CodePanel::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(this, "Открыть файл", QDir::currentPath());
    if (path.isEmpty()) return;
    loadFile(path);
}

void CodePanel::onSaveFile() {
    if (m_currentPath.isEmpty()) { onSaveAsFile(); return; }
    saveToPath(m_currentPath);
}

void CodePanel::onSaveAsFile() {
    QString path = QFileDialog::getSaveFileName(this, "Сохранить как", m_currentPath.isEmpty()?QDir::currentPath():m_currentPath);
    if (path.isEmpty()) return;
    if (saveToPath(path)) loadFile(path);
}

void CodePanel::onFindInFile() {
    bool ok=false;
    QString q = QInputDialog::getText(this, "Найти", "Текст для поиска:", QLineEdit::Normal, "", &ok);
    if (!ok || q.isEmpty()) return;
    QTextDocument::FindFlags flags;
    m_editor->find(q, flags);
}

void CodePanel::onAskAI() {
    // Open ai_agent.py as a helper window
    QProcess::startDetached("python3", {QString::fromStdString(std::string(PYTHON_DIR) + "/ai_agent.py")});
}

void CodePanel::loadFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл"); return; }
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    f.close();
    m_currentPath = path;
}

bool CodePanel::saveToPath(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл"); return false; }
    auto data = m_editor->toPlainText().toUtf8();
    f.write(data); f.close();
    return true;
}

