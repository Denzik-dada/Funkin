#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/CCDirector.hpp>

#include <algorithm>
#include <functional>
#include "hotkey.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------
// ВАЖНО: мод намеренно НЕ хукает классы из GeometryDash.exe
// (PlayLayer, MenuLayer и т.д.) и не делает typeinfo_cast к ним.
// На версиях игры новее, чем поддерживает Geode (например 2.209),
// адреса и vtable из exe не совпадают, и это роняет игру.
// Здесь используются только функции cocos2d (libcocos2d.dll),
// поэтому мод не зависит от версии игры.
// ---------------------------------------------------------------

namespace State {
    inline bool showStats = false; // FPS / статистика cocos2d
    inline float speed = 1.0f;     // 1.0 = обычная скорость
    inline bool menuOpen = false;
}

// ---------------------------------------------------------------
// Окно мод-меню (Geode v5: Popup больше не шаблонный)
// ---------------------------------------------------------------
class ModMenuPopup : public Popup {
protected:
    CCLabelBMFont* m_speedLabel = nullptr;

    void addToggle(
        CCMenu* menu, char const* title, bool* value, float y,
        std::function<void(bool)> onChange = nullptr
    ) {
        auto label = CCLabelBMFont::create(title, "bigFont.fnt");
        label->setScale(0.5f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({-110.f, y});
        menu->addChild(label);

        auto toggler = CCMenuItemExt::createTogglerWithStandardSprites(
            0.7f,
            [value, onChange](CCMenuItemToggler* t) {
                // в колбэке isToggled() ещё содержит старое значение
                *value = !t->isToggled();
                if (onChange) onChange(*value);
            }
        );
        toggler->toggle(*value);
        toggler->setPosition({100.f, y});
        menu->addChild(toggler);
    }

    void updateSpeedLabel() {
        m_speedLabel->setString(fmt::format("Speed: {:.2f}x", State::speed).c_str());
    }

    bool init() {
        if (!Popup::init(300.f, 170.f)) return false;
        this->setTitle("Mod Menu");

        auto menu = CCMenu::create();
        menu->setPosition(m_mainLayer->getContentSize() / 2);
        m_mainLayer->addChild(menu);

        addToggle(menu, "Show FPS", &State::showStats, 25.f, [](bool on) {
            CCDirector::get()->setDisplayStats(on);
        });

        // Speedhack: кнопки - и +
        m_speedLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_speedLabel->setScale(0.5f);
        m_speedLabel->setPosition({0.f, -20.f});
        menu->addChild(m_speedLabel);
        updateSpeedLabel();

        auto minus = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("-", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f),
            [this](CCObject*) {
                State::speed = std::max(0.1f, State::speed - 0.25f);
                updateSpeedLabel();
            }
        );
        minus->setPosition({-100.f, -20.f});
        menu->addChild(minus);

        auto plus = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("+", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f),
            [this](CCObject*) {
                State::speed = std::min(5.0f, State::speed + 0.25f);
                updateSpeedLabel();
            }
        );
        plus->setPosition({100.f, -20.f});
        menu->addChild(plus);

        auto hint = CCLabelBMFont::create("F1 opens this menu", "chatFont.fnt");
        hint->setScale(0.6f);
        hint->setOpacity(150);
        hint->setPosition({0.f, -58.f});
        menu->addChild(hint);

        State::menuOpen = true;
        return true;
    }

public:
    ~ModMenuPopup() override {
        State::menuOpen = false;
    }

    static ModMenuPopup* create() {
        auto ret = new ModMenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

static void openModMenu() {
    if (State::menuOpen) return;
    if (auto popup = ModMenuPopup::create()) {
        popup->show();
    }
}

// ---------------------------------------------------------------
// Кнопка-шестерёнка в левом верхнем углу.
// Добавляется прямо в текущую сцену (только cocos2d, без MenuLayer),
// поэтому видна на любом экране. Если не получилось, есть F1.
// ---------------------------------------------------------------
static constexpr int kMenuButtonTag = 0x4D4D;
static constexpr char const* kButtonFrame = "GJ_optionsBtn_001.png";

// Шаг, на котором выполняется код (для диагностики вылетов в логе).
static volatile int g_stage = 0;
static bool g_buttonDisabled = false;

// ВАЖНО: на версиях игры новее поддерживаемой нельзя читать поля CCDirector
// (getRunningScene, getWinSize): их раскладка в памяти другая. Поэтому сцену
// берём из параметра хука willSwitchToScene, а высота экрана у GD всегда 320.
static void addButtonToScene(CCScene* scene) {
    g_stage = 1;
    if (!scene) return;

    g_stage = 2;
    if (!CCSpriteFrameCache::get()->spriteFrameByName(kButtonFrame)) return;

    g_stage = 3;
    auto sprite = CCSprite::createWithSpriteFrameName(kButtonFrame);
    if (!sprite) return;
    g_stage = 4;
    sprite->setScale(0.5f);
    sprite->setOpacity(170);

    g_stage = 5;
    auto button = CCMenuItemExt::createSpriteExtra(
        sprite,
        [](CCObject*) { openModMenu(); }
    );

    g_stage = 6;
    auto menu = CCMenu::create();
    menu->setTag(kMenuButtonTag);
    g_stage = 7;
    menu->setPosition({30.f, 290.f});
    menu->addChild(button);

    g_stage = 8;
    scene->addChild(menu, 9999);
    g_stage = 0;
}

// Защита от сбоев Windows (SEH): при падении кода кнопка/меню отключаются,
// а игра продолжает работать. В этих функциях нет объектов с деструкторами.
static bool guardedAddButton(CCScene* scene) {
    __try {
        addButtonToScene(scene);
        return true;
    } __except (1) {
        return false;
    }
}

static bool guardedOpenMenu() {
    __try {
        g_stage = 100;
        openModMenu();
        g_stage = 0;
        return true;
    } __except (1) {
        return false;
    }
}

// ---------------------------------------------------------------
// Кнопка: добавляется в каждую новую сцену (в том числе главное меню)
// ---------------------------------------------------------------
class $modify(ModMenuDirector, CCDirector) {
    void willSwitchToScene(CCScene* newScene) {
        CCDirector::willSwitchToScene(newScene);
        if (!g_buttonDisabled && !guardedAddButton(newScene)) {
            g_buttonDisabled = true;
            log::error("Menu button crashed at stage {}, button disabled (F1 still works)", (int)g_stage);
        }
    }
};

// ---------------------------------------------------------------
// F1 + Speedhack (хук в libcocos2d.dll, не в exe игры)
// ---------------------------------------------------------------
class $modify(ModMenuScheduler, CCScheduler) {
    void update(float dt) {
        if (modMenuHotkeyPressed()) {
            if (!guardedOpenMenu()) {
                State::menuOpen = false;
                log::error("Opening the menu crashed at stage {}", (int)g_stage);
            } else {
                log::info("Mod menu opened with F1");
            }
        }
        CCScheduler::update(dt * State::speed);
    }
};
