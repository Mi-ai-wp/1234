#pragma once

#include <QMainWindow>

class GameWidget;

class GameWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit GameWindow(QWidget *parent = nullptr);

private:
    GameWidget *m_gameWidget = nullptr;
};