// src/ui/MainWindow.cpp
#include "MainWindow.hpp"
#include "CodeEditor.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolButton>
#include <QFileDialog>
#include <QMouseEvent>
#include <QMessageBox>
#include <QProcess>
#include <iostream>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QFileInfo>
#include <QImage>
#include <QByteArray>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// === Реализация SceneObject ===

SceneObject::SceneObject(const std::string& objData, const std::string& objName)
    : name(objName) {
    loadFromObj(objData);
}

void SceneObject::loadFromObj(const std::string& objData) {
    vertices.clear();
    faces.clear();

    std::istringstream iss(objData);
    std::string line;

    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string type;
        lineStream >> type;

        if (type == "v") {
            float x, y, z;
            lineStream >> x >> y >> z;
            vertices.push_back({x, y, z});
        } else if (type == "f") {
            std::vector<int> face;
            std::string vertex;
            while (lineStream >> vertex) {
                size_t first_slash = vertex.find('/');
                std::string index_str = vertex.substr(0, first_slash);
                try {
                    int idx = std::stoi(index_str) - 1;
                    if (idx >= 0 && idx < vertices.size()) {
                        face.push_back(idx);
                    }
                } catch (...) { continue; }
            }
            if (face.size() >= 3) {
                Face f;
                f.indices = face;
                faces.push_back(f);
            }
        }
    }
}

void SceneObject::draw() {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rx, 1.0f, 0.0f, 0.0f);
    glRotatef(ry, 0.0f, 1.0f, 0.0f);
    glRotatef(rz, 0.0f, 0.0f, 1.0f);
    glScalef(sx, sy, sz);

    glDisable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glColor3f(0.75f, 0.8f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (const auto& face : faces) {
        if (face.indices.size() < 3) continue;
        for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
            int v0 = face.indices[0];
            int v1 = face.indices[i];
            int v2 = face.indices[i + 1];
            // Нормаль треугольника
            float ax = vertices[v1].x - vertices[v0].x;
            float ay = vertices[v1].y - vertices[v0].y;
            float az = vertices[v1].z - vertices[v0].z;
            float bx = vertices[v2].x - vertices[v0].x;
            float by = vertices[v2].y - vertices[v0].y;
            float bz = vertices[v2].z - vertices[v0].z;
            float nx = ay * bz - az * by;
            float ny = az * bx - ax * bz;
            float nz = ax * by - ay * bx;
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 1e-6f) { nx/=len; ny/=len; nz/=len; }
            glNormal3f(nx, ny, nz);
            glVertex3f(vertices[v0].x, vertices[v0].y, vertices[v0].z);
            glVertex3f(vertices[v1].x, vertices[v1].y, vertices[v1].z);
            glVertex3f(vertices[v2].x, vertices[v2].y, vertices[v2].z);
        }
    }
    glEnd();
    glDisable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glPopMatrix();
}

void SceneObject::drawForPicking() {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rx, 1.0f, 0.0f, 0.0f);
    glRotatef(ry, 0.0f, 1.0f, 0.0f);
    glRotatef(rz, 0.0f, 0.0f, 1.0f);
    glScalef(sx, sy, sz);

    glBegin(GL_TRIANGLES);
    for (const auto& face : faces) {
        if (face.indices.size() < 3) continue;
        for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
            int v0 = face.indices[0];
            int v1 = face.indices[i];
            int v2 = face.indices[i + 1];
            glVertex3f(vertices[v0].x, vertices[v0].y, vertices[v0].z);
            glVertex3f(vertices[v1].x, vertices[v1].y, vertices[v1].z);
            glVertex3f(vertices[v2].x, vertices[v2].y, vertices[v2].z);
        }
    }
    glEnd();

    glPopMatrix();
}

std::string SceneObject::toObj() const {
    std::ostringstream out;
    for (const auto &v : vertices) {
        out << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    for (const auto &f : faces) {
        if (f.indices.size() < 3) continue;
        out << "f";
        for (int idx : f.indices) out << ' ' << (idx + 1);
        out << '\n';
    }
    return out.str();
}

// === Реализация GLWidget ===

GLWidget::GLWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);

    m_camZ = -5.0f;
    m_camRotX = 30.0f;
    m_camRotY = 45.0f;
}

void GLWidget::addObject(const std::string& objData, const std::string& name) {
    m_objects.push_back(std::make_unique<SceneObject>(objData, name));
    update();
}

void GLWidget::setupProjection() {
    float aspect = (float)width() / std::max(1, height());
    float zNear = 0.1f, zFar = 1000.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (m_ortho) {
        float size = 5.0f * std::abs(m_camZ);
        glOrtho(-size * aspect, size * aspect, -size, size, -zFar, zFar);
    } else {
        // Перспектива через gluPerspective-подобную математику
        float f = 1.0f / std::tan((m_fovY * M_PI / 180.0f) / 2.0f);
        // Эквивалент glFrustum по FOV
        float top = zNear / f;
        float right = top * aspect;
        glFrustum(-right, right, -top, top, zNear, zFar);
    }
}

void GLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    setupProjection();

    GLfloat light0Pos[] = { 5.0f, 10.0f, 5.0f, 1.0f };
    GLfloat light1Pos[] = { -5.0f, 3.0f, -5.0f, 1.0f };
    GLfloat lightAmb[] = { 0.15f, 0.15f, 0.15f, 1.0f };
    GLfloat lightDiff[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightDiff);
}

void GLWidget::paintGL() {
    qDebug() << "GLWidget: paintGL called";
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glPolygonMode(GL_FRONT_AND_BACK, m_wireframe ? GL_LINE : GL_FILL);
    glEnable(GL_DEPTH_TEST);

    // Камера
    glRotatef(-m_camRotX, 1.0f, 0.0f, 0.0f);
    glRotatef(-m_camRotY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-m_camX, -m_camY, m_camZ);

    // Объекты
    for (size_t i = 0; i < m_objects.size(); ++i) {
        glPushName(static_cast<GLuint>(i));
        m_objects[i]->draw();
        glPopName();
    }

    // Оси и сетка в мировых координатах (вращаются вместе со сценой)
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glColor3f(0.2f, 0.2f, 0.4f);
    glBegin(GL_LINES);
    float size = 20.0f;
    for (float i = -size; i <= size; i += 1.0f) {
        if (fabs(i) < 0.1f) continue;
        glColor3f(0.3f, 0.3f, 0.3f);
        glVertex3f(i, 0, -size);
        glVertex3f(i, 0, size);
        glVertex3f(-size, 0, i);
        glVertex3f(size, 0, i);
    }
    glEnd();

    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(0,0,0); glVertex3f(5,0,0);
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(0,0,0); glVertex3f(0,5,0);
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(0,0,0); glVertex3f(0,0,5);
    glEnd();
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
}

void GLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    setupProjection();
}

void GLWidget::mousePressEvent(QMouseEvent *ev) {
    qDebug() << "GLWidget: mouse press at" << ev->position();
    m_lastMousePos = ev->position().toPoint();

    if (ev->button() == Qt::LeftButton) {
        selectObject(ev->position().toPoint());
        m_leftButtonPressed = true;
    } else if (ev->button() == Qt::RightButton) {
        m_rightButtonPressed = true;
    } else if (ev->button() == Qt::MiddleButton) {
        m_middleButtonPressed = true;
    }

    setCursor(m_selectedObject ? Qt::SizeAllCursor : Qt::ArrowCursor);
    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *ev) {
    qDebug() << "GLWidget: mouse release at" << ev->position();
    if (ev->button() == Qt::RightButton) {
        m_rightButtonPressed = false;
    }
    if (ev->button() == Qt::LeftButton) {
        m_leftButtonPressed = false;
        m_orbitWithLMB = false;
    }
    if (ev->button() == Qt::MiddleButton) {
        m_middleButtonPressed = false;
    }
    setCursor(m_selectedObject ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void GLWidget::mouseMoveEvent(QMouseEvent *ev) {
    qDebug() << "GLWidget: mouse move at" << ev->position();
    QPoint diff = ev->position().toPoint() - m_lastMousePos;

    if (m_rightButtonPressed) {
        m_camRotX += diff.y() * 0.5f;
        m_camRotY += diff.x() * 0.5f;
        // Разрешаем полный наклон по X
        if (m_camRotX > 179.9f) m_camRotX -= 360.0f;
        if (m_camRotX < -179.9f) m_camRotX += 360.0f;
        // Нормализуем азимут
        if (m_camRotY > 360.0f) m_camRotY -= 360.0f;
        if (m_camRotY < -360.0f) m_camRotY += 360.0f;
        qDebug() << "orbit via RMB" << m_camRotX << m_camRotY;
    }
    else if (m_middleButtonPressed && (ev->modifiers() & Qt::ShiftModifier)) {
        // Blender: Shift + MMB = панорамирование
        float speed = 0.01f * fabs(m_camZ);
        m_camX -= diff.x() * speed;
        m_camY += diff.y() * speed;
        qDebug() << "pan via Shift+MMB" << m_camX << m_camY;
    }
    else if (m_middleButtonPressed && (ev->modifiers() & Qt::ControlModifier)) {
        // Blender: Ctrl + MMB = доли-зум (приближение/удаление)
        m_camZ += (diff.y() - diff.x()) * 0.01f; // вертикаль сильнее влияет
        m_camZ = std::clamp(m_camZ, -20.0f, -1.0f);
        qDebug() << "dolly via Ctrl+MMB" << m_camZ;
    }
    else if (m_middleButtonPressed) {
        // Blender: MMB = орбита
        m_camRotX += diff.y() * 0.5f;
        m_camRotY += diff.x() * 0.5f;
        if (m_camRotX > 179.9f) m_camRotX -= 360.0f;
        if (m_camRotX < -179.9f) m_camRotX += 360.0f;
        if (m_camRotY > 360.0f) m_camRotY -= 360.0f;
        if (m_camRotY < -360.0f) m_camRotY += 360.0f;
        qDebug() << "orbit via MMB" << m_camRotX << m_camRotY;
    }
    // Alt/Ctrl с ЛКМ для орбиты/прочего выключаем — Blender не использует ЛКМ для орбиты
    else if (m_leftButtonPressed && ev->modifiers() & Qt::ShiftModifier) {
        float speed = 0.01f * fabs(m_camZ);
        m_camX -= diff.x() * speed;
        m_camY += diff.y() * speed;
        qDebug() << "pan via Shift+LMB" << m_camX << m_camY;
    }
    else if (m_leftButtonPressed && m_selectedObject) {
        float speed = 0.01f * fabs(m_camZ);
        m_selectedObject->x += diff.x() * speed;
        m_selectedObject->y -= diff.y() * speed;
        emit objectMoved(m_selectedObject->name, m_selectedObject->x, m_selectedObject->y, m_selectedObject->z);
        qDebug() << "move object via LMB" << m_selectedObject->x << m_selectedObject->y;
    }
    // иначе ЛКМ без модификаторов — ничего (выбор/перемещение уже обработаны)

    m_lastMousePos = ev->position().toPoint();
    update();
}

void GLWidget::wheelEvent(QWheelEvent *ev) {
    float delta = ev->angleDelta().y() / 120.0f;
    m_camZ += delta * 0.5f;
    m_camZ = std::clamp(m_camZ, -20.0f, -1.0f);
    update();
}

void GLWidget::zoomIn() {
    m_camZ += 0.5f;
    m_camZ = std::clamp(m_camZ, -20.0f, -1.0f);
    update();
}

void GLWidget::zoomOut() {
    m_camZ -= 0.5f;
    m_camZ = std::clamp(m_camZ, -20.0f, -1.0f);
    update();
}

void GLWidget::resetView() {
    m_camX = 0.0f; m_camY = 0.0f; m_camZ = -5.0f;
    m_camRotX = 30.0f; m_camRotY = 45.0f;
    update();
}

void GLWidget::frameAll() {
    // Примитивная реализация: сдвигаем Z так, чтобы вся сцена влезла
    float maxRadius = 1.0f;
    for (const auto &ptr : m_objects) {
        for (const auto &v : ptr->vertices) {
            maxRadius = std::max(maxRadius, std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z));
        }
    }
    m_camZ = -std::clamp(maxRadius * 1.5f, 2.0f, 18.0f);
    update();
}

void GLWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto &u : event->mimeData()->urls()) {
            if (u.toLocalFile().endsWith(".obj", Qt::CaseInsensitive)) { event->acceptProposedAction(); return; }
        }
    }
}

void GLWidget::dropEvent(QDropEvent *event) {
    for (const auto &u : event->mimeData()->urls()) {
        QString p = u.toLocalFile();
        if (!p.endsWith(".obj", Qt::CaseInsensitive)) continue;
        QFile f(p);
        if (f.open(QIODevice::ReadOnly)) {
            auto data = QString::fromUtf8(f.readAll());
            auto name = QFileInfo(p).baseName();
            addObject(data.toStdString(), name.toStdString());
        }
    }
}

void GLWidget::selectObject(const QPoint& pos) {
    GLuint selectBuf[512];
    glSelectBuffer(512, selectBuf);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    float sx = static_cast<float>(pos.x());
    float sy = static_cast<float>(viewport[3] - pos.y());

    glTranslatef((2.0f * sx - viewport[2]) / viewport[2],
                 (2.0f * sy - viewport[3]) / viewport[3], 0.0f);
    glScalef(5.0f / viewport[2], 5.0f / viewport[3], 1.0f);

    setupProjection();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-m_camRotX, 1.0f, 0.0f, 0.0f);
    glRotatef(-m_camRotY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-m_camX, -m_camY, m_camZ);

    for (size_t i = 0; i < m_objects.size(); ++i) {
        glLoadName(static_cast<GLuint>(i));
        m_objects[i]->drawForPicking();
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glFlush();

    GLint hits = glRenderMode(GL_RENDER);
    if (hits > 0) {
        GLuint minZ = 0xFFFFFFFF;
        GLuint selected = 0;
        for (int i = 0; i < hits; ++i) {
            GLuint z1 = selectBuf[i * 4 + 1];
            if (z1 < minZ) {
                minZ = z1;
                selected = selectBuf[i * 4 + 3];
            }
        }
        m_selectedObject = m_objects[selected].get();
        emit objectSelected(m_selectedObject->name);
        update();
    }
}

bool GLWidget::removeSelectedObject() {
    if (!m_selectedObject) return false;
    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (m_objects[i].get() == m_selectedObject) {
            m_objects.erase(m_objects.begin() + static_cast<long>(i));
            m_selectedObject = nullptr;
            update();
            return true;
        }
    }
    return false;
}

// === Реализация MainWindow ===

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SimpleCASCADE — 3D Engine");
    setWindowState(Qt::WindowMaximized);
    setupUI();

    m_modelTimer = new QTimer(this);
    connect(m_modelTimer, &QTimer::timeout, this, &MainWindow::checkForModel);
    m_modelTimer->start(500);

    connect(m_glWidget, &GLWidget::objectSelected, this, [this](const std::string& name) {
        m_console->append(QString("✅ Выделено: %1").arg(QString::fromStdString(name)));
    });

    connect(m_glWidget, &GLWidget::objectMoved, this, [this](const std::string& name, float x, float y, float z) {
        m_console->append(QString("📍 Перемещено: %1 → X=%2, Y=%3, Z=%4")
            .arg(QString::fromStdString(name))
            .arg(x, 0, 'f', 2)
            .arg(y, 0, 'f', 2)
            .arg(z, 0, 'f', 2));
    });
}

void MainWindow::setupUI() {
    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolbar();
    layout->addWidget(m_toolbar);

    m_tabWidget = new QTabWidget();

    // --- Вкладка 1: Сцена ---
    auto sceneTab = new QWidget();
    auto sceneLayout = new QVBoxLayout(sceneTab);
    sceneLayout->setContentsMargins(0, 0, 0, 0);

    auto mainSplitter = new QSplitter(Qt::Horizontal);

    m_projectTree = new QTreeWidget();
    m_projectTree->setHeaderLabel("Проект");
    auto assets = new QTreeWidgetItem(m_projectTree, QStringList("Assets"));
    auto scenesNode = new QTreeWidgetItem(assets, QStringList("Scenes"));
    auto modelsNode = new QTreeWidgetItem(assets, QStringList("Models"));
    new QTreeWidgetItem(assets, QStringList("Scripts"));
    new QTreeWidgetItem(assets, QStringList("Materials"));
    m_projectTree->setMinimumWidth(220);
    mainSplitter->addWidget(m_projectTree);

    auto rightSplitter = new QSplitter(Qt::Vertical);
    m_viewTabs = new QTabWidget();
    m_glWidget = new GLWidget();
    m_glWidget->setFocus();
    m_glWidget->setFocusPolicy(Qt::StrongFocus);
    m_viewTabs->addTab(m_glWidget, "Scene");
    // Вкладка Game пока отображает тот же вид
    auto gameView = new QLabel("Game View (позже — камера из сцены)");
    gameView->setAlignment(Qt::AlignCenter);
    gameView->setStyleSheet("background:#111;color:#777");
    m_viewTabs->addTab(gameView, "Game");
    rightSplitter->addWidget(m_viewTabs);

    m_console = new QTextEdit();
    m_console->setReadOnly(true);
    m_console->setFontFamily("Monospace");
    m_console->setStyleSheet("background:#0f1218; color:#d0d0d0; padding:8px;");
    m_console->append("SimpleCASCADE запущен.");
    m_console->append("Текущая директория: " + QDir().absolutePath());
    rightSplitter->addWidget(m_console);
    rightSplitter->setStretchFactor(0, 4);
    rightSplitter->setStretchFactor(1, 1);

    mainSplitter->addWidget(rightSplitter);

    auto sideSplitter = new QSplitter(Qt::Vertical);
    m_sceneTree = new QTreeWidget();
    m_sceneTree->setHeaderLabel("Иерархия");
    auto sceneRoot = new QTreeWidgetItem(m_sceneTree, QStringList("Сцена"));
    new QTreeWidgetItem(sceneRoot, QStringList("Камера"));
    new QTreeWidgetItem(sceneRoot, QStringList("Свет"));
    m_sceneTree->setMinimumWidth(220);
    sideSplitter->addWidget(m_sceneTree);

    m_inspector = createInspector();
    sideSplitter->addWidget(m_inspector);
    sideSplitter->setStretchFactor(0, 2);
    sideSplitter->setStretchFactor(1, 3);
    mainSplitter->addWidget(sideSplitter);

    mainSplitter->setStretchFactor(0, 1); // Project
    mainSplitter->setStretchFactor(1, 4); // Viewport+Console (больше места сцене)
    mainSplitter->setStretchFactor(2, 1); // Hierarchy+Inspector

    sceneLayout->addWidget(mainSplitter);
    m_tabWidget->addTab(sceneTab, " 🎮 Сцена ");

    // --- Вкладка 2: Редактор кода ---
    auto codeTab = new QWidget();
    auto codeLayout = new QVBoxLayout(codeTab);
    codeLayout->setContentsMargins(0, 0, 0, 0);

    auto editor = new CodeEditor();
    codeLayout->addWidget(editor);
    editor->setPlainText(R"(
#include "MyActor.hpp"

class PlayerController : public Actor {
public:
    void Update(float dt) override {
        Transform.Translate(0, 0, 10 * dt);
    }
};
)");

    m_tabWidget->addTab(codeTab, " 💻 Редактор кода ");

    // --- Вкладка 3: Редактор моделей ---
    auto modelTab = new QWidget();
    auto modelLayout = new QVBoxLayout(modelTab);
    modelLayout->setContentsMargins(0, 0, 0, 0);

    auto modelPreview = new QLabel("Редактор моделей\n(пока пусто)");
    modelPreview->setAlignment(Qt::AlignCenter);
    modelPreview->setStyleSheet("background:#1e1e1e; color:#d0d0d0; font-size:16px;");
    modelLayout->addWidget(modelPreview);

    m_tabWidget->addTab(modelTab, " 🧱 Редактор моделей ");

    layout->addWidget(m_tabWidget);

    setupStatusBar();
    setCentralWidget(central);
}

QWidget* MainWindow::createInspector() {
    auto panel = new QWidget();
    auto form = new QFormLayout(panel);
    form->setLabelAlignment(Qt::AlignLeft);

    auto mkSpin = [](double min, double max, double step) {
        auto *s = new QDoubleSpinBox();
        s->setRange(min, max);
        s->setDecimals(3);
        s->setSingleStep(step);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setMaximumWidth(120);
        return s;
    };

    m_posX = mkSpin(-10000, 10000, 0.1); m_posY = mkSpin(-10000, 10000, 0.1); m_posZ = mkSpin(-10000, 10000, 0.1);
    m_rotX = mkSpin(-360, 360, 1); m_rotY = mkSpin(-360, 360, 1); m_rotZ = mkSpin(-360, 360, 1);
    m_sclX = mkSpin(0.001, 1000, 0.1); m_sclY = mkSpin(0.001, 1000, 0.1); m_sclZ = mkSpin(0.001, 1000, 0.1);

    auto posRow = new QWidget(); auto posLay = new QHBoxLayout(posRow); posLay->setContentsMargins(0,0,0,0); posLay->addWidget(m_posX); posLay->addWidget(m_posY); posLay->addWidget(m_posZ);
    auto rotRow = new QWidget(); auto rotLay = new QHBoxLayout(rotRow); rotLay->setContentsMargins(0,0,0,0); rotLay->addWidget(m_rotX); rotLay->addWidget(m_rotY); rotLay->addWidget(m_rotZ);
    auto sclRow = new QWidget(); auto sclLay = new QHBoxLayout(sclRow); sclLay->setContentsMargins(0,0,0,0); sclLay->addWidget(m_sclX); sclLay->addWidget(m_sclY); sclLay->addWidget(m_sclZ);

    form->addRow("Позиция", posRow);
    form->addRow("Вращение", rotRow);
    form->addRow("Масштаб", sclRow);

    // Привязки к выбранному объекту
    auto connectSpin = [this](QDoubleSpinBox* s, auto setter) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, setter](double v){
            if (m_glWidget->getSelectedObject()) {
                setter(*m_glWidget->getSelectedObject(), static_cast<float>(v));
                m_glWidget->update();
            }
        });
    };

    connectSpin(m_posX, [](SceneObject& o, float v){ o.x = v; });
    connectSpin(m_posY, [](SceneObject& o, float v){ o.y = v; });
    connectSpin(m_posZ, [](SceneObject& o, float v){ o.z = v; });
    connectSpin(m_rotX, [](SceneObject& o, float v){ o.rx = v; });
    connectSpin(m_rotY, [](SceneObject& o, float v){ o.ry = v; });
    connectSpin(m_rotZ, [](SceneObject& o, float v){ o.rz = v; });
    connectSpin(m_sclX, [](SceneObject& o, float v){ o.sx = v; });
    connectSpin(m_sclY, [](SceneObject& o, float v){ o.sy = v; });
    connectSpin(m_sclZ, [](SceneObject& o, float v){ o.sz = v; });

    // Обновление при выборе объекта
    connect(m_glWidget, &GLWidget::objectSelected, this, [this](const std::string&){
        bindInspector(m_glWidget->getSelectedObject());
    });

    return panel;
}

void MainWindow::bindInspector(SceneObject* o) {
    if (!o) return;
    m_posX->blockSignals(true); m_posY->blockSignals(true); m_posZ->blockSignals(true);
    m_rotX->blockSignals(true); m_rotY->blockSignals(true); m_rotZ->blockSignals(true);
    m_sclX->blockSignals(true); m_sclY->blockSignals(true); m_sclZ->blockSignals(true);

    m_posX->setValue(o->x); m_posY->setValue(o->y); m_posZ->setValue(o->z);
    m_rotX->setValue(o->rx); m_rotY->setValue(o->ry); m_rotZ->setValue(o->rz);
    m_sclX->setValue(o->sx); m_sclY->setValue(o->sy); m_sclZ->setValue(o->sz);

    m_posX->blockSignals(false); m_posY->blockSignals(false); m_posZ->blockSignals(false);
    m_rotX->blockSignals(false); m_rotY->blockSignals(false); m_rotZ->blockSignals(false);
    m_sclX->blockSignals(false); m_sclY->blockSignals(false); m_sclZ->blockSignals(false);
}

void MainWindow::setupToolbar() {
    m_toolbar = new QToolBar("Главное меню", this);
    m_toolbar->setMovable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_toolbar->setIconSize(QSize(24, 24));
    m_toolbar->setStyleSheet(R"(
        QToolBar {
            spacing: 0px;
            padding: 4px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2d2d3d, stop:1 #22222e);
            border-bottom: 1px solid #444;
        }
        QToolButton {
            width: 44px;
            height: 44px;
            margin: 2px;
            padding: 4px;
            border: 2px solid transparent;
            border-radius: 8px;
            background: transparent;
        }
        QToolButton:hover {
            background: rgba(100, 140, 255, 60);
            border: 2px solid rgba(100, 140, 255, 120);
        }
        QToolButton:pressed {
            background: rgba(100, 140, 255, 120);
        }
    )");

    // Вспомогательная функция для загрузки иконок
    auto addAction = [this](const QString& text, const QString& iconPath) {
        QAction* action = new QAction(text, this);
        if (QFile::exists(iconPath)) {
            action->setIcon(QIcon(iconPath));
            if (m_console) m_console->append("✅ Иконка: " + iconPath);
        } else {
            if (m_console) m_console->append("❌ Нет иконки: " + iconPath);
            QPixmap px(24, 24);
            px.fill(Qt::white);
            action->setIcon(QIcon(px));
        }
        m_toolbar->addAction(action);
        return action;
    };

    auto newAction = addAction("Новый", "icons/new.png");
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewScene);
    auto openAction = addAction("Открыть", "icons/open.png");
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenScene);
    auto saveAction = addAction("Сохранить", "icons/save.png");
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveScene);

    // Импорт OBJ
    QAction* importObj = new QAction("Импорт OBJ", this);
    m_toolbar->addAction(importObj);
    connect(importObj, &QAction::triggered, this, [this]{
        QString p = QFileDialog::getOpenFileName(this, "Импорт OBJ", "", "OBJ Files (*.obj)");
        if (p.isEmpty()) return;
        QFile f(p);
        if (f.open(QIODevice::ReadOnly)) {
            auto data = QString::fromUtf8(f.readAll());
            auto modelName = QFileInfo(p).baseName();
            m_glWidget->addObject(data.toStdString(), modelName.toStdString());
            new QTreeWidgetItem(m_sceneTree->topLevelItem(0), QStringList(modelName));
            m_console->append("[IMPORT] OBJ: " + p);
        }
    });

    m_toolbar->addSeparator();

    auto playAction = addAction("Запуск", "icons/play.png");
    connect(playAction, &QAction::triggered, this, &MainWindow::onRun);

    auto pauseAction = addAction("Пауза", "icons/pause.png");
    connect(pauseAction, &QAction::triggered, this, &MainWindow::onPause);

    auto stopAction = addAction("Стоп", "icons/stop.png");
    connect(stopAction, &QAction::triggered, this, &MainWindow::onStop);

    m_toolbar->addSeparator();

    // Управление камерой
    QAction* zoomInAct = new QAction("Zoom+", this);
    m_toolbar->addAction(zoomInAct);
    connect(zoomInAct, &QAction::triggered, this, [this]{ m_glWidget->zoomIn(); });
    QAction* zoomOutAct = new QAction("Zoom-", this);
    m_toolbar->addAction(zoomOutAct);
    connect(zoomOutAct, &QAction::triggered, this, [this]{ m_glWidget->zoomOut(); });
    QAction* resetViewAct = new QAction("Reset", this);
    m_toolbar->addAction(resetViewAct);
    connect(resetViewAct, &QAction::triggered, this, [this]{ m_glWidget->resetView(); });
    QAction* frameAllAct = new QAction("Frame", this);
    m_toolbar->addAction(frameAllAct);
    connect(frameAllAct, &QAction::triggered, this, [this]{ m_glWidget->frameAll(); });

    auto aiAction = addAction("ИИ", "icons/ai.png");
    connect(aiAction, &QAction::triggered, [this] {
        QProcess::startDetached("python3", {QString::fromStdString(std::string(PYTHON_DIR) + "/ai_agent.py")});
    });

    auto modelAction = addAction("Модель", "icons/model.png");
    connect(modelAction, &QAction::triggered, [this] {
        m_tabWidget->setCurrentIndex(2);
    });

    // Дополнительно: дублирование/удаление/экспорт/скриншот
    m_toolbar->addSeparator();
    QAction* duplicateAct = new QAction("Дубль", this);
    m_toolbar->addAction(duplicateAct);
    connect(duplicateAct, &QAction::triggered, this, &MainWindow::onDuplicateSelected);

    QAction* deleteAct = new QAction("Удалить", this);
    m_toolbar->addAction(deleteAct);
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    QAction* exportAct = new QAction("Export", this);
    m_toolbar->addAction(exportAct);
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportSelectedObj);

    QAction* screenshotAct = new QAction("Shot", this);
    m_toolbar->addAction(screenshotAct);
    connect(screenshotAct, &QAction::triggered, this, &MainWindow::onSaveScreenshot);

    // Переключатели Wireframe/Ortho
    m_toolbar->addSeparator();
    QAction* wireframeAct = new QAction("Wireframe", this);
    wireframeAct->setCheckable(true);
    m_toolbar->addAction(wireframeAct);
    connect(wireframeAct, &QAction::toggled, this, [this](bool on){ m_glWidget->setWireframe(on); });
    QAction* orthoAct = new QAction("Ortho", this);
    orthoAct->setCheckable(true);
    m_toolbar->addAction(orthoAct);
    connect(orthoAct, &QAction::toggled, this, [this](bool on){ m_glWidget->setOrtho(on); });
}

void MainWindow::setupStatusBar() {
    m_statusBar = new QStatusBar(this);
    m_statusLabel = new QLabel("Готов");
    m_statusBar->addPermanentWidget(m_statusLabel);
    m_statusBar->showMessage("Загрузка завершена", 3000);
    setStatusBar(m_statusBar);
}

static QString saveSceneDialog(QWidget* parent) {
    return QFileDialog::getSaveFileName(parent, "Сохранить сцену", "", "SimpleCASCADE Scene (*.scene)");
}

static QString openSceneDialog(QWidget* parent) {
    return QFileDialog::getOpenFileName(parent, "Открыть сцену", "", "SimpleCASCADE Scene (*.scene)");
}

void MainWindow::checkForModel() {
    QString flagPath = QString::fromStdString(std::string(PYTHON_DIR) + "/model_ready.flag");
    QString objPath = QString::fromStdString(std::string(PYTHON_DIR) + "/temp_model.obj");

    if (QFile::exists(flagPath)) {
        QFile::remove(flagPath);
        if (QFile::exists(objPath)) {
            QFile file(objPath);
            if (file.open(QIODevice::ReadOnly)) {
                QTextStream in(&file);
                QString data = in.readAll();
                auto modelName = QString("Модель_%1").arg(m_glWidget->getObjectCount() + 1);
                m_glWidget->addObject(data.toStdString(), modelName.toStdString());
                m_console->append("[AI] Модель добавлена в сцену");
                file.close();
                QFile::remove(objPath);
                auto item = new QTreeWidgetItem(m_sceneTree->topLevelItem(0), QStringList(modelName));
            }
        }
    }
}

void MainWindow::onNewScene() {
    if (m_glWidget) m_glWidget->clearObjects();
    if (m_sceneTree && m_sceneTree->topLevelItemCount() > 0) {
        auto root = m_sceneTree->topLevelItem(0);
        // Удаляем детей, кроме Камера/Свет
        while (root->childCount() > 2) {
            delete root->takeChild(2);
        }
    }
    m_console->append("[SCENE] Новая сцена");
}

void MainWindow::onSaveScene() {
    QString path = saveSceneDialog(this);
    if (path.isEmpty()) return;
    QJsonArray objects;
    for (const auto &ptr : m_glWidget->objects()) {
        const auto &o = *ptr;
        QJsonObject jo;
        jo["name"] = QString::fromStdString(o.name);
        jo["transform"] = QJsonObject{{"pos", QJsonArray{o.x,o.y,o.z}}, {"rot", QJsonArray{o.rx,o.ry,o.rz}}, {"scl", QJsonArray{o.sx,o.sy,o.sz}}};
        jo["mesh_obj"] = QString::fromStdString(o.toObj());
        objects.push_back(jo);
    }
    QJsonObject root{{"objects", objects}};
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
        m_console->append("[SCENE] Сцена сохранена: " + path);
    }
}

void MainWindow::onOpenScene() {
    QString path = openSceneDialog(this);
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;
    onNewScene();
    auto arr = doc.object().value("objects").toArray();
    for (const auto &it : arr) {
        auto jo = it.toObject();
        auto name = jo.value("name").toString("Object");
        auto mesh = jo.value("mesh_obj").toString();
        m_glWidget->addObject(mesh.toStdString(), name.toStdString());
        if (auto so = m_glWidget->getSelectedObject()) {
            // noop
        }
        auto tr = jo.value("transform").toObject();
        auto pos = tr.value("pos").toArray();
        auto rot = tr.value("rot").toArray();
        auto scl = tr.value("scl").toArray();
        if (!m_glWidget->objects().empty()) {
            auto &o = *m_glWidget->objects().back();
            if (pos.size()==3){ o.x=pos[0].toDouble(); o.y=pos[1].toDouble(); o.z=pos[2].toDouble(); }
            if (rot.size()==3){ o.rx=rot[0].toDouble(); o.ry=rot[1].toDouble(); o.rz=rot[2].toDouble(); }
            if (scl.size()==3){ o.sx=scl[0].toDouble(); o.sy=scl[1].toDouble(); o.sz=scl[2].toDouble(); }
        }
        auto item = new QTreeWidgetItem(m_sceneTree->topLevelItem(0), QStringList(name));
    }
    m_glWidget->update();
    m_console->append("[SCENE] Сцена загружена: " + path);
}

void MainWindow::onRun() {
    m_statusLabel->setText("Сцена: Запущена");
    m_console->append("[RUN] Сцена запущена.");
}

void MainWindow::onDuplicateSelected() {
    auto sel = m_glWidget->getSelectedObject();
    if (!sel) { if (m_console) m_console->append("[DUP] Нет выбранного объекта"); return; }
    QString name = QString::fromStdString(sel->name) + "_copy";
    m_glWidget->addObject(sel->toObj(), name.toStdString());
    if (m_sceneTree && m_sceneTree->topLevelItemCount() > 0) {
        auto root = m_sceneTree->topLevelItem(0);
        new QTreeWidgetItem(root, QStringList(name));
    }
    if (m_console) m_console->append("[DUP] Создан дубль: " + name);
}

void MainWindow::onDeleteSelected() {
    auto sel = m_glWidget->getSelectedObject();
    if (!sel) { if (m_console) m_console->append("[DEL] Нет выбранного объекта"); return; }
    QString name = QString::fromStdString(sel->name);
    if (m_glWidget->removeSelectedObject()) {
        if (m_sceneTree && m_sceneTree->topLevelItemCount() > 0) {
            auto root = m_sceneTree->topLevelItem(0);
            for (int i = 0; i < root->childCount(); ++i) {
                auto child = root->child(i);
                if (child->text(0) == name && child->text(0) != "Камера" && child->text(0) != "Свет") {
                    delete root->takeChild(i);
                    break;
                }
            }
        }
        if (m_console) m_console->append("[DEL] Удалён: " + name);
    } else {
        if (m_console) m_console->append("[DEL] Не удалось удалить: " + name);
    }
}

void MainWindow::onExportSelectedObj() {
    auto sel = m_glWidget->getSelectedObject();
    if (!sel) { if (m_console) m_console->append("[EXPORT] Нет выбранного объекта"); return; }
    QString path = QFileDialog::getSaveFileName(this, "Экспорт OBJ", sel->name.empty() ? "object.obj" : QString::fromStdString(sel->name) + ".obj", "OBJ Files (*.obj)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        QByteArray data = QByteArray::fromStdString(sel->toObj());
        f.write(data);
        f.close();
        if (m_console) m_console->append("[EXPORT] Сохранён OBJ: " + path);
    }
}

void MainWindow::onSaveScreenshot() {
    QString path = QFileDialog::getSaveFileName(this, "Сохранить скриншот", "screenshot.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    QImage img = m_glWidget->grabFramebuffer();
    if (!img.isNull()) {
        if (img.save(path)) {
            if (m_console) m_console->append("[SHOT] Скриншот: " + path);
        } else {
            if (m_console) m_console->append("[SHOT] Ошибка сохранения: " + path);
        }
    }
}

void MainWindow::onPause() {
    m_statusLabel->setText("Сцена: На паузе");
    m_console->append("[PAUSE] Сцена приостановлена.");
}

void MainWindow::onStop() {
    m_statusLabel->setText("Сцена: Остановлена");
    m_console->append("[STOP] Сцена остановлена.");
}
