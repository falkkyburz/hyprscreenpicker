#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/core/Output.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/Checkbox.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Null.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/window/Window.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

namespace HyprPicker {

struct WindowEntry {
    std::string title;
    std::string clazz;
    unsigned long long id = 0;
};

struct MonitorEntry {
    std::string name;
    std::string description;
    int         x      = 0;
    int         y      = 0;
    int         width  = 0;
    int         height = 0;
};

struct RegionSelection {
    std::string output;
    int         x      = 0;
    int         y      = 0;
    int         width  = 0;
    int         height = 0;
};

struct Settings {
    int width  = 620;
    int height = 560;
};

namespace {

std::optional<unsigned long long> parseULL(const std::string_view value) {
    unsigned long long result = 0;
    const auto*        begin  = value.data();
    const auto*        end    = value.data() + value.size();
    const auto         [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end)
        return std::nullopt;
    return result;
}

std::optional<int> parseInt(const std::string_view value) {
    int         result = 0;
    const auto* begin  = value.data();
    const auto* end    = value.data() + value.size();
    const auto  [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end)
        return std::nullopt;
    return result;
}

std::string jsonStringValue(const std::string& object, const std::string& key) {
    const auto keyPos = object.find(std::format("\"{}\"", key));
    if (keyPos == std::string::npos)
        return {};

    const auto colon = object.find(':', keyPos);
    if (colon == std::string::npos)
        return {};

    auto quote = object.find('"', colon + 1);
    if (quote == std::string::npos)
        return {};

    std::string result;
    bool        escaped = false;
    for (size_t i = quote + 1; i < object.size(); ++i) {
        const char c = object[i];
        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"')
            return result;
        result += c;
    }

    return {};
}

std::optional<int> jsonIntValue(const std::string& object, const std::string& key) {
    const auto keyPos = object.find(std::format("\"{}\"", key));
    if (keyPos == std::string::npos)
        return std::nullopt;

    const auto colon = object.find(':', keyPos);
    if (colon == std::string::npos)
        return std::nullopt;

    size_t begin = colon + 1;
    while (begin < object.size() && std::isspace(static_cast<unsigned char>(object[begin])))
        ++begin;

    size_t end = begin;
    if (end < object.size() && object[end] == '-')
        ++end;
    while (end < object.size() && std::isdigit(static_cast<unsigned char>(object[end])))
        ++end;

    if (begin == end)
        return std::nullopt;
    return parseInt(std::string_view{object}.substr(begin, end - begin));
}

std::vector<std::string> splitWhitespace(const std::string& value) {
    std::vector<std::string> result;
    size_t                   pos = 0;

    while (pos < value.size()) {
        while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos])))
            ++pos;
        const auto begin = pos;
        while (pos < value.size() && !std::isspace(static_cast<unsigned char>(value[pos])))
            ++pos;
        if (begin != pos)
            result.emplace_back(value.substr(begin, pos - begin));
    }

    return result;
}

} // namespace

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::ranges::find_if(value, notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<WindowEntry> parseWindowList(const char* env) {
    std::vector<WindowEntry> result;
    if (!env)
        return result;

    std::string rolling = env;
    while (!rolling.empty()) {
        const auto idSep = rolling.find("[HC>]");
        if (idSep == std::string::npos)
            break;

        const auto classSep = rolling.find("[HT>]", idSep + 5);
        if (classSep == std::string::npos)
            break;

        const auto titleSep = rolling.find("[HE>]", classSep + 5);
        if (titleSep == std::string::npos)
            break;

        const auto addressSep = rolling.find("[HA>]", titleSep + 5);
        if (addressSep == std::string::npos)
            break;

        const auto id = parseULL(std::string_view{rolling}.substr(0, idSep));
        if (id)
            result.push_back({rolling.substr(classSep + 5, titleSep - classSep - 5), rolling.substr(idSep + 5, classSep - idSep - 5), *id});

        rolling = rolling.substr(addressSep + 5);
    }

    return result;
}

std::vector<MonitorEntry> parseMonitorList(const char* env) {
    std::vector<MonitorEntry> result;
    if (!env)
        return result;

    std::string rolling = env;
    while (!rolling.empty()) {
        const auto nameSep = rolling.find("[MX>]");
        if (nameSep == std::string::npos)
            break;

        const auto xSep = rolling.find("[MY>]", nameSep + 5);
        if (xSep == std::string::npos)
            break;

        const auto ySep = rolling.find("[MW>]", xSep + 5);
        if (ySep == std::string::npos)
            break;

        const auto wSep = rolling.find("[MH>]", ySep + 5);
        if (wSep == std::string::npos)
            break;

        const auto hSep = rolling.find("[ME>]", wSep + 5);
        if (hSep == std::string::npos)
            break;

        const auto x = parseInt(std::string_view{rolling}.substr(nameSep + 5, xSep - nameSep - 5));
        const auto y = parseInt(std::string_view{rolling}.substr(xSep + 5, ySep - xSep - 5));
        const auto w = parseInt(std::string_view{rolling}.substr(ySep + 5, wSep - ySep - 5));
        const auto h = parseInt(std::string_view{rolling}.substr(wSep + 5, hSep - wSep - 5));

        if (x && y && w && h && *w > 0 && *h > 0)
            result.push_back({.name = rolling.substr(0, nameSep), .description = "", .x = *x, .y = *y, .width = *w, .height = *h});

        rolling = rolling.substr(hSep + 5);
    }

    return result;
}

std::vector<MonitorEntry> parseHyprctlMonitorsJson(const std::string& json) {
    std::vector<MonitorEntry> result;

    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        int    depth = 0;
        size_t end   = pos;
        for (; end < json.size(); ++end) {
            if (json[end] == '{')
                ++depth;
            else if (json[end] == '}' && --depth == 0)
                break;
        }
        if (end >= json.size())
            break;

        const auto object = json.substr(pos, end - pos + 1);
        auto       name   = jsonStringValue(object, "name");
        auto       width  = jsonIntValue(object, "width");
        auto       height = jsonIntValue(object, "height");
        if (!name.empty() && width && height) {
            result.push_back({.name        = std::move(name),
                              .description = jsonStringValue(object, "description"),
                              .x           = jsonIntValue(object, "x").value_or(0),
                              .y           = jsonIntValue(object, "y").value_or(0),
                              .width       = *width,
                              .height      = *height});
        }

        pos = end + 1;
    }

    return result;
}

std::string screenLabel(size_t index, const MonitorEntry& monitor) {
    return std::format("Screen {} at {}, {} ({}x{}) ({})", index, monitor.x, monitor.y, monitor.width, monitor.height, monitor.name);
}

std::string windowLabel(const WindowEntry& window) {
    if (window.clazz.empty())
        return window.title;
    if (window.title.empty())
        return window.clazz;
    return std::format("{}: {}", window.clazz, window.title);
}

int preferredListScrollHeight(size_t rowCount) {
    constexpr int ROW_HEIGHT          = 24;
    constexpr int ROW_GAP             = 2;
    constexpr int EMPTY_NOTE_HEIGHT   = 42;
    constexpr int SCROLL_AREA_PADDING = 4;
    constexpr int MAX_VISIBLE_ROWS    = 9;

    if (rowCount == 0)
        return EMPTY_NOTE_HEIGHT;

    const auto visibleRows = static_cast<int>(std::min(rowCount, static_cast<size_t>(MAX_VISIBLE_ROWS)));
    return visibleRows * ROW_HEIGHT + std::max(0, visibleRows - 1) * ROW_GAP + SCROLL_AREA_PADDING;
}

int preferredWindowHeight(size_t monitorCount, size_t windowCount, bool regionAvailable) {
    constexpr int TITLE_HEIGHT      = 58;
    constexpr int SECTION_HEADERS   = 96;
    constexpr int FOOTER_HEIGHT     = 50;
    constexpr int ROW_HEIGHT        = 33;
    constexpr int EMPTY_NOTE_HEIGHT = 31;
    constexpr int WINDOW_PADDING    = 36;

    const int screenRows = monitorCount == 0 ? EMPTY_NOTE_HEIGHT : preferredListScrollHeight(monitorCount);
    const int windowRows = preferredListScrollHeight(windowCount);
    const int regionRows = ROW_HEIGHT + (regionAvailable ? 0 : EMPTY_NOTE_HEIGHT);

    return std::clamp(TITLE_HEIGHT + SECTION_HEADERS + FOOTER_HEIGHT + WINDOW_PADDING + screenRows + windowRows + regionRows, 300, 760);
}

int preferredWindowScrollHeight(size_t windowCount) {
    return preferredListScrollHeight(windowCount);
}

size_t visibleEntryCount(size_t rowCount, size_t limit) {
    return std::min(rowCount, limit);
}

std::string formatScreenSelection(const std::string& output, bool allowToken) {
    return std::format("[SELECTION]{}/screen:{}\n", allowToken ? "r" : "", output);
}

std::string formatWindowSelection(unsigned long long id, bool allowToken) {
    return std::format("[SELECTION]{}/window:{}\n", allowToken ? "r" : "", id);
}

std::string formatRegionSelection(const RegionSelection& region, bool allowToken) {
    return std::format("[SELECTION]{}/region:{}@{},{},{},{}\n", allowToken ? "r" : "", region.output, region.x, region.y, region.width, region.height);
}

std::optional<RegionSelection> parseSlurpRegion(const std::string& slurpOutput, const std::vector<MonitorEntry>& monitors) {
    const auto parts = splitWhitespace(trim(slurpOutput));
    if (parts.size() != 5)
        return std::nullopt;

    const auto x = parseInt(parts[1]);
    const auto y = parseInt(parts[2]);
    const auto w = parseInt(parts[3]);
    const auto h = parseInt(parts[4]);
    if (!x || !y || !w || !h || *w <= 0 || *h <= 0)
        return std::nullopt;

    const auto monitor = std::ranges::find_if(monitors, [&](const MonitorEntry& entry) { return entry.name == parts[0]; });
    if (monitor == monitors.end())
        return std::nullopt;

    return RegionSelection{.output = parts[0], .x = *x - monitor->x, .y = *y - monitor->y, .width = *w, .height = *h};
}

Settings parseSettings(const std::string& contents, Settings defaults) {
    std::istringstream stream{contents};
    std::string        line;

    while (std::getline(stream, line)) {
        std::istringstream lineStream{line};
        std::string        key;
        char               eq    = 0;
        int                value = 0;

        if (!(lineStream >> key >> eq >> value))
            continue;

        if (eq != '=')
            continue;

        if (key == "width" && value > 0)
            defaults.width = value;
        else if (key == "height" && value > 0)
            defaults.height = value;
    }

    return defaults;
}

std::string formatSettings(const Settings& settings) {
    return std::format("width = {}\nheight = {}\n", settings.width, settings.height);
}

}

using namespace Hyprtoolkit;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;

namespace {

template <class T>
using SP = CSharedPointer<T>;

SP<IBackend>         g_backend;
SP<IWindow>          g_window;
SP<CCheckboxElement> g_allowToken;

constexpr const char* SETTINGS_PATH = "/tmp/hypr/hypr-picker.conf";
constexpr const char* LOG_ENV       = "HYPRSCREENPICKER_LOG";

void debugLog(const std::string& message) {
    const char* path = std::getenv(LOG_ENV);
    if (!path || !*path)
        return;

    std::ofstream file{path, std::ios::app};
    if (!file)
        return;

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    file << std::put_time(std::localtime(&now), "%F %T") << " " << message << '\n';
}

std::string runCommand(const char* command) {
    std::array<char, 4096> buffer{};
    std::string           result;

    FILE* pipe = popen(command, "r");
    if (!pipe)
        return {};

    while (fgets(buffer.data(), buffer.size(), pipe))
        result += buffer.data();

    pclose(pipe);
    return result;
}

void writeSettings() {
    if (!g_window)
        return;

    std::filesystem::create_directories(std::filesystem::path{SETTINGS_PATH}.parent_path());
    std::ofstream file{SETTINGS_PATH};
    const auto    size = g_window->pixelSize();
    file << HyprPicker::formatSettings({.width = static_cast<int>(size.x), .height = static_cast<int>(size.y)});
}

bool allowToken() {
    return g_allowToken && g_allowToken->state();
}

[[noreturn]] void finishWithSelection(const std::string& selection) {
    debugLog(std::format("selection {}", HyprPicker::trim(selection)));
    writeSettings();
    std::cout << selection;
    std::cout.flush();
    // This runs inside toolkit input callbacks. Destroying the backend here can
    // block in the event loop, while xdph only needs the flushed selection.
    std::_Exit(0);
}

[[noreturn]] void finishWithoutSelection() {
    debugLog("cancel");
    writeSettings();
    std::cout.flush();
    std::_Exit(1);
}

std::vector<HyprPicker::MonitorEntry> monitorsFromHyprctl() {
    return HyprPicker::parseHyprctlMonitorsJson(runCommand("hyprctl -j monitors 2>/dev/null"));
}

std::vector<HyprPicker::MonitorEntry> monitorsFromToolkit() {
    std::vector<HyprPicker::MonitorEntry> monitors;
    for (const auto& output : g_backend->getOutputs()) {
        const auto port = output->port();
        if (port.empty())
            continue;
        monitors.push_back({.name = port, .description = output->desc(), .x = 0, .y = 0, .width = 0, .height = 0});
    }
    return monitors;
}

std::vector<HyprPicker::MonitorEntry> getMonitors() {
    auto testMonitors = HyprPicker::parseMonitorList(std::getenv("HYPR_PICKER_TEST_MONITORS"));
    if (!testMonitors.empty())
        return testMonitors;

    auto monitors = monitorsFromHyprctl();
    if (!monitors.empty())
        return monitors;
    return monitorsFromToolkit();
}

SP<IElement> sectionTitle(std::string&& label) {
    return CTextBuilder::begin()->text(std::move(label))->fontSize({CFontSize::HT_FONT_H2})->color([] { return g_backend->getPalette()->m_colors.text; })->commence();
}

SP<IElement> sectionSpacer() {
    return CNullBuilder::begin()->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 4.F}})->commence();
}

SP<IElement> note(std::string&& label) {
    auto text = CTextBuilder::begin()->text(std::move(label))->color([] { return g_backend->getPalette()->m_colors.text.darken(0.25); })->commence();
    text->setMargin(5);
    return text;
}

SP<CButtonElement> selectionButton(std::string&& label, std::function<void(SP<CButtonElement>)>&& onClick) {
    constexpr size_t MAX_BUTTON_LABEL_LENGTH = 40;

    if (label.size() > MAX_BUTTON_LABEL_LENGTH)
        label = label.substr(0, MAX_BUTTON_LABEL_LENGTH - 3) + "...";

    auto button = CButtonBuilder::begin()
                      ->label(std::move(label))
                      ->alignText(HT_FONT_ALIGN_CENTER)
                      ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 24.F}})
                      ->onMainClick(std::move(onClick))
                      ->commence();
    button->setMargin(1);
    return button;
}

SP<CColumnLayoutElement> addListPane(SP<CColumnLayoutElement> layout, size_t rowCount) {
    constexpr float  LIST_HEIGHT      = 240.F;
    constexpr size_t MAX_VISIBLE_ROWS = 9;

    auto scroll = CScrollAreaBuilder::begin()->scrollY(rowCount > MAX_VISIBLE_ROWS)->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, LIST_HEIGHT}})->commence();
    layout->addChild(scroll);

    auto content = CColumnLayoutBuilder::begin()->gap(2)->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})->commence();
    content->setMargin(2);
    scroll->addChild(content);
    return content;
}

void addScreenSection(SP<CColumnLayoutElement> layout, const std::vector<HyprPicker::MonitorEntry>& monitors) {
    layout->addChild(sectionTitle("Screens"));
    layout->addChild(sectionSpacer());
    auto pane = addListPane(layout, monitors.size());

    if (monitors.empty()) {
        pane->addChild(note("No Wayland outputs were reported."));
        return;
    }

    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto monitor = monitors[i];
        pane->addChild(selectionButton(HyprPicker::screenLabel(i, monitor), [monitor](SP<CButtonElement>) {
            finishWithSelection(HyprPicker::formatScreenSelection(monitor.name, allowToken()));
        }));
    }
}

void addWindowSection(SP<CColumnLayoutElement> layout, const std::vector<HyprPicker::WindowEntry>& windows) {
    layout->addChild(sectionTitle("Windows"));
    layout->addChild(sectionSpacer());
    auto pane = addListPane(layout, windows.size());

    if (windows.empty()) {
        pane->addChild(note("No shareable windows were reported by xdg-desktop-portal-hyprland."));
        return;
    }

    for (size_t i = 0; i < windows.size(); ++i) {
        const auto window = windows[i];
        pane->addChild(selectionButton(HyprPicker::windowLabel(window), [window](SP<CButtonElement>) {
            finishWithSelection(HyprPicker::formatWindowSelection(window.id, allowToken()));
        }));
    }
}

void addRegionSection(SP<CColumnLayoutElement> layout, std::vector<HyprPicker::MonitorEntry> monitors) {
    layout->addChild(sectionTitle("Region"));
    layout->addChild(sectionSpacer());

    const bool canSelectRegion = !monitors.empty() && std::system("command -v slurp >/dev/null 2>&1") == 0;
    layout->addChild(selectionButton("Select region...", [monitors = std::move(monitors), canSelectRegion](SP<CButtonElement>) {
        if (!canSelectRegion)
            finishWithoutSelection();

        const auto slurp = runCommand("slurp -f '%o %x %y %w %h' 2>/dev/null");
        const auto region = HyprPicker::parseSlurpRegion(slurp, monitors);
        if (!region)
            finishWithoutSelection();

        finishWithSelection(HyprPicker::formatRegionSelection(*region, allowToken()));
    }));

    if (!canSelectRegion)
        layout->addChild(note("Region selection needs Hyprland monitor data and slurp in PATH."));
}

void addFooter(SP<CColumnLayoutElement> layout, bool allowTokenByDefault) {
    auto row = CRowLayoutBuilder::begin()->gap(8)->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})->commence();
    row->setMargin(5);

    g_allowToken = CCheckboxBuilder::begin()->toggled(allowTokenByDefault)->commence();
    auto label   = CTextBuilder::begin()->text("Allow restore token")->color([] { return g_backend->getPalette()->m_colors.text; })->commence();
    auto spacer  = CNullBuilder::begin()->commence();
    spacer->setGrow(true);

    auto cancel = CButtonBuilder::begin()
                      ->label("Cancel")
                      ->size({CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                      ->onMainClick([](SP<CButtonElement>) {
                          finishWithoutSelection();
                      })
                      ->commence();

    row->addChild(g_allowToken);
    row->addChild(label);
    row->addChild(spacer);
    row->addChild(cancel);
    layout->addChild(row);
}

} // namespace

int main(int argc, char** argv) {
    bool allowTokenByDefault = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--allow-token")
            allowTokenByDefault = true;
    }

    g_backend = IBackend::create();
    g_backend->setLogFn([](eLogLevel, const std::string&) {});

    const auto monitors = getMonitors();
    const auto windows  = HyprPicker::parseWindowList(std::getenv("XDPH_WINDOW_SHARING_LIST"));
    debugLog(std::format("start monitors={} windows={} allowTokenByDefault={}", monitors.size(), windows.size(), allowTokenByDefault));
    const int width     = 810;
    const int height    = 420;

    g_window = CWindowBuilder::begin()
                   ->preferredSize({static_cast<float>(width), static_cast<float>(height)})
                   ->minSize({static_cast<float>(width), static_cast<float>(height)})
                   ->maxSize({static_cast<float>(width), static_cast<float>(height)})
                   ->appTitle("Screen sharing")
                   ->appClass("hyprscreenpicker")
                   ->commence();

    auto bg = CRectangleBuilder::begin()->color([] { return g_backend->getPalette()->m_colors.background; })->commence();
    g_window->m_rootElement->addChild(bg);

    auto outer = CColumnLayoutBuilder::begin()->gap(4)->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})->commence();
    outer->setMargin(6);
    bg->addChild(outer);

    auto listsRow = CRowLayoutBuilder::begin()->gap(8)->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 285.F}})->commence();
    outer->addChild(listsRow);

    auto screensColumn = CColumnLayoutBuilder::begin()->gap(2)->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {387.F, 285.F}})->commence();
    auto windowsColumn = CColumnLayoutBuilder::begin()->gap(2)->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {387.F, 285.F}})->commence();
    listsRow->addChild(screensColumn);
    listsRow->addChild(windowsColumn);

    addScreenSection(screensColumn, monitors);
    addWindowSection(windowsColumn, windows);

    addRegionSection(outer, monitors);
    addFooter(outer, allowTokenByDefault);

    g_window->m_events.closeRequest.listenStatic([] {
        finishWithoutSelection();
    });

    g_window->open();
    g_backend->enterLoop();
    return 0;
}
