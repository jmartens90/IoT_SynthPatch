# SynthPatch app - Qt 6 Quick + Qt Bluetooth.
#
# Open this file in Qt Creator (File > Open File or Project) and pick a kit:
#   * "Desktop Qt 6.x"        runs on the PC (needs a Bluetooth adapter, good for trying the pages)
#   * "Android Qt 6.x arm64"  builds an .apk and installs it on the phone over USB
# Qt >= 6.5 is needed for Android (Bluetooth permission API); 6.8 LTS is recommended.
#
# CMake lesson: qt_add_executable makes the program, qt_add_qml_module packs the QML files into
# it (as resources, so the .apk carries them) under the import name "SynthPatch".
cmake_minimum_required(VERSION 3.16)
project(SynthPatchApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2 Bluetooth)
qt_standard_project_setup()

qt_add_executable(SynthPatchApp
    src/main.cpp
    src/BleLink.h src/BleLink.cpp
    src/Device.h src/Device.cpp
    src/DemoEsp.h src/DemoEsp.cpp
)

qt_add_qml_module(SynthPatchApp
    URI SynthPatch
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
        qml/Knob.qml
        qml/Keyboard.qml
        qml/SynthPage.qml
        qml/SeqPage.qml
        qml/PatchPage.qml
        qml/OutputsPage.qml
        qml/MonitorPage.qml
)

set_target_properties(SynthPatchApp PROPERTIES
    QT_ANDROID_PACKAGE_NAME be.jokke.synthpatch
    QT_ANDROID_TARGET_SDK_VERSION 34
    QT_ANDROID_VERSION_NAME 1.0
    QT_ANDROID_VERSION_CODE 1
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(SynthPatchApp PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Bluetooth)
