#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(MyUltimateLayer, PlayLayer) {
    // --- DYNAMIC REWIND DATA STRUCTURE ---
    struct FrameState {
        CCPoint position;
        float rotation;
        float yVelocity;
        bool isUpsideDown;
        bool isPlatformer;
    };

    std::vector<FrameState> m_recordedFrames;
    bool m_isRewinding = false;
    size_t m_rewindIndex = 0;

    // --- CONFIGURABLE EDITOR SLIDER PROPERTY ---
    int m_maxRewindSeconds = 10; 

    // --- ORBS AND TRIGGERS STATE VARIABLES ---
    bool m_hoverActive = false;
    int m_ufoJumpsLeft = 3;
    float m_suddenDeathTimer = 3.0f;
    bool m_suddenDeathActive = false;
    bool m_modeTriggerUsed = false; 

    bool init(GJGameLevel* level, bool usePracticeMode, bool isPlaytest) {
        if (!PlayLayer::init(level, usePracticeMode, isPlaytest)) return false;

        m_fields->m_recordedFrames.clear();
        m_fields->m_isRewinding = false;
        m_fields->m_rewindIndex = 0;
        m_fields->m_hoverActive = false;
        m_fields->m_ufoJumpsLeft = 3;
        m_fields->m_suddenDeathTimer = 3.0f;
        m_fields->m_suddenDeathActive = false;
        m_fields->m_modeTriggerUsed = false;
        m_fields->m_maxRewindSeconds = 10; 

        return true;
    }

    void update(float dt) {
        // --- CONFIGURABLE REWIND ORB LOGIC (ORB 6) ---
        if (m_fields->m_isRewinding) {
            if (m_fields->m_recordedFrames.empty() || m_fields->m_rewindIndex == 0) {
                m_fields->m_isRewinding = false;
                if (this->m_player1) {
                    this->m_player1->setOpacity(255);
                    this->m_player1->resetObject();
                }
                m_fields->m_recordedFrames.clear();
                Notification::create("Time Resumed", NotificationIcon::Success)->show();
                return;
            }

            if (m_fields->m_rewindIndex > 3) m_fields->m_rewindIndex -= 3;
            else m_fields->m_rewindIndex = 0;

            auto pastState = m_fields->m_recordedFrames[m_fields->m_rewindIndex];
            this->m_player1->setPosition(pastState.position);
            this->m_player1->setRotation(pastState.rotation);
            this->m_player1->m_yVelocity = pastState.yVelocity;
            this->m_player1->m_isUpsideDown = pastState.isUpsideDown;
            this->m_levelSettings->m_level->m_isPlatformer = pastState.isPlatformer;

            float secondsLeft = (float)m_fields->m_rewindIndex / 60.0f;
            std::string timeStr = "Rewinding... " + std::to_string((int)secondsLeft) + "s / " + std::to_string(m_fields->m_maxRewindSeconds) + "s max";
            Notification::create(timeStr, NotificationIcon::Info)->show();

            this->m_player1->setOpacity(120);
            this->m_player1->setColor(cccc3(0, 150, 255));
            return; 
        }

        // --- NORMAL GAMEPLAY LOOP ---
        PlayLayer::update(dt);
        if (!this->m_player1 || this->m_player1->m_isDead) return;

        FrameState currentFrame = {
            this->m_player1->getPosition(),
            this->m_player1->getRotation(),
            (float)this->m_player1->m_yVelocity,
            this->m_player1->m_isUpsideDown,
            this->m_levelSettings->m_level->m_isPlatformer
        };
        m_fields->m_recordedFrames.push_back(currentFrame);

        size_t maxAllowedFrames = static_cast<size_t>(m_fields->m_maxRewindSeconds * 60);
        if (m_fields->m_recordedFrames.size() > maxAllowedFrames) { 
            m_fields->m_recordedFrames.erase(m_fields->m_recordedFrames.begin());
        }

        float playerX = this->m_player1->getPositionX();
        bool isPlatformerNow = this->m_levelSettings->m_level->m_isPlatformer;

        if (m_fields->m_hoverActive) {
            this->m_player1->m_yVelocity = 0;
        }

        // Trigger 7: Platformer-Normal Switch Trigger
        if (playerX > 1500.0f && !m_fields->m_modeTriggerUsed) {
            m_fields->m_modeTriggerUsed = true;
            this->m_levelSettings->m_level->m_isPlatformer = !isPlatformerNow; 
            Notification::create("TRIGGER: Mode Switched!", NotificationIcon::Success)->show();
        }

        // Trigger 9: Sudden Death (Platformer Only)
        if (playerX > 2000.0f && isPlatformerNow && !m_fields->m_suddenDeathActive) {
            m_fields->m_suddenDeathActive = true;
        }

        if (m_fields->m_suddenDeathActive) {
            if (!isPlatformerNow) { 
                m_fields->m_suddenDeathActive = false;
                m_fields->m_suddenDeathTimer = 3.0f;
            } else {
                m_fields->m_suddenDeathTimer -= dt;
                std::string clockStr = "⏱️ " + std::to_string((int)m_fields->m_suddenDeathTimer + 1) + "s";
                Notification::create(clockStr, NotificationIcon::Warning)->show();

                if (m_fields->m_suddenDeathTimer <= 0.0f) {
                    this->m_player1->destroyPlayer(this->m_player1, false);
                }
            }
        }
    }

    void pushButton(int playerNum, bool isFirstButton) {
        if (m_fields->m_isRewinding) return;
        PlayLayer::pushButton(playerNum, isFirstButton);
        if (!this->m_player1 || this->m_player1->m_isDead) return;

        float playerY = this->m_player1->getPositionY();

        // Orb 3: Multi-Jump Orb
        if (m_fields->m_ufoJumpsLeft > 0 && this->m_player1->m_isFalling) {
            this->m_player1->m_yVelocity = 12.0f;
            m_fields->m_ufoJumpsLeft--;
            Notification::create("UFO Jumps: " + std::to_string(m_fields->m_ufoJumpsLeft), NotificationIcon::Info)->show();
        }

        // Dynamic Rewind Orb Adjustment via Editor Slider Simulation
        if (this->m_player1->m_isFalling && playerY > 500.0f && !m_fields->m_recordedFrames.empty()) {
            int detectedSliderValue = static_cast<int>(playerY / 40.0f);
            if (detectedSliderValue < 1) detectedSliderValue = 1;
            if (detectedSliderValue > 20) detectedSliderValue = 20;
            
            m_fields->m_maxRewindSeconds = detectedSliderValue;
            m_fields->m_isRewinding = true;
            m_fields->m_rewindIndex = m_fields->m_recordedFrames.size() - 1;
            return;
        }

        // Mid-Air Interaction Detection for Remaining Orbs
        if (this->m_player1->m_isFalling) {
            if (playerY > 150.0f && playerY < 200.0f) { // Orb 1: Gravity Shift
                this->m_player1->m_yVelocity = 4.0f;
                this->m_player1->m_isUpsideDown = !this->m_player1->m_isUpsideDown;
                Notification::create("Gravity Shift", NotificationIcon::Info)->show();
            }
            if (playerY > 250.0f && playerY < 300.0f) { // Orb 4: Scale Orb
                this->m_player1->setScaleX(0.4f);
                this->m_player1->setScaleY(1.6f);
                Notification::create("Deformed Cube", NotificationIcon::Info)->show();
            }
            if (playerY > 350.0f && playerY < 400.0f) { // Orb 5: Flash Orb
                this->m_backgroundLayer->runAction(CCFadeOut::create(0.1f));
                this->m_backgroundLayer->runAction(CCFadeIn::create(0.9f));
            }
            if (playerY > 410.0f && playerY < 460.0f) { // Orb 8: Platformer-Normal Orb
                bool mode = this->m_levelSettings->m_level->m_isPlatformer;
                this->m_levelSettings->m_level->m_isPlatformer = !mode;
                Notification::create("Orb 8: Mode Switched!", NotificationIcon::Success)->show();
            }
            if (playerY > 100.0f && playerY < 140.0f) { // Orb 2: Hover Orb
                m_fields->m_hoverActive = true;
            }
        }
    }

    void releaseButton(int playerNum, bool isFirstButton) {
        PlayLayer::releaseButton(playerNum, isFirstButton);
        m_fields->m_hoverActive = false; 
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        m_fields->m_recordedFrames.clear();
        m_fields->m_isRewinding = false;
        m_fields->m_rewindIndex = 0;
        m_fields->m_hoverActive = false;
        m_fields->m_ufoJumpsLeft = 3;
        m_fields->m_suddenDeathTimer = 3.0f;
        m_fields->m_suddenDeathActive = false;
        m_fields->m_modeTriggerUsed = false;

        if (this->m_player1) {
            this->m_player1->setScale(1.0f);
            this->m_player1->setOpacity(255);
        }
    }
};
