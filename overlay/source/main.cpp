#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "ipc_client.h"

class YandexGui : public tsl::Gui {
public:
    YandexGui() = default;

    virtual tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("Яндекс Музыка", "Фоновый плеер");
        auto* list = new tsl::elm::List();

        statusItem_ = new tsl::elm::ListItem("Проверка связи...");
        list->addItem(statusItem_);

        auto* toggleBtn = new tsl::elm::ListItem("▶ / ⏸ Пауза / Плей");
        toggleBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("TOGGLE");
                return true;
            }
            return false;
        });
        list->addItem(toggleBtn);

        auto* nextBtn = new tsl::elm::ListItem("⏭ Следующий трек [X]");
        nextBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("NEXT");
                return true;
            }
            return false;
        });
        list->addItem(nextBtn);

        auto* prevBtn = new tsl::elm::ListItem("⏮ Предыдущий трек");
        prevBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("PREV");
                return true;
            }
            return false;
        });
        list->addItem(prevBtn);

        auto* likeBtn = new tsl::elm::ListItem("❤️ Мне нравится [Y]");
        likeBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("LIKE");
                return true;
            }
            return false;
        });
        list->addItem(likeBtn);

        auto* headerModes = new tsl::elm::CategoryHeader("Потоки");
        list->addItem(headerModes);

        auto* waveBtn = new tsl::elm::ListItem("⚡ Моя волна (Бесконечный поток)");
        waveBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("MODE wave");
                return true;
            }
            return false;
        });
        list->addItem(waveBtn);

        auto* likedBtn = new tsl::elm::ListItem("❤️ Любимые треки");
        likedBtn->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                IpcClient::sendCommand("MODE liked");
                return true;
            }
            return false;
        });
        list->addItem(likedBtn);

        auto* headerVol = new tsl::elm::CategoryHeader("Громкость");
        list->addItem(headerVol);

        volumeBar_ = new tsl::elm::TrackBar("\uE015"); // Иконка громкости
        volumeBar_->setProgress(85);
        volumeBar_->setValueChangedListener([](u8 val) {
            IpcClient::sendCommand("VOLUME " + std::to_string(val));
        });
        list->addItem(volumeBar_);

        frame->setContent(list);
        return frame;
    }

    virtual void update() override {
        updateCounter_++;
        if (updateCounter_ % 30 == 0) { // Примерно раз в полсекунды
            OverlayStatus st = IpcClient::getStatus();
            if (!st.connected) {
                if (statusItem_) statusItem_->setText("❌ sys-yandex не запущен");
            } else {
                std::string icon = (st.state == "playing") ? "▶ " : (st.state == "paused" ? "⏸ " : "■ ");
                std::string title = st.title.empty() ? "Ожидание..." : (st.title + " - " + st.artist);
                if (statusItem_) statusItem_->setText(icon + title);
                if (volumeBar_) volumeBar_->setProgress(st.volume);
            }
        }
    }

    virtual bool handleInput(u64 keysDown, u64 /*keysHeld*/, const HidTouchState& /*touchPos*/,
                             HidAnalogStickState /*joyStickPosLeft*/, HidAnalogStickState /*joyStickPosRight*/) override {
        // Быстрые клавиши геймпада прямо в меню:
        if (keysDown & HidNpadButton_X) {
            IpcClient::sendCommand("NEXT");
            return true;
        }
        if (keysDown & HidNpadButton_Y) {
            IpcClient::sendCommand("LIKE");
            return true;
        }
        return false;
    }

private:
    tsl::elm::ListItem* statusItem_{nullptr};
    tsl::elm::TrackBar* volumeBar_{nullptr};
    uint32_t updateCounter_{0};
};

class YandexOverlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        socketInitializeDefault();
    }

    virtual void exitServices() override {
        socketExit();
    }

    virtual void onShow() override {}
    virtual void onHide() override {}

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return std::make_unique<YandexGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<YandexOverlay>(argc, argv);
}
