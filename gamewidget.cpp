#include "gamewidget.h"
#include <QCoreApplication>
#include <QDir>

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
    , m_rng(std::random_device{}())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(800, 600);
    setContextMenuPolicy(Qt::NoContextMenu);

    QString tpDir = QCoreApplication::applicationDirPath() + QStringLiteral("/../tp");
    if (!QDir(tpDir).exists())
        tpDir = QCoreApplication::applicationDirPath() + QStringLiteral("/../../tp");
    m_ultImage1.load(tpDir + QStringLiteral("/1.jpg"), "JPG");
    m_ultImage2.load(tpDir + QStringLiteral("/2.jpg"), "JPG");
    m_ultImage3.load(tpDir + QStringLiteral("/3.jpg"), "JPG");

    m_loopTimer = new QTimer(this);
    connect(m_loopTimer, &QTimer::timeout, this, &GameWidget::gameLoop);
    m_loopTimer->start(16);

    m_gameTimer.start();
    m_lastFrameTime = m_gameTimer.elapsed();

    m_mousePos = QPointF(width() / 2.0f + 50.0f, height() / 2.0f);
}

void GameWidget::resetGame()
{
    m_player = Player();
    m_monsters.clear();
    m_spikes.clear();
    m_items.clear();
    m_upgradeOptions.clear();
    m_survivalTime = 0.0f;
    m_monsterSpawnTimer = 0.0f;
    m_itemSpawnTimer = 0.0f;
    m_pressedKeys.clear();
    m_moveDirX = 1.0f;
    m_moveDirY = 0.0f;
    m_ultImageDisplayTier = 0;
    m_ultImageDisplayTimer = 0.0f;
}

void GameWidget::gameLoop()
{
    qint64 now = m_gameTimer.elapsed();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    m_lastFrameTime = now;

    if (dt > 0.1f)
        dt = 0.1f;

    if (m_state == GameState::Playing)
    {
        m_survivalTime += dt;
        updateGame(dt);
    }

    if (m_ultImageDisplayTimer > 0.0f)
    {
        m_ultImageDisplayTimer -= dt;
        if (m_ultImageDisplayTimer <= 0.0f)
            m_ultImageDisplayTier = 0;
    }

    update();
}

void GameWidget::updateGame(float dt)
{
    updatePlayer(dt);
    updateMonsters(dt);
    updateSpikes(dt);

    for (auto it = m_monsters.begin(); it != m_monsters.end();)
    {
        if (it->hp <= 0.0f)
        {
            m_player.experience += it->expReward;
            m_player.killCount++;
            it = m_monsters.erase(it);
        }
        else
        {
            ++it;
        }
    }

    checkLevelUp();

    m_monsterSpawnTimer += dt;
    float spawnInterval = std::max(0.5f, 2.0f - m_survivalTime / 120.0f);
    if (m_monsterSpawnTimer >= spawnInterval)
    {
        m_monsterSpawnTimer -= spawnInterval;
        spawnMonsters();
    }

    m_itemSpawnTimer += dt;
    if (m_itemSpawnTimer >= ITEM_SPAWN_INTERVAL)
    {
        m_itemSpawnTimer -= ITEM_SPAWN_INTERVAL;
        spawnItems();
    }
}

void GameWidget::updatePlayer(float dt)
{
    float inputX = 0.0f, inputY = 0.0f;
    if (m_pressedKeys.contains(Qt::Key_W) || m_pressedKeys.contains(Qt::Key_Up))
        inputY -= 1.0f;
    if (m_pressedKeys.contains(Qt::Key_S) || m_pressedKeys.contains(Qt::Key_Down))
        inputY += 1.0f;
    if (m_pressedKeys.contains(Qt::Key_A) || m_pressedKeys.contains(Qt::Key_Left))
        inputX -= 1.0f;
    if (m_pressedKeys.contains(Qt::Key_D) || m_pressedKeys.contains(Qt::Key_Right))
        inputX += 1.0f;

    float inputLen = std::sqrt(inputX * inputX + inputY * inputY);
    if (inputLen > 0.01f)
    {
        m_moveDirX = inputX / inputLen;
        m_moveDirY = inputY / inputLen;
    }

    float currentSpeed = m_player.speed;
    if (m_player.speedBoostActive)
        currentSpeed *= 2.0f;

    m_player.x += m_moveDirX * currentSpeed * UNIT_PX * dt;
    m_player.y += m_moveDirY * currentSpeed * UNIT_PX * dt;

    if (m_player.attackCooldownTimer > 0.0f)
        m_player.attackCooldownTimer -= dt;

    if (m_player.speedBoostActive)
    {
        m_player.speedBoostTimer -= dt;
        if (m_player.speedBoostTimer <= 0.0f)
        {
            m_player.speedBoostActive = false;
            m_player.speedBoostCooldownTimer = SPEED_BOOST_COOLDOWN;
        }
    }
    else if (m_player.speedBoostCooldownTimer > 0.0f)
    {
        m_player.speedBoostCooldownTimer -= dt;
    }

    if (m_player.ultActive)
    {
        m_player.ultTimer -= dt;
        if (m_player.ultTimer <= 0.0f)
        {
            m_player.ultActive = false;
            m_player.ultCooldownTimer = ULT_COOLDOWN;
        }
    }
    else if (m_player.ultCooldownTimer > 0.0f)
    {
        m_player.ultCooldownTimer -= dt;
    }

    if (m_player.invincibleTimer > 0.0f)
        m_player.invincibleTimer -= dt;

    bool shieldActive = m_player.ultActive && m_player.ultChargesUsed == 1;

    for (auto &m : m_monsters)
    {
        float dx = m.x - m_player.x;
        float dy = m.y - m_player.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float collisionDist = 20.0f;

        if (dist < collisionDist && m_player.invincibleTimer <= 0.0f)
        {
            if (!shieldActive)
            {
                m_player.hp -= static_cast<int>(m.damage);
                m_player.invincibleTimer = INVINCIBLE_DURATION;

                if (m_player.hp <= 0)
                {
                    m_player.hp = 0;
                    m_state = GameState::End;
                    emit gameEnded(static_cast<int>(m_survivalTime), m_player.level, m_player.killCount);
                    return;
                }
            }
        }
    }

    for (auto it = m_spikes.begin(); it != m_spikes.end();)
    {
        float dx = it->x - m_player.x;
        float dy = it->y - m_player.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 15.0f && m_player.invincibleTimer <= 0.0f)
        {
            if (!shieldActive)
            {
                m_player.hp -= static_cast<int>(it->damage);
                m_player.invincibleTimer = INVINCIBLE_DURATION;
            }
            it = m_spikes.erase(it);

            if (m_player.hp <= 0)
            {
                m_player.hp = 0;
                m_state = GameState::End;
                emit gameEnded(static_cast<int>(m_survivalTime), m_player.level, m_player.killCount);
                return;
            }
        }
        else
        {
            ++it;
        }
    }

    for (auto it = m_items.begin(); it != m_items.end();)
    {
        float dx = it->x - m_player.x;
        float dy = it->y - m_player.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < it->radius + 15.0f)
        {
            bool picked = false;
            if (it->type == 0)
            {
                if (m_player.ultCooldownTimer <= 0.0f && m_player.ultCharges < 3 && !m_player.ultActive)
                {
                    m_player.ultCharges++;
                    picked = true;
                }
            }
            else
            {
                if (m_player.hp < MAX_HP)
                {
                    m_player.hp++;
                    picked = true;
                }
            }
            if (picked)
                it = m_items.erase(it);
            else
                ++it;
        }
        else
        {
            ++it;
        }
    }
}

void GameWidget::updateMonsters(float dt)
{
    float tier1Speed = 2.0f + (m_survivalTime / 60.0f) * 1.0f;
    if (tier1Speed > 7.0f) tier1Speed = 7.0f;
    float tier2Speed = 1.5f + (m_survivalTime / 120.0f) * 1.5f;
    if (tier2Speed > 6.0f) tier2Speed = 6.0f;

    float lightDirAngle = std::atan2(m_mousePos.y() - height() / 2.0f, m_mousePos.x() - width() / 2.0f);
    float halfAngle = (m_player.lightAngle / 2.0f) * (PI / 180.0f);
    bool is360Light = m_player.ultActive && m_player.ultChargesUsed >= 3;

    for (auto &m : m_monsters)
    {
        if (m.tier == 1)
            m.speed = tier1Speed;
        else if (m.tier == 2)
            m.speed = tier2Speed;

        float dx = m.x - m_player.x;
        float dy = m.y - m_player.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        bool inLight = false;
        if (dist <= LIGHT_RADIUS)
        {
            if (is360Light)
            {
                inLight = true;
            }
            else
            {
                float angleToMonster = std::atan2(dy, dx);
                float diff = std::fmod(angleToMonster - lightDirAngle + PI, 2.0f * PI) - PI;
                if (std::fabs(diff) <= halfAngle)
                    inLight = true;
            }
        }
        m.inLight = inLight;

        if (m.tier <= 2 && !inLight && dist > 0.01f)
        {
            float dirX = m_player.x - m.x;
            float dirY = m_player.y - m.y;
            float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
            m.x += (dirX / dirLen) * m.speed * UNIT_PX * dt;
            m.y += (dirY / dirLen) * m.speed * UNIT_PX * dt;
        }

        if (m.tier == 3 && !inLight)
        {
            m.attackTimer += dt;
            if (m.attackTimer >= m.attackInterval && dist <= 3.0f * UNIT_PX)
            {
                m.attackTimer = 0.0f;
                Spike spike;
                spike.x = m.x;
                spike.y = m.y;
                float spikeSpeed = 300.0f;
                float angleToPlayer = std::atan2(m_player.y - m.y, m_player.x - m.x);
                spike.vx = std::cos(angleToPlayer) * spikeSpeed;
                spike.vy = std::sin(angleToPlayer) * spikeSpeed;
                spike.damage = m.damage;
                m_spikes.push_back(spike);
            }
        }
    }

    m_monsters.erase(std::remove_if(m_monsters.begin(), m_monsters.end(), [this](const Monster &m) {
        return distanceToPlayer(m.x, m.y) > MONSTER_DESPAWN_RADIUS;
    }), m_monsters.end());
}

void GameWidget::updateSpikes(float dt)
{
    for (auto &s : m_spikes)
    {
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        s.lifetime += dt;
    }

    m_spikes.erase(std::remove_if(m_spikes.begin(), m_spikes.end(), [](const Spike &s) {
        return s.lifetime >= s.maxLifetime;
    }), m_spikes.end());
}

void GameWidget::spawnMonsters()
{
    int count = 1 + static_cast<int>(m_survivalTime / 30.0f);
    count = std::min(count, 12);

    float tier2Chance = 0.0f;
    float tier3Chance = 0.0f;
    if (m_survivalTime > 30.0f)
        tier2Chance = std::min(0.35f, (m_survivalTime - 30.0f) / 300.0f);
    if (m_survivalTime > 120.0f)
        tier3Chance = std::min(0.25f, (m_survivalTime - 120.0f) / 600.0f);

    std::uniform_real_distribution<float> typeDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> radiusDist(3.5f * UNIT_PX, MONSTER_SPAWN_RADIUS);

    for (int i = 0; i < count; ++i)
    {
        float angle = angleDist(m_rng);
        float radius = radiusDist(m_rng);
        float mx = m_player.x + std::cos(angle) * radius;
        float my = m_player.y + std::sin(angle) * radius;

        float typeRoll = typeDist(m_rng);
        int tier = 1;
        if (typeRoll < tier3Chance)
            tier = 3;
        else if (typeRoll < tier2Chance + tier3Chance)
            tier = 2;

        Monster m;
        m.x = mx;
        m.y = my;
        m.tier = tier;

        switch (tier)
        {
        case 1:
            m.hp = 2.0f; m.maxHp = 2.0f;
            m.speed = 2.0f;
            m.damage = 2.0f;
            m.expReward = 2;
            break;
        case 2:
            m.hp = 3.0f; m.maxHp = 3.0f;
            m.speed = 1.5f;
            m.damage = 3.5f;
            m.expReward = 6;
            break;
        case 3:
            m.hp = 9.0f; m.maxHp = 9.0f;
            m.speed = 0.0f;
            m.damage = 5.0f;
            m.expReward = 10;
            m.attackTimer = 0.0f;
            m.attackInterval = 2.0f;
            break;
        }

        m_monsters.push_back(m);
    }
}

void GameWidget::spawnItems()
{
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> radiusDist(1.5f * UNIT_PX, 3.5f * UNIT_PX);
    std::uniform_int_distribution<int> typeDist(0, 1);

    for (int i = 0; i < ITEMS_PER_SPAWN; ++i)
    {
        float angle = angleDist(m_rng);
        float radius = radiusDist(m_rng);

        Item item;
        item.x = m_player.x + std::cos(angle) * radius;
        item.y = m_player.y + std::sin(angle) * radius;
        item.type = typeDist(m_rng);
        item.radius = 22.0f;

        m_items.push_back(item);
    }

    if (static_cast<int>(m_items.size()) > 20)
        m_items.erase(m_items.begin(), m_items.begin() + (m_items.size() - 20));
}

void GameWidget::checkLevelUp()
{
    while (m_player.experience >= EXP_PER_LEVEL)
    {
        m_player.experience -= EXP_PER_LEVEL;
        m_player.level++;
        generateUpgradeOptions();
        m_state = GameState::Upgrade;
        break;
    }
}

void GameWidget::generateUpgradeOptions()
{
    m_upgradeOptions.clear();

    std::vector<int> available;
    if (m_player.attackDamage < MAX_DAMAGE)
        available.push_back(0);
    if (m_player.lightAngle < MAX_LIGHT_ANGLE)
        available.push_back(1);
    if (m_player.speed < MAX_SPEED)
        available.push_back(2);

    if (available.empty())
        return;

    std::shuffle(available.begin(), available.end(), m_rng);
    int count = std::min(3, static_cast<int>(available.size()));

    for (int i = 0; i < count; ++i)
    {
        UpgradeOption opt;
        opt.type = available[i];
        switch (opt.type)
        {
        case 0:
            opt.name = QStringLiteral("技能攻击力 +1");
            opt.description = QStringLiteral("当前：%1 → %2").arg(m_player.attackDamage).arg(m_player.attackDamage + 1);
            break;
        case 1:
            opt.name = QStringLiteral("光照角度 +30°");
            opt.description = QStringLiteral("当前：%1° → %2°").arg(m_player.lightAngle).arg(m_player.lightAngle + 30);
            break;
        case 2:
            opt.name = QStringLiteral("移动速度 +1");
            opt.description = QStringLiteral("当前：%1 → %2").arg(m_player.speed).arg(m_player.speed + 1);
            break;
        }
        m_upgradeOptions.push_back(opt);
    }
}

void GameWidget::applyUpgrade(int index)
{
    if (index < 0 || index >= static_cast<int>(m_upgradeOptions.size()))
        return;

    switch (m_upgradeOptions[index].type)
    {
    case 0: m_player.attackDamage += DAMAGE_INCREMENT; break;
    case 1: m_player.lightAngle += LIGHT_ANGLE_INCREMENT; break;
    case 2: m_player.speed += SPEED_INCREMENT; break;
    }
}

void GameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    switch (m_state)
    {
    case GameState::Start:
        renderStartScreen(painter);
        break;
    case GameState::Playing:
    case GameState::Upgrade:
        renderGame(painter);
        if (m_state == GameState::Upgrade)
            renderUpgradePanel(painter);
        break;
    case GameState::End:
        renderEndScreen(painter);
        break;
    }
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    if (m_state == GameState::Playing)
    {
        if (event->key() == Qt::Key_F)
        {
            if (m_player.ultCharges > 0 && m_player.ultCooldownTimer <= 0.0f && !m_player.ultActive)
            {
                int charges = m_player.ultCharges;
                m_player.ultActive = true;
                m_player.ultTimer = ULT_DURATION;
                m_player.ultChargesUsed = charges;
                m_player.ultCharges = 0;

                m_ultImageDisplayTier = charges;
                m_ultImageDisplayTimer = 2.0f;

                if (charges == 2)
                {
                    for (auto &m : m_monsters)
                    {
                        if (distanceToPlayer(m.x, m.y) <= 3.0f * UNIT_PX)
                            m.hp -= 3.0f;
                    }
                }
            }
            return;
        }
    }

    m_pressedKeys.insert(event->key());
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;
    m_pressedKeys.remove(event->key());
}

void GameWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_state == GameState::Start)
    {
        if (m_startButtonRect.contains(event->pos()))
        {
            resetGame();
            m_state = GameState::Playing;
            m_gameTimer.restart();
            m_lastFrameTime = m_gameTimer.elapsed();
            m_survivalTime = 0.0f;
        }
        return;
    }

    if (m_state == GameState::End)
    {
        if (m_returnButtonRect.contains(event->pos()))
            m_state = GameState::Start;
        return;
    }

    if (m_state == GameState::Upgrade)
    {
        int w = width();
        int h = height();
        float optionW = 200.0f, optionH = 120.0f, spacing = 30.0f;
        float totalW = m_upgradeOptions.size() * optionW + (m_upgradeOptions.size() - 1) * spacing;
        float startX = (w - totalW) / 2.0f;
        float optionY = h / 2.0f - optionH / 2.0f;

        for (size_t i = 0; i < m_upgradeOptions.size(); ++i)
        {
            float ox = startX + i * (optionW + spacing);
            QRectF optionRect(ox, optionY, optionW, optionH);
            if (optionRect.contains(event->pos()))
            {
                applyUpgrade(static_cast<int>(i));
                m_state = GameState::Playing;
                return;
            }
        }
        return;
    }

    if (m_state == GameState::Playing)
    {
        if (event->button() == Qt::LeftButton)
        {
            bool noCooldown = m_player.ultActive && m_player.ultChargesUsed >= 3;
            if (noCooldown || m_player.attackCooldownTimer <= 0.0f)
            {
                for (auto &m : m_monsters)
                {
                    if (m.inLight)
                        m.hp -= m_player.attackDamage;
                }
                if (!noCooldown)
                    m_player.attackCooldownTimer = ATTACK_COOLDOWN;
            }
        }
        else if (event->button() == Qt::RightButton)
        {
            if (!m_player.speedBoostActive && m_player.speedBoostCooldownTimer <= 0.0f)
            {
                m_player.speedBoostActive = true;
                m_player.speedBoostTimer = SPEED_BOOST_DURATION;
            }
        }
    }
}

void GameWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
}

void GameWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_lightOverlay = QPixmap();
    m_glowOverlay = QPixmap();
}

QPointF GameWidget::worldToScreen(float wx, float wy) const
{
    return QPointF(wx - m_player.x + width() / 2.0f,
                   wy - m_player.y + height() / 2.0f);
}

bool GameWidget::isInLightCone(float wx, float wy) const
{
    float dx = wx - m_player.x;
    float dy = wy - m_player.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > LIGHT_RADIUS)
        return false;

    if (m_player.ultActive && m_player.ultChargesUsed >= 3)
        return true;

    float lightDirAngle = std::atan2(m_mousePos.y() - height() / 2.0f, m_mousePos.x() - width() / 2.0f);
    float angleToTarget = std::atan2(dy, dx);
    float diff = std::fmod(angleToTarget - lightDirAngle + PI, 2.0f * PI) - PI;
    float halfAngle = (m_player.lightAngle / 2.0f) * (PI / 180.0f);

    return std::fabs(diff) <= halfAngle;
}

float GameWidget::distanceToPlayer(float wx, float wy) const
{
    float dx = wx - m_player.x;
    float dy = wy - m_player.y;
    return std::sqrt(dx * dx + dy * dy);
}

TileType GameWidget::getTileType(int tx, int ty)
{
    uint32_t h = static_cast<uint32_t>(tx) * 374761393u + static_cast<uint32_t>(ty) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    int r = h % 100;
    if (r < 70) return TileType::Ground;
    if (r < 85) return TileType::Dark;
    if (r < 95) return TileType::Cracked;
    return TileType::Rune;
}

void GameWidget::renderGame(QPainter &painter)
{
    painter.fillRect(rect(), QColor(10, 10, 15));

    renderMap(painter);
    renderItems(painter);
    renderSpikes(painter);
    renderMonsters(painter);
    renderPlayer(painter);
    renderLightOverlay(painter);
    renderUltImage(painter);

    for (const auto &m : m_monsters)
    {
        if (m.inLight)
        {
            QPointF screen = worldToScreen(m.x, m.y);
            float radius = (m.tier == 3) ? 22.0f : (m.tier == 2) ? 16.0f : 12.0f;
            QPen highlightPen(QColor(255, 255, 100, 180), 3.0f);
            painter.setPen(highlightPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(screen, radius + 3.0f, radius + 3.0f);
        }
    }

    renderLightOverlay(painter);
    renderHUD(painter);
}

void GameWidget::renderMap(QPainter &painter)
{
    int screenW = width();
    int screenH = height();

    int startTX = static_cast<int>((m_player.x - screenW / 2.0f) / TILE_SIZE) - 1;
    int startTY = static_cast<int>((m_player.y - screenH / 2.0f) / TILE_SIZE) - 1;
    int endTX = static_cast<int>((m_player.x + screenW / 2.0f) / TILE_SIZE) + 1;
    int endTY = static_cast<int>((m_player.y + screenH / 2.0f) / TILE_SIZE) + 1;

    for (int tx = startTX; tx <= endTX; ++tx)
    {
        for (int ty = startTY; ty <= endTY; ++ty)
        {
            TileType type = getTileType(tx, ty);

            QColor color;
            switch (type)
            {
            case TileType::Ground:  color = QColor(40, 40, 45); break;
            case TileType::Dark:    color = QColor(25, 25, 30); break;
            case TileType::Cracked: color = QColor(55, 40, 25); break;
            case TileType::Rune:    color = QColor(35, 30, 55); break;
            }

            float wx = tx * TILE_SIZE;
            float wy = ty * TILE_SIZE;
            QPointF screen = worldToScreen(wx, wy);

            painter.fillRect(QRectF(screen.x(), screen.y(), TILE_SIZE, TILE_SIZE), color);

            painter.setPen(QPen(QColor(60, 60, 65, 80), 1.0f));
            painter.drawRect(QRectF(screen.x(), screen.y(), TILE_SIZE, TILE_SIZE));
        }
    }
}

void GameWidget::renderItems(QPainter &painter)
{
    for (const auto &item : m_items)
    {
        QPointF screen = worldToScreen(item.x, item.y);
        float radius = item.radius;

        if (item.type == 0)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 200, 50, 40));
            painter.drawEllipse(screen, radius + 5.0f, radius + 5.0f);

            painter.setBrush(QColor(255, 220, 50));
            painter.setPen(QPen(QColor(255, 180, 30), 2.0f));
            painter.drawEllipse(screen, radius, radius);

            painter.setPen(QPen(Qt::white, 2.0f));
            float s = radius * 0.5f;
            painter.drawLine(QPointF(screen.x() + s * 0.3f, screen.y() - s),
                             QPointF(screen.x() - s * 0.3f, screen.y()));
            painter.drawLine(QPointF(screen.x() - s * 0.3f, screen.y()),
                             QPointF(screen.x() + s * 0.3f, screen.y() + s));
        }
        else
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(50, 220, 100, 40));
            painter.drawEllipse(screen, radius + 5.0f, radius + 5.0f);

            painter.setBrush(QColor(50, 200, 80));
            painter.setPen(QPen(QColor(30, 180, 60), 2.0f));
            painter.drawEllipse(screen, radius, radius);

            painter.setPen(QPen(Qt::white, 3.0f));
            float s = radius * 0.5f;
            painter.drawLine(QPointF(screen.x(), screen.y() - s),
                             QPointF(screen.x(), screen.y() + s));
            painter.drawLine(QPointF(screen.x() - s, screen.y()),
                             QPointF(screen.x() + s, screen.y()));
        }
    }
}

void GameWidget::renderMonsters(QPainter &painter)
{
    for (const auto &m : m_monsters)
    {
        QPointF screen = worldToScreen(m.x, m.y);
        float radius;
        QColor bodyColor, borderColor;

        switch (m.tier)
        {
        case 1:
            radius = 12.0f;
            bodyColor = QColor(180, 50, 50);
            borderColor = QColor(220, 80, 80);
            break;
        case 2:
            radius = 16.0f;
            bodyColor = QColor(200, 100, 30);
            borderColor = QColor(240, 140, 50);
            break;
        case 3:
            radius = 22.0f;
            bodyColor = QColor(120, 40, 160);
            borderColor = QColor(160, 80, 200);
            break;
        default:
            radius = 12.0f;
            bodyColor = QColor(180, 50, 50);
            borderColor = QColor(220, 80, 80);
        }

        painter.setPen(QPen(borderColor, 2.0f));
        painter.setBrush(bodyColor);
        painter.drawEllipse(screen, radius, radius);

        float hpRatio = m.hp / m.maxHp;
        float barW = radius * 2.0f;
        float barH = 4.0f;
        float barY = screen.y() - radius - 8.0f;
        float barX = screen.x() - radius;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 40, 40));
        painter.drawRoundedRect(QRectF(barX, barY, barW, barH), 2.0, 2.0);
        painter.setBrush(QColor(200, 50, 50));
        painter.drawRoundedRect(QRectF(barX, barY, barW * hpRatio, barH), 2.0, 2.0);

        painter.setPen(Qt::white);
        QFont tierFont(QStringLiteral("Arial"), 8);
        painter.setFont(tierFont);
        painter.drawText(QRectF(screen.x() - radius, screen.y() - radius, radius * 2.0f, radius * 2.0f),
                         Qt::AlignCenter, QString::number(m.tier));
    }
}

void GameWidget::renderSpikes(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(200, 60, 60));

    for (const auto &s : m_spikes)
    {
        QPointF screen = worldToScreen(s.x, s.y);
        float angle = std::atan2(s.vy, s.vx);
        float size = 6.0f;

        QPointF p1(screen.x() + std::cos(angle) * size,
                   screen.y() + std::sin(angle) * size);
        QPointF p2(screen.x() + std::cos(angle + 2.4f) * size * 0.6f,
                   screen.y() + std::sin(angle + 2.4f) * size * 0.6f);
        QPointF p3(screen.x() + std::cos(angle - 2.4f) * size * 0.6f,
                   screen.y() + std::sin(angle - 2.4f) * size * 0.6f);

        QPolygonF triangle;
        triangle << p1 << p2 << p3;
        painter.drawPolygon(triangle);
    }
}

void GameWidget::renderPlayer(QPainter &painter)
{
    QPointF screen = worldToScreen(m_player.x, m_player.y);
    float radius = 15.0f;

    bool drawPlayer = true;
    if (m_player.invincibleTimer > 0.0f)
    {
        int flash = static_cast<int>(m_player.invincibleTimer * 20.0f) % 2;
        if (flash == 0)
            drawPlayer = false;
    }

    if (!drawPlayer)
        return;

    if (m_player.speedBoostActive)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(100, 200, 255, 60));
        painter.drawEllipse(screen, radius + 8.0f, radius + 8.0f);
    }

    if (m_player.ultActive && m_player.ultChargesUsed == 1)
    {
        painter.setPen(QPen(QColor(255, 255, 100, 180), 3.0f));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(screen, radius + 10.0f, radius + 10.0f);
    }

    if (m_player.ultActive && m_player.ultChargesUsed >= 3)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 200, 50, 30));
        painter.drawEllipse(screen, LIGHT_RADIUS, LIGHT_RADIUS);
    }

    painter.setPen(QPen(QColor(150, 200, 255), 2.0f));
    painter.setBrush(QColor(60, 120, 200));
    painter.drawEllipse(screen, radius, radius);

    float lightDirAngle = std::atan2(m_mousePos.y() - height() / 2.0f, m_mousePos.x() - width() / 2.0f);
    float indicatorLen = radius + 8.0f;
    QPointF tip(screen.x() + std::cos(lightDirAngle) * indicatorLen,
                screen.y() + std::sin(lightDirAngle) * indicatorLen);

    painter.setPen(QPen(QColor(255, 220, 100), 2.0f));
    painter.drawLine(screen, tip);

    painter.setBrush(QColor(255, 220, 100));
    painter.drawEllipse(tip, 3.0f, 3.0f);
}

void GameWidget::renderLightOverlay(QPainter &painter)
{
    int w = width();
    int h = height();

    if (m_lightOverlay.isNull() || m_lightOverlay.width() != w || m_lightOverlay.height() != h)
    {
        m_lightOverlay = QPixmap(w, h);
        m_glowOverlay = QPixmap(w, h);
    }

    m_lightOverlay.fill(QColor(0, 0, 0, 80));

    QPointF playerScreen = worldToScreen(m_player.x, m_player.y);

    {
        QPainter op(&m_lightOverlay);
        op.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        op.setRenderHint(QPainter::Antialiasing);
        op.setBrush(Qt::white);
        op.setPen(Qt::NoPen);

        bool is360 = m_player.ultActive && m_player.ultChargesUsed >= 3;

        if (is360)
        {
            op.drawEllipse(playerScreen, LIGHT_RADIUS, LIGHT_RADIUS);
        }
        else
        {
            float lightDirAngle = std::atan2(m_mousePos.y() - h / 2.0f, m_mousePos.x() - w / 2.0f);
            float halfAngle = (m_player.lightAngle / 2.0f) * (PI / 180.0f);

            QPainterPath conePath;
            conePath.moveTo(playerScreen);
            int segments = 48;
            for (int i = 0; i <= segments; ++i)
            {
                float a = lightDirAngle - halfAngle + (2.0f * halfAngle) * i / segments;
                conePath.lineTo(QPointF(playerScreen.x() + std::cos(a) * LIGHT_RADIUS,
                                         playerScreen.y() + std::sin(a) * LIGHT_RADIUS));
            }
            conePath.closeSubpath();
            op.drawPath(conePath);
        }
        op.end();
    }

    m_glowOverlay.fill(Qt::transparent);
    {
        QPainter gp(&m_glowOverlay);
        gp.setRenderHint(QPainter::Antialiasing);

        bool is360 = m_player.ultActive && m_player.ultChargesUsed >= 3;

        if (is360)
        {
            QRadialGradient gradient(playerScreen, LIGHT_RADIUS);
            gradient.setColorAt(0.0, QColor(255, 200, 100, 0));
            gradient.setColorAt(0.85, QColor(255, 180, 80, 0));
            gradient.setColorAt(0.95, QColor(255, 150, 50, 60));
            gradient.setColorAt(1.0, QColor(255, 100, 30, 0));
            gp.setBrush(gradient);
            gp.setPen(Qt::NoPen);
            gp.drawEllipse(playerScreen, LIGHT_RADIUS, LIGHT_RADIUS);
        }
        else
        {
            float lightDirAngle = std::atan2(m_mousePos.y() - h / 2.0f, m_mousePos.x() - w / 2.0f);
            float halfAngle = (m_player.lightAngle / 2.0f) * (PI / 180.0f);

            float glowRadius = LIGHT_RADIUS * 1.15f;
            QPainterPath glowPath;
            glowPath.moveTo(playerScreen);
            int segments = 48;
            for (int i = 0; i <= segments; ++i)
            {
                float a = lightDirAngle - halfAngle + (2.0f * halfAngle) * i / segments;
                glowPath.lineTo(QPointF(playerScreen.x() + std::cos(a) * glowRadius,
                                         playerScreen.y() + std::sin(a) * glowRadius));
            }
            glowPath.closeSubpath();

            QRadialGradient gradient(playerScreen, glowRadius);
            gradient.setColorAt(0.0, QColor(255, 200, 100, 0));
            gradient.setColorAt(0.7, QColor(255, 180, 80, 0));
            gradient.setColorAt(0.9, QColor(255, 150, 50, 40));
            gradient.setColorAt(1.0, QColor(255, 100, 30, 0));

            gp.setBrush(gradient);
            gp.setPen(Qt::NoPen);
            gp.drawPath(glowPath);
        }
        gp.end();
    }

    painter.drawPixmap(0, 0, m_lightOverlay);
    painter.drawPixmap(0, 0, m_glowOverlay);
}

void GameWidget::renderHUD(QPainter &painter)
{
    int w = width();
    int margin = 15;
    int y = 10;
    int barH = 16;
    int barW = 180;

    QFont hudFont(QStringLiteral("Arial"), 11);
    painter.setFont(hudFont);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 20, 20));
    painter.drawRoundedRect(QRectF(margin, y, barW, barH), 4.0, 4.0);
    float hpRatio = static_cast<float>(m_player.hp) / MAX_HP;
    painter.setBrush(QColor(200, 50, 50));
    painter.drawRoundedRect(QRectF(margin, y, barW * hpRatio, barH), 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(margin, y, barW, barH), Qt::AlignCenter,
                     QStringLiteral("HP: %1/%2").arg(m_player.hp).arg(MAX_HP));

    y += barH + 5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 60));
    painter.drawRoundedRect(QRectF(margin, y, barW, barH), 4.0, 4.0);
    float expRatio = static_cast<float>(m_player.experience) / EXP_PER_LEVEL;
    painter.setBrush(QColor(100, 100, 200));
    painter.drawRoundedRect(QRectF(margin, y, barW * expRatio, barH), 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(margin, y, barW, barH), Qt::AlignCenter,
                     QStringLiteral("Lv.%1  EXP: %2/%3").arg(m_player.level).arg(m_player.experience).arg(EXP_PER_LEVEL));

    int rightX = w - 250;
    y = 10;
    int lineH = 20;
    painter.setPen(QColor(200, 200, 200));

    int mins = static_cast<int>(m_survivalTime) / 60;
    int secs = static_cast<int>(m_survivalTime) % 60;
    painter.drawText(QRectF(rightX, y, 240, lineH), Qt::AlignRight,
                     QStringLiteral("时间: %1:%2  击杀: %3")
                         .arg(mins, 2, 10, QChar('0'))
                         .arg(secs, 2, 10, QChar('0'))
                         .arg(m_player.killCount));
    y += lineH;
    painter.drawText(QRectF(rightX, y, 240, lineH), Qt::AlignRight,
                     QStringLiteral("光照: %1°  速度: %2  伤害: %3")
                         .arg(m_player.lightAngle)
                         .arg(m_player.speed, 0, 'f', 1)
                         .arg(m_player.attackDamage, 0, 'f', 1));
    y += lineH;

    QString atkCD = (m_player.attackCooldownTimer > 0.0f)
                        ? QStringLiteral("攻击CD: %1s").arg(m_player.attackCooldownTimer, 0, 'f', 1)
                        : QStringLiteral("攻击: 就绪");
    painter.drawText(QRectF(rightX, y, 240, lineH), Qt::AlignRight, atkCD);
    y += lineH;

    QString spdCD;
    if (m_player.speedBoostActive)
        spdCD = QStringLiteral("加速中: %1s").arg(m_player.speedBoostTimer, 0, 'f', 1);
    else if (m_player.speedBoostCooldownTimer > 0.0f)
        spdCD = QStringLiteral("加速CD: %1s").arg(m_player.speedBoostCooldownTimer, 0, 'f', 1);
    else
        spdCD = QStringLiteral("加速: 就绪");
    painter.drawText(QRectF(rightX, y, 240, lineH), Qt::AlignRight, spdCD);
    y += lineH;

    QString ultStr;
    if (m_player.ultActive)
        ultStr = QStringLiteral("大招中: %1s").arg(m_player.ultTimer, 0, 'f', 1);
    else if (m_player.ultCooldownTimer > 0.0f)
        ultStr = QStringLiteral("大招CD: %1s").arg(m_player.ultCooldownTimer, 0, 'f', 1);
    else
        ultStr = QStringLiteral("大招: 就绪");

    QString charges;
    for (int i = 0; i < 3; ++i)
        charges += (i < m_player.ultCharges) ? QStringLiteral("◆") : QStringLiteral("◇");

    painter.drawText(QRectF(rightX, y, 240, lineH), Qt::AlignRight,
                     QStringLiteral("%1  充能: %2").arg(ultStr, charges));
}

void GameWidget::renderUltImage(QPainter &painter)
{
    if (m_ultImageDisplayTimer <= 0.0f || m_ultImageDisplayTier <= 0)
        return;

    QPixmap *pixmap = nullptr;
    switch (m_ultImageDisplayTier)
    {
    case 1: pixmap = &m_ultImage1; break;
    case 2: pixmap = &m_ultImage2; break;
    case 3: pixmap = &m_ultImage3; break;
    default: return;
    }

    if (!pixmap || pixmap->isNull())
        return;

    int w = width();
    int h = height();
    int margin = 20;
    int targetW = 350;

    QImage img = pixmap->toImage();
    QImage scaled = img.scaledToWidth(targetW, Qt::SmoothTransformation);
    int x = w - scaled.width() - margin;
    int y = h - scaled.height() - margin;

    painter.drawImage(x, y, scaled);
}

void GameWidget::renderStartScreen(QPainter &painter)
{
    painter.fillRect(rect(), QColor(15, 15, 25));

    int w = width();
    int h = height();

    QFont titleFont = painter.font();
    titleFont.setPointSize(36);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(255, 200, 100));
    painter.drawText(QRect(0, h / 8, w, 60), Qt::AlignCenter,
                     QStringLiteral("光影行者 Light Walker"));

    QFont instrFont = painter.font();
    instrFont.setPointSize(13);
    painter.setFont(instrFont);
    painter.setPen(QColor(200, 200, 200));

    int y = h / 4 + 20;
    int lineH = 28;
    QStringList instructions = {
        QStringLiteral("操作说明："),
        QStringLiteral("WASD / 方向键  —  移动"),
        QStringLiteral("鼠标移动  —  控制光照方向"),
        QStringLiteral("鼠标左键  —  攻击技能（对光照范围内所有怪物造成伤害）"),
        QStringLiteral("鼠标右键  —  加速技能（2秒内速度翻倍）"),
        QStringLiteral("F 键  —  大招（需拾取能量球充能，最多3格）"),
        QString(),
        QStringLiteral("拾取能量球为大招充能，拾取血包恢复生命值"),
        QStringLiteral("在黑暗中生存尽可能久！"),
    };

    for (const auto &line : instructions)
    {
        painter.drawText(QRect(w / 6, y, w * 2 / 3, lineH), Qt::AlignCenter, line);
        y += lineH;
    }

    float btnW = 220.0f, btnH = 50.0f;
    float btnX = (w - btnW) / 2.0f;
    float btnY = h * 3.0f / 4.0f;
    m_startButtonRect = QRectF(btnX, btnY, btnW, btnH);

    painter.setPen(QPen(QColor(200, 180, 100), 2.0f));
    painter.setBrush(QColor(50, 50, 60));
    painter.drawRoundedRect(m_startButtonRect, 10.0, 10.0);

    QFont btnFont = painter.font();
    btnFont.setPointSize(16);
    btnFont.setBold(true);
    painter.setFont(btnFont);
    painter.setPen(QColor(255, 220, 100));
    painter.drawText(m_startButtonRect, Qt::AlignCenter, QStringLiteral("点击开始游戏"));
}

void GameWidget::renderEndScreen(QPainter &painter)
{
    painter.fillRect(rect(), QColor(15, 15, 25));

    int w = width();
    int h = height();

    QFont titleFont = painter.font();
    titleFont.setPointSize(36);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(255, 100, 100));
    painter.drawText(QRect(0, h / 6, w, 60), Qt::AlignCenter, QStringLiteral("游戏结束"));

    QFont statFont = painter.font();
    statFont.setPointSize(16);
    painter.setFont(statFont);
    painter.setPen(QColor(220, 220, 220));

    int y = h / 3;
    int lineH = 40;

    painter.drawText(QRect(0, y, w, lineH), Qt::AlignCenter,
                     QStringLiteral("存活时间：%1 秒").arg(static_cast<int>(m_survivalTime)));
    y += lineH;
    painter.drawText(QRect(0, y, w, lineH), Qt::AlignCenter,
                     QStringLiteral("等级：%1").arg(m_player.level));
    y += lineH;
    painter.drawText(QRect(0, y, w, lineH), Qt::AlignCenter,
                     QStringLiteral("击杀数：%1").arg(m_player.killCount));

    float btnW = 180.0f, btnH = 50.0f;
    float btnX = (w - btnW) / 2.0f;
    float btnY = h * 2.0f / 3.0f;
    m_returnButtonRect = QRectF(btnX, btnY, btnW, btnH);

    painter.setPen(QPen(QColor(200, 180, 100), 2.0f));
    painter.setBrush(QColor(50, 50, 60));
    painter.drawRoundedRect(m_returnButtonRect, 10.0, 10.0);

    QFont btnFont = painter.font();
    btnFont.setPointSize(16);
    btnFont.setBold(true);
    painter.setFont(btnFont);
    painter.setPen(QColor(255, 220, 100));
    painter.drawText(m_returnButtonRect, Qt::AlignCenter, QStringLiteral("返回"));
}

void GameWidget::renderUpgradePanel(QPainter &painter)
{
    painter.fillRect(rect(), QColor(0, 0, 0, 160));

    int w = width();
    int h = height();

    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, h / 6, w, 50), Qt::AlignCenter,
                     QStringLiteral("升级！选择一个强化"));

    float optionW = 200.0f, optionH = 120.0f, spacing = 30.0f;
    float totalW = m_upgradeOptions.size() * optionW + (m_upgradeOptions.size() - 1) * spacing;
    float startX = (w - totalW) / 2.0f;
    float optionY = h / 2.0f - optionH / 2.0f;

    QFont optionFont = painter.font();
    optionFont.setPointSize(14);

    for (size_t i = 0; i < m_upgradeOptions.size(); ++i)
    {
        float ox = startX + i * (optionW + spacing);
        QRectF optionRect(ox, optionY, optionW, optionH);

        painter.setPen(QPen(QColor(200, 180, 100), 2.0f));
        painter.setBrush(QColor(40, 40, 50, 200));
        painter.drawRoundedRect(optionRect, 10.0, 10.0);

        painter.setPen(Qt::white);
        optionFont.setBold(true);
        painter.setFont(optionFont);
        painter.drawText(QRectF(ox, optionY + 10.0f, optionW, 30.0f), Qt::AlignCenter,
                         m_upgradeOptions[i].name);

        optionFont.setBold(false);
        optionFont.setPointSize(11);
        painter.setFont(optionFont);
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(QRectF(ox + 10.0f, optionY + 45.0f, optionW - 20.0f, optionH - 55.0f),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         m_upgradeOptions[i].description);
    }
}