#include "src/ui/MainWindow.hpp"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    // Установка стиля на всё приложение
    qApp->setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #1e1e2e;
            color: #cdd6f4;
            font-family: "Segoe UI", "Calibri", sans-serif;
        }
        QMenuBar {
            background: #2a2a3a;
            color: #cdd6f4;
            border-bottom: 1px solid #444;
        }
        QMenuBar::item {
            background: transparent;
            padding: 8px 12px;
            border-radius: 6px;
        }
        QMenuBar::item:selected {
            background: rgba(100, 140, 255, 80);
        }
        QToolBar {
            spacing: 0px;
            padding: 6px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2d2d3d, stop:1 #22222e);
            border-bottom: 1px solid #444;
            margin: 0px;
        }
        QToolButton {
            background: transparent;
            border: 2px solid transparent;
            border-radius: 8px;
            margin: 4px;
            padding: 8px;
        }
        QToolButton:hover {
            background: rgba(100, 140, 255, 60);
            border: 2px solid rgba(100, 140, 255, 120);
        }
        QToolButton:pressed {
            background: rgba(100, 140, 255, 120);
        }
        QStatusBar {
            background: #2a2a3a;
            color: #cdd6f4;
            border-top: 1px solid #444;
        }
        QTabWidget::pane {
            border: 1px solid #444;
            background: #1e1e2e;
        }
        QTabBar::tab {
            background: #2a2a3a;
            color: #cdd6f4;
            padding: 8px 16px;
            margin: 2px;
            border-radius: 6px;
        }
        QTabBar::tab:hover {
            background: #3a3a5a;
        }
        QTabBar::tab:selected {
            background: #5a5aff;
            color: white;
        }
        QTreeWidget, QTextEdit, QSplitter {
            background: #222230;
            border: 1px solid #444;
            border-radius: 4px;
        }
        QTreeWidget::item:hover {
            background: rgba(100, 140, 255, 30);
        }
    )");
    return app.exec();
}
