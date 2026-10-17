#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

class PlayerWindow;

// `FeroPlayer.exe --self-test <media> <output-folder>`: plays a file, captures
// the window in several interface states and writes measurements as JSON.
class SelfTest : public QObject {
    Q_OBJECT
public:
    SelfTest(PlayerWindow *window, const QString &media, const QString &output);
    void start();
private:
    void step();
    void capture(const QString &name);
    PlayerWindow *window;
    QString media, output;
    int stage = 0;
    double idleMemory = 0;
    QJsonObject firstOpen;
};
