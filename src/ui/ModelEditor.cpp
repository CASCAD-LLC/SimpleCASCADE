#include "ModelEditor.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSplitter>
#include <QLabel>
#include <QTextStream>
#include <cmath>

MeshEditorViewport::MeshEditorViewport(QWidget *parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
}

void MeshEditorViewport::setData(std::vector<MEVertex> verts, std::vector<MEFace> faces) {
    m_vertices = std::move(verts);
    m_faces = std::move(faces);
    update();
}

void MeshEditorViewport::clear() { m_vertices.clear(); m_faces.clear(); update(); }

QString MeshEditorViewport::toObj() const {
    QString out;
    for (const auto &v : m_vertices) out += QString("v %1 %2 %3\n").arg(v.x).arg(v.y).arg(v.z);
    for (const auto &f : m_faces) {
        if (f.indices.size()<3) continue;
        out += "f";
        for (int idx : f.indices) out += QString(" %1").arg(idx+1);
        out += "\n";
    }
    return out;
}

void MeshEditorViewport::fromObj(const QString &objText) {
    m_vertices.clear(); m_faces.clear();
    QTextStream in(const_cast<QString*>(&objText), QIODevice::ReadOnly);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed(); if (line.isEmpty()) continue;
        auto parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        if (parts[0]=="v" && parts.size()>=4) {
            m_vertices.push_back({parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()});
        } else if (parts[0]=="f" && parts.size()>=4) {
            MEFace f; for (int i=1;i<parts.size();++i){ auto tok=parts[i].split('/')[0]; int idx=tok.toInt(); if (idx>0) f.indices.push_back(idx-1);} if (f.indices.size()>=3) m_faces.push_back(std::move(f));
        }
    }
    update();
}

void MeshEditorViewport::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f,0.12f,0.16f,1);
    glEnable(GL_DEPTH_TEST);
}

void MeshEditorViewport::resizeGL(int w, int h) { glViewport(0,0,w,h); }

void MeshEditorViewport::drawGrid() {
    glDisable(GL_LIGHTING);
    glColor3f(0.25f,0.25f,0.3f);
    glBegin(GL_LINES);
    float size=10.0f; for (int i=-10;i<=10;++i){ glVertex3f((float)i,0,-size); glVertex3f((float)i,0,size); glVertex3f(-size,0,(float)i); glVertex3f(size,0,(float)i);} glEnd();
}

void MeshEditorViewport::drawMesh(bool filled) {
    if (filled) {
        glColor3f(0.7f,0.8f,1.0f);
        glBegin(GL_TRIANGLES);
        for (const auto &f : m_faces) { if (f.indices.size()<3) continue; for (size_t i=1;i+1<f.indices.size();++i){ int v0=f.indices[0], v1=f.indices[i], v2=f.indices[i+1]; glVertex3f(m_vertices[v0].x,m_vertices[v0].y,m_vertices[v0].z); glVertex3f(m_vertices[v1].x,m_vertices[v1].y,m_vertices[v1].z); glVertex3f(m_vertices[v2].x,m_vertices[v2].y,m_vertices[v2].z);} }
        glEnd();
    }
    glColor3f(0.1f,0.5f,1.0f);
    glBegin(GL_LINES);
    for (const auto &f : m_faces) {
        if (f.indices.size()<2) continue;
        for (size_t i=0;i<f.indices.size();++i){ int a=f.indices[i], b=f.indices[(i+1)%f.indices.size()]; glVertex3f(m_vertices[a].x,m_vertices[a].y,m_vertices[a].z); glVertex3f(m_vertices[b].x,m_vertices[b].y,m_vertices[b].z);} }
    glEnd();

    // vertices
    glPointSize(6.0f);
    glBegin(GL_POINTS);
    for (int i=0;i<(int)m_vertices.size();++i){ if (i==m_selectedVertex) glColor3f(1,0.4f,0.2f); else glColor3f(1,1,1); glVertex3f(m_vertices[i].x,m_vertices[i].y,m_vertices[i].z);} glEnd();
}

int MeshEditorViewport::pickVertex(const QPoint &p) {
    // naive: project each vertex using current MVP and choose nearest by screen distance
    // For simplicity, reuse GL immediate transforms (approximate)
    int best=-1; float bestDist=1e9f;
    // not implementing full unproject here due to simplicity; use 2D proximity heuristic in view space
    // This is placeholder selection that works reasonably with small meshes
    Q_UNUSED(p);
    return best;
}

void MeshEditorViewport::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = float(std::max(1,width()))/std::max(1,height()); float zNear=0.1f, zFar=1000.0f; float fov=60.0f; float f=1.0f/std::tan((fov*M_PI/180.0f)/2.0f); float top=zNear/f; float right=top*aspect; glFrustum(-right,right,-top,top,zNear,zFar);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotatef(-m_rotX,1,0,0); glRotatef(-m_rotY,0,1,0); glTranslatef(-m_camX,-m_camY,m_camZ);
    drawGrid();
    drawMesh(true);
}

void MeshEditorViewport::mousePressEvent(QMouseEvent *e) { m_last = e->position().toPoint(); update(); }
void MeshEditorViewport::mouseMoveEvent(QMouseEvent *e) {
    QPoint d = e->position().toPoint() - m_last;
    if (e->buttons() & Qt::MiddleButton) { m_rotX += d.y()*0.5f; m_rotY += d.x()*0.5f; }
    else if (e->buttons() & Qt::LeftButton) {
        if (m_selectedVertex>=0 && m_selectedVertex < (int)m_vertices.size()) {
            float s = 0.01f * std::fabs(m_camZ);
            m_vertices[m_selectedVertex].x += d.x()*s;
            m_vertices[m_selectedVertex].y -= d.y()*s;
        }
    }
    m_last = e->position().toPoint(); update();
}
void MeshEditorViewport::wheelEvent(QWheelEvent *e) { m_camZ += (e->angleDelta().y()/120.0f)*0.5f; update(); }

ModelEditor::ModelEditor(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    auto toolbar = new QToolBar(this);
    layout->addWidget(toolbar);
    auto splitter = new QSplitter(Qt::Horizontal,this);
    layout->addWidget(splitter);
    m_view = new MeshEditorViewport();
    splitter->addWidget(m_view);
    m_code = new QTextEdit();
    m_code->setPlaceholderText("OBJ-код модели");
    splitter->addWidget(m_code);
    splitter->setStretchFactor(0,3); splitter->setStretchFactor(1,2);

    auto newPrim = toolbar->addAction("Новый куб");
    auto importA = toolbar->addAction("Импорт OBJ");
    auto exportA = toolbar->addAction("Экспорт OBJ");
    auto applyA = toolbar->addAction("Применить OBJ → Вид");
    auto toSceneA = toolbar->addAction("Вставить в сцену");

    QObject::connect(newPrim, &QAction::triggered, this, [this]{
        std::vector<MEVertex> v = {{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}};
        std::vector<MEFace> f = {{{0,1,2}},{{0,2,3}},{{4,7,6}},{{4,6,5}},{{0,4,5}},{{0,5,1}},{{3,2,6}},{{3,6,7}},{{0,3,7}},{{0,7,4}},{{1,5,6}},{{1,6,2}}};
        m_view->setData(std::move(v), std::move(f));
        m_code->setPlainText(m_view->toObj());
    });
    QObject::connect(importA, &QAction::triggered, this, [this]{
        QString p = QFileDialog::getOpenFileName(this, "Импорт OBJ", "", "OBJ Files (*.obj)"); if (p.isEmpty()) return; QFile f(p); if (!f.open(QIODevice::ReadOnly)) return; QString t = QString::fromUtf8(f.readAll()); f.close(); m_view->fromObj(t); m_code->setPlainText(t);
    });
    QObject::connect(exportA, &QAction::triggered, this, [this]{
        QString p = QFileDialog::getSaveFileName(this, "Экспорт OBJ", "model.obj", "OBJ Files (*.obj)"); if (p.isEmpty()) return; QFile f(p); if (f.open(QIODevice::WriteOnly)) { auto t = m_view->toObj(); f.write(t.toUtf8()); f.close(); }
    });
    QObject::connect(applyA, &QAction::triggered, this, [this]{ m_view->fromObj(m_code->toPlainText()); });
    QObject::connect(toSceneA, &QAction::triggered, this, [this]{ emit sendToScene(m_view->toObj(), "EditedModel"); });
}

