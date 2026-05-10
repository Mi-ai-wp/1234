#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>
#include <QPointF>
#include <QRectF>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QRadialGradient>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

constexpr float UNIT_PX = 120.0f;
constexpr float TILE_SIZE = 60.0f;

constexpr float LIGHT_RADIUS = 2.0f * UNIT_PX;
constexpr float INITIAL_LIGHT_ANGLE = 60.0f;
constexpr float MAX_LIGHT_ANGLE = 180.0f;
constexpr float LIGHT_ANGLE_INCREMENT = 30.0f;

constexpr float INITIAL_SPEED = 1.5f;
constexpr float MAX_SPEED = 5.5f;
constexpr float SPEED_INCREMENT = 1.0f;

constexpr float INITIAL_DAMAGE = 1.0f;
constexpr float MAX_DAMAGE = 3.0f;
constexpr float DAMAGE_INCREMENT = 1.0f;

constexpr float ATTACK_COOLDOWN = 2.5f;
constexpr float SPEED_BOOST_DURATION = 2.0f;
constexpr float SPEED_BOOST_COOLDOWN = 3.0f;
constexpr float ULT_DURATION = 2.5f;
constexpr float ULT_COOLDOWN = 20.0f;

constexpr int MAX_HP = 10;
constexpr float INVINCIBLE_DURATION = 0.5f;

constexpr int EXP_PER_LEVEL = 20;

constexpr float MONSTER_SPAWN_RADIUS = 5.0f * UNIT_PX;
constexpr float MONSTER_DESPAWN_RADIUS = 7.0f * UNIT_PX;

constexpr float ITEM_SPAWN_INTERVAL = 4.0f;
constexpr int ITEMS_PER_SPAWN = 2;

constexpr float PI = 3.14159265358979323846f;

enum class TileType { Ground, Dark, Cracked, Rune };
enum class GameState { Start, Playing, Upgrade, End };

struct Player
{
    float x = 0.0f, y = 0.0f;
    int hp = MAX_HP;
    float speed = INITIAL_SPEED;
    float lightAngle = INITIAL_LIGHT_ANGLE;
    float attackDamage = INITIAL_DAMAGE;

    float attackCooldownTimer = 0.0f;
    float speedBoostTimer = 0.0f;
    float speedBoostCooldownTimer = 0.0f;
    bool speedBoostActive = false;

    int ultCharges = 0;
    int ultChargesUsed = 0;
    float ultTimer = 0.0f;
    float ultCooldownTimer = 0.0f;
    bool ultActive = false;

    float invincibleTimer = 0.0f;

    int level = 1;
    int experience = 0;
    int killCount = 0;
};

struct Monster
{
    float x = 0.0f, y = 0.0f;
    float hp = 0.0f;
    float maxHp = 0.0f;
    float speed = 0.0f;
    float damage = 0.0f;
    int expReward = 0;
    int tier = 1;
    float attackTimer = 0.0f;
    float attackInterval = 2.0f;
    bool inLight = false;
};

struct Spike
{
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float damage = 0.0f;
    float lifetime = 0.0f;
    float maxLifetime = 2.0f;
};

struct Item
{
    float x = 0.0f, y = 0.0f;
    int type = 0;
    float radius = 22.0f;
};

struct UpgradeOption
{
    int type = 0;
    QString name;
    QString description;
};

class GameWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GameWidget(QWidget *parent = nullptr);

signals:
    void gameEnded(int survivalTime, int level, int killCount);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void gameLoop();

private:
    GameState m_state = GameState::Start;
    Player m_player;
    std::vector<Monster> m_monsters;
    std::vector<Spike> m_spikes;
    std::vector<Item> m_items;
    std::vector<UpgradeOption> m_upgradeOptions;

    QSet<int> m_pressedKeys;
    QPointF m_mousePos;

    QTimer *m_loopTimer = nullptr;
    QElapsedTimer m_gameTimer;
    qint64 m_lastFrameTime = 0;
    float m_survivalTime = 0.0f;

    float m_monsterSpawnTimer = 0.0f;
    float m_itemSpawnTimer = 0.0f;

    std::mt19937 m_rng;

    QPixmap m_lightOverlay;
    QPixmap m_glowOverlay;

    QRectF m_startButtonRect;
    QRectF m_returnButtonRect;

    void resetGame();
    void updateGame(float dt);
    void updatePlayer(float dt);
    void updateMonsters(float dt);
    void updateSpikes(float dt);
    void spawnMonsters();
    void spawnItems();
    void checkLevelUp();
    void generateUpgradeOptions();
    void applyUpgrade(int index);

    void renderGame(QPainter &painter);
    void renderMap(QPainter &painter);
    void renderItems(QPainter &painter);
    void renderMonsters(QPainter &painter);
    void renderSpikes(QPainter &painter);
    void renderPlayer(QPainter &painter);
    void renderLightOverlay(QPainter &painter);
    void renderHUD(QPainter &painter);
    void renderStartScreen(QPainter &painter);
    void renderEndScreen(QPainter &painter);
    void renderUpgradePanel(QPainter &painter);

    QPointF worldToScreen(float wx, float wy) const;
    bool isInLightCone(float wx, float wy) const;
    float distanceToPlayer(float wx, float wy) const;
    static TileType getTileType(int tx, int ty);
};