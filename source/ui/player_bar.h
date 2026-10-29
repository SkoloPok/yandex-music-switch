#pragma once
// Панель плеера — полоса внизу экрана с информацией о текущем треке

#include <vector>
#include <string>
#include <cstdint>

class App;

class PlayerBar {
public:
    explicit PlayerBar(App* app);

    void update();
    void render();

    // Видимость: показываем только когда есть текущий трек
    bool isVisible() const;

private:
    App* app_;
};
