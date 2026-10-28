#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

#include <algorithm>
#include <functional>
#include "hotkey.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------
// Состояние мода
// ---------------------------------------------------------------
namespace State {
    inline bool noclip = false;
    inline bool safeMode = true;   // с читами прохождение уровня не засчитывается
    inline bool showStats = false; // FPS / статистика cocos2d
    inline float speed = 1.0f;     // 1.0 = обычная скорость
    inline bool menuOpen = false;

    inline bool anyCheat() {
        return noclip || speed != 1.0f;
    }
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
        if (!Popup::init(300.f, 230.f)) return false;
        this->setTitle("Mod Menu");

        auto menu = CCMenu::create();
        menu->setPosition(m_mainLayer->getContentSize() / 2);
        m_mainLayer->addChild(menu);

        addToggle(menu, "Noclip", &State::noclip, 60.f);
        addToggle(menu, "Safe Mode", &State::safeMode, 30.f);
        addToggle(menu, "Show FPS", &State::showStats, 0.f, [](bool on) {
            CCDirector::get()->setDisplayStats(on);
        });

        // Speedhack: кнопки - и +
        m_speedLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_speedLabel->setScale(0.5f);
        m_speedLabel->setPosition({0.f, -40.f});
        menu->addChild(m_speedLabel);
        updateSpeedLabel();

        auto minus = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("-", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f),
            [this](CCObject*) {
                State::speed = std::max(0.1f, State::speed - 0.25f);
                updateSpeedLabel();
            }
        );
        minus->setPosition({-100.f, -40.f});
        menu->addChild(minus);

        auto plus = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("+", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f),
            [this](CCObject*) {
                State::speed = std::min(5.0f, State::speed + 0.25f);
                updateSpeedLabel();
            }
        );
        plus->setPosition({100.f, -40.f});
        menu->addChild(plus);

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
// Кнопка на главном экране.
// Добавляется из хука CCScheduler (он в libcocos2d.dll и не зависит
// от адресов функций игры), поэтому не требует хука MenuLayer::init.
// Если на 2.209 главный экран не опознаётся, кнопки не будет,
// но меню всё равно открывается по F1.
// ---------------------------------------------------------------
static constexpr int kMenuButtonTag = 0x4D4D;

static void injectMenuButton() {
    static int frame = 0;
    if (++frame % 30 != 0) return; // проверяем примерно дважды в секунду

    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return;

    auto layer = scene->getChildByType<MenuLayer>(0);
    if (!layer) return;
    if (layer->getChildByTag(kMenuButtonTag)) return;

    auto sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    if (!sprite) return;
    sprite->setScale(0.7f);

    auto button = CCMenuItemExt::createSpriteExtra(
        sprite,
        [](CCObject*) { openModMenu(); }
    );

    auto menu = CCMenu::create();
    menu->setTag(kMenuButtonTag);
    auto win = CCDirector::get()->getWinSize();
    menu->setPosition({40.f, win.height - 40.f});
    menu->addChild(button);
    layer->addChild(menu, 100);
}

// ---------------------------------------------------------------
// Открытие меню по F1 + кнопка + Speedhack
// ---------------------------------------------------------------
class $modify(ModMenuScheduler, CCScheduler) {
    void update(float dt) {
        if (modMenuHotkeyPressed()) {
            openModMenu();
        }
        injectMenuButton();
        CCScheduler::update(dt * State::speed);
    }
};

// ---------------------------------------------------------------
// Noclip + Safe Mode
// Внимание: PlayLayer находится в exe игры. На 2.209 адреса могут
// не совпадать, пока Geode не получит полную поддержку этой версии.
// ---------------------------------------------------------------
class $modify(ModMenuPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // смерть от античит-шипа не отключаем
        if (State::noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        // Safe Mode: если были читы, прохождение не засчитывается
        if (State::safeMode && State::anyCheat()) {
            m_isTestMode = true;
        }
        PlayLayer::levelComplete();
    }
};
