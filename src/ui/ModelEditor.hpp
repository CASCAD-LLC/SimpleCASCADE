#pragma once
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QToolBar>
#include <QTextEdit>
#include <QSplitter>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

struct MEVertex { float x, y, z; };
struct MEFace { std::vector<int> indices; };

class MeshEditorViewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit MeshEditorViewport(QWidget *parent=nullptr);
    void setData(std::vector<MEVertex> verts, std::vector<MEFace> faces);
    void clear();
    QString toObj() const;
    void fromObj(const QString &objText);
signals:
    void status(const QString &msg);
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
private:
    void drawGrid();
    void drawMesh(bool filled);
    int pickVertex(const QPoint &p);
private:
    std::vector<MEVertex> m_vertices;
    std::vector<MEFace> m_faces;
    float m_camX=0, m_camY=0, m_camZ=-5;
    float m_rotX=20, m_rotY=30;
    QPoint m_last;
    int m_selectedVertex=-1;
};

class ModelEditor : public QWidget {
    Q_OBJECT
public:
    explicit ModelEditor(QWidget *parent=nullptr);
    QString exportObj() const { return m_view->toObj(); }
    void importObj(const QString &obj) { m_view->fromObj(obj); }
signals:
    void sendToScene(const QString &objText, const QString &name);
private:
    MeshEditorViewport *m_view;
    QTextEdit *m_code;
};
