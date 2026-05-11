#include "gamewindow.h"
#include "gamewidget.h"

GameWindow::GameWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("猎杀多托雷"));
    setMinimumSize(800, 600);
    resize(1024, 768);

    m_gameWidget = new GameWidget(this);
    setCentralWidget(m_gameWidget);
}