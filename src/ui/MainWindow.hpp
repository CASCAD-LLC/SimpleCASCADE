#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QMouseEvent>
#include <QTimer>
#include <QLabel>
#include <QTreeWidget>
#include <memory>
#include <vector>
#include <string>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include "ModelEditor.hpp"
#include "CodePanel.hpp"

struct Vertex { float x, y, z; };
struct Face { std::vector<int> indices; };

class SceneObject {
public:
    std::string name;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    float r = 0.75f, g = 0.8f, b = 1.0f;
    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    SceneObject(const std::string& objData, const std::string& name = "Object");
    void loadFromObj(const std::string& objData);
    void draw();
    void drawForPicking();
    std::string toObj() const;
    void getAABB(Vertex &minV, Vertex &maxV) const {
        if (vertices.empty()) { minV = {0,0,0}; maxV = {0,0,0}; return; }
        minV = {vertices[0].x, vertices[0].y, vertices[0].z};
        maxV = minV;
        for (const auto &v : vertices) {
            if (v.x < minV.x) minV.x = v.x; if (v.y < minV.y) minV.y = v.y; if (v.z < minV.z) minV.z = v.z;
            if (v.x > maxV.x) maxV.x = v.x; if (v.y > maxV.y) maxV.y = v.y; if (v.z > maxV.z) maxV.z = v.z;
        }
    }
};

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);
    void addObject(const std::string& objData, const std::string& name = "Object");
    bool removeSelectedObject();
    size_t getObjectCount() const { return m_objects.size(); }
    SceneObject* getSelectedObject() const { return m_selectedObject; }
    void clearObjects() { m_objects.clear(); m_selectedObject = nullptr; update(); }
    const std::vector<std::unique_ptr<SceneObject>>& objects() const { return m_objects; }
    void setWireframe(bool on) { m_wireframe = on; update(); }
    void setOrtho(bool on) { m_ortho = on; setupProjection(); update(); }
    void zoomIn();
    void zoomOut();
    void resetView();
    void frameAll();
    int currentFPS() const { return m_lastFps; }

signals:
    void objectSelected(const std::string& name);
    void objectMoved(const std::string& name, float x, float y, float z);
    void fpsUpdated(int fps);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupProjection();
    void selectObject(const QPoint& pos);
    void drawSelectedBoundingBox();
    void updateFpsCounter();

    float m_camX = 0.0f, m_camY = 0.0f, m_camZ = -5.0f;
    float m_camRotX = 30.0f, m_camRotY = 45.0f;
    float m_lightX = 5.0f, m_lightY = 10.0f, m_lightZ = 5.0f;
    QPoint m_lastMousePos;
    bool m_leftButtonPressed = false;
    bool m_rightButtonPressed = false;
    bool m_middleButtonPressed = false;
    bool m_orbitWithLMB = false;
    bool m_wireframe = false;
    bool m_ortho = false;
    float m_fovY = 60.0f; // degrees, для перспективы

    enum class MoveAxis { None, X, Y, Z };
    MoveAxis m_axisConstraint = MoveAxis::None;

    QElapsedTimer m_fpsTimer;
    int m_frameCount = 0;
    int m_lastFps = 0;

    std::vector<std::unique_ptr<SceneObject>> m_objects;
    SceneObject* m_selectedObject = nullptr;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onRun();
    void onPause();
    void onStop();
    void checkForModel();
    void onNewScene();
    void onOpenScene();
    void onSaveScene();
    void onSaveSession();
    void onLoadSession();
    void onBuildGame();
    void onDuplicateSelected();
    void onDeleteSelected();
    void onExportSelectedObj();
    void onSaveScreenshot();

private:
    void setupUI();
    void setupToolbar();
    void setupConsole();
    void setupStatusBar();
    QWidget* createInspector();
    void bindInspector(SceneObject* object);

    QToolBar *m_toolbar = nullptr;
    QTabWidget *m_tabWidget = nullptr; // верхние вкладки (Сцена/Редактор/Модели)
    QTextEdit *m_console = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    GLWidget *m_glWidget = nullptr;
    QTimer *m_modelTimer = nullptr;
    QTreeWidget *m_sceneTree = nullptr;
    QTreeWidget *m_projectTree = nullptr;
    QTabWidget *m_viewTabs = nullptr; // вкладки Scene/Game
    // Inspector
    QWidget *m_inspector = nullptr;
    QDoubleSpinBox *m_posX = nullptr; QDoubleSpinBox *m_posY = nullptr; QDoubleSpinBox *m_posZ = nullptr;
    QDoubleSpinBox *m_rotX = nullptr; QDoubleSpinBox *m_rotY = nullptr; QDoubleSpinBox *m_rotZ = nullptr;
    QDoubleSpinBox *m_sclX = nullptr; QDoubleSpinBox *m_sclY = nullptr; QDoubleSpinBox *m_sclZ = nullptr;
    QPushButton *m_colorBtn = nullptr;
};

#endif // MAINWINDOW_HPP
