#include "gamewidget.h"

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
    , m_rng(std::random_device{}())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(800, 600);
    setContextMenuPolicy(Qt::NoContextMenu);

    m_ultImage1.load(QStringLiteral(":/images/tp/1.jpg"));
    m_ultImage2.load(QStringLiteral(":/images/tp/2.jpg"));
    m_ultImage3.load(QStringLiteral(":/images/tp/3.jpg"));

    m_monsterImage1.load(QStringLiteral(":/images/tp/8.jpg"));
    m_monsterImage2.load(QStringLiteral(":/images/tp/9.jpg"));
    m_monsterImage3.load(QStringLiteral(":/images/tp/10.jpg"));

    m_charKongLeft.load(QStringLiteral(":/images/tp/6.jpg"));
    m_charKongRight.load(QStringLiteral(":/images/tp/7.jpg"));
    m_charYingLeft.load(QStringLiteral(":/images/tp/4.jpg"));
    m_charYingRight.load(QStringLiteral(":/images/tp/5.jpg"));

    m_startBg.load(QStringLiteral(":/images/tp/11.jpg"));

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
    m_ultUpgradeLevel = 0;
    m_spikeUpgradeLevel = 0;
    m_gameElapsedTime = 0.0f;
    m_nextSpikeUpgradeTime = 120.0f;
    m_paused = false;
}

void GameWidget::gameLoop()
{
    qint64 now = m_gameTimer.elapsed();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    m_lastFrameTime = now;

    if (dt > 0.1f)
        dt = 0.1f;

    if (m_state == GameState::Playing && !m_paused)
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
    m_gameElapsedTime += dt;
    if (m_spikeUpgradeLevel < 3 && m_gameElapsedTime >= m_nextSpikeUpgradeTime)
    {
        m_spikeUpgradeLevel++;
        m_nextSpikeUpgradeTime += 120.0f;
    }

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
    float cappedTime = std::min(m_survivalTime, 360.0f);
    float spawnInterval = std::max(0.8f, 3.0f - cappedTime / 180.0f);
    if (m_monsterSpawnTimer >= spawnInterval)
    {
        m_monsterSpawnTimer -= spawnInterval;
        spawnMonsters();
    }

    m_itemSpawnTimer += dt;
    float itemMult = 1.0f;
    if (m_survivalTime > 180.0f)
        itemMult = 1.5f;
    if (m_survivalTime > 360.0f)
        itemMult = 2.25f;
    float itemInterval = ITEM_SPAWN_INTERVAL / itemMult;
    if (m_itemSpawnTimer >= itemInterval)
    {
        m_itemSpawnTimer -= itemInterval;
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
            float cd = ULT_COOLDOWN - m_ultUpgradeLevel * 2.0f;
            if (cd < 2.0f) cd = 2.0f;
            m_player.ultCooldownTimer = cd;
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
                if (m_player.ultCharges < 3 && !m_player.ultActive)
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
        float effRadius = effectiveLightRadius();
        if (dist <= effRadius)
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

        if (inLight)
            m.timeOutsideView = 0.0f;
        else
            m.timeOutsideView += dt;

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
                int spikeCount = 1 + m_spikeUpgradeLevel;
                float spikeSpeed = 300.0f + m_spikeUpgradeLevel * 60.0f;
                float baseAngle = std::atan2(m_player.y - m.y, m_player.x - m.x);

                for (int s = 0; s < spikeCount; ++s)
                {
                    Spike spike;
                    spike.x = m.x;
                    spike.y = m.y;
                    float spreadAngle = baseAngle;
                    if (spikeCount > 1)
                    {
                        float spread = 0.15f * (s - (spikeCount - 1) / 2.0f);
                        spreadAngle = baseAngle + spread;
                    }
                    spike.vx = std::cos(spreadAngle) * spikeSpeed;
                    spike.vy = std::sin(spreadAngle) * spikeSpeed;
                    spike.damage = m.damage;
                    m_spikes.push_back(spike);
                }
            }
        }
    }

    m_monsters.erase(std::remove_if(m_monsters.begin(), m_monsters.end(), [this](const Monster &m) {
        return distanceToPlayer(m.x, m.y) > MONSTER_DESPAWN_RADIUS || m.timeOutsideView > 30.0f;
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
        tier2Chance = std::min(0.40f, (m_survivalTime - 30.0f) / 250.0f);
    if (m_survivalTime > 120.0f)
        tier3Chance = std::min(0.25f, (m_survivalTime - 120.0f) / 600.0f);

    std::uniform_real_distribution<float> typeDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> radiusDist(LIGHT_RADIUS + 1.5f * UNIT_PX, MONSTER_SPAWN_RADIUS);

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
            m.damage = 1.5f;
            m.expReward = 2;
            break;
        case 2:
            m.hp = 3.0f; m.maxHp = 3.0f;
            m.speed = 1.5f;
            m.damage = 2.5f;
            m.expReward = 6;
            break;
        case 3:
            m.hp = 9.0f; m.maxHp = 9.0f;
            m.speed = 0.0f;
            m.damage = 4.0f;
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
    std::uniform_int_distribution<int> typeDist(0, 1);

    float minDist = 44.0f;
    float halfW = width() / 2.0f - 30.0f;
    float halfH = height() / 2.0f - 30.0f;
    std::uniform_real_distribution<float> xDist(-halfW, halfW);
    std::uniform_real_distribution<float> yDist(-halfH, halfH);

    for (int attempt = 0; attempt < 20; ++attempt)
    {
        Item item;
        item.x = m_player.x + xDist(m_rng);
        item.y = m_player.y + yDist(m_rng);
        item.type = typeDist(m_rng);
        item.radius = 22.0f;

        bool overlap = false;
        for (const auto &existing : m_items)
        {
            float dx = item.x - existing.x;
            float dy = item.y - existing.y;
            if (std::sqrt(dx * dx + dy * dy) < minDist)
            {
                overlap = true;
                break;
            }
        }

        if (!overlap)
        {
            m_items.push_back(item);
            break;
        }
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
        if (!m_upgradeOptions.empty())
        {
            m_state = GameState::Upgrade;
        }
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
    if (m_ultUpgradeLevel < 5)
        available.push_back(3);

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
            opt.name = QStringLiteral("灯照角度 +30°");
            opt.description = QStringLiteral("当前：%1° → %2°").arg(m_player.lightAngle).arg(m_player.lightAngle + 30);
            break;
        case 2:
            opt.name = QStringLiteral("移动速度 +1");
            opt.description = QStringLiteral("当前：%1 → %2").arg(m_player.speed).arg(m_player.speed + 1);
            break;
        case 3:
            opt.name = QStringLiteral("大招强化 Lv.%1").arg(m_ultUpgradeLevel + 1);
            opt.description = QStringLiteral("CD-2s | 一段护盾+1s | 二段半径+1 | 三段持续+1s半径+0.5");
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
    case 3: m_ultUpgradeLevel++; break;
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
    case GameState::CharSelect:
        renderCharSelect(painter);
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
                m_player.ultChargesUsed = charges;
                m_player.ultCharges = 0;

                float ultDuration = ULT_DURATION;
                if (charges == 1)
                    ultDuration += m_ultUpgradeLevel * 1.0f;
                else if (charges >= 3)
                    ultDuration += m_ultUpgradeLevel * 1.0f;
                m_player.ultTimer = ultDuration;

                float effectiveCD = ULT_COOLDOWN - m_ultUpgradeLevel * 2.0f;
                if (effectiveCD < 2.0f)
                    effectiveCD = 2.0f;

                m_ultImageDisplayTier = charges;
                m_ultImageDisplayTimer = 2.0f;

                if (charges == 2)
                {
                    float aoeRadius = 3.0f * UNIT_PX + m_ultUpgradeLevel * 1.0f * UNIT_PX;
                    for (auto &m : m_monsters)
                    {
                        if (distanceToPlayer(m.x, m.y) <= aoeRadius)
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
            m_state = GameState::CharSelect;
        }
        return;
    }

    if (m_state == GameState::CharSelect)
    {
        if (m_charSelectRect1.contains(event->pos()))
        {
            m_selectedCharacter = 1;
            resetGame();
            m_state = GameState::Playing;
            m_gameTimer.restart();
            m_lastFrameTime = m_gameTimer.elapsed();
            m_survivalTime = 0.0f;
        }
        else if (m_charSelectRect2.contains(event->pos()))
        {
            m_selectedCharacter = 2;
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
        {
            m_state = GameState::Start;
            m_selectedCharacter = 0;
        }
        return;
    }

    if ((m_state == GameState::Playing || m_state == GameState::Upgrade)
        && m_endButtonRect.contains(event->pos()))
    {
        m_state = GameState::End;
        return;
    }

    if ((m_state == GameState::Playing || m_state == GameState::Upgrade)
        && m_pauseButtonRect.contains(event->pos()))
    {
        m_paused = !m_paused;
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

    float effRadius = effectiveLightRadius();
    if (dist > effRadius)
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

float GameWidget::effectiveLightRadius() const
{
    float radius = LIGHT_RADIUS;
    if (m_player.ultActive && m_player.ultChargesUsed >= 3)
        radius += m_ultUpgradeLevel * 0.5f * UNIT_PX;
    return radius;
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
    painter.fillRect(rect(), QColor(0, 0, 0));

    renderMap(painter);
    renderItems(painter);
    renderSpikes(painter);
    renderMonsters(painter);
    renderLightOverlay(painter);
    renderPlayer(painter);
    renderUltImage(painter);

    for (const auto &m : m_monsters)
    {
        if (m.inLight)
        {
            QPointF screen = worldToScreen(m.x, m.y);
            float radius = (m.tier == 3) ? 32.0f : (m.tier == 2) ? 25.0f : 18.0f;
            QPen highlightPen(QColor(255, 255, 100, 180), 3.0f);
            painter.setPen(highlightPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(screen, radius + 3.0f, radius + 3.0f);
        }
    }

    renderLightOverlay(painter);
    renderHUD(painter);

    if (m_paused)
    {
        painter.fillRect(rect(), QColor(0, 0, 0, 120));
        QFont pauseOverlayFont(QStringLiteral("Arial"), 36);
        pauseOverlayFont.setBold(true);
        painter.setFont(pauseOverlayFont);
        painter.setPen(QColor(255, 255, 255, 180));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("已暂停"));
    }
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
            case TileType::Ground:  color = QColor(5, 5, 5); break;
            case TileType::Dark:    color = QColor(0, 0, 0); break;
            case TileType::Cracked: color = QColor(8, 8, 8); break;
            case TileType::Rune:    color = QColor(3, 3, 3); break;
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
        QPixmap *img = nullptr;

        switch (m.tier)
        {
        case 1:
            radius = 38.0f;
            img = &m_monsterImage1;
            break;
        case 2:
            radius = 55.0f;
            img = &m_monsterImage2;
            break;
        case 3:
            radius = 72.0f;
            img = &m_monsterImage3;
            break;
        default:
            radius = 38.0f;
            img = &m_monsterImage1;
        }

        if (img && !img->isNull())
        {
            QImage scaled = img->toImage().scaled(
                static_cast<int>(radius * 2), static_cast<int>(radius * 2),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.drawImage(QPointF(screen.x() - scaled.width() / 2.0,
                                      screen.y() - scaled.height() / 2.0), scaled);
        }
        else
        {
            QColor bodyColor, borderColor;
            switch (m.tier)
            {
            case 1: bodyColor = QColor(180, 50, 50); borderColor = QColor(220, 80, 80); break;
            case 2: bodyColor = QColor(200, 100, 30); borderColor = QColor(240, 140, 50); break;
            case 3: bodyColor = QColor(120, 40, 160); borderColor = QColor(160, 80, 200); break;
            default: bodyColor = QColor(180, 50, 50); borderColor = QColor(220, 80, 80);
            }
            painter.setPen(QPen(borderColor, 2.0f));
            painter.setBrush(bodyColor);
            painter.drawEllipse(screen, radius, radius);
        }

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
        painter.drawEllipse(screen, radius + 42.0f, radius + 42.0f);
    }

    if (m_player.ultActive && m_player.ultChargesUsed >= 3)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 200, 50, 30));
        painter.drawEllipse(screen, LIGHT_RADIUS, LIGHT_RADIUS);
    }

    QPixmap *playerImg = nullptr;
    if (m_selectedCharacter == 1)
        playerImg = (m_moveDirX < 0.0f) ? &m_charKongLeft : &m_charKongRight;
    else if (m_selectedCharacter == 2)
        playerImg = (m_moveDirX < 0.0f) ? &m_charYingLeft : &m_charYingRight;

    float imgSize = radius * 5.5f;
    float glowR = imgSize * 0.7f;
    QRadialGradient playerGlow(screen, glowR);
    playerGlow.setColorAt(0.0, QColor(255, 255, 255, 80));
    playerGlow.setColorAt(0.5, QColor(255, 255, 200, 30));
    playerGlow.setColorAt(1.0, QColor(255, 255, 200, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(playerGlow);
    painter.drawEllipse(screen, glowR, glowR);

    if (playerImg && !playerImg->isNull())
    {
        QImage scaled = playerImg->toImage().scaled(
            static_cast<int>(imgSize), static_cast<int>(imgSize),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(QPointF(screen.x() - scaled.width() / 2.0,
                                  screen.y() - scaled.height() / 2.0), scaled);
    }
    else
    {
        painter.setPen(QPen(QColor(150, 200, 255), 2.0f));
        painter.setBrush(QColor(60, 120, 200));
        painter.drawEllipse(screen, radius, radius);
    }

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

    m_lightOverlay.fill(QColor(0, 0, 0, 100));

    QPointF playerScreen = worldToScreen(m_player.x, m_player.y);

    {
        QPainter op(&m_lightOverlay);
        op.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        op.setRenderHint(QPainter::Antialiasing);
        op.setBrush(Qt::white);
        op.setPen(Qt::NoPen);

        bool is360 = m_player.ultActive && m_player.ultChargesUsed >= 3;
        float effectiveLightRadius = LIGHT_RADIUS;
        if (is360)
            effectiveLightRadius += m_ultUpgradeLevel * 0.5f * UNIT_PX;

        if (is360)
        {
            op.drawEllipse(playerScreen, effectiveLightRadius, effectiveLightRadius);
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

        float playerGlowRadius = 15.0f * 2.25f;
        op.drawEllipse(playerScreen, playerGlowRadius, playerGlowRadius);

        op.end();
    }

    m_glowOverlay.fill(Qt::transparent);
    {
        QPainter gp(&m_glowOverlay);
        gp.setRenderHint(QPainter::Antialiasing);

        bool is360 = m_player.ultActive && m_player.ultChargesUsed >= 3;
        float effectiveLightRadius = LIGHT_RADIUS;
        if (is360)
            effectiveLightRadius += m_ultUpgradeLevel * 0.5f * UNIT_PX;

        if (is360)
        {
            QRadialGradient gradient(playerScreen, effectiveLightRadius);
            gradient.setColorAt(0.0, QColor(255, 200, 100, 0));
            gradient.setColorAt(0.80, QColor(255, 180, 80, 0));
            gradient.setColorAt(0.92, QColor(255, 150, 50, 100));
            gradient.setColorAt(1.0, QColor(255, 100, 30, 0));
            gp.setBrush(gradient);
            gp.setPen(Qt::NoPen);
            gp.drawEllipse(playerScreen, effectiveLightRadius, effectiveLightRadius);
        }
        else
        {
            float lightDirAngle = std::atan2(m_mousePos.y() - h / 2.0f, m_mousePos.x() - w / 2.0f);
            float halfAngle = (m_player.lightAngle / 2.0f) * (PI / 180.0f);

            float glowRadius = LIGHT_RADIUS * 1.2f;
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
            gradient.setColorAt(0.65, QColor(255, 180, 80, 0));
            gradient.setColorAt(0.85, QColor(255, 150, 50, 80));
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
    int h = height();
    int margin = 12;
    int y = 8;
    int barH = 20;
    int barW = 200;
    int lineH = 24;

    QFont hudFont(QStringLiteral("Arial"), 12);
    hudFont.setBold(true);
    painter.setFont(hudFont);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 20, 20));
    painter.drawRoundedRect(QRectF(margin, y, barW, barH), 4.0, 4.0);
    float hpRatio = static_cast<float>(m_player.hp) / MAX_HP;
    painter.setBrush(QColor(200, 50, 50));
    painter.drawRoundedRect(QRectF(margin, y, barW * hpRatio, barH), 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(margin, y, barW, barH), Qt::AlignCenter,
                     QStringLiteral("HP:%1/%2").arg(m_player.hp).arg(MAX_HP));

    int expX = margin + barW + 10;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 60));
    painter.drawRoundedRect(QRectF(expX, y, barW, barH), 4.0, 4.0);
    float expRatio = static_cast<float>(m_player.experience) / EXP_PER_LEVEL;
    painter.setBrush(QColor(100, 100, 200));
    painter.drawRoundedRect(QRectF(expX, y, barW * expRatio, barH), 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(expX, y, barW, barH), Qt::AlignCenter,
                     QStringLiteral("Lv.%1 EXP:%2/%3").arg(m_player.level).arg(m_player.experience).arg(EXP_PER_LEVEL));

    int mins = static_cast<int>(m_survivalTime) / 60;
    int secs = static_cast<int>(m_survivalTime) % 60;

    int infoX = expX + barW + 20;
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(QRectF(infoX, y, 350, lineH), Qt::AlignLeft,
                     QStringLiteral("时间:%1:%2  击杀:%3  灯照:%4°  速度:%5  伤害:%6")
                         .arg(mins, 2, 10, QChar('0'))
                         .arg(secs, 2, 10, QChar('0'))
                         .arg(m_player.killCount)
                         .arg(m_player.lightAngle)
                         .arg(m_player.speed, 0, 'f', 1)
                         .arg(m_player.attackDamage, 0, 'f', 1));

    y += lineH + 4;

    QString atkCD = (m_player.attackCooldownTimer > 0.0f)
                        ? QStringLiteral("攻击CD:%1s").arg(m_player.attackCooldownTimer, 0, 'f', 1)
                        : QStringLiteral("攻击:就绪");

    QString spdCD;
    if (m_player.speedBoostActive)
        spdCD = QStringLiteral("加速中:%1s").arg(m_player.speedBoostTimer, 0, 'f', 1);
    else if (m_player.speedBoostCooldownTimer > 0.0f)
        spdCD = QStringLiteral("加速CD:%1s").arg(m_player.speedBoostCooldownTimer, 0, 'f', 1);
    else
        spdCD = QStringLiteral("加速:就绪");

    QString ultStr;
    if (m_player.ultActive)
        ultStr = QStringLiteral("大招中:%1s").arg(m_player.ultTimer, 0, 'f', 1);
    else if (m_player.ultCooldownTimer > 0.0f)
        ultStr = QStringLiteral("大招CD:%1s").arg(m_player.ultCooldownTimer, 0, 'f', 1);
    else
        ultStr = QStringLiteral("大招:就绪");

    QString charges;
    for (int i = 0; i < 3; ++i)
        charges += (i < m_player.ultCharges) ? QStringLiteral("◆") : QStringLiteral("◇");

    painter.setPen(QColor(200, 200, 200));
    painter.drawText(QRectF(margin, y, w - margin * 2, lineH), Qt::AlignLeft,
                     QStringLiteral("%1  |  %2  |  %3  充能:%4").arg(atkCD, spdCD, ultStr, charges));

    float endBtnW = 80.0f, endBtnH = 32.0f;
    float pauseBtnW = 80.0f, pauseBtnH = 32.0f;
    float endBtnX = w - endBtnW - 12.0f;
    float pauseBtnX = endBtnX - pauseBtnW - 8.0f;
    float endBtnY = 8.0f;
    m_endButtonRect = QRectF(endBtnX, endBtnY, endBtnW, endBtnH);
    m_pauseButtonRect = QRectF(pauseBtnX, endBtnY, pauseBtnW, pauseBtnH);

    painter.setPen(QPen(QColor(180, 140, 60), 2.0f));
    painter.setBrush(QColor(40, 30, 20, 200));
    painter.drawRoundedRect(m_pauseButtonRect, 6.0, 6.0);

    QFont pauseFont = painter.font();
    pauseFont.setPointSize(11);
    pauseFont.setBold(true);
    painter.setFont(pauseFont);
    painter.setPen(QColor(255, 220, 150));
    painter.drawText(m_pauseButtonRect, Qt::AlignCenter,
                     m_paused ? QStringLiteral("继续") : QStringLiteral("暂停"));

    painter.setPen(QPen(QColor(180, 60, 60), 2.0f));
    painter.setBrush(QColor(40, 20, 20, 200));
    painter.drawRoundedRect(m_endButtonRect, 6.0, 6.0);

    QFont endFont = painter.font();
    endFont.setPointSize(11);
    endFont.setBold(true);
    painter.setFont(endFont);
    painter.setPen(QColor(255, 150, 150));
    painter.drawText(m_endButtonRect, Qt::AlignCenter, QStringLiteral("结束"));
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
    if (!m_startBg.isNull())
    {
        QImage scaledBg = m_startBg.toImage().scaled(
            width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(0, 0, scaledBg);
        painter.fillRect(rect(), QColor(0, 0, 0, 120));
    }
    else
    {
        painter.fillRect(rect(), QColor(15, 15, 25));
    }

    int w = width();
    int h = height();

    QFont titleFont = painter.font();
    titleFont.setPointSize(36);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(255, 200, 100));
    painter.drawText(QRect(0, h / 8, w, 60), Qt::AlignCenter,
                     QStringLiteral("猎杀多托雷"));

    QFont instrFont = painter.font();
    instrFont.setPointSize(11);
    painter.setFont(instrFont);
    painter.setPen(QColor(220, 200, 160));

    QString loreText = QStringLiteral(
        "旅行者请手持充满月矩力的提灯斩杀博士的切片吧，博士行事阴暗，喜爱背地里下死手，"
        "故在灯的照射下，其行动受限；拾取一个月矩力释放大招会暂时得到来自执灯士菲林斯的保护，"
        "拾取两个月矩力释放大招会得到来自叮铃哐啷蛋卷工坊老大爱诺的月矩力大炮支援，"
        "拾取三个月矩力释放大招会得到三月女神哥伦比娅的祝福。请旅行者尽情战斗吧。");

    QRectF loreRect(w * 0.08f, h * 0.26f, w * 0.84f, h * 0.28f);
    painter.drawText(loreRect, Qt::AlignCenter | Qt::TextWordWrap, loreText);

    QFont ctrlFont = painter.font();
    ctrlFont.setPointSize(9);
    painter.setFont(ctrlFont);
    painter.setPen(QColor(180, 180, 200));

    QString ctrlText = QStringLiteral(
        "WASD/方向键移动 | 鼠标控制灯照方向 | 左键攻击 | 右键加速 | F键大招 | 拾取月矩力球充能，血包回血");

    QRectF ctrlRect(w * 0.05f, h * 0.56f, w * 0.9f, 20);
    painter.drawText(ctrlRect, Qt::AlignCenter, ctrlText);

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

void GameWidget::renderCharSelect(QPainter &painter)
{
    painter.fillRect(rect(), QColor(15, 15, 25));

    int w = width();
    int h = height();

    QFont titleFont = painter.font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(255, 200, 100));
    painter.drawText(QRect(0, h / 10, w, 50), Qt::AlignCenter,
                     QStringLiteral("选择角色"));

    int btnW = 180;
    int btnH = 80;
    int spacing = 50;
    float totalW = btnW * 2 + spacing;
    float startX = (w - totalW) / 2.0f;
    float btnY = h / 2.0f - btnH / 2.0f;

    m_charSelectRect1 = QRectF(startX, btnY, btnW, btnH);
    m_charSelectRect2 = QRectF(startX + btnW + spacing, btnY, btnW, btnH);

    bool hover1 = m_charSelectRect1.contains(m_mousePos);
    bool hover2 = m_charSelectRect2.contains(m_mousePos);

    auto drawBtn = [&](const QRectF &rect, const QString &name, bool hovered)
    {
        QColor bgColor = hovered ? QColor(60, 60, 100) : QColor(30, 30, 50);
        QColor borderColor = hovered ? QColor(255, 200, 100) : QColor(80, 80, 120);
        painter.setPen(QPen(borderColor, 2.0f));
        painter.setBrush(bgColor);
        painter.drawRoundedRect(rect, 12.0, 12.0);

        QFont nameFont = painter.font();
        nameFont.setPointSize(22);
        nameFont.setBold(true);
        painter.setFont(nameFont);
        painter.setPen(hovered ? QColor(255, 220, 100) : QColor(200, 200, 200));
        painter.drawText(rect, Qt::AlignCenter, name);
    };

    drawBtn(m_charSelectRect1, QStringLiteral("空"), hover1);
    drawBtn(m_charSelectRect2, QStringLiteral("荧"), hover2);

    QFont backFont = painter.font();
    backFont.setPointSize(12);
    painter.setFont(backFont);
    painter.setPen(QColor(150, 150, 150));
    painter.drawText(QRect(0, h - 50, w, 30), Qt::AlignCenter,
                     QStringLiteral("点击角色名称开始游戏"));
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
    painter.drawText(QRect(0, h / 8, w, 60), Qt::AlignCenter, QStringLiteral("游戏结束"));

    int totalSecs = static_cast<int>(m_survivalTime);
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;

    QString rating;
    QColor ratingColor;
    if (totalSecs <= 180)
    {
        rating = QStringLiteral("提米鸽子");
        ratingColor = QColor(150, 150, 150);
    }
    else if (totalSecs <= 360)
    {
        rating = QStringLiteral("博士克星");
        ratingColor = QColor(100, 200, 255);
    }
    else
    {
        rating = QStringLiteral("真降临者");
        ratingColor = QColor(255, 200, 50);
    }

    QFont ratingFont = painter.font();
    ratingFont.setPointSize(28);
    ratingFont.setBold(true);
    painter.setFont(ratingFont);
    painter.setPen(ratingColor);
    painter.drawText(QRect(0, h / 8 + 60, w, 50), Qt::AlignCenter, rating);

    QFont statFont = painter.font();
    statFont.setPointSize(16);
    painter.setFont(statFont);
    painter.setPen(QColor(220, 220, 220));

    int y = h / 3;
    int lineH = 40;

    painter.drawText(QRect(0, y, w, lineH), Qt::AlignCenter,
                     QStringLiteral("存活时间：%1分%2秒").arg(mins).arg(secs, 2, 10, QChar('0')));
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