#pragma once
#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QToolBar>
#include <QSplitter>
#include <QLabel>
#include <QAction>
#include <QProcess>
#include "CodeEditor.hpp"

class FilenameFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FilenameFilterModel(QObject *parent=nullptr) : QSortFilterProxyModel(parent) {}
    void setFilterString(const QString &s) { m_filter = s; invalidateFilter(); }
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        if (m_filter.trimmed().isEmpty()) return true;
        QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
        if (!idx.isValid()) return true;
        QString name = sourceModel()->data(idx, QFileSystemModel::FileNameRole).toString();
        if (name.contains(m_filter, Qt::CaseInsensitive)) return true;
        // Accept parents if any child matches
        if (sourceModel()->hasChildren(idx)) {
            int rows = sourceModel()->rowCount(idx);
            for (int r=0;r<rows;++r) if (filterAcceptsRow(r, idx)) return true;
        }
        return false;
    }
private:
    QString m_filter;
};

class CodePanel : public QWidget {
    Q_OBJECT
public:
    explicit CodePanel(QWidget *parent=nullptr);
private slots:
    void onOpenSelected();
    void onTreeDoubleClicked(const QModelIndex &index);
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onSaveAsFile();
    void onSearchTextChanged(const QString &text);
    void onAskAI();
    void onFindInFile();
private:
    void loadFile(const QString &path);
    bool saveToPath(const QString &path);
private:
    QToolBar *m_toolbar=nullptr;
    QLineEdit *m_filterEdit=nullptr;
    QTreeView *m_tree=nullptr;
    QFileSystemModel *m_fsModel=nullptr;
    FilenameFilterModel *m_filterModel=nullptr;
    CodeEditor *m_editor=nullptr;
    QString m_currentPath;
};

