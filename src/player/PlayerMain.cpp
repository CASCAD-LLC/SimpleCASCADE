#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <vector>
#include <string>
#include <cmath>

struct Vertex { float x, y, z; };
struct Face { std::vector<int> indices; };

class RuntimeObject {
public:
    std::string name;
    float x=0,y=0,z=0, rx=0,ry=0,rz=0, sx=1,sy=1,sz=1;
    float r=0.75f,g=0.8f,b=1.0f;
    std::vector<Vertex> vertices; std::vector<Face> faces;
    void draw() const {
        glPushMatrix();
        glTranslatef(x,y,z);
        glRotatef(rx,1,0,0); glRotatef(ry,0,1,0); glRotatef(rz,0,0,1);
        glScalef(sx,sy,sz);
        glDisable(GL_COLOR_MATERIAL);
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glColor3f(r,g,b);
        glBegin(GL_TRIANGLES);
        for (const auto &f : faces) {
            if (f.indices.size() < 3) continue;
            for (size_t i=1;i+1<f.indices.size();++i) {
                int v0=f.indices[0], v1=f.indices[i], v2=f.indices[i+1];
                float ax = vertices[v1].x - vertices[v0].x;
                float ay = vertices[v1].y - vertices[v0].y;
                float az = vertices[v1].z - vertices[v0].z;
                float bx = vertices[v2].x - vertices[v0].x;
                float by = vertices[v2].y - vertices[v0].y;
                float bz = vertices[v2].z - vertices[v0].z;
                float nx = ay*bz - az*by;
                float ny = az*bx - ax*bz;
                float nz = ax*by - ay*bx;
                float len = std::sqrt(nx*nx+ny*ny+nz*nz);
                if (len>1e-6f){ nx/=len; ny/=len; nz/=len; }
                glNormal3f(nx,ny,nz);
                glVertex3f(vertices[v0].x, vertices[v0].y, vertices[v0].z);
                glVertex3f(vertices[v1].x, vertices[v1].y, vertices[v1].z);
                glVertex3f(vertices[v2].x, vertices[v2].y, vertices[v2].z);
            }
        }
        glEnd();
        glPopMatrix();
    }
};

static void loadObjText(const QString &objText, std::vector<Vertex> &verts, std::vector<Face> &faces) {
    verts.clear(); faces.clear();
    const auto lines = objText.split('\n');
    for (const auto &line : lines) {
        auto trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        auto parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        if (parts[0] == "v" && parts.size() >= 4) {
            verts.push_back({parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()});
        } else if (parts[0] == "f" && parts.size() >= 4) {
            Face f; f.indices.reserve(parts.size()-1);
            for (int i=1;i<parts.size();++i) {
                auto token = parts[i].split('/')[0];
                bool ok=false; int idx = token.toInt(&ok); if (!ok) continue;
                idx -= 1; if (idx>=0 && idx < (int)verts.size()) f.indices.push_back(idx);
            }
            if (f.indices.size()>=3) faces.push_back(std::move(f));
        }
    }
}

class RuntimeView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit RuntimeView(QWidget *parent=nullptr) : QOpenGLWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }
    void loadSceneFile(const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot open scene file");
            return;
        }
        auto doc = QJsonDocument::fromJson(f.readAll()); f.close();
        if (!doc.isObject()) return;
        m_objects.clear();
        auto arr = doc.object().value("objects").toArray();
        for (const auto &it : arr) {
            auto jo = it.toObject();
            RuntimeObject o; o.name = jo.value("name").toString("Object").toStdString();
            auto tr = jo.value("transform").toObject();
            auto pos = tr.value("pos").toArray();
            auto rot = tr.value("rot").toArray();
            auto scl = tr.value("scl").toArray();
            if (pos.size()==3){ o.x=pos[0].toDouble(); o.y=pos[1].toDouble(); o.z=pos[2].toDouble(); }
            if (rot.size()==3){ o.rx=rot[0].toDouble(); o.ry=rot[1].toDouble(); o.rz=rot[2].toDouble(); }
            if (scl.size()==3){ o.sx=scl[0].toDouble(); o.sy=scl[1].toDouble(); o.sz=scl[2].toDouble(); }
            auto color = jo.value("color").toArray();
            if (color.size()==3){ o.r=color[0].toDouble(); o.g=color[1].toDouble(); o.b=color[2].toDouble(); }
            auto mesh = jo.value("mesh_obj").toString();
            loadObjText(mesh, o.vertices, o.faces);
            m_objects.push_back(std::move(o));
        }
        update();
    }
protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0.1f, 0.12f, 0.16f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    }
    void resizeGL(int w, int h) override { glViewport(0,0,w,h); }
    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = float(std::max(1,width()))/std::max(1,height());
        float zNear=0.1f, zFar=1000.0f; float fov=60.0f; float f = 1.0f/std::tan((fov*M_PI/180.0f)/2.0f);
        float top = zNear / f; float right = top * aspect; glFrustum(-right, right, -top, top, zNear, zFar);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(-30.0f,1,0,0); glRotatef(-45.0f,0,1,0); glTranslatef(0,0,-8.0f);
        for (const auto &o : m_objects) o.draw();
    }
private:
    std::vector<RuntimeObject> m_objects;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QString scenePath;
    if (argc >= 2) scenePath = QString::fromLocal8Bit(argv[1]);
    if (scenePath.isEmpty()) scenePath = QFileDialog::getOpenFileName(nullptr, "Open Scene", QString(), "SimpleCASCADE Scene (*.scene)");
    if (scenePath.isEmpty()) return 0;
    RuntimeView view; view.resize(1024, 768); view.show();
    view.loadSceneFile(scenePath);
    return app.exec();
}

#include "moc_PlayerMain.cpp"
