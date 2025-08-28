cmake_minimum_required(VERSION 3.20)
project(SimpleCASCADE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Поиск основных компонентов Qt6 ===
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

# === Генераторы Qt ===
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# === Директория Python для интеграции ИИ ===
add_compile_definitions(PYTHON_DIR="${CMAKE_SOURCE_DIR}/python")
add_compile_definitions(SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# === Создаём исполняемый файл ===
add_executable(SimpleCASCADE
    main.cpp
    src/core/Engine3D.cpp
    src/ui/MainWindow.cpp
    src/ui/CodeEditor.cpp
    src/utils/FileHelper.cpp
    resources.qrc
)

# === Подключаем библиотеки ===
target_link_libraries(SimpleCASCADE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# === Include directories ===
target_include_directories(SimpleCASCADE PRIVATE src)

# === Копируем иконки в build/ ===
add_custom_command(
    TARGET SimpleCASCADE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/icons"
        "${CMAKE_BINARY_DIR}/icons"
    COMMENT "✅ Копирование icons/ в build/"
)

# === Отладка ===
message(STATUS "✅ Qt6 найден: Core, Widgets, OpenGL, OpenGLWidgets")

# === Player target (runtime build) ===
add_executable(Player
    src/player/PlayerMain.cpp
)

target_link_libraries(Player
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_include_directories(Player PRIVATE src)