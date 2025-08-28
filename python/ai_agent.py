# python/ai_agent.py — ИИ-агент для SimpleCASCADE
import sys
import asyncio
import httpx
import json
import os
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QTabWidget, QWidget, QVBoxLayout,
    QTextEdit, QPushButton, QLabel, QFileDialog, QHBoxLayout, QMessageBox
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtGui import QDrag
from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import *
from OpenGL.GLU import gluPerspective

# 🔐 Настройки
OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"
DEFAULT_MODEL = "qwen/qwen-turbo"
# Пользовательский конфиг в домашней директории
SETTINGS_PATH = os.path.join(os.path.expanduser("~"), ".config", "SimpleCASCADE", "settings.json")
TEMP_OBJ_PATH = os.path.join(os.path.dirname(__file__), "temp_model.obj")
FLAG_PATH = os.path.join(os.path.dirname(__file__), "model_ready.flag")


class AsyncWorker(QThread):
    finished = pyqtSignal(str)

    def __init__(self, prompt, model=DEFAULT_MODEL, system_prompt=""):
        super().__init__()
        self.prompt = prompt
        self.model = model
        self.system_prompt = system_prompt

    async def ask_model(self) -> str:
        api_key = self.get_api_key()
        headers = {"HTTP-Referer": "http://localhost", "X-Title": "SimpleCASCADE"}
        if api_key:
            headers["Authorization"] = f"Bearer {api_key}"

        messages = []
        if self.system_prompt:
            messages.append({"role": "system", "content": self.system_prompt})
        messages.append({"role": "user", "content": self.prompt})

        payload = {
            "model": self.model,
            "messages": messages,
            "temperature": 0.7,
            "max_tokens": 2048
        }

        async with httpx.AsyncClient(timeout=30.0) as client:
            try:
                response = await client.post(OPENROUTER_URL, json=payload, headers=headers)
                if response.status_code == 200:
                    return response.json()["choices"][0]["message"]["content"]
                else:
                    return f"❌ Ошибка {response.status_code}: {response.text}"
            except Exception as e:
                return f"❌ Исключение: {str(e)}"

    def run(self):
        result = asyncio.run(self.ask_model())
        self.finished.emit(result)

    def get_api_key(self):
        try:
            # 1) Переменная окружения имеет приоритет
            env_key = os.environ.get("OPENROUTER_API_KEY", "").strip()
            if env_key:
                return env_key
            # 2) Файл настроек пользователя
            if os.path.exists(SETTINGS_PATH):
                with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
                    settings = json.load(f)
                return settings.get("api_key", "").strip()
        except Exception:
            pass
        return ""


class OBJPreviewWidget(QOpenGLWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.vertices = []
        self.faces = []
        self.xRot = 0.0
        self.yRot = 0.0
        self.lastPos = None
        self.setMinimumSize(400, 400)

    def load_obj(self, obj_text):
        self.vertices = []
        self.faces = []

        if not obj_text.strip():
            self.update()
            return

        for line in obj_text.strip().splitlines():
            parts = line.strip().split()
            if not parts:
                continue
            if parts[0] == 'v' and len(parts) >= 4:
                try:
                    x, y, z = map(float, parts[1:4])
                    self.vertices.append((x, y, z))
                except:
                    continue
            elif parts[0] == 'f':
                face = []
                for p in parts[1:]:
                    idx_str = p.split('/')[0]
                    if idx_str.isdigit():
                        idx = int(idx_str) - 1
                        if 0 <= idx < len(self.vertices):
                            face.append(idx)
                if len(face) >= 3:
                    self.faces.append(face)
        self.update()

    def initializeGL(self):
        glClearColor(0.1, 0.12, 0.16, 1.0)
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_CULL_FACE)

    def resizeGL(self, w, h):
        glViewport(0, 0, w, h)

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45.0, self.width() / self.height(), 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        glTranslatef(0.0, 0.0, -5.0)
        glRotatef(self.xRot, 1.0, 0.0, 0.0)
        glRotatef(self.yRot, 0.0, 1.0, 0.0)

        # Сетка
        glColor3f(0.3, 0.3, 0.3)
        glBegin(GL_LINES)
        size = 10.0
        step = 1.0
        for i in range(int(-size), int(size) + 1):
            glVertex3f(i, 0, -size)
            glVertex3f(i, 0,  size)
            glVertex3f(-size, 0, i)
            glVertex3f( size, 0, i)
        glEnd()

        # Оси
        glBegin(GL_LINES)
        glColor3f(1.0, 0.0, 0.0); glVertex3f(0,0,0); glVertex3f(5,0,0)
        glColor3f(0.0, 1.0, 0.0); glVertex3f(0,0,0); glVertex3f(0,5,0)
        glColor3f(0.0, 0.0, 1.0); glVertex3f(0,0,0); glVertex3f(0,0,5)
        glEnd()

        if not self.vertices or not self.faces:
            return

        # Модель
        glColor3f(0.8, 0.8, 1.0)
        for face in self.faces:
            if len(face) < 3:
                continue
            glBegin(GL_LINE_LOOP)
            for idx in face:
                v = self.vertices[idx]
                glVertex3f(v[0], v[1], v[2])
            glEnd()

    def mousePressEvent(self, ev):
        self.lastPos = ev.position()

    def mouseMoveEvent(self, ev):
        if self.lastPos is None:
            return
        dx = ev.position().x() - self.lastPos.x()
        dy = ev.position().y() - self.lastPos.y()
        self.xRot += dy * 0.5
        self.yRot += dx * 0.5
        self.lastPos = ev.position()
        self.update()


class AIWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SimpleCASCADE — ИИ-агент")
        self.resize(900, 700)

        self.setStyleSheet("""
            QMainWindow, QWidget {
                background-color: #1e1e1e;
                color: #dcdcdc;
                font-family: 'Segoe UI', Arial, sans-serif;
            }
            QTextEdit {
                background-color: #2d2d2d;
                color: #ffffff;
                border: 1px solid #444;
                padding: 8px;
            }
            QPushButton {
                background-color: #007acc;
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 6px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #005a9e;
            }
            QLabel {
                color: #bbbbbb;
            }
            QTabWidget::pane {
                border: 1px solid #444;
                border-radius: 4px;
            }
            QTabBar::tab {
                background: #2d2d2d;
                color: #dcdcdc;
                padding: 10px 20px;
                margin: 2px;
                border: 1px solid #444;
                border-bottom: none;
                border-radius: 6px 6px 0 0;
            }
            QTabBar::tab:selected {
                background: #1e1e1e;
                border-bottom: 1px solid #1e1e1e;
            }
        """)

        self.setup_tabs()

    def setup_tabs(self):
        tabs = QTabWidget()
        tabs.addTab(self.create_text_tab(), "Текст")
        tabs.addTab(self.create_code_tab(), "Код")
        tabs.addTab(self.create_mesh_tab(), "3D Модель")
        tabs.addTab(self.create_settings_tab(), "Настройки")
        self.setCentralWidget(tabs)

    def create_text_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()

        self.text_prompt = QTextEdit()
        self.text_prompt.setPlaceholderText("Задайте вопрос ИИ...")

        btn_layout = QHBoxLayout()
        self.text_generate_btn = QPushButton("Спросить")
        self.text_save_btn = QPushButton("Сохранить")
        btn_layout.addWidget(self.text_generate_btn)
        btn_layout.addWidget(self.text_save_btn)

        self.text_output = QTextEdit()
        self.text_output.setReadOnly(True)

        layout.addWidget(QLabel("Запрос:"))
        layout.addWidget(self.text_prompt)
        layout.addLayout(btn_layout)
        layout.addWidget(QLabel("Ответ:"))
        layout.addWidget(self.text_output)

        tab.setLayout(layout)

        self.text_generate_btn.clicked.connect(self.on_generate_text)
        self.text_save_btn.clicked.connect(self.on_save_text)

        return tab

    def on_generate_text(self):
        prompt = self.text_prompt.toPlainText().strip()
        if not prompt:
            return
        self.text_generate_btn.setText("Генерирую...")
        self.text_generate_btn.setEnabled(False)

        system_prompt = self.get_setting("text_prompt", "Ты — ИИ-ассистент SimpleCASCADE. Отвечай кратко, ясно, по делу.")

        self.worker = AsyncWorker(prompt, "qwen/qwen-turbo", system_prompt)
        self.worker.finished.connect(self.on_text_generated)
        self.worker.start()

    def on_text_generated(self, result):
        self.text_generate_btn.setText("Спросить")
        self.text_generate_btn.setEnabled(True)
        self.text_output.setPlainText(result)

    def on_save_text(self):
        text = self.text_output.toPlainText()
        if not text:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить текст", "", "Text Files (*.txt);;All Files (*)"
        )
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)

    def create_code_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()

        self.code_prompt = QTextEdit()
        self.code_prompt.setPlaceholderText("Опишите, какой код нужен...")

        btn_layout = QHBoxLayout()
        self.code_generate_btn = QPushButton("Сгенерировать")
        self.code_save_btn = QPushButton("Сохранить в файл")
        btn_layout.addWidget(self.code_generate_btn)
        btn_layout.addWidget(self.code_save_btn)

        self.code_output = QTextEdit()
        self.code_output.setReadOnly(True)

        layout.addWidget(QLabel("Запрос:"))
        layout.addWidget(self.code_prompt)
        layout.addLayout(btn_layout)
        layout.addWidget(QLabel("Код:"))
        layout.addWidget(self.code_output)

        tab.setLayout(layout)

        self.code_generate_btn.clicked.connect(self.on_generate_code)
        self.code_save_btn.clicked.connect(self.on_save_code)

        return tab

    def on_generate_code(self):
        prompt = self.code_prompt.toPlainText().strip()
        if not prompt:
            return
        full_prompt = f"Сгенерируй C++ код. Только код, без объяснений.\n{prompt}"
        self.code_generate_btn.setText("Генерирую...")
        self.code_generate_btn.setEnabled(False)

        system_prompt = self.get_setting("code_prompt", "Ты — генератор C++ кода. Выводи ТОЛЬКО код, без объяснений.")

        self.worker = AsyncWorker(full_prompt, "qwen/qwen-turbo", system_prompt)
        self.worker.finished.connect(self.on_code_generated)
        self.worker.start()

    def on_code_generated(self, result):
        self.code_generate_btn.setText("Сгенерировать")
        self.code_generate_btn.setEnabled(True)
        self.code_output.setPlainText(result)

    def on_save_code(self):
        text = self.code_output.toPlainText()
        if not text:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить код", "", "C++ Files (*.hpp *.cpp);;All Files (*)"
        )
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)

    def create_mesh_tab(self):
        tab = QWidget()
        layout = QHBoxLayout()

        # Левая часть — OBJ-код
        left_layout = QVBoxLayout()
        left_layout.addWidget(QLabel("Запрос:"))
        self.mesh_prompt = QTextEdit()
        self.mesh_prompt.setPlaceholderText("Опишите 3D-объект...")
        self.mesh_prompt.setMaximumHeight(80)
        left_layout.addWidget(self.mesh_prompt)

        self.mesh_generate_btn = QPushButton("Сгенерировать OBJ")
        left_layout.addWidget(self.mesh_generate_btn)

        self.mesh_output = QTextEdit()
        self.mesh_output.setPlaceholderText("OBJ-код появится здесь...")
        left_layout.addWidget(self.mesh_output)

        self.mesh_save_btn = QPushButton("Сохранить как .obj")
        left_layout.addWidget(self.mesh_save_btn)

        self.insert_btn = QPushButton("Добавить в сцену")
        left_layout.addWidget(self.insert_btn)

        self.drag_btn = QPushButton("Перетащить в сцену/редактор")
        left_layout.addWidget(self.drag_btn)

        left_widget = QWidget()
        left_widget.setLayout(left_layout)
        left_widget.setFixedWidth(400)
        layout.addWidget(left_widget)

        # Правая часть — 3D-превью
        self.preview_widget = OBJPreviewWidget()
        layout.addWidget(self.preview_widget)

        tab.setLayout(layout)

        self.mesh_generate_btn.clicked.connect(self.on_generate_mesh)
        self.mesh_save_btn.clicked.connect(self.on_save_mesh)
        self.insert_btn.clicked.connect(self.insert_to_main_scene)
        self.drag_btn.pressed.connect(self.on_drag_mesh)
        self.mesh_output.textChanged.connect(
            lambda: self.preview_widget.load_obj(self.mesh_output.toPlainText())
        )

        return tab

    def on_generate_mesh(self):
        prompt = self.mesh_prompt.toPlainText().strip()
        if not prompt:
            return
        full_prompt = f"Сгенерируй 3D-модель в формате Wavefront OBJ.\nОписание: {prompt}\nВыведи ТОЛЬКО OBJ-код: v, f, vn. Без комментариев."
        self.mesh_generate_btn.setText("Генерирую...")
        self.mesh_generate_btn.setEnabled(False)

        system_prompt = self.get_setting("mesh_prompt", "Ты — генератор 3D-моделей в формате OBJ. Выводи ТОЛЬКО OBJ-код: v, f, vn. Без комментариев.")

        self.worker = AsyncWorker(full_prompt, "qwen/qwen-turbo", system_prompt)
        self.worker.finished.connect(self.on_mesh_generated)
        self.worker.start()

    def on_mesh_generated(self, result):
        self.mesh_generate_btn.setText("Сгенерировать OBJ")
        self.mesh_generate_btn.setEnabled(True)
        self.mesh_output.setPlainText(result)

    def on_save_mesh(self):
        text = self.mesh_output.toPlainText()
        if not text:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить 3D-модель", "", "OBJ Files (*.obj);;All Files (*)"
        )
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)

    def insert_to_main_scene(self):
        obj_data = self.mesh_output.toPlainText().strip()
        if not obj_data:
            QMessageBox.warning(self, "Ошибка", "Нет данных для вставки")
            return
        try:
            with open(TEMP_OBJ_PATH, "w", encoding="utf-8") as f:
                f.write(obj_data)
            with open(FLAG_PATH, "w") as f:
                f.write("insert")
            QMessageBox.information(self, "Готово", "Модель отправлена в сцену")
        except Exception as e:
            QMessageBox.critical(self, "Ошибка", f"Не удалось отправить: {e}")

    def on_drag_mesh(self):
        obj_data = self.mesh_output.toPlainText().strip()
        if not obj_data:
            return
        drag = QDrag(self)
        mime = self.mesh_output.createMimeDataFromSelection()
        # Ensure full content is used
        mime.setText(obj_data)
        drag.setMimeData(mime)
        drag.exec(Qt.DropAction.CopyAction)

    def create_settings_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("API Ключ (OpenRouter):"))
        self.api_key_input = QTextEdit()
        self.api_key_input.setPlaceholderText("Введите ваш API-ключ (опционально)")
        self.api_key_input.setFixedHeight(60)
        layout.addWidget(self.api_key_input)

        layout.addWidget(QLabel("Системный промт для текста:"))
        self.text_prompt_input = QTextEdit()
        self.text_prompt_input.setPlainText(
            "Ты — ИИ-ассистент SimpleCASCADE. Отвечай кратко, ясно, по делу."
        )
        self.text_prompt_input.setFixedHeight(80)
        layout.addWidget(self.text_prompt_input)

        layout.addWidget(QLabel("Системный промт для кода:"))
        self.code_prompt_input = QTextEdit()
        self.code_prompt_input.setPlainText(
            "Ты — генератор C++ кода. Выводи ТОЛЬКО код, без объяснений. Используй современный C++20."
        )
        self.code_prompt_input.setFixedHeight(80)
        layout.addWidget(self.code_prompt_input)

        layout.addWidget(QLabel("Системный промт для 3D-моделей:"))
        self.mesh_prompt_input = QTextEdit()
        self.mesh_prompt_input.setPlainText(
            "Ты — генератор 3D-моделей в формате OBJ. Выводи ТОЛЬКО OBJ-код: v, f, vn. Без комментариев."
        )
        self.mesh_prompt_input.setFixedHeight(80)
        layout.addWidget(self.mesh_prompt_input)

        btn_layout = QHBoxLayout()
        self.save_settings_btn = QPushButton("Сохранить")
        self.reset_settings_btn = QPushButton("Сбросить к стандартным")
        btn_layout.addWidget(self.save_settings_btn)
        btn_layout.addWidget(self.reset_settings_btn)
        layout.addLayout(btn_layout)

        tab.setLayout(layout)

        self.save_settings_btn.clicked.connect(self.save_settings)
        self.reset_settings_btn.clicked.connect(self.reset_settings)
        self.load_settings()

        return tab

    def get_setting(self, key, default):
        try:
            if os.path.exists(SETTINGS_PATH):
                with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
                    settings = json.load(f)
                return settings.get(key, default)
        except Exception:
            pass
        return default

    def load_settings(self):
        try:
            if os.path.exists(SETTINGS_PATH):
                with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
                    settings = json.load(f)
                self.api_key_input.setPlainText(settings.get("api_key", ""))
                self.text_prompt_input.setPlainText(settings.get("text_prompt", ""))
                self.code_prompt_input.setPlainText(settings.get("code_prompt", ""))
                self.mesh_prompt_input.setPlainText(settings.get("mesh_prompt", ""))
        except Exception as e:
            print(f"❌ Ошибка загрузки настроек: {e}")

    def save_settings(self):
        settings = {
            "api_key": self.api_key_input.toPlainText().strip(),
            "text_prompt": self.text_prompt_input.toPlainText(),
            "code_prompt": self.code_prompt_input.toPlainText(),
            "mesh_prompt": self.mesh_prompt_input.toPlainText()
        }
        try:
            os.makedirs(os.path.dirname(SETTINGS_PATH), exist_ok=True)
            with open(SETTINGS_PATH, "w", encoding="utf-8") as f:
                json.dump(settings, f, indent=4, ensure_ascii=False)
            QMessageBox.information(self, "Готово", "Настройки сохранены.")
        except Exception as e:
            QMessageBox.critical(self, "Ошибка", f"Не удалось сохранить: {e}")

    def reset_settings(self):
        reply = QMessageBox.question(
            self, "Сброс", "Сбросить все промты к стандартным?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.text_prompt_input.setPlainText(
                "Ты — ИИ-ассистент SimpleCASCADE. Отвечай кратко, ясно, по делу."
            )
            self.code_prompt_input.setPlainText(
                "Ты — генератор C++ кода. Выводи ТОЛЬКО код, без объяснений. Используй современный C++20."
            )
            self.mesh_prompt_input.setPlainText(
                "Ты — генератор 3D-моделей в формате OBJ. Выводи ТОЛЬКО OBJ-код: v, f, vn. Без комментариев."
            )
            self.api_key_input.clear()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = AIWindow()
    window.show()
    sys.exit(app.exec())
