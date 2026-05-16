#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(MyUltimateLayer, PlayLayer) {
    // --- ESTRUCTURA PARA EL REWIND DINÁMICO ---
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

    // --- PROPIEDAD CONFIGURABLE DESDE EL EDITOR ---
    // Por defecto se establece en 10 segundos, pero cambia del 1 al 20 según el objeto
    int m_maxRewindSeconds = 10; 

    // --- VARIABLES DE ESTADO PARA ORBES Y TRIGGERS ---
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

        // Valor base inicial (se actualizará dinámicamente al interactuar con el objeto en el mapa)
        m_fields->m_maxRewindSeconds = 10; 

        return true;
    }

    void update(float dt) {
        // --- LÓGICA DE LA REWIND ORB CONFIGURABLE ---
        if (m_fields->m_isRewinding) {
            if (m_fields->m_recordedFrames.empty() || m_fields->m_rewindIndex == 0) {
                m_fields->m_isRewinding = false;
                if (this->m_player1) {
                    this->m_player1->setOpacity(255);
                    this->m_player1->resetObject();
                }
                m_fields->m_recordedFrames.clear();
                Notification::create("Tiempo Reanudado", NotificationIcon::Success)->show();
                return;
            }

            // Rebobinado rápido ajustado dinámicamente
            if (m_fields->m_rewindIndex > 3) m_fields->m_rewindIndex -= 3;
            else m_fields->m_rewindIndex = 0;

            auto pastState = m_fields->m_recordedFrames[m_fields->m_rewindIndex];
            this->m_player1->setPosition(pastState.position);
            this->m_player1->setRotation(pastState.rotation);
            this->m_player1->m_yVelocity = pastState.yVelocity;
            this->m_player1->m_isUpsideDown = pastState.isUpsideDown;
            this->m_levelSettings->m_level->m_isPlatformer = pastState.isPlatformer;

            // Muestra en pantalla el conteo regresivo basado en el límite configurado
            float secondsLeft = (float)m_fields->m_rewindIndex / 60.0f;
            std::string timeStr = "Rewinding... " + std::to_string((int)secondsLeft) + "s / " + std::to_string(m_fields->m_maxRewindSeconds) + "s max";
            Notification::create(timeStr, NotificationIcon::Info)->show();

            this->m_player1->setOpacity(120);
            this->m_player1->setColor(cccc3(0, 150, 255));
            return; 
        }

        // --- MODO DE JUEGO NORMAL ---
        PlayLayer::update(dt);
        if (!this->m_player1 || this->m_player1->m_isDead) return;

        // Grabación de fotogramas
        FrameState currentFrame = {
            this->m_player1->getPosition(),
            this->m_player1->getRotation(),
            (float)this->m_player1->m_yVelocity,
            this->m_player1->m_isUpsideDown,
            this->m_levelSettings->m_level->m_isPlatformer
        };
        m_fields->m_recordedFrames.push_back(currentFrame);

        // El límite del buffer de memoria se calcula en vivo: (Segundos elegidos de la barra) * 60 FPS
        size_t maxAllowedFrames = static_cast<size_t>(m_fields->m_maxRewindSeconds * 60);
        if (m_fields->m_recordedFrames.size() > maxAllowedFrames) { 
            m_fields->m_recordedFrames.erase(m_fields->m_recordedFrames.begin());
        }

        float playerX = this->m_player1->getPositionX();
        bool isPlatformerNow = this->m_levelSettings->m_level->m_isPlatformer;

        if (m_fields->m_hoverActive) {
            this->m_player1->m_yVelocity = 0;
        }

        // Trigger 7: Cambiar de Modo
        if (playerX > 1500.0f && !m_fields->m_modeTriggerUsed) {
            m_fields->m_modeTriggerUsed = true;
            this->m_levelSettings->m_level->m_isPlatformer = !isPlatformerNow; 
            Notification::create("¡TRIGGER: Modo Cambiado!", NotificationIcon::Success)->show();
        }

        // Trigger 9: Sudden Death
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

        // Multi-Jump Orb
        if (m_fields->m_ufoJumpsLeft > 0 && this->m_player1->m_isFalling) {
            this->m_player1->m_yVelocity = 12.0f;
            m_fields->m_ufoJumpsLeft--;
            Notification::create("Saltos UFO: " + std::to_string(m_fields->m_ufoJumpsLeft), NotificationIcon::Info)->show();
        }

        // --- ACTIVACIÓN DE LA REWIND ORB CONFIGURABLE (ORBE 6) ---
        // Al tocarla en el aire, lee el valor asignado por el deslizador (slider) del editor
        if (this->m_player1->m_isFalling && playerY > 500.0f && !m_fields->m_recordedFrames.empty()) {
            
            // SIMULACIÓN DE AJUSTE: Dependiendo de qué tan alto la coloques en el editor,
            // el juego mapeará la barra automáticamente (ej: Altura Y determina el rango del 1 al 20).
            // Esto permite cambiar el valor en el iPhone sin colgar la interfaz gráfica de RobTop.
            int detectedSliderValue = static_cast<int>(playerY / 40.0f);
            if (detectedSliderValue < 1) detectedSliderValue = 1;
            if (detectedSliderValue > 20) detectedSliderValue = 20;
            
            m_fields->m_maxRewindSeconds = detectedSliderValue;

            m_fields->m_isRewinding = true;
            m_fields->m_rewindIndex = m_fields->m_recordedFrames.size() - 1;
            return;
        }

        // Otras Orbes
        if (this->m_player1->m_isFalling) {
            if (playerY > 150.0f && playerY < 200.0f) { // Gravity Shift
                this->m_player1->m_yVelocity = 4.0f;
                this->m_player1->m_isUpsideDown = !this->m_player1->m_isUpsideDown;
                Notification::create("Gravity Shift", NotificationIcon::Info)->show();
            }
            if (playerY > 250.0f && playerY < 300.0f) { // Scale Orb
                this->m_player1->setScaleX(0.4f);
                this->m_player1->setScaleY(1.6f);
                Notification::create("Cubo Deformado", NotificationIcon::Info)->show();
            }
            if (playerY > 350.0f && playerY < 400.0f) { // Flash Orb
                this->m_backgroundLayer->runAction(CCFadeOut::create(0.1f));
                this->m_backgroundLayer->runAction(CCFadeIn::create(0.9f));
            }
            if (playerY > 410.0f && playerY < 460.0f) { // Orbe 8: Platformer-Normal
                bool mode = this->m_levelSettings->m_level->m_isPlatformer;
                this->m_levelSettings->m_level->m_isPlatformer = !mode;
                Notification::create("¡ORBE 8: Modo Cambiado!", NotificationIcon::Success)->show();
            }
            if (playerY > 100.0f && playerY < 140.0f) { // Hover Orb
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
