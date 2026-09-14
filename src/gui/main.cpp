#include "engine/gui/MyGUI.hpp"
#include "planner/Planner.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

constexpr float sidebarWidth = 350.f;

sf::String fromUtf8(const std::string& value) {
    return sf::String::fromUtf8(value.begin(), value.end());
}

std::string toUtf8(const sf::String& value) {
    const auto bytes = value.toUtf8();
    return {bytes.begin(), bytes.end()};
}

std::string displayTime(const std::string& timestamp) {
    if (timestamp.empty()) {
        return "--";
    }
    std::string result = timestamp;
    if (result.size() > 19) {
        result.resize(19);
    }
    if (result.size() > 10 && result[10] == 'T') {
        result[10] = ' ';
    }
    return result + " UTC";
}

std::string oneLine(std::string text) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    return text;
}

std::string shortened(const std::string& value, std::size_t maxCharacters) {
    sf::String text = fromUtf8(oneLine(value));
    if (text.getSize() <= maxCharacters) {
        return toUtf8(text);
    }
    return toUtf8(text.substring(0, maxCharacters)) + "…";
}

struct WrappedText {
    sf::String text;
    std::size_t lines = 1;
};

WrappedText wrapText(const std::string& value, std::size_t columns) {
    const sf::String source = fromUtf8(value);
    sf::String result;
    std::size_t column = 0;
    std::size_t lines = 1;
    for (char32_t character : source) {
        if (character == U'\r') {
            continue;
        }
        if (character == U'\n') {
            result += U'\n';
            column = 0;
            ++lines;
            continue;
        }
        if (column >= columns) {
            result += U'\n';
            column = 0;
            ++lines;
        }
        result += character;
        ++column;
    }
    return {result, lines};
}

bool blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

gui::Style style(sf::Color background, sf::Color outline, float thickness = 1.f) {
    gui::Style result;
    result.set(background, outline, thickness);
    return result;
}

void setTextColors(gui::TextObject& object, sf::Color normal, sf::Color over,
                   sf::Color focus) {
    object.textStyle(gui::UIBase::Normal).set(normal, normal);
    object.textStyle(gui::UIBase::Over).set(over, over);
    object.textStyle(gui::UIBase::Focus).set(focus, focus);
}

void setFill(gui::UIBase& object, const gui::Style& value) {
    object.setStyle(value, value, value);
}

void setRect(gui::UIBase& object, float left, float top, float width, float height) {
    object.setPosition({left, top});
    object.setSize({width, height});
}

void setHorizontalFill(gui::UIBase& object, float left, float right, float top, float height) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, right}},
        {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::TopEdge, top},
         {gui::UIBase::Anchor::Height, height}});
}

void setVerticalFill(gui::UIBase& object, float left, float width, float top, float bottom) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Width, width}},
        {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::TopEdge, top},
         {gui::UIBase::Anchor::Bottom, gui::UIBase::Relative::BottomEdge, bottom}});
}

void setFillRect(gui::UIBase& object, float left, float right, float top, float bottom) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, right}},
        {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::TopEdge, top},
         {gui::UIBase::Anchor::Bottom, gui::UIBase::Relative::BottomEdge, bottom}});
}

void setUpperHalf(gui::UIBase& object, float left, float right, float top, float middleGap) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, right}},
        {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::TopEdge, top},
         {gui::UIBase::Anchor::Bottom, gui::UIBase::Relative::MidLine, -middleGap}});
}

void setLowerHalf(gui::UIBase& object, float left, float right, float middleGap, float bottom) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, right}},
        {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::MidLine, middleGap},
         {gui::UIBase::Anchor::Bottom, gui::UIBase::Relative::BottomEdge, bottom}});
}

void setBottomPanel(gui::UIBase& object, float left, float right, float bottom, float height) {
    object.setPositionRelative(
        {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, left},
         {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, right}},
        {{gui::UIBase::Anchor::Bottom, gui::UIBase::Relative::BottomEdge, bottom},
         {gui::UIBase::Anchor::Height, height}});
}

void setRightAnchored(gui::UIBase& object, float right, float top, float width, float height) {
    object.setPosition({-right, top},
                       {gui::UIBase::Anchor::Right, gui::UIBase::Anchor::Top},
                       {gui::UIBase::Relative::RightEdge, gui::UIBase::Relative::TopEdge});
    object.setSize({width, height});
}

std::string logTypeName(const std::string& type) {
    return type == "progress" ? "进度" : "备注";
}

std::string targetStatusName(const std::string& status) {
    if (status == "completed") {
        return "已完成";
    }
    if (status == "archived") {
        return "已归档";
    }
    return "进行中";
}

std::string targetStateSummary(const myplan::Target& target) {
    if (target.status == "archived") {
        if (target.completedAt) {
            return "完成并归档：" + displayTime(*target.completedAt);
        }
        return "归档：" + displayTime(target.archivedAt.value_or(""));
    }
    if (target.status == "completed") {
        return "完成：" + displayTime(target.completedAt.value_or(""));
    }
    return "状态：进行中";
}

std::string sidebarSummary(const myplan::Target& target) {
    std::string result = "[" + targetStatusName(target.status) + "] " +
                         shortened(target.title, 17) + "\n";
    result += "截止：" + (target.deadline ? shortened(*target.deadline, 24) : "未设置") + "\n";
    if (target.log.empty()) {
        result += "暂无记录";
        return result;
    }
    std::size_t count = 0;
    for (auto iterator = target.log.rbegin(); iterator != target.log.rend() && count < 3;
         ++iterator, ++count) {
        result += "[" + logTypeName(iterator->type) + "] " +
                  shortened(iterator->text, 19);
        if (count < 2 && std::next(iterator) != target.log.rend()) {
            result += "\n";
        }
    }
    return result;
}

struct AppOptions {
    std::filesystem::path dataFile;
    std::filesystem::path executable;
    bool help = false;
};

std::filesystem::path pathFromUtf8(const std::string& value) {
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string(begin, begin + value.size()));
}

#ifdef _WIN32
std::string utf8FromWide(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("无法把命令行参数转换为 UTF-8");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::vector<std::string> commandLineArguments() {
    int count = 0;
    wchar_t** wideArguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wideArguments) {
        throw std::runtime_error("无法读取 Windows 命令行");
    }
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    try {
        for (int index = 0; index < count; ++index) {
            result.push_back(utf8FromWide(wideArguments[index]));
        }
    } catch (...) {
        LocalFree(wideArguments);
        throw;
    }
    LocalFree(wideArguments);
    return result;
}
#else
std::vector<std::string> commandLineArguments(int argc, char* argv[]) {
    return {argv, argv + argc};
}
#endif

AppOptions parseOptions(const std::vector<std::string>& arguments) {
    AppOptions options;
    bool hasExplicitDataFile = false;
#ifdef _WIN32
    if (const wchar_t* environmentPath = _wgetenv(L"MYPLAN_DATA")) {
        options.dataFile = environmentPath;
        hasExplicitDataFile = true;
    }
#else
    if (const char* environmentPath = std::getenv("MYPLAN_DATA")) {
        options.dataFile = environmentPath;
        hasExplicitDataFile = true;
    }
#endif
    if (!arguments.empty()) {
        options.executable = pathFromUtf8(arguments[0]);
    }
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        if (arguments[index] == "--help" || arguments[index] == "-h") {
            options.help = true;
        } else if (arguments[index] == "--data") {
            if (++index >= arguments.size()) {
                throw std::runtime_error("--data 后面需要数据文件路径");
            }
            options.dataFile = pathFromUtf8(arguments[index]);
            hasExplicitDataFile = true;
        } else {
            throw std::runtime_error("未知参数：" + arguments[index]);
        }
    }
    if (!hasExplicitDataFile) {
        std::error_code error;
        const std::filesystem::path executable = options.executable.empty()
                                                     ? std::filesystem::path{}
                                                     : std::filesystem::absolute(
                                                           options.executable, error);
        options.dataFile = !error && executable.has_parent_path()
                               ? executable.parent_path() / "myplan-data.json"
                               : std::filesystem::path("myplan-data.json");
    }
    return options;
}

bool loadDefaultFont(const std::filesystem::path& executable) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "resources" / "fonts" / "default.TTF"};
    if (!executable.empty()) {
        std::error_code error;
        const auto executablePath = std::filesystem::absolute(executable, error);
        if (!error) {
            candidates.push_back(executablePath.parent_path() / "resources" / "fonts" /
                                 "default.TTF");
        }
    }
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return fontManager.loadFont(std::filesystem::absolute(candidate));
        }
    }
    return false;
}

class PlannerGui {
public:
    explicit PlannerGui(std::filesystem::path dataFile) : store_(std::move(dataFile)) {
        initializeStyles();
    }

    int run() {
        manager_.open("main");
        buildUi();

        window_.create(sf::VideoMode({1280, 800}), L"My Plan · 规划日志",
                       sf::Style::Close | sf::Style::Resize, sf::State::Windowed);
        window_.setMinimumSize(sf::Vector2u(980, 640));
        window_.setFramerateLimit(60);
        manager_.update(static_cast<sf::Vector2f>(window_.getSize()));

        TickManager ticks;
        bool running = true;
        while (running) {
            for (TickManager::TickCount remaining = ticks.getCurrentTick(); remaining > 0;
                 --remaining) {
                while (const std::optional event = window_.pollEvent()) {
                    if (event->is<sf::Event::Closed>()) {
                        running = false;
                        break;
                    }
                    if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                        window_.setView(sf::View(sf::FloatRect(
                            {0.f, 0.f}, static_cast<sf::Vector2f>(resized->size))));
                    } else {
                        manager_.update(event);
                    }
                }
                processUiEvents();
                restorePendingDetailScroll();
                manager_.update(static_cast<sf::Vector2f>(window_.getSize()));
            }
            if (!running) {
                break;
            }
            window_.clear(sf::Color(244, 246, 250));
            manager_.draw(window_);
            window_.display();
        }
        return 0;
    }

private:
    struct ItemRef {
        std::string targetId;
        std::string itemId;
    };

    struct DetailScrollState {
        std::optional<sf::Vector2f> subtargets;
        std::optional<sf::Vector2f> logs;
    };

    myplan::PlannerStore store_;
    gui::UIwindowManager manager_;
    sf::RenderWindow window_;
    std::vector<myplan::Target> targets_;
    std::optional<std::string> selectedTargetId_;
    std::string sidebarFilter_ = "active";
    std::string composerMode_ = "subtarget";
    std::string subtargetSortMode_ = "sortDeadline";
    bool subtargetSortAscending_ = true;
    std::string statusMessage_;
    bool statusIsError_ = false;
    std::unordered_map<std::string, std::string> targetButtons_;
    std::unordered_map<std::string, std::pair<std::string, std::string>> completeButtons_;
    std::unordered_map<std::string, ItemRef> editSubtargetButtons_;
    std::unordered_map<std::string, ItemRef> deleteSubtargetButtons_;
    std::unordered_map<std::string, ItemRef> editLogButtons_;
    std::unordered_map<std::string, ItemRef> deleteLogButtons_;
    std::optional<ItemRef> editingSubtarget_;
    std::optional<ItemRef> deletingSubtarget_;
    std::optional<ItemRef> editingLog_;
    std::optional<ItemRef> deletingLog_;
    std::optional<std::string> editingTargetId_;
    std::optional<std::string> deletingTargetId_;
    std::optional<DetailScrollState> pendingDetailScrollRestore_;

    gui::Style rootStyle_;
    gui::Style sidebarStyle_;
    gui::Style panelStyle_;
    gui::Style cardStyle_;
    gui::Style progressStyle_;
    gui::Style noteStyle_;
    gui::Style subtargetStyle_;
    gui::Style completedStyle_;
    gui::Style buttonNormal_;
    gui::Style buttonOver_;
    gui::Style buttonFocus_;
    gui::Style completeNormal_;
    gui::Style completeOver_;
    gui::Style completeFocus_;
    gui::Style deleteNormal_;
    gui::Style deleteOver_;
    gui::Style deleteFocus_;
    gui::Style optionNormal_;
    gui::Style optionOver_;
    gui::Style optionSelected_;
    gui::Style tabNormal_;
    gui::Style tabOver_;
    gui::Style tabSelected_;
    gui::Style inputNormal_;
    gui::Style inputOver_;
    gui::Style inputFocus_;

    void initializeStyles() {
        rootStyle_ = style(sf::Color(244, 246, 250), sf::Color::Transparent, 0);
        sidebarStyle_ = style(sf::Color(234, 238, 245), sf::Color(205, 211, 222), 1);
        panelStyle_ = style(sf::Color(250, 251, 253), sf::Color(211, 216, 226), 1);
        cardStyle_ = style(sf::Color::White, sf::Color(215, 220, 229), 1);
        progressStyle_ = style(sf::Color(239, 249, 242), sf::Color(160, 209, 173), 1);
        noteStyle_ = style(sf::Color(255, 249, 232), sf::Color(225, 200, 126), 1);
        subtargetStyle_ = style(sf::Color(241, 246, 255), sf::Color(153, 183, 231), 1);
        completedStyle_ = style(sf::Color(240, 242, 245), sf::Color(194, 199, 207), 1);
        buttonNormal_ = style(sf::Color(63, 116, 214), sf::Color(45, 91, 177), 1);
        buttonOver_ = style(sf::Color(78, 133, 231), sf::Color(45, 91, 177), 1);
        buttonFocus_ = style(sf::Color(48, 96, 188), sf::Color(37, 73, 142), 2);
        completeNormal_ = style(sf::Color(48, 150, 85), sf::Color(34, 111, 61), 1);
        completeOver_ = style(sf::Color(60, 169, 99), sf::Color(34, 111, 61), 1);
        completeFocus_ = style(sf::Color(38, 128, 72), sf::Color(27, 91, 50), 2);
        deleteNormal_ = style(sf::Color(202, 65, 65), sf::Color(157, 42, 42), 1);
        deleteOver_ = style(sf::Color(222, 77, 77), sf::Color(157, 42, 42), 1);
        deleteFocus_ = style(sf::Color(177, 46, 46), sf::Color(129, 31, 31), 2);
        optionNormal_ = style(sf::Color::White, sf::Color(205, 211, 222), 1);
        optionOver_ = style(sf::Color(238, 244, 255), sf::Color(132, 164, 218), 1);
        optionSelected_ = style(sf::Color(218, 231, 255), sf::Color(63, 116, 214), 2);
        tabNormal_ = style(sf::Color::White, sf::Color::Transparent, 0);
        tabOver_ = style(sf::Color(238, 244, 255), sf::Color::Transparent, 0);
        tabSelected_ = style(sf::Color(218, 231, 255), sf::Color::Transparent, 0);
        inputNormal_ = style(sf::Color::White, sf::Color(190, 197, 209), 1);
        inputOver_ = style(sf::Color::White, sf::Color(120, 152, 207), 1);
        inputFocus_ = style(sf::Color::White, sf::Color(63, 116, 214), 2);
    }

    const myplan::Target* selectedTarget() const {
        if (!selectedTargetId_) {
            return nullptr;
        }
        const auto result = std::find_if(targets_.begin(), targets_.end(), [&](const auto& item) {
            return item.id == *selectedTargetId_;
        });
        return result == targets_.end() ? nullptr : &*result;
    }

    bool matchesSidebarFilter(const myplan::Target& target) const {
        return sidebarFilter_ == "archived" ? target.status == "archived"
                                             : target.status != "archived";
    }

    void loadTargets() {
        try {
            targets_ = store_.listTargets();
            if (selectedTargetId_) {
                const myplan::Target* selected = selectedTarget();
                if (!selected || !matchesSidebarFilter(*selected)) {
                    selectedTargetId_.reset();
                }
            }
            if (!selectedTargetId_) {
                const auto firstVisible = std::find_if(
                    targets_.begin(), targets_.end(),
                    [&](const myplan::Target& target) { return matchesSidebarFilter(target); });
                if (firstVisible != targets_.end()) {
                    selectedTargetId_ = firstVisible->id;
                }
            }
        } catch (const std::exception& error) {
            targets_.clear();
            selectedTargetId_.reset();
            statusMessage_ = error.what();
            statusIsError_ = true;
        }
    }

    void configureText(gui::TextObject& text, const sf::String& value, unsigned int size,
                       gui::UIBase::Align horizontal = gui::UIBase::Align::Left,
                       gui::UIBase::Align vertical = gui::UIBase::Align::Mid) {
        text.setText(value);
        text.setFont("default");
        text.setCharacterSize(static_cast<int>(size));
        text.setAlign(horizontal, vertical);
        setTextColors(text, sf::Color(38, 43, 53), sf::Color(38, 43, 53),
                      sf::Color(38, 43, 53));
    }

    void configureButton(gui::ButtonObject& button, const sf::String& value,
                         unsigned int size = 18) {
        configureText(button, value, size, gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
        button.setStyle(buttonNormal_, buttonOver_, buttonFocus_);
        setTextColors(button, sf::Color::White, sf::Color::White, sf::Color::White);
    }

    void configureDeleteButton(gui::ButtonObject& button, const sf::String& value,
                               unsigned int size = 17) {
        configureText(button, value, size, gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
        button.setStyle(deleteNormal_, deleteOver_, deleteFocus_);
        setTextColors(button, sf::Color::White, sf::Color::White, sf::Color::White);
    }

    void configureCompleteButton(gui::ButtonObject& button, const sf::String& value,
                                 unsigned int size = 17) {
        configureText(button, value, size, gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
        button.setStyle(completeNormal_, completeOver_, completeFocus_);
        setTextColors(button, sf::Color::White, sf::Color::White, sf::Color::White);
    }

    void configureInput(gui::InputObject& input, unsigned int size = 19) {
        input.setFont("default");
        input.setCharacterSize(static_cast<int>(size));
        input.setAlign(gui::UIBase::Align::Left, gui::UIBase::Align::Top);
        input.setStyle(inputNormal_, inputOver_, inputFocus_);
        input.setSizeLimit(4000);
        setTextColors(input, sf::Color(42, 47, 57), sf::Color(42, 47, 57),
                      sf::Color(31, 70, 137));
    }

    DetailScrollState captureDetailScroll() {
        DetailScrollState result;
        if (const auto* subtargets = manager_.path_find<gui::AreaObject>(
                "main.content.details.subtargetList")) {
            result.subtargets = subtargets->getScroll();
        }
        if (const auto* logs =
                manager_.path_find<gui::AreaObject>("main.content.details.logList")) {
            result.logs = logs->getScroll();
        }
        return result;
    }

    void restoreDetailScroll(const DetailScrollState& state) {
        if (state.subtargets) {
            if (auto* subtargets = manager_.path_find<gui::AreaObject>(
                    "main.content.details.subtargetList")) {
                subtargets->setScroll(*state.subtargets);
            }
        }
        if (state.logs) {
            if (auto* logs =
                    manager_.path_find<gui::AreaObject>("main.content.details.logList")) {
                logs->setScroll(*state.logs);
            }
        }
    }

    void restorePendingDetailScroll() {
        if (!pendingDetailScrollRestore_) {
            return;
        }
        restoreDetailScroll(*pendingDetailScrollRestore_);
        pendingDetailScrollRestore_.reset();
    }

    void buildUi(bool preserveDetailScroll = true) {
        const DetailScrollState previousScroll =
            preserveDetailScroll ? captureDetailScroll() : DetailScrollState{};
        if (previousScroll.subtargets || previousScroll.logs) {
            pendingDetailScrollRestore_ = previousScroll;
        } else {
            pendingDetailScrollRestore_.reset();
        }
        loadTargets();
        targetButtons_.clear();
        completeButtons_.clear();
        editSubtargetButtons_.clear();
        deleteSubtargetButtons_.clear();
        editLogButtons_.clear();
        deleteLogButtons_.clear();

        auto& root = manager_.window("main");
        root.sub.clear();
        setFill(root, rootStyle_);
        setFillRect(root, 0, 0, 0, 0);

        auto& sidebar = root.path_get<gui::AreaObject>("sidebar");
        setFill(sidebar, sidebarStyle_);
        setVerticalFill(sidebar, 0, sidebarWidth, 0, 0);
        buildSidebar(sidebar);

        auto& content = root.path_get<gui::AreaObject>("content");
        setFill(content, rootStyle_);
        setFillRect(content, sidebarWidth, 0, 0, 0);
        buildContent(content);
    }

    void buildSidebar(gui::AreaObject& sidebar) {
        auto& title = sidebar.path_get<gui::TextObject>("title");
        configureText(title, L"目标概况", 27);
        setRect(title, 14, 8, 220, 38);

        auto& refresh = sidebar.path_get<gui::ButtonObject>("refresh");
        configureButton(refresh, L"刷新", 16);
        setRightAnchored(refresh, 12, 12, 72, 32);

        auto& newLabel = sidebar.path_get<gui::TextObject>("newLabel");
        configureText(newLabel, L"新建大目标", 16);
        setRect(newLabel, 14, 48, 120, 24);

        auto& input = sidebar.path_get<gui::InputObject>("newTargetInput");
        configureInput(input, 18);
        input.setAlign(gui::UIBase::Align::Left, gui::UIBase::Align::Mid);
        setHorizontalFill(input, 14, -104, 75, 38);

        auto& create = sidebar.path_get<gui::ButtonObject>("createTarget");
        configureButton(create, L"创建", 17);
        setRightAnchored(create, 14, 75, 80, 38);

        auto& filterTabs = sidebar.path_get<gui::AreaObject>("filterTabs");
        setFill(filterTabs,
                style(sf::Color(234, 238, 245), sf::Color::Transparent, 0));
        setRect(filterTabs, 6, 119, sidebarWidth - 12, 35);

        auto& activeTab = filterTabs.path_get<gui::OptionObject>("active");
        configureText(activeTab, L"进行中", 16, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Mid);
        activeTab.setStyle(tabNormal_, tabOver_, tabSelected_);
        setRect(activeTab, 0, 0, (sidebarWidth - 12) / 2, 35);

        auto& archivedTab = filterTabs.path_get<gui::OptionObject>("archived");
        configureText(archivedTab, L"已归档", 16, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Mid);
        archivedTab.setStyle(tabNormal_, tabOver_, tabSelected_);
        setRect(archivedTab, (sidebarWidth - 12) / 2, 0,
                (sidebarWidth - 12) / 2, 35);
        filterTabs.setOption(sidebarFilter_);

        auto& list = sidebar.path_get<gui::AreaObject>("targetList");
        setFill(list, sidebarStyle_);
        list.setScrollable({0, 1}, {0, 1});
        // Overlap the tab bottoms by one pixel so the list border becomes the
        // shared lower edge of the selected tab.
        setFillRect(list, 6, -6, 153, -8);

        float y = 4.f;
        std::string selectedKey;
        std::size_t visibleIndex = 0;
        for (const auto& target : targets_) {
            if (!matchesSidebarFilter(target)) {
                continue;
            }
            const std::string key = "target" + std::to_string(visibleIndex++);
            targetButtons_[key] = target.id;
            auto& item = list.path_get<gui::OptionObject>(key);
            configureText(item, fromUtf8(sidebarSummary(target)), 17,
                          gui::UIBase::Align::Left, gui::UIBase::Align::Top);
            item.setStyle(optionNormal_, optionOver_, optionSelected_);
            setHorizontalFill(item, 5, -5, y, 126);
            if (selectedTargetId_ && target.id == *selectedTargetId_) {
                selectedKey = key;
            }
            y += 136.f;
        }
        if (visibleIndex == 0) {
            auto& empty = list.path_get<gui::TextObject>("empty");
            configureText(empty,
                          sidebarFilter_ == "archived"
                              ? L"还没有已归档的大目标。"
                              : L"还没有进行中的大目标。\n在上方输入标题并点击“创建”。",
                          17,
                          gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
            setHorizontalFill(empty, 12, -12, 45, 80);
        } else if (!selectedKey.empty()) {
            list.setOption(selectedKey);
        }
    }

    void buildContent(gui::AreaObject& content) {
        const myplan::Target* target = selectedTarget();
        auto& header = content.path_get<gui::AreaObject>("header");
        setFill(header, panelStyle_);
        setHorizontalFill(header, 0, 0, 0, 132);

        if (!target) {
            auto& empty = content.path_get<gui::TextObject>("empty");
            configureText(empty, L"请选择左侧的大目标，或先创建一个大目标。", 23,
                          gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
            setFillRect(empty, 20, -20, 130, -80);
            buildStatus(header);
            return;
        }

        const bool archivedTarget = target->status == "archived";
        const bool editingTarget = editingTargetId_ && *editingTargetId_ == target->id;
        const float titleRight = archivedTarget ? -388.f : -230.f;
        if (editingTarget) {
            auto& titleInput = header.path_get<gui::InputObject>("titleInput");
            configureInput(titleInput, 22);
            titleInput.setAlign(gui::UIBase::Align::Left, gui::UIBase::Align::Mid);
            titleInput.setText(fromUtf8(target->title));
            setHorizontalFill(titleInput, 18, titleRight, 6, 38);
        } else {
            auto& title = header.path_get<gui::TextObject>("title");
            configureText(title, fromUtf8(target->title), 28);
            setHorizontalFill(title, 18, titleRight, 5, 40);
        }

        if (archivedTarget) {
            auto& archivedLabel = header.path_get<gui::TextObject>("archivedLabel");
            configureText(archivedLabel, L"已完成并归档", 14,
                          gui::UIBase::Align::Mid, gui::UIBase::Align::Mid);
            setRightAnchored(archivedLabel, 260, 8, 118, 34);

            auto& restoreTarget = header.path_get<gui::ButtonObject>("restoreTarget");
            configureButton(restoreTarget, L"恢复", 16);
            setRightAnchored(restoreTarget, 176, 8, 76, 34);

            auto& deleteTarget = header.path_get<gui::ButtonObject>("deleteTarget");
            configureDeleteButton(
                deleteTarget,
                deletingTargetId_ && *deletingTargetId_ == target->id ? L"确定"
                                                                       : L"永久删除",
                14);
            setRightAnchored(deleteTarget, 8, 8, 76, 34);
        } else {
            auto& completeTarget = header.path_get<gui::ButtonObject>("completeTarget");
            configureCompleteButton(completeTarget, L"完成并归档", 15);
            setRightAnchored(completeTarget, 92, 8, 128, 34);
        }

        auto& editTarget = header.path_get<gui::ButtonObject>("editTarget");
        configureButton(editTarget, editingTarget ? L"确定" : L"编辑", 16);
        setRightAnchored(editTarget, archivedTarget ? 92 : 8, 8, 76, 34);

        auto& deadlineLabel = header.path_get<gui::TextObject>("deadlineLabel");
        configureText(deadlineLabel, L"截止时间", 16);
        setRect(deadlineLabel, 18, 51, 78, 36);

        auto& deadline = header.path_get<gui::InputObject>("deadlineInput");
        configureInput(deadline, 17);
        deadline.setAlign(gui::UIBase::Align::Left, gui::UIBase::Align::Mid);
        deadline.setText(fromUtf8(target->deadline.value_or("")));
        setRect(deadline, 98, 52, 275, 34);

        auto& saveDeadline = header.path_get<gui::ButtonObject>("saveDeadline");
        configureButton(saveDeadline, L"保存截止时间", 16);
        setRect(saveDeadline, 383, 52, 128, 34);

        auto& deadlineHint = header.path_get<gui::TextObject>("deadlineHint");
        configureText(deadlineHint, L"格式：2026-12-31；留空可清除", 14);
        setHorizontalFill(deadlineHint, 520, -12, 76, 22);
        setTextColors(deadlineHint, sf::Color(103, 110, 124), sf::Color(103, 110, 124),
                      sf::Color(103, 110, 124));

        auto& targetState = header.path_get<gui::TextObject>("targetState");
        configureText(targetState, fromUtf8(targetStateSummary(*target)), 14);
        setHorizontalFill(targetState, 520, -12, 52, 22);
        setTextColors(targetState, sf::Color(74, 82, 96), sf::Color(74, 82, 96),
                      sf::Color(74, 82, 96));
        buildStatus(header);

        auto& details = content.path_get<gui::AreaObject>("details");
        setFill(details, rootStyle_);
        setFillRect(details, 10, -10, 142, -224);

        auto& subheading = details.path_get<gui::TextObject>("subheading");
        configureText(subheading,
                      fromUtf8("小目标（" + std::to_string(target->subtargets.size()) + "）"),
                      20);
        setHorizontalFill(subheading, 8, -380, 0, 34);

        auto& sortLabel = details.path_get<gui::TextObject>("sortLabel");
        configureText(sortLabel, L"排序", 14, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Mid);
        setRightAnchored(sortLabel, 320, 2, 50, 30);

        auto& sortDeadline = details.path_get<gui::OptionObject>("sortDeadline");
        configureText(sortDeadline, L"按截止日期", 14, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Mid);
        sortDeadline.setStyle(optionNormal_, optionOver_, optionSelected_);
        setRightAnchored(sortDeadline, 204, 2, 110, 30);

        auto& sortCreated = details.path_get<gui::OptionObject>("sortCreated");
        configureText(sortCreated, L"按创建日期", 14, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Mid);
        sortCreated.setStyle(optionNormal_, optionOver_, optionSelected_);
        setRightAnchored(sortCreated, 88, 2, 110, 30);
        details.setOption(subtargetSortMode_);

        auto& sortDirection = details.path_get<gui::ButtonObject>("sortDirection");
        configureButton(sortDirection,
                        subtargetSortAscending_ ? L"升序" : L"降序", 14);
        setRightAnchored(sortDirection, 8, 2, 72, 30);

        auto& subtargetList = details.path_get<gui::AreaObject>("subtargetList");
        setFill(subtargetList, panelStyle_);
        subtargetList.setScrollable({0, 1}, {0, 1});
        setUpperHalf(subtargetList, 0, 0, 38, 5);
        buildSubtargets(subtargetList, *target);

        auto& logHeading = details.path_get<gui::TextObject>("logheading");
        configureText(logHeading,
                      fromUtf8("全部日志（" + std::to_string(target->log.size()) +
                               "，最新在前）"),
                      20);
        logHeading.setPositionRelative(
            {{gui::UIBase::Anchor::Left, gui::UIBase::Relative::LeftEdge, 8},
             {gui::UIBase::Anchor::Right, gui::UIBase::Relative::RightEdge, -8}},
            {{gui::UIBase::Anchor::Top, gui::UIBase::Relative::MidLine, 5},
             {gui::UIBase::Anchor::Height, 34}});

        auto& logList = details.path_get<gui::AreaObject>("logList");
        setFill(logList, panelStyle_);
        logList.setScrollable({0, 1}, {0, 1});
        setLowerHalf(logList, 0, 0, 43, 0);
        buildLogs(logList, *target);

        auto& composer = content.path_get<gui::AreaObject>("composer");
        setFill(composer, panelStyle_);
        setBottomPanel(composer, 0, 0, 0, 214);
        buildComposer(composer);
    }

    void buildStatus(gui::AreaObject& header) {
        if (statusMessage_.empty()) {
            return;
        }
        auto& status = header.path_get<gui::TextObject>("status");
        configureText(status, fromUtf8(shortened(statusMessage_, 38)), 15,
                      gui::UIBase::Align::Left, gui::UIBase::Align::Top);
        setHorizontalFill(status, 18, -18, 105, 22);
        const sf::Color color = statusIsError_ ? sf::Color(185, 55, 55)
                                               : sf::Color(39, 137, 74);
        setTextColors(status, color, color, color);
    }

    static bool refersTo(const std::optional<ItemRef>& selection,
                         const std::string& targetId, const std::string& itemId) {
        return selection && selection->targetId == targetId && selection->itemId == itemId;
    }

    void buildSubtargets(gui::AreaObject& detail, const myplan::Target& target) {
        float y = 6.f;

        if (target.subtargets.empty()) {
            auto& empty = detail.path_get<gui::TextObject>("subempty");
            configureText(empty, L"暂无小目标，可在下方选择“添加目标”。", 16);
            setHorizontalFill(empty, 8, -8, y, 42);
            setTextColors(empty, sf::Color(115, 121, 133), sf::Color(115, 121, 133),
                          sf::Color(115, 121, 133));
            y += 50.f;
        } else {
            struct SortableSubtarget {
                const myplan::Subtarget* subtarget;
                std::size_t storageIndex;
            };
            VarianTmap<SortableSubtarget> displayOrder;
            for (std::size_t index = 0; index < target.subtargets.size(); ++index) {
                displayOrder.push_back(
                    SortableSubtarget{&target.subtargets[index], index});
            }
            displayOrder.sort([&](SortableSubtarget* left, SortableSubtarget* right) {
                const auto& leftTarget = *left->subtarget;
                const auto& rightTarget = *right->subtarget;
                const std::string& leftDate =
                    subtargetSortMode_ == "sortDeadline" && leftTarget.deadline
                        ? *leftTarget.deadline
                        : leftTarget.createdAt;
                const std::string& rightDate =
                    subtargetSortMode_ == "sortDeadline" && rightTarget.deadline
                        ? *rightTarget.deadline
                        : rightTarget.createdAt;
                if (leftDate != rightDate) {
                    return subtargetSortAscending_ ? leftDate < rightDate
                                                   : leftDate > rightDate;
                }
                return subtargetSortAscending_
                           ? left->storageIndex < right->storageIndex
                           : left->storageIndex > right->storageIndex;
            });

            std::size_t visualIndex = 0;
            for (auto* sortable : displayOrder.iterate()) {
                const auto& subtarget = *sortable->subtarget;
                const std::string suffix = std::to_string(visualIndex);
                const std::string textKey = "subtext" + suffix;
                std::string text = shortened(subtarget.title, 28) + "\n创建：" +
                                   displayTime(subtarget.createdAt);
                const float height = 84.f;
                if (subtarget.status == "promoted") {
                    text += "\n完成：" + displayTime(subtarget.promotedAt.value_or(""));
                } else {
                    text += "\n截止：" +
                            (subtarget.deadline ? shortened(*subtarget.deadline, 30) : "未设置");
                }

                const bool isActive = subtarget.status == "active";
                const float textRight = isActive ? -250.f : -168.f;
                if (refersTo(editingSubtarget_, target.id, subtarget.id)) {
                    auto& editCard = detail.path_get<gui::AreaObject>("subedit" + suffix);
                    setFill(editCard, isActive ? subtargetStyle_ : completedStyle_);
                    setHorizontalFill(editCard, 8, textRight, y, height);

                    std::string metaText = "创建：" + displayTime(subtarget.createdAt);
                    if (!isActive) {
                        metaText += "  完成：" + displayTime(subtarget.promotedAt.value_or(""));
                    }
                    auto& meta = editCard.path_get<gui::TextObject>("meta");
                    configureText(meta, fromUtf8(metaText), isActive ? 13 : 14,
                                  gui::UIBase::Align::Left,
                                  gui::UIBase::Align::Top);
                    setHorizontalFill(meta, 6, isActive ? -230 : -6, 3, 23);

                    if (isActive) {
                        auto& deadlineLabel =
                            editCard.path_get<gui::TextObject>("deadlineLabel");
                        configureText(deadlineLabel, L"截止时间", 13,
                                      gui::UIBase::Align::Mid,
                                      gui::UIBase::Align::Mid);
                        setRightAnchored(deadlineLabel, 160, 2, 64, 25);

                        auto& deadlineInput =
                            editCard.path_get<gui::InputObject>("deadlineInput");
                        configureInput(deadlineInput, 14);
                        deadlineInput.setAlign(gui::UIBase::Align::Left,
                                               gui::UIBase::Align::Mid);
                        deadlineInput.setText(fromUtf8(subtarget.deadline.value_or("")));
                        setRightAnchored(deadlineInput, 6, 2, 150, 25);
                    }

                    auto& input = editCard.path_get<gui::InputObject>("input");
                    configureInput(input, 17);
                    input.setText(fromUtf8(subtarget.title));
                    setFillRect(input, 6, -6, 27, -6);
                } else {
                    auto& item = detail.path_get<gui::TextObject>(textKey);
                    configureText(item, fromUtf8(text), 16, gui::UIBase::Align::Left,
                                  gui::UIBase::Align::Top);
                    setFill(item, isActive ? subtargetStyle_ : completedStyle_);
                    setHorizontalFill(item, 8, textRight, y, height);
                    if (!isActive) {
                        setTextColors(item, sf::Color(94, 101, 114), sf::Color(94, 101, 114),
                                      sf::Color(94, 101, 114));
                    }
                }

                const float buttonY = y + 23.f;
                if (isActive) {
                    const std::string buttonKey = "complete" + suffix;
                    completeButtons_[buttonKey] = {target.id, subtarget.id};
                    auto& complete = detail.path_get<gui::ButtonObject>(buttonKey);
                    configureButton(complete, L"完成", 17);
                    setRightAnchored(complete, 168, buttonY, 74, 38);
                }

                const std::string editKey = "editSub" + suffix;
                editSubtargetButtons_[editKey] = {target.id, subtarget.id};
                auto& edit = detail.path_get<gui::ButtonObject>(editKey);
                configureButton(edit,
                                refersTo(editingSubtarget_, target.id, subtarget.id) ? L"确定"
                                                                                   : L"编辑",
                                17);
                setRightAnchored(edit, 86, buttonY, 74, 38);

                const std::string deleteKey = "deleteSub" + suffix;
                deleteSubtargetButtons_[deleteKey] = {target.id, subtarget.id};
                auto& remove = detail.path_get<gui::ButtonObject>(deleteKey);
                configureDeleteButton(
                    remove, refersTo(deletingSubtarget_, target.id, subtarget.id) ? L"确定"
                                                                                  : L"删除");
                setRightAnchored(remove, 4, buttonY, 74, 38);
                y += height + 9.f;
                ++visualIndex;
            }
        }
    }

    void buildLogs(gui::AreaObject& detail, const myplan::Target& target) {
        float y = 6.f;

        if (target.log.empty()) {
            auto& empty = detail.path_get<gui::TextObject>("logempty");
            configureText(empty, L"暂无进度或备注。", 16);
            setHorizontalFill(empty, 8, -8, y, 46);
            setTextColors(empty, sf::Color(115, 121, 133), sf::Color(115, 121, 133),
                          sf::Color(115, 121, 133));
            return;
        }

        std::size_t visualIndex = 0;
        for (auto iterator = target.log.rbegin(); iterator != target.log.rend();
             ++iterator, ++visualIndex) {
            const std::string suffix = std::to_string(visualIndex);
            const WrappedText wrapped = wrapText(iterator->text, 42);
            sf::String text = fromUtf8("[" + logTypeName(iterator->type) + "]  " +
                                      displayTime(iterator->createdAt) + "\n");
            text += wrapped.text;
            const float height = std::max(84.f, 49.f + static_cast<float>(wrapped.lines) * 22.f);
            if (refersTo(editingLog_, target.id, iterator->id)) {
                auto& editCard = detail.path_get<gui::AreaObject>("logedit" + suffix);
                setFill(editCard, iterator->type == "progress" ? progressStyle_ : noteStyle_);
                setHorizontalFill(editCard, 8, -168, y, height);

                auto& meta = editCard.path_get<gui::TextObject>("meta");
                configureText(meta,
                              fromUtf8("[" + logTypeName(iterator->type) + "]  " +
                                       displayTime(iterator->createdAt)),
                              15, gui::UIBase::Align::Left, gui::UIBase::Align::Top);
                setHorizontalFill(meta, 6, -6, 3, 23);

                auto& input = editCard.path_get<gui::InputObject>("input");
                configureInput(input, 17);
                input.setText(fromUtf8(iterator->text));
                setFillRect(input, 6, -6, 27, -6);
            } else {
                auto& card = detail.path_get<gui::TextObject>("log" + suffix);
                configureText(card, text, 17, gui::UIBase::Align::Left,
                              gui::UIBase::Align::Top);
                setFill(card, iterator->type == "progress" ? progressStyle_ : noteStyle_);
                setHorizontalFill(card, 8, -168, y, height);
            }

            const float buttonY = y + (height - 38.f) / 2.f;
            const std::string editKey = "editLog" + suffix;
            editLogButtons_[editKey] = {target.id, iterator->id};
            auto& edit = detail.path_get<gui::ButtonObject>(editKey);
            configureButton(edit, refersTo(editingLog_, target.id, iterator->id) ? L"确定"
                                                                                 : L"编辑",
                            17);
            setRightAnchored(edit, 86, buttonY, 74, 38);

            const std::string deleteKey = "deleteLog" + suffix;
            deleteLogButtons_[deleteKey] = {target.id, iterator->id};
            auto& remove = detail.path_get<gui::ButtonObject>(deleteKey);
            configureDeleteButton(remove,
                                  refersTo(deletingLog_, target.id, iterator->id) ? L"确定"
                                                                                 : L"删除");
            setRightAnchored(remove, 4, buttonY, 74, 38);
            y += height + 9.f;
        }
    }

    void buildComposer(gui::AreaObject& composer) {
        auto& label = composer.path_get<gui::TextObject>("label");
        configureText(label, L"添加到当前大目标", 18);
        setRect(label, 14, 8, 180, 34);

        struct ModeButton {
            const char* key;
            const wchar_t* label;
            float x;
        };
        const ModeButton modes[] = {{"subtarget", L"添加目标", 196.f},
                                    {"progress", L"添加进度", 306.f},
                                    {"note", L"添加备注", 416.f}};
        for (const auto& mode : modes) {
            auto& option = composer.path_get<gui::OptionObject>(mode.key);
            configureText(option, mode.label, 16, gui::UIBase::Align::Mid,
                          gui::UIBase::Align::Mid);
            option.setStyle(optionNormal_, optionOver_, optionSelected_);
            setRect(option, mode.x, 9, 100, 32);
        }
        composer.setOption(composerMode_);

        float contentTop = 51.f;
        if (composerMode_ == "subtarget") {
            auto& deadlineLabel = composer.path_get<gui::TextObject>("deadlineLabel");
            configureText(deadlineLabel, L"截止时间", 15);
            setRect(deadlineLabel, 14, 51, 72, 32);

            auto& deadlineInput = composer.path_get<gui::InputObject>("deadlineInput");
            configureInput(deadlineInput, 16);
            deadlineInput.setAlign(gui::UIBase::Align::Left, gui::UIBase::Align::Mid);
            setRect(deadlineInput, 90, 50, 275, 32);

            auto& deadlineHint = composer.path_get<gui::TextObject>("deadlineHint");
            configureText(deadlineHint, L"格式：2026-12-31；可留空", 13);
            setHorizontalFill(deadlineHint, 375, -126, 55, 22);
            setTextColors(deadlineHint, sf::Color(100, 107, 120),
                          sf::Color(100, 107, 120), sf::Color(100, 107, 120));
            contentTop = 90.f;
        }

        auto& input = composer.path_get<gui::InputObject>("input");
        configureInput(input, 19);
        setFillRect(input, 14, -126, contentTop, -16);

        auto& submit = composer.path_get<gui::ButtonObject>("submit");
        configureButton(submit, L"提交", 19);
        setRightAnchored(submit, 16, contentTop, 96, 68);

        auto& hint = composer.path_get<gui::TextObject>("hint");
        configureText(hint, fromUtf8(modeHint()), 14, gui::UIBase::Align::Mid,
                      gui::UIBase::Align::Top);
        setRightAnchored(hint, 14, contentTop + 76, 100,
                         composerMode_ == "subtarget" ? 32 : 58);
        setTextColors(hint, sf::Color(100, 107, 120), sf::Color(100, 107, 120),
                      sf::Color(100, 107, 120));
    }

    std::string modeHint() const {
        if (composerMode_ == "progress") {
            return "记录已经完成的\n阶段性进展";
        }
        if (composerMode_ == "note") {
            return "记录想法、提醒\n或补充说明";
        }
        return "添加一个可完成的\n小目标";
    }

    void processUiEvents() {
        while (const auto event = manager_.pollEvent()) {
            if (const auto* selected = event->getIf<gui::Events::OptionSelected>()) {
                handleOption(*selected);
            }
            if (const auto* pressed = event->getIf<gui::Events::ButtonPressed>()) {
                handleButton(*pressed);
            }
        }
    }

    void handleOption(const gui::Events::OptionSelected& event) {
        if (event.path == "main_content_details" &&
            (event.name == "sortDeadline" || event.name == "sortCreated")) {
            subtargetSortMode_ = event.name;
            buildUi();
            return;
        }
        if (event.path == "main_sidebar_filterTabs" &&
            (event.name == "active" || event.name == "archived")) {
            sidebarFilter_ = event.name;
            selectedTargetId_.reset();
            statusMessage_.clear();
            resetItemActions();
            buildUi(false);
            return;
        }
        if (event.path == "main_sidebar_targetList") {
            if (const auto found = targetButtons_.find(event.name); found != targetButtons_.end()) {
                selectedTargetId_ = found->second;
                statusMessage_.clear();
                resetItemActions();
                buildUi(false);
            }
            return;
        }
        if (event.path == "main_content_composer" &&
            (event.name == "subtarget" || event.name == "progress" || event.name == "note")) {
            composerMode_ = event.name;
            buildUi();
        }
    }

    void handleButton(const gui::Events::ButtonPressed& event) {
        if (event.path == "main_content_details" && event.name == "sortDirection") {
            subtargetSortAscending_ = !subtargetSortAscending_;
            buildUi();
            return;
        }
        if (event.path == "main_sidebar" && event.name == "refresh") {
            statusMessage_ = "已重新载入数据文件";
            statusIsError_ = false;
            resetItemActions();
            buildUi();
            return;
        }
        if (event.path == "main_sidebar" && event.name == "createTarget") {
            createTarget();
            return;
        }
        if (event.path == "main_content_header" && event.name == "saveDeadline") {
            saveDeadline();
            return;
        }
        if (event.path == "main_content_header" && event.name == "completeTarget") {
            completeTarget();
            return;
        }
        if (event.path == "main_content_header" && event.name == "restoreTarget") {
            restoreTarget();
            return;
        }
        if (event.path == "main_content_header" && event.name == "deleteTarget") {
            deleteTarget();
            return;
        }
        if (event.path == "main_content_header" && event.name == "editTarget") {
            editTarget();
            return;
        }
        if (event.path == "main_content_composer" && event.name == "submit") {
            submitComposer();
            return;
        }
        if (event.path == "main_content_details_subtargetList") {
            if (const auto found = completeButtons_.find(event.name);
                found != completeButtons_.end()) {
                completeSubtarget(found->second.first, found->second.second);
                return;
            }
            if (const auto found = editSubtargetButtons_.find(event.name);
                found != editSubtargetButtons_.end()) {
                editSubtarget(event.name, found->second);
                return;
            }
            if (const auto found = deleteSubtargetButtons_.find(event.name);
                found != deleteSubtargetButtons_.end()) {
                deleteSubtarget(found->second);
                return;
            }
        }
        if (event.path == "main_content_details_logList") {
            if (const auto found = editLogButtons_.find(event.name);
                found != editLogButtons_.end()) {
                editLog(event.name, found->second);
                return;
            }
            if (const auto found = deleteLogButtons_.find(event.name);
                found != deleteLogButtons_.end()) {
                deleteLog(found->second);
            }
        }
    }

    void resetItemActions() {
        editingSubtarget_.reset();
        deletingSubtarget_.reset();
        editingLog_.reset();
        deletingLog_.reset();
        editingTargetId_.reset();
        deletingTargetId_.reset();
    }

    void editTarget() {
        if (!selectedTargetId_) {
            return;
        }
        const std::string targetId = *selectedTargetId_;
        if (!editingTargetId_ || *editingTargetId_ != targetId) {
            resetItemActions();
            editingTargetId_ = targetId;
            statusMessage_.clear();
            buildUi();
            return;
        }

        const auto* input =
            manager_.path_find<gui::InputObject>("main.content.header.titleInput");
        const std::string value = input ? toUtf8(input->getText()) : "";
        perform(
            [&] {
                store_.editTarget(targetId, value);
                resetItemActions();
            },
            "大目标标题已更新");
    }

    void completeTarget() {
        if (!selectedTargetId_) {
            return;
        }
        const std::string targetId = *selectedTargetId_;
        const myplan::Target* target = selectedTarget();
        if (target && target->status == "archived") {
            statusMessage_ = "大目标已经完成并归档";
            statusIsError_ = false;
            buildUi();
            return;
        }
        perform(
            [&] {
                store_.completeTarget(targetId);
                resetItemActions();
            },
            "大目标已完成并归档");
    }

    void restoreTarget() {
        if (!selectedTargetId_) {
            return;
        }
        const std::string targetId = *selectedTargetId_;
        perform(
            [&] {
                store_.restoreTarget(targetId);
                sidebarFilter_ = "active";
                selectedTargetId_ = targetId;
                resetItemActions();
            },
            "大目标已恢复为进行中");
    }

    void deleteTarget() {
        if (!selectedTargetId_) {
            return;
        }
        const std::string targetId = *selectedTargetId_;
        if (!deletingTargetId_ || *deletingTargetId_ != targetId) {
            resetItemActions();
            deletingTargetId_ = targetId;
            statusMessage_ = "再次点击红色“确定”按钮永久删除大目标";
            statusIsError_ = false;
            buildUi();
            return;
        }
        perform(
            [&] {
                store_.deleteTarget(targetId);
                selectedTargetId_.reset();
                resetItemActions();
            },
            "大目标已永久删除", false);
    }

    void editSubtarget(const std::string& buttonName, const ItemRef& item) {
        if (!refersTo(editingSubtarget_, item.targetId, item.itemId)) {
            resetItemActions();
            editingSubtarget_ = item;
            statusMessage_.clear();
            buildUi();
            return;
        }

        const std::string suffix = buttonName.substr(std::string("editSub").size());
        const auto* input = manager_.path_find<gui::InputObject>(
            "main.content.details.subtargetList.subedit" + suffix + ".input");
        const auto* deadlineInput = manager_.path_find<gui::InputObject>(
            "main.content.details.subtargetList.subedit" + suffix + ".deadlineInput");
        const std::string value = input ? toUtf8(input->getText()) : "";
        const bool editsDeadline = deadlineInput != nullptr;
        const std::string deadlineValue = deadlineInput ? toUtf8(deadlineInput->getText()) : "";
        const std::optional<std::string> deadline =
            blank(deadlineValue) ? std::nullopt
                                 : std::optional<std::string>(deadlineValue);
        perform(
            [&] {
                if (editsDeadline) {
                    store_.editSubtarget(item.targetId, item.itemId, value, deadline);
                } else {
                    store_.editSubtarget(item.targetId, item.itemId, value);
                }
                resetItemActions();
            },
            "小目标已更新");
    }

    void deleteSubtarget(const ItemRef& item) {
        if (!refersTo(deletingSubtarget_, item.targetId, item.itemId)) {
            resetItemActions();
            deletingSubtarget_ = item;
            statusMessage_ = "再次点击红色“确定”按钮删除小目标";
            statusIsError_ = false;
            buildUi();
            return;
        }
        perform(
            [&] {
                store_.deleteSubtarget(item.targetId, item.itemId);
                resetItemActions();
            },
            "小目标已删除");
    }

    void editLog(const std::string& buttonName, const ItemRef& item) {
        if (!refersTo(editingLog_, item.targetId, item.itemId)) {
            resetItemActions();
            editingLog_ = item;
            statusMessage_.clear();
            buildUi();
            return;
        }

        const std::string suffix = buttonName.substr(std::string("editLog").size());
        const auto* input = manager_.path_find<gui::InputObject>(
            "main.content.details.logList.logedit" + suffix + ".input");
        const std::string value = input ? toUtf8(input->getText()) : "";
        perform(
            [&] {
                store_.editLog(item.targetId, item.itemId, value);
                resetItemActions();
            },
            "日志已更新");
    }

    void deleteLog(const ItemRef& item) {
        if (!refersTo(deletingLog_, item.targetId, item.itemId)) {
            resetItemActions();
            deletingLog_ = item;
            statusMessage_ = "再次点击红色“确定”按钮删除日志";
            statusIsError_ = false;
            buildUi();
            return;
        }
        perform(
            [&] {
                store_.deleteLog(item.targetId, item.itemId);
                resetItemActions();
            },
            "日志已删除");
    }

    template <typename Action>
    void perform(Action&& action, const std::string& successMessage,
                 bool preserveDetailScroll = true) {
        try {
            action();
            statusMessage_ = successMessage;
            statusIsError_ = false;
            buildUi(preserveDetailScroll);
        } catch (const std::exception& error) {
            statusMessage_ = error.what();
            statusIsError_ = true;
            if (auto* header =
                    manager_.path_find<gui::AreaObject>("main.content.header")) {
                buildStatus(*header);
            }
        }
    }

    void createTarget() {
        const auto* input = manager_.path_find<gui::InputObject>("main.sidebar.newTargetInput");
        const std::string title = input ? toUtf8(input->getText()) : "";
        perform(
            [&] {
                const auto created = store_.createTarget(title);
                sidebarFilter_ = "active";
                selectedTargetId_ = created.id;
                resetItemActions();
            },
            "大目标已创建", false);
    }

    void saveDeadline() {
        if (!selectedTargetId_) {
            return;
        }
        const auto* input = manager_.path_find<gui::InputObject>("main.content.header.deadlineInput");
        const std::string value = input ? toUtf8(input->getText()) : "";
        perform(
            [&] {
                store_.setDeadline(*selectedTargetId_, blank(value)
                                                           ? std::nullopt
                                                           : std::optional<std::string>(value));
                resetItemActions();
            },
            blank(value) ? "截止时间已清除" : "截止时间已保存");
    }

    void submitComposer() {
        if (!selectedTargetId_) {
            return;
        }
        const auto* input = manager_.path_find<gui::InputObject>("main.content.composer.input");
        const std::string value = input ? toUtf8(input->getText()) : "";
        const std::string targetId = *selectedTargetId_;
        if (composerMode_ == "progress") {
            perform(
                [&] {
                    store_.addProgress(targetId, value);
                    resetItemActions();
                },
                "进度已写入日志");
        } else if (composerMode_ == "note") {
            perform(
                [&] {
                    store_.addNote(targetId, value);
                    resetItemActions();
                },
                "备注已写入日志");
        } else {
            const auto* deadlineInput = manager_.path_find<gui::InputObject>(
                "main.content.composer.deadlineInput");
            const std::string deadlineValue =
                deadlineInput ? toUtf8(deadlineInput->getText()) : "";
            const std::optional<std::string> deadline =
                blank(deadlineValue) ? std::nullopt
                                     : std::optional<std::string>(deadlineValue);
            perform(
                [&] {
                    store_.addSubtarget(targetId, value, deadline);
                    resetItemActions();
                },
                "小目标已添加");
        }
    }

    void completeSubtarget(const std::string& targetId, const std::string& subtargetId) {
        perform([&] {
                    store_.promoteSubtarget(targetId, subtargetId);
                    resetItemActions();
                },
                "小目标已完成并转为进度");
    }
};

void printHelp() {
    std::cout << "My Plan GUI\n\n"
              << "Usage:\n"
              << "  myplan-gui [--data FILE]\n\n"
              << "The default data file is myplan-data.json beside myplan-gui.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        const AppOptions options = parseOptions(commandLineArguments());
#else
        const AppOptions options = parseOptions(commandLineArguments(argc, argv));
#endif
        if (options.help) {
            printHelp();
            return 0;
        }
        if (!loadDefaultFont(options.executable)) {
            std::cerr << "无法加载 resources/fonts/default.TTF\n";
            return 1;
        }
        PlannerGui application(options.dataFile);
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "My Plan GUI 启动失败：" << error.what() << '\n';
        return 1;
    }
}
