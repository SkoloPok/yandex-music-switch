#pragma once
// Экран поиска — ввод запроса и отображение результатов

#include "screen.h"
#include "../api/types.h"
#include <vector>
#include <string>

class App;

class SearchScreen : public Screen {
public:
    explicit SearchScreen(App* app);

    void handleInput(uint64_t kDown, uint64_t kHeld) override;
    void update() override;
    void render() override;
    void onEnter() override;
    void onExit() override;

private:
    void performSearch();

    std::string query_;
    SearchResult results_;
    bool hasResults_ = false;
    bool loading_ = false;

    int activeTab_ = 0;       // 0 = Треки, 1 = Альбомы, 2 = Артисты
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;

    static constexpr int VISIBLE_ITEMS = 7;
};
