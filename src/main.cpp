#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/binding/Slider.hpp>
#include <Windows.h>
#include <TlHelp32.h>
#include <chrono>
#include "Engine.hpp"

using namespace geode::prelude;
namespace quiet {
constexpr ccColor3B Purple{153,132,255}, White{236,238,248}, Muted{145,151,175}, Mint{106,225,184};
struct Config { bool enabled=false; float percent=60; };
std::string keyFor(GJGameLevel* l) {
    int id=l->m_levelID;
    if(id>0) return fmt::format("level-{}",id);
    uint64_t hash=14695981039346656037ull;
    std::string data=std::string(l->m_levelName)+"|"+std::string(l->m_levelString);
    for(unsigned char c:data){hash^=c;hash*=1099511628211ull;}
    return fmt::format("local-{:x}",hash);
}
Config load(std::string const& key) {
    auto m=Mod::get();
    float p=m->getSavedValue<float>(key+"/percent",60.f);
    return {m->getSavedValue<bool>(key+"/enabled",false),std::isfinite(p)?std::clamp(p,1.f,99.9f):60.f};
}
void save(std::string const& key,Config c) {
    Mod::get()->setSavedValue(key+"/enabled",c.enabled);
    Mod::get()->setSavedValue(key+"/percent",c.percent);
}
bool discordRunning() {
    static auto checked=std::chrono::steady_clock::time_point{};static bool result=false;
    auto now=std::chrono::steady_clock::now();
    if(now-checked<std::chrono::seconds(2))return result;
    checked=now;result=false;
    HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(h==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W p{};p.dwSize=sizeof(p);
    if(Process32FirstW(h,&p))do {
        if(!_wcsicmp(p.szExeFile,L"Discord.exe")||!_wcsicmp(p.szExeFile,L"DiscordPTB.exe")||!_wcsicmp(p.szExeFile,L"DiscordCanary.exe")){result=true;break;}
    }while(Process32NextW(h,&p));
    CloseHandle(h);return result;
}
int functionKey(){return std::clamp(int(Mod::get()->getSettingValue<int64_t>("deafen-key")),6,12);}
std::string lastError;
int ownedKey=VK_F8;
// Never change the restore key while the mod owns a toggle.
bool tap(int vk,bool restoring=false) {
    static auto last=std::chrono::steady_clock::time_point{};
    auto now=std::chrono::steady_clock::now();
    if(now-last<std::chrono::milliseconds(180))return false;
    if(!restoring && !discordRunning()){lastError="Discord не запущен";return false;}
    if((GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_SHIFT)|GetAsyncKeyState(VK_MENU)|GetAsyncKeyState(vk))&0x8000){lastError="Отпусти клавиши Ctrl / Shift / Alt";return false;}
    INPUT events[2]{};events[0].type=events[1].type=INPUT_KEYBOARD;
    events[0].ki.wVk=events[1].ki.wVk=WORD(vk);events[1].ki.dwFlags=KEYEVENTF_KEYUP;
    auto sent=SendInput(2,events,sizeof(INPUT));last=now;
    if(sent!=2){
        if(sent==1)SendInput(1,&events[1],sizeof(INPUT));
        lastError="Windows не приняла нажатие клавиши";return false;
    }
    lastError.clear();return true;
}
Engine engine;
std::string currentKey;
Config current;
bool testOwned=false;
bool restorePending=false,testRestorePending=false;
bool sendDeafen(){int vk=VK_F1+functionKey()-1;if(!tap(vk))return false;ownedKey=vk;return true;}
bool sendRestore(){return tap(ownedKey,true);}
void restore(){restorePending=!engine.restore(sendRestore);}
class RestorePump : public CCNode {
public:
    void update(float) override {
        if(engine.owned&&(restorePending||engine.ended))restore();
        if(testOwned&&testRestorePending&&sendRestore()){testOwned=false;testRestorePending=false;}
    }
};

CCLabelTTF* text(CCNode* parent,std::string value,float x,float y,float size,ccColor3B color=White,bool left=false) {
    auto l=CCLabelTTF::create(value.c_str(),"Segoe UI",size);
    l->setPosition({x,y});l->setColor(color);if(left)l->setAnchorPoint({0,.5f});parent->addChild(l);return l;
}
NineSlice* box(CCNode* parent,float x,float y,float w,float h,ccColor3B color) {
    auto s=NineSlice::create("GJ_square02.png");s->setScaleMultiplier(.5f);s->setContentSize({w,h});s->setPosition({x,y});s->setColor(color);parent->addChild(s);return s;
}
void fit(CCLabelTTF* l,float width){if(l->getContentWidth()>width)l->setScale(width/l->getContentWidth());}

class Settings : public Popup {
    std::string m_key;
    Config m_config;
    bool m_hasLevel=false,m_platformer=false,m_invalid=false;
    CCLabelTTF *m_percent=nullptr,*m_state=nullptr,*m_toggleText=nullptr,*m_keyText=nullptr,*m_pauseText=nullptr,*m_practiceText=nullptr,*m_hint=nullptr;
    NineSlice* m_toggleBg=nullptr;
    TextInput* m_input=nullptr;
    Slider* m_slider=nullptr;
    void persist() {
        if(m_hasLevel)save(m_key,m_config);
        if(m_key==currentKey) {current=m_config;engine.threshold=m_config.percent;if(!current.enabled)restore();}
    }
    void refresh() {
        m_percent->setString(fmt::format("{:g}%",m_config.percent).c_str());
        m_toggleText->setString(m_config.enabled?"ВКЛЮЧЕНО":"ВЫКЛЮЧЕНО");
        m_toggleText->setColor(m_config.enabled?White:Muted);
        m_toggleBg->setColor(m_config.enabled?ccColor3B{91,72,177}:ccColor3B{39,42,58});
        m_keyText->setString(fmt::format("Клавиша Discord     F{}   >",functionKey()).c_str());
        m_pauseText->setString(Mod::get()->getSettingValue<bool>("restore-on-pause")?"Пауза: вернуть звук   ДА":"Пауза: вернуть звук   НЕТ");
        m_practiceText->setString(Mod::get()->getSettingValue<bool>("in-practice")?"В тренировке   ДА":"В тренировке   НЕТ");
    }
    void onToggle(CCObject*) {
        if(!m_hasLevel||m_platformer){showGuide(nullptr);return;}
        if(m_invalid){m_hint->setString("Введи процент от 1 до 99.9");return;}
        m_config.enabled=!m_config.enabled;persist();refresh();
        m_toggleBg->stopAllActions();m_toggleBg->runAction(CCSequence::create(CCScaleTo::create(.07f,.96f),CCScaleTo::create(.12f,1.f),nullptr));
    }
    void setPercent(float p,bool updateInput) {
        m_config.percent=std::round(std::clamp(p,1.f,99.9f)*10)/10;m_invalid=false;
        if(updateInput)m_input->setString(fmt::format("{:g}",m_config.percent));
        m_slider->setValue((m_config.percent-1)/98.9f);m_slider->updateBar();
        persist();refresh();m_hint->setString("Настройки сохраняются автоматически");
    }
    void onSlider(CCObject*){setPercent(1+m_slider->getValue()*98.9f,true);}
    void onKey(CCObject*) {
        if(engine.owned||testOwned){m_hint->setString("Сначала верни звук кнопкой проверки");return;}
        int k=functionKey();Mod::get()->setSettingValue<int64_t>("deafen-key",k==12?6:k+1);refresh();
        m_hint->setString("Назначь эту же клавишу в Discord");
    }
    void onPause(CCObject*) {auto m=Mod::get();m->setSettingValue("restore-on-pause",!m->getSettingValue<bool>("restore-on-pause"));refresh();}
    void onPractice(CCObject*) {auto m=Mod::get();m->setSettingValue("in-practice",!m->getSettingValue<bool>("in-practice"));refresh();}
    void onTest(CCObject*) {
        if(engine.owned){restore();m_hint->setString(engine.owned?"Не удалось вернуть звук. Повтори":"Команда возврата звука отправлена");return;}
        if(testOwned){if(sendRestore()){testOwned=false;m_hint->setString("Тест завершён. Звук должен вернуться");}else m_hint->setString(lastError.c_str());return;}
        if(sendDeafen()){testOwned=true;m_hint->setString("Звук отключился? Нажми ещё раз, чтобы вернуть");}else m_hint->setString(lastError.c_str());
    }
    void showGuide(CCObject*) {
        // Use the game alert for the short, universally readable setup guide.
        FLAlertLayer::create("Discord setup",fmt::format("1. Open Discord desktop > Settings > Keybinds.\n2. Add a keybind: <cg>Toggle Deafen</c>.\n3. Record <cy>F{}</c> and LEAVE the settings page.\n4. Join voice with sound ON.\n5. Click TEST twice in QuietRun.\n\nDisable other AutoDeafen features. Do not manually toggle deafen during a run.\nThe hotkey cannot read Discord's actual state.",functionKey()),"OK")->show();
    }
    void onClose(CCObject* sender) override {
        if(testOwned) {testRestorePending=true;if(sendRestore()){testOwned=false;testRestorePending=false;}}
        persist();(void)Mod::get()->saveData();Popup::onClose(sender);
    }
    CCMenuItemSpriteExtra* button(float x,float y,float w,float h,ccColor3B color,SEL_MenuHandler cb) {
        auto n=CCNode::create();n->setContentSize({w,h});box(n,w/2,h/2,w,h,color);
        auto b=CCMenuItemSpriteExtra::create(n,this,cb);b->setPosition({x,y});m_buttonMenu->addChild(b);return b;
    }
    void update(float) override {
        if(!m_hasLevel)m_state->setString("ОТКРОЙ УРОВЕНЬ ДЛЯ НАСТРОЙКИ");
        else if(m_platformer)m_state->setString("ТОЛЬКО КЛАССИЧЕСКИЕ УРОВНИ");
        else if(!discordRunning()){m_state->setString("DISCORD НЕ ЗАПУЩЕН");m_state->setColor({244,184,114});}
        else {m_state->setString("DISCORD НАЙДЕН  ·  ПРОВЕРЬ КЛАВИШУ");m_state->setColor(Mint);}
    }
    bool setup(GJGameLevel* level) {
        if(!Popup::init(438,300,"GJ_square02.png"))return false;
        setID("settings-popup"_spr);
        m_bgSprite->setColor({17,18,28});
        m_hasLevel=level!=nullptr;m_platformer=level&&level->isPlatformer();
        if(level){m_key=keyFor(level);m_config=load(m_key);}
        // A restrained editorial layout with two cards and a single primary action.
        box(m_mainLayer,29,268,3,24,Purple);
        text(m_mainLayer,"QuietRun",42,273,23,White,true);
        text(m_mainLayer,"AUTO DEAFEN",43,254,8,Purple,true);
        text(m_mainLayer,"FOCUS ON THE RUN",411,270,8,Muted)->setAnchorPoint({1,.5f});
        m_state=text(m_mainLayer,"",43,235,8,Mint,true);
        box(m_mainLayer,126,143,198,150,{25,27,40});
        text(m_mainLayer,"ЗАГЛУШАТЬ С",126,201,8,Muted);
        m_percent=text(m_mainLayer,"60%",126,168,38,White);
        m_slider=Slider::create(this,menu_selector(Settings::onSlider),.66f);m_slider->setPosition({126,130});m_mainLayer->addChild(m_slider);
        m_slider->m_sliderBar->setColor(Purple);
        m_input=TextInput::create(72,"60");m_input->setScale(.65f);m_input->setPosition({126,97});m_input->setFilter("0123456789.,");m_input->setMaxCharCount(5);m_mainLayer->addChild(m_input);
        m_input->setString(fmt::format("{:g}",m_config.percent));
        m_input->setCallback([this](std::string const& s){
            std::string value=s;std::replace(value.begin(),value.end(),',','.');
            try {size_t end=0;float p=std::stof(value,&end);if(end!=value.size()||!std::isfinite(p)||p<1||p>99.9f)throw 1;setPercent(p,false);}
            catch(...){m_invalid=true;m_hint->setString("Введи процент от 1 до 99.9");}
        });
        text(m_mainLayer,"можно ввести точное число",126,76,7,Muted);
        auto toggle=button(326,194,167,41,{39,42,58},menu_selector(Settings::onToggle));
        m_toggleBg=box(toggle->getNormalImage(),83.5f,20.5f,167,41,{39,42,58});
        m_toggleText=text(toggle->getNormalImage(),"ВЫКЛЮЧЕНО",83.5f,20.5f,11,Muted);
        auto key=button(326,145,167,32,{31,32,48},menu_selector(Settings::onKey));
        m_keyText=text(key->getNormalImage(),"",83.5f,16,9,White);
        auto pause=button(326,109,167,28,{25,27,40},menu_selector(Settings::onPause));
        m_pauseText=text(pause->getNormalImage(),"",83.5f,14,8,Muted);
        auto practice=button(326,78,167,26,{25,27,40},menu_selector(Settings::onPractice));
        m_practiceText=text(practice->getNormalImage(),"",83.5f,13,8,Muted);
        auto test=button(150,40,246,30,{93,73,177},menu_selector(Settings::onTest));
        text(test->getNormalImage(),"Проверить Discord / вернуть звук",123,15,10,White);
        auto guide=button(347,40,125,30,{37,38,55},menu_selector(Settings::showGuide));
        text(guide->getNormalImage(),"Как настроить",62.5f,15,10,White);
        m_hint=text(m_mainLayer,"Настройки сохраняются автоматически",219,14,7,Muted);
        if(level){auto name=text(m_mainLayer,std::string(level->m_levelName),411,250,9,Muted);name->setAnchorPoint({1,.5f});fit(name,180);}
        setPercent(m_config.percent,true);refresh();scheduleUpdate();update(0);
        return true;
    }
public:
    static Settings* create(GJGameLevel* level=nullptr){auto p=new Settings();if(p->setup(level)){p->autorelease();return p;}delete p;return nullptr;}
};
}

class $modify(QuietPlay,PlayLayer) {
    struct Fields {bool pendingStart=true;CCLabelTTF* hud=nullptr;float elapsed=0;};
    bool init(GJGameLevel* level,bool replay,bool noObjects) {
        if(!PlayLayer::init(level,replay,noObjects))return false;
        quiet::restore();quiet::currentKey=quiet::keyFor(level);quiet::current=quiet::load(quiet::currentKey);
        quiet::engine.begin(0,quiet::current.percent);m_fields->pendingStart=true;
        if(!m_isPlatformer&&Mod::get()->getSettingValue<bool>("show-hud")) {
            auto size=CCDirector::get()->getWinSize();
            m_fields->hud=quiet::text(this,"",size.width-10,11,8,quiet::Muted);m_fields->hud->setAnchorPoint({1,0});m_fields->hud->setZOrder(100);m_fields->hud->setOpacity(205);
        }
        return true;
    }
    void resetLevel(){quiet::engine.finish(quiet::sendRestore);PlayLayer::resetLevel();m_fields->pendingStart=true;}
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if(m_isPlatformer||m_isTestMode||!m_player1)return;
        float percent=getCurrentPercent();
        if(m_fields->pendingStart && std::isfinite(percent)) {quiet::engine.begin(percent,quiet::current.percent);m_fields->pendingStart=false;}
        bool allowed=!m_player1->m_isDead&&!m_isPaused&&(Mod::get()->getSettingValue<bool>("in-practice")||!m_isPracticeMode);
        if(!quiet::testOwned)quiet::engine.tick(percent,quiet::current.enabled,allowed,[]{return quiet::engine.owned?quiet::sendRestore():quiet::sendDeafen();});
        if(quiet::engine.ended && quiet::engine.owned)quiet::restore();
        m_fields->elapsed+=dt;
        if(m_fields->hud&&m_fields->elapsed>.2f) {
            m_fields->elapsed=0;
            std::string value;
            if(quiet::current.enabled) {
                if(!quiet::lastError.empty())value=quiet::lastError;
                else if(quiet::engine.owned)value="QuietRun  ·  команда заглушения отправлена";
                else if(quiet::engine.skippedStart)value="QuietRun  ·  старт после порога";
                else value=fmt::format("QuietRun  ·  ожидание {:g}%",quiet::current.percent);
            }
            m_fields->hud->setString(value.c_str());m_fields->hud->setColor(quiet::engine.owned?quiet::Purple:quiet::Muted);
        }
    }
    void destroyPlayer(PlayerObject* p,GameObject* o) {
        PlayLayer::destroyPlayer(p,o);
        if(m_player1&&m_player1->m_isDead)quiet::engine.finish(quiet::sendRestore);
    }
    void levelComplete(){quiet::engine.finish(quiet::sendRestore);PlayLayer::levelComplete();}
    void pauseGame(bool unfocused){if(Mod::get()->getSettingValue<bool>("restore-on-pause"))quiet::restore();PlayLayer::pauseGame(unfocused);}
    void togglePracticeMode(bool enabled){quiet::engine.finish(quiet::sendRestore);PlayLayer::togglePracticeMode(enabled);m_fields->pendingStart=true;}
    void onQuit(){quiet::engine.finish(quiet::sendRestore);quiet::currentKey.clear();PlayLayer::onQuit();}
};

class $modify(QuietPause,PauseLayer) {
    void customSetup(){
        PauseLayer::customSetup();auto size=CCDirector::get()->getWinSize();
        auto menu=CCMenu::create();menu->setID("pause-menu"_spr);menu->setPosition({0,0});addChild(menu,150);
        auto node=CCNode::create();node->setContentSize({108,27});quiet::box(node,54,13.5f,108,27,{36,31,59});quiet::text(node,"QuietRun",54,13.5f,12,quiet::Purple);
        auto item=CCMenuItemSpriteExtra::create(node,this,menu_selector(QuietPause::openQuiet));item->setID("open-settings"_spr);item->setPosition({size.width-67,size.height-23});menu->addChild(item);
    }
    void openQuiet(CCObject*){auto p=PlayLayer::get();if(auto s=quiet::Settings::create(p?p->m_level:nullptr))s->show();}
};
class $modify(QuietMenu,MenuLayer) {
    bool init(){
        if(!MenuLayer::init())return false;
        quiet::restore();auto size=CCDirector::get()->getWinSize();
        auto menu=CCMenu::create();menu->setID("main-menu"_spr);menu->setPosition({0,0});addChild(menu,150);
        auto node=CCNode::create();node->setContentSize({100,25});quiet::box(node,50,12.5f,100,25,{36,31,59});quiet::text(node,"QuietRun",50,12.5f,11,quiet::Purple);
        auto item=CCMenuItemSpriteExtra::create(node,this,menu_selector(QuietMenu::openQuiet));item->setID("open-settings"_spr);item->setPosition({size.width-63,size.height-20});menu->addChild(item);return true;
    }
    void openQuiet(CCObject*){if(auto s=quiet::Settings::create())s->show();}
};

$execute {
    auto pump=new quiet::RestorePump();
    pump->init();CCDirector::get()->getScheduler()->scheduleUpdateForTarget(pump,0,false);pump->release();
}
