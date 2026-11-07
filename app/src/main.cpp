// SynthPatch app: control the module over Bluetooth from Android (or a PC with Bluetooth).
//
// C++ does the Bluetooth and the protocol (BleLink, Device); QML draws the pages (qml/*.qml).
// The one Device object is handed to QML as the context property `device`, so every page can
// write device.live.temp_c or device.setParam("lfo1", "rate", 2).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "Device.h"

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName("SynthPatch");
  QQuickStyle::setStyle("Material");

  Device device;
  // --demo (or SYNTHPATCH_DEMO=1): a pretend ESP inside the app, to try the pages without hardware
  if (app.arguments().contains("--demo") || qEnvironmentVariableIsSet("SYNTHPATCH_DEMO")) device.useDemo();
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("device", &device);
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                   Qt::QueuedConnection);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  engine.loadFromModule("SynthPatch", "Main");
#else
  engine.load(QUrl(QStringLiteral("qrc:/SynthPatch/qml/Main.qml")));
#endif
  device.start();
  return app.exec();
}
