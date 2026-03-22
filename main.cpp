#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr wchar_t kAppName[] = L"Secure Key Overlay";
constexpr wchar_t kMainClassName[] = L"SecureKeyOverlayMainClass";
constexpr wchar_t kOverlayClassName[] = L"SecureKeyOverlayOverlayClass";
constexpr wchar_t kSettingsClassName[] = L"SecureKeyOverlaySettingsClass";
constexpr wchar_t kPreviewClassName[] = L"SecureKeyOverlayPreviewClass";

constexpr UINT WM_TRAYICON = WM_APP + 1;

constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kTrayHealthTimerId = 1;
constexpr UINT kTrayHealthCheckMs = 1000;

constexpr UINT IDM_TRAY_START = 1001;
constexpr UINT IDM_TRAY_STOP = 1002;
constexpr UINT IDM_TRAY_SETTINGS = 1003;
constexpr UINT IDM_TRAY_EXIT = 1004;

constexpr int IDC_KEY_W = 2001;
constexpr int IDC_KEY_A = 2002;
constexpr int IDC_KEY_S = 2003;
constexpr int IDC_KEY_D = 2004;
constexpr int IDC_KEY_CTRL = 2005;
constexpr int IDC_KEY_SHIFT = 2006;
constexpr int IDC_KEY_ALT = 2007;
constexpr int IDC_KEY_TAB = 2008;
constexpr int IDC_KEY_Q = 2009;
constexpr int IDC_KEY_E = 2014;
constexpr int IDC_KEY_R = 2015;
constexpr int IDC_KEY_F = 2016;
constexpr int IDC_CUSTOM_KEY_EDIT = 2010;
constexpr int IDC_CUSTOM_KEY_ADD = 2011;
constexpr int IDC_CUSTOM_KEY_LIST = 2012;
constexpr int IDC_CUSTOM_KEY_REMOVE = 2013;

constexpr int IDC_POS_TOP_LEFT = 2101;
constexpr int IDC_POS_TOP_RIGHT = 2102;
constexpr int IDC_POS_BOTTOM_LEFT = 2103;
constexpr int IDC_POS_BOTTOM_RIGHT = 2104;
constexpr int IDC_POS_CUSTOM = 2105;
constexpr int IDC_CUSTOM_X_EDIT = 2106;
constexpr int IDC_CUSTOM_Y_EDIT = 2107;

constexpr int IDC_SIZE_SLIDER = 2201;
constexpr int IDC_SPACING_SLIDER = 2202;
constexpr int IDC_COLOR_IDLE = 2203;
constexpr int IDC_COLOR_ACTIVE = 2204;
constexpr int IDC_COLOR_TEXT = 2205;
constexpr int IDC_ACTIVE_ALPHA_SLIDER = 2206;
constexpr int IDC_ARRAY_BUTTON = 2207;

constexpr int IDC_PREVIEW_PANEL = 2301;
constexpr int IDC_SETTINGS_CLOSE = 2302;
constexpr int IDC_SETTINGS_START = 2303;
constexpr int IDC_SETTINGS_STOP = 2304;
constexpr int IDC_SETTINGS_STATUS = 2305;
constexpr int IDC_CAPTURE_STATUS = 2306;
constexpr int IDI_APP_ICON = 101;

enum class AppState {
    Idle,
    Active,
    Stopped,
};

enum class PositionPreset {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Custom,
};

struct KeyDefinition {
    const wchar_t* label;
    UINT vk;
    int controlId;
};

constexpr std::array<KeyDefinition, 12> kDefaultKeyDefs{{
    {L"Tab", VK_TAB, IDC_KEY_TAB},
    {L"Q", 'Q', IDC_KEY_Q},
    {L"W", 'W', IDC_KEY_W},
    {L"E", 'E', IDC_KEY_E},
    {L"R", 'R', IDC_KEY_R},
    {L"Shift", VK_SHIFT, IDC_KEY_SHIFT},
    {L"A", 'A', IDC_KEY_A},
    {L"S", 'S', IDC_KEY_S},
    {L"D", 'D', IDC_KEY_D},
    {L"F", 'F', IDC_KEY_F},
    {L"Ctrl", VK_CONTROL, IDC_KEY_CTRL},
    {L"Alt", VK_MENU, IDC_KEY_ALT},
}};

struct KeyLayoutSpec {
    UINT vk;
    int row;
    float startUnit;
    float widthUnits;
};

constexpr std::array<KeyLayoutSpec, 13> kKeyboardLayoutSpecs{{
    {VK_TAB, 0, 0.0f, 1.45f},
    {'Q', 0, 1.60f, 1.00f},
    {'W', 0, 2.70f, 1.00f},
    {'E', 0, 3.80f, 1.00f},
    {'R', 0, 4.90f, 1.00f},
    {VK_SHIFT, 1, 0.0f, 1.80f},
    {'A', 1, 1.95f, 1.00f},
    {'S', 1, 3.05f, 1.00f},
    {'D', 1, 4.15f, 1.00f},
    {'F', 1, 5.25f, 1.00f},
    {VK_CONTROL, 2, 0.0f, 1.55f},
    {VK_MENU, 2, 1.75f, 1.55f},
    {VK_SPACE, 2, 3.50f, 3.90f},
}};

struct KeyPlacement {
    UINT vk;
    int row;
    RECT rect;
};

struct KeyLayoutMetrics {
    std::vector<KeyPlacement> placements;
    int contentWidth = 0;
    int contentHeight = 0;
    int keyWidth = 0;
    int keyHeight = 0;
};

struct AppConfig {
    std::array<bool, kDefaultKeyDefs.size()> enabledDefaults{};
    std::vector<UINT> customKeys;
    PositionPreset preset = PositionPreset::TopRight;
    POINT customPos{100, 100};
    int keySize = 50;
    int spacing = 6;
    int activeAlpha = 232;
    COLORREF idleColor = RGB(58, 62, 68);
    COLORREF activeColor = RGB(196, 212, 65);
    COLORREF textColor = RGB(245, 247, 242);
    bool useCustomArrayLayout = false;
    std::unordered_map<UINT, POINT> customLayoutPositions;
};

struct SettingsUi {
    std::array<HWND, kDefaultKeyDefs.size()> keyChecks{};
    HWND captureStatus = nullptr;
    HWND customAddButton = nullptr;
    HWND customList = nullptr;
    HWND customRemoveButton = nullptr;
    std::array<HWND, 5> positionRadios{};
    HWND customXEdit = nullptr;
    HWND customYEdit = nullptr;
    HWND keySizeSlider = nullptr;
    HWND spacingSlider = nullptr;
    HWND activeAlphaSlider = nullptr;
    HWND idleColorButton = nullptr;
    HWND activeColorButton = nullptr;
    HWND textColorButton = nullptr;
    HWND arrayButton = nullptr;
    HWND previewPanel = nullptr;
    HWND statusLabel = nullptr;
    HWND startButton = nullptr;
    HWND stopButton = nullptr;
    HWND closeButton = nullptr;
};

HINSTANCE g_hInstance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_overlayWindow = nullptr;
HWND g_settingsWindow = nullptr;
SettingsUi g_settingsUi{};
bool g_trayIconCreated = false;

AppConfig g_config{};
AppState g_state = AppState::Idle;
bool g_rawInputRegistered = false;
std::unordered_map<UINT, bool> g_pressed;
bool g_captureCustomKey = false;
bool g_arrayEditMode = false;
UINT g_draggingKey = 0;
POINT g_dragOffset{};

HFONT g_uiFont = nullptr;
UINT g_taskbarCreatedMessage = 0;

bool IsVkSelected(UINT vk) {
    for (size_t i = 0; i < kDefaultKeyDefs.size(); ++i) {
        if (kDefaultKeyDefs[i].vk == vk && g_config.enabledDefaults[i]) {
            return true;
        }
    }
    return std::find(g_config.customKeys.begin(), g_config.customKeys.end(), vk) != g_config.customKeys.end();
}

bool IsTrayHostAvailable() {
    return FindWindowW(L"Shell_TrayWnd", nullptr) != nullptr;
}

std::wstring Trim(const std::wstring& source) {
    size_t begin = 0;
    while (begin < source.size() && iswspace(source[begin]) != 0) {
        ++begin;
    }
    size_t end = source.size();
    while (end > begin && iswspace(source[end - 1]) != 0) {
        --end;
    }
    return source.substr(begin, end - begin);
}

std::wstring ToUpper(std::wstring value) {
    for (wchar_t& ch : value) {
        ch = towupper(ch);
    }
    return value;
}

std::wstring VkToLabel(UINT vk) {
    for (const auto& def : kDefaultKeyDefs) {
        if (def.vk == vk) {
            return def.label;
        }
    }

    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }
    if (vk == VK_SPACE) {
        return L"Space";
    }
    if (vk == VK_TAB) {
        return L"Tab";
    }
    if (vk == VK_RETURN) {
        return L"Enter";
    }
    if (vk == VK_ESCAPE) {
        return L"Esc";
    }
    if (vk == VK_LEFT) {
        return L"Left";
    }
    if (vk == VK_RIGHT) {
        return L"Right";
    }
    if (vk == VK_UP) {
        return L"Up";
    }
    if (vk == VK_DOWN) {
        return L"Down";
    }

    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scan != 0) {
        wchar_t name[64] = {};
        LONG lParam = static_cast<LONG>(scan << 16);
        if (GetKeyNameTextW(lParam, name, static_cast<int>(std::size(name))) > 0) {
            return name;
        }
    }

    return L"VK " + std::to_wstring(vk);
}

bool ParseKeyText(const std::wstring& userText, UINT& outVk) {
    std::wstring token = ToUpper(Trim(userText));
    if (token.empty()) {
        return false;
    }

    if (token.size() == 1) {
        wchar_t ch = token[0];
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
            outVk = static_cast<UINT>(ch);
            return true;
        }
    }

    if (token == L"CTRL" || token == L"CONTROL") {
        outVk = VK_CONTROL;
        return true;
    }
    if (token == L"SHIFT") {
        outVk = VK_SHIFT;
        return true;
    }
    if (token == L"ALT" || token == L"MENU") {
        outVk = VK_MENU;
        return true;
    }
    if (token == L"SPACE") {
        outVk = VK_SPACE;
        return true;
    }
    if (token == L"TAB") {
        outVk = VK_TAB;
        return true;
    }
    if (token == L"ENTER" || token == L"RETURN") {
        outVk = VK_RETURN;
        return true;
    }
    if (token == L"ESC" || token == L"ESCAPE") {
        outVk = VK_ESCAPE;
        return true;
    }
    if (token == L"LEFT") {
        outVk = VK_LEFT;
        return true;
    }
    if (token == L"RIGHT") {
        outVk = VK_RIGHT;
        return true;
    }
    if (token == L"UP") {
        outVk = VK_UP;
        return true;
    }
    if (token == L"DOWN") {
        outVk = VK_DOWN;
        return true;
    }

    if (token.size() >= 2 && token[0] == L'F') {
        int fn = _wtoi(token.c_str() + 1);
        if (fn >= 1 && fn <= 24) {
            outVk = static_cast<UINT>(VK_F1 + (fn - 1));
            return true;
        }
    }

    return false;
}

UINT NormalizeCapturedVKey(UINT vk) {
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        return VK_SHIFT;
    }
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
        return VK_CONTROL;
    }
    if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
        return VK_MENU;
    }
    return vk;
}

const KeyLayoutSpec* FindLayoutSpec(UINT vk) {
    for (const auto& spec : kKeyboardLayoutSpecs) {
        if (spec.vk == vk) {
            return &spec;
        }
    }
    return nullptr;
}

UINT NormalizeRawVKey(const RAWKEYBOARD& keyboard) {
    UINT vk = keyboard.VKey;
    if (vk == 0 || vk == 255) {
        return 0;
    }
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        return VK_SHIFT;
    }
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
        return VK_CONTROL;
    }
    if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
        return VK_MENU;
    }
    return vk;
}

std::vector<UINT> CurrentSelectedKeys() {
    std::vector<UINT> selected;
    selected.reserve(kDefaultKeyDefs.size() + g_config.customKeys.size());
    std::array<bool, 256> seen{};

    for (size_t i = 0; i < kDefaultKeyDefs.size(); ++i) {
        if (!g_config.enabledDefaults[i]) {
            continue;
        }
        const UINT vk = kDefaultKeyDefs[i].vk;
        if (vk < seen.size() && seen[vk]) {
            continue;
        }
        if (vk < seen.size()) {
            seen[vk] = true;
        }
        selected.push_back(vk);
    }

    for (UINT vk : g_config.customKeys) {
        if (vk < seen.size() && seen[vk]) {
            continue;
        }
        if (vk < seen.size()) {
            seen[vk] = true;
        }
        selected.push_back(vk);
    }
    return selected;
}

KeyLayoutMetrics BuildAutoKeyLayoutMetrics() {
    KeyLayoutMetrics metrics{};
    const auto selectedKeys = CurrentSelectedKeys();
    if (selectedKeys.empty()) {
        return metrics;
    }

    metrics.keyWidth = g_config.keySize;
    metrics.keyHeight = std::max(30, static_cast<int>(std::lround(g_config.keySize * 0.78f)));
    const int spacing = g_config.spacing;
    const int rowGap = std::max(12, spacing + 8);
    const float pitchX = static_cast<float>(metrics.keyWidth + spacing);
    const int pitchY = metrics.keyHeight + rowGap;

    int maxKnownRow = -1;
    std::vector<UINT> fallbackKeys;
    for (UINT vk : selectedKeys) {
        const KeyLayoutSpec* spec = FindLayoutSpec(vk);
        if (spec == nullptr) {
            fallbackKeys.push_back(vk);
            continue;
        }

        const int left = static_cast<int>(std::lround(spec->startUnit * pitchX));
        const int top = spec->row * pitchY;
        const int width = static_cast<int>(std::lround((spec->widthUnits * metrics.keyWidth) + ((spec->widthUnits - 1.0f) * spacing)));
        const int height = metrics.keyHeight;
        metrics.placements.push_back(KeyPlacement{vk, spec->row, RECT{left, top, left + width, top + height}});
        metrics.contentWidth = std::max(metrics.contentWidth, left + width);
        metrics.contentHeight = std::max(metrics.contentHeight, top + height);
        maxKnownRow = std::max(maxKnownRow, spec->row);
    }

    const int fallbackStartRow = maxKnownRow + 1;
    constexpr int kFallbackColumns = 5;
    for (size_t i = 0; i < fallbackKeys.size(); ++i) {
        const int row = fallbackStartRow + static_cast<int>(i / kFallbackColumns);
        const int column = static_cast<int>(i % kFallbackColumns);
        const int left = column * static_cast<int>(std::lround(pitchX));
        const int top = row * pitchY;
        const int width = metrics.keyWidth;
        const int height = metrics.keyHeight;
        metrics.placements.push_back(KeyPlacement{fallbackKeys[i], row, RECT{left, top, left + width, top + height}});
        metrics.contentWidth = std::max(metrics.contentWidth, left + width);
        metrics.contentHeight = std::max(metrics.contentHeight, top + height);
    }

    std::sort(metrics.placements.begin(), metrics.placements.end(), [](const KeyPlacement& lhs, const KeyPlacement& rhs) {
        if (lhs.row != rhs.row) {
            return lhs.row < rhs.row;
        }
        return lhs.rect.left < rhs.rect.left;
    });

    return metrics;
}

void EnsureCustomArrayLayoutSeeded() {
    if (!g_config.useCustomArrayLayout) {
        return;
    }

    const KeyLayoutMetrics autoMetrics = BuildAutoKeyLayoutMetrics();
    for (const KeyPlacement& placement : autoMetrics.placements) {
        if (g_config.customLayoutPositions.find(placement.vk) == g_config.customLayoutPositions.end()) {
            g_config.customLayoutPositions[placement.vk] = POINT{placement.rect.left, placement.rect.top};
        }
    }
}

KeyLayoutMetrics BuildKeyLayoutMetrics() {
    if (!g_config.useCustomArrayLayout) {
        return BuildAutoKeyLayoutMetrics();
    }

    EnsureCustomArrayLayoutSeeded();

    KeyLayoutMetrics metrics{};
    const auto selectedKeys = CurrentSelectedKeys();
    if (selectedKeys.empty()) {
        return metrics;
    }

    metrics.keyWidth = g_config.keySize;
    metrics.keyHeight = std::max(30, static_cast<int>(std::lround(g_config.keySize * 0.78f)));

    bool first = true;
    int minLeft = 0;
    int minTop = 0;
    for (UINT vk : selectedKeys) {
        POINT pt{0, 0};
        auto it = g_config.customLayoutPositions.find(vk);
        if (it != g_config.customLayoutPositions.end()) {
            pt = it->second;
        }

        if (first) {
            minLeft = pt.x;
            minTop = pt.y;
            first = false;
        } else {
            minLeft = std::min(minLeft, static_cast<int>(pt.x));
            minTop = std::min(minTop, static_cast<int>(pt.y));
        }
    }

    for (UINT vk : selectedKeys) {
        POINT pt{0, 0};
        auto it = g_config.customLayoutPositions.find(vk);
        if (it != g_config.customLayoutPositions.end()) {
            pt = it->second;
        }

        const int left = pt.x - minLeft;
        const int top = pt.y - minTop;
        int width = metrics.keyWidth;
        if (const KeyLayoutSpec* spec = FindLayoutSpec(vk); spec != nullptr) {
            width = static_cast<int>(std::lround((spec->widthUnits * metrics.keyWidth) + ((spec->widthUnits - 1.0f) * g_config.spacing)));
        }
        const int height = metrics.keyHeight;
        metrics.placements.push_back(KeyPlacement{vk, 0, RECT{left, top, left + width, top + height}});
        metrics.contentWidth = std::max(metrics.contentWidth, left + width);
        metrics.contentHeight = std::max(metrics.contentHeight, top + height);
    }

    return metrics;
}

void SetCapturePrompt(const wchar_t* text) {
    if (g_settingsUi.captureStatus != nullptr) {
        SetWindowTextW(g_settingsUi.captureStatus, text);
    }
}

RECT ComputePreviewCanvasRect(const RECT& bounds) {
    RECT canvas = bounds;
    InflateRect(&canvas, -10, -10);
    return canvas;
}

POINT ComputeLayoutOrigin(const RECT& bounds, const KeyLayoutMetrics& metrics, bool previewMode) {
    RECT canvas = bounds;
    if (previewMode) {
        canvas = ComputePreviewCanvasRect(bounds);
    }

    int originX = static_cast<int>(canvas.left) + 18;
    int originY = static_cast<int>(canvas.top) + 22;
    if (previewMode && !g_arrayEditMode) {
        const int canvasWidth = static_cast<int>(canvas.right - canvas.left);
        const int canvasHeight = static_cast<int>(canvas.bottom - canvas.top);
        originX = static_cast<int>(canvas.left) + std::max(18, (canvasWidth - metrics.contentWidth) / 2);
        originY = static_cast<int>(canvas.top) + std::max(34, (canvasHeight - metrics.contentHeight) / 2);
    } else if (previewMode) {
        originX = static_cast<int>(canvas.left) + 18;
        originY = static_cast<int>(canvas.top) + 48;
    }
    return POINT{originX, originY};
}

float ComputePreviewScale(const RECT& bounds, const KeyLayoutMetrics& metrics) {
    if (metrics.contentWidth <= 0 || metrics.contentHeight <= 0) {
        return 1.0f;
    }

    const RECT canvas = ComputePreviewCanvasRect(bounds);
    const float availableWidth = static_cast<float>((canvas.right - canvas.left) - 40);
    const float availableHeight = static_cast<float>((canvas.bottom - canvas.top) - 82);
    const float scaleX = availableWidth / static_cast<float>(metrics.contentWidth);
    const float scaleY = availableHeight / static_cast<float>(metrics.contentHeight);
    return std::clamp(std::min({scaleX, scaleY, 0.72f}), 0.38f, 0.72f);
}

RECT ScaleRectForPreview(const RECT& rect, const POINT& origin, float scale) {
    const int width = std::max(20, static_cast<int>(std::lround((rect.right - rect.left) * scale)));
    const int height = std::max(18, static_cast<int>(std::lround((rect.bottom - rect.top) * scale)));
    const int left = origin.x + static_cast<int>(std::lround(rect.left * scale));
    const int top = origin.y + static_cast<int>(std::lround(rect.top * scale));
    return RECT{left, top, left + width, top + height};
}

void RefreshCustomKeyList();
void NotifyConfigChanged();
void UpdateOverlayLayout();
void UpdateSettingsActionControls();

bool TryAddCustomKey(UINT vk) {
    vk = NormalizeCapturedVKey(vk);
    if (vk == 0 || vk == VK_ESCAPE) {
        return false;
    }
    if (!IsVkSelected(vk)) {
        g_config.customKeys.push_back(vk);
        RefreshCustomKeyList();
        if (g_config.useCustomArrayLayout) {
            EnsureCustomArrayLayoutSeeded();
        }
        NotifyConfigChanged();
    }
    return true;
}

void RefreshCustomKeyList() {
    if (g_settingsUi.customList == nullptr) {
        return;
    }
    SendMessageW(g_settingsUi.customList, LB_RESETCONTENT, 0, 0);
    for (UINT vk : g_config.customKeys) {
        const std::wstring label = VkToLabel(vk);
        SendMessageW(g_settingsUi.customList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}

HICON LoadAppIcon(int size) {
    return reinterpret_cast<HICON>(LoadImageW(
        g_hInstance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        size,
        size,
        LR_DEFAULTCOLOR
    ));
}

void RequestAppExit() {
    if (g_mainWindow != nullptr) {
        DestroyWindow(g_mainWindow);
        return;
    }
    if (g_settingsWindow != nullptr) {
        DestroyWindow(g_settingsWindow);
    }
}

void NotifyConfigChanged() {
    if (g_settingsUi.previewPanel != nullptr) {
        InvalidateRect(g_settingsUi.previewPanel, nullptr, TRUE);
    }
    if (g_overlayWindow != nullptr) {
        UpdateOverlayLayout();
        InvalidateRect(g_overlayWindow, nullptr, TRUE);
    }
}

void SetPresetRadioChecks(PositionPreset preset) {
    const int checked = BST_CHECKED;
    const int unchecked = BST_UNCHECKED;
    if (g_settingsUi.positionRadios[0]) {
        SendMessageW(g_settingsUi.positionRadios[0], BM_SETCHECK, preset == PositionPreset::TopLeft ? checked : unchecked, 0);
    }
    if (g_settingsUi.positionRadios[1]) {
        SendMessageW(g_settingsUi.positionRadios[1], BM_SETCHECK, preset == PositionPreset::TopRight ? checked : unchecked, 0);
    }
    if (g_settingsUi.positionRadios[2]) {
        SendMessageW(g_settingsUi.positionRadios[2], BM_SETCHECK, preset == PositionPreset::BottomLeft ? checked : unchecked, 0);
    }
    if (g_settingsUi.positionRadios[3]) {
        SendMessageW(g_settingsUi.positionRadios[3], BM_SETCHECK, preset == PositionPreset::BottomRight ? checked : unchecked, 0);
    }
    if (g_settingsUi.positionRadios[4]) {
        SendMessageW(g_settingsUi.positionRadios[4], BM_SETCHECK, preset == PositionPreset::Custom ? checked : unchecked, 0);
    }
}

void SyncSettingsControlsFromConfig() {
    for (size_t i = 0; i < kDefaultKeyDefs.size(); ++i) {
        if (g_settingsUi.keyChecks[i] != nullptr) {
            SendMessageW(g_settingsUi.keyChecks[i], BM_SETCHECK, g_config.enabledDefaults[i] ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    SetPresetRadioChecks(g_config.preset);

    if (g_settingsUi.customXEdit != nullptr) {
        SetWindowTextW(g_settingsUi.customXEdit, std::to_wstring(g_config.customPos.x).c_str());
    }
    if (g_settingsUi.customYEdit != nullptr) {
        SetWindowTextW(g_settingsUi.customYEdit, std::to_wstring(g_config.customPos.y).c_str());
    }
    if (g_settingsUi.keySizeSlider != nullptr) {
        SendMessageW(g_settingsUi.keySizeSlider, TBM_SETPOS, TRUE, g_config.keySize);
    }
    if (g_settingsUi.spacingSlider != nullptr) {
        SendMessageW(g_settingsUi.spacingSlider, TBM_SETPOS, TRUE, g_config.spacing);
    }
    if (g_settingsUi.activeAlphaSlider != nullptr) {
        SendMessageW(g_settingsUi.activeAlphaSlider, TBM_SETPOS, TRUE, g_config.activeAlpha);
    }
    if (g_settingsUi.arrayButton != nullptr) {
        SetWindowTextW(g_settingsUi.arrayButton, g_arrayEditMode ? L"Done" : L"Array");
    }

    RefreshCustomKeyList();
    SetCapturePrompt(g_captureCustomKey ? L"Press the key you want to overlay..." : L"Press the key you want to overlay");
    NotifyConfigChanged();
}

void PickColor(HWND owner, COLORREF& target) {
    static COLORREF customColors[16] = {};
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.rgbResult = target;
    cc.lpCustColors = customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        target = cc.rgbResult;
        NotifyConfigChanged();
    }
}

RECT ComputeOverlayRect() {
    const KeyLayoutMetrics metrics = BuildKeyLayoutMetrics();
    const int paddingX = 18;
    const int paddingY = 18;
    const int width = metrics.placements.empty() ? 260 : metrics.contentWidth + (paddingX * 2);
    const int height = metrics.placements.empty() ? 120 : metrics.contentHeight + (paddingY * 2);
    const int margin = 24;

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const RECT screen = monitorInfo.rcMonitor;

    RECT rc{};
    switch (g_config.preset) {
        case PositionPreset::TopLeft:
            rc.left = screen.left + margin;
            rc.top = screen.top + margin;
            break;
        case PositionPreset::TopRight:
            rc.left = screen.right - width - margin;
            rc.top = screen.top + margin;
            break;
        case PositionPreset::BottomLeft:
            rc.left = screen.left + margin;
            rc.top = screen.bottom - height - margin;
            break;
        case PositionPreset::BottomRight:
            rc.left = screen.right - width - margin;
            rc.top = screen.bottom - height - margin;
            break;
        case PositionPreset::Custom:
            rc.left = g_config.customPos.x;
            rc.top = g_config.customPos.y;
            break;
    }
    rc.right = rc.left + width;
    rc.bottom = rc.top + height;
    return rc;
}

COLORREF BlendColor(COLORREF base, COLORREF target, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto blendChannel = [amount](BYTE from, BYTE to) -> BYTE {
        return static_cast<BYTE>(std::lround((from * (1.0f - amount)) + (to * amount)));
    };
    return RGB(
        blendChannel(GetRValue(base), GetRValue(target)),
        blendChannel(GetGValue(base), GetGValue(target)),
        blendChannel(GetBValue(base), GetBValue(target))
    );
}

void DrawKeys(HDC hdc, const RECT& bounds, bool previewMode) {
    const KeyLayoutMetrics metrics = BuildKeyLayoutMetrics();

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_config.textColor);

    if (metrics.placements.empty()) {
        RECT messageRect = bounds;
        DrawTextW(hdc, L"No keys selected", -1, &messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    RECT canvas = bounds;
    if (previewMode) {
        canvas = ComputePreviewCanvasRect(bounds);
        const HBRUSH panelBrush = CreateSolidBrush(RGB(26, 30, 36));
        const HPEN panelPen = CreatePen(PS_SOLID, 1, RGB(69, 75, 84));
        const HGDIOBJ oldBrush = SelectObject(hdc, panelBrush);
        const HGDIOBJ oldPen = SelectObject(hdc, panelPen);
        RoundRect(hdc, canvas.left, canvas.top, canvas.right, canvas.bottom, 20, 20);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(panelBrush);
        DeleteObject(panelPen);

        RECT captionRect{canvas.left + 18, canvas.top + 12, canvas.right - 18, canvas.top + 30};
        SetTextColor(hdc, RGB(170, 178, 188));
        DrawTextW(hdc, g_arrayEditMode ? L"Overlay Preview  •  Drag to rearrange" : L"Overlay Preview", -1, &captionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(hdc, g_config.textColor);
    }

    float previewScale = 1.0f;
    POINT origin = ComputeLayoutOrigin(bounds, metrics, previewMode);
    if (previewMode) {
        previewScale = ComputePreviewScale(bounds, metrics);
        const RECT canvas = ComputePreviewCanvasRect(bounds);
        const int scaledWidth = static_cast<int>(std::lround(metrics.contentWidth * previewScale));
        const int scaledHeight = static_cast<int>(std::lround(metrics.contentHeight * previewScale));
        const int canvasWidth = static_cast<int>(canvas.right - canvas.left);
        const int canvasHeight = static_cast<int>(canvas.bottom - canvas.top);
        if (!g_arrayEditMode) {
            origin.x = static_cast<int>(canvas.left) + std::max(18, (canvasWidth - scaledWidth) / 2);
            origin.y = static_cast<int>(canvas.top) + std::max(36, (canvasHeight - scaledHeight) / 2);
        } else {
            origin.x = static_cast<int>(canvas.left) + 18;
            origin.y = static_cast<int>(canvas.top) + 48;
        }
    }

    HFONT keyFont = CreateFontW(
        -std::max(12, static_cast<int>(std::lround((previewMode ? metrics.keyHeight * previewScale : metrics.keyHeight) / 2.0f))),
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        VARIABLE_PITCH,
        L"Bahnschrift SemiCondensed"
    );
    HGDIOBJ oldFont = SelectObject(hdc, keyFont != nullptr ? keyFont : g_uiFont);

    for (size_t index = 0; index < metrics.placements.size(); ++index) {
        const KeyPlacement& placement = metrics.placements[index];
        const bool isPressed = previewMode ? (index % 4 == 2) : g_pressed[placement.vk];
        RECT keyRect = previewMode
            ? ScaleRectForPreview(placement.rect, origin, previewScale)
            : RECT{origin.x + placement.rect.left, origin.y + placement.rect.top, origin.x + placement.rect.right, origin.y + placement.rect.bottom};
        RECT shadowRect = keyRect;
        OffsetRect(&shadowRect, 0, 3);

        COLORREF baseColor = isPressed ? g_config.activeColor : g_config.idleColor;
        if (previewMode) {
            baseColor = BlendColor(RGB(26, 30, 36), baseColor, g_config.activeAlpha / 255.0f);
        }
        const COLORREF topColor = BlendColor(baseColor, RGB(255, 255, 255), isPressed ? 0.10f : 0.18f);
        const COLORREF faceColor = BlendColor(baseColor, RGB(0, 0, 0), isPressed ? 0.08f : 0.02f);
        const COLORREF borderColor = BlendColor(baseColor, RGB(0, 0, 0), 0.45f);

        const HBRUSH shadowBrush = CreateSolidBrush(RGB(10, 12, 16));
        const HGDIOBJ oldShadowBrush = SelectObject(hdc, shadowBrush);
        const HPEN shadowPen = CreatePen(PS_SOLID, 1, RGB(10, 12, 16));
        const HGDIOBJ oldShadowPen = SelectObject(hdc, shadowPen);
        RoundRect(hdc, shadowRect.left, shadowRect.top, shadowRect.right, shadowRect.bottom, 14, 14);
        SelectObject(hdc, oldShadowBrush);
        SelectObject(hdc, oldShadowPen);
        DeleteObject(shadowBrush);
        DeleteObject(shadowPen);

        const HBRUSH keyBrush = CreateSolidBrush(faceColor);
        const HPEN keyPen = CreatePen(PS_SOLID, 1, borderColor);
        const HGDIOBJ oldKeyBrush = SelectObject(hdc, keyBrush);
        const HGDIOBJ oldKeyPen = SelectObject(hdc, keyPen);
        RoundRect(hdc, keyRect.left, keyRect.top, keyRect.right, keyRect.bottom, 14, 14);
        SelectObject(hdc, oldKeyBrush);
        SelectObject(hdc, oldKeyPen);
        DeleteObject(keyBrush);
        DeleteObject(keyPen);

        RECT highlightRect = keyRect;
        highlightRect.bottom = highlightRect.top + std::max(8, metrics.keyHeight / 3);
        InflateRect(&highlightRect, -2, -2);
        const HBRUSH highlightBrush = CreateSolidBrush(topColor);
        FillRect(hdc, &highlightRect, highlightBrush);
        DeleteObject(highlightBrush);

        const std::wstring label = VkToLabel(placement.vk);
        RECT textRect = keyRect;
        textRect.top += isPressed ? 1 : 0;
        DrawTextW(hdc, label.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
    if (keyFont != nullptr) {
        DeleteObject(keyFont);
    }
}

bool HitTestPreviewKey(POINT clientPoint, UINT& outVk, POINT& outOffset) {
    if (!g_arrayEditMode) {
        return false;
    }

    RECT client{};
    GetClientRect(g_settingsUi.previewPanel, &client);
    const KeyLayoutMetrics metrics = BuildKeyLayoutMetrics();
    const float previewScale = ComputePreviewScale(client, metrics);
    POINT origin = ComputeLayoutOrigin(client, metrics, true);
    const RECT canvas = ComputePreviewCanvasRect(client);
    const int canvasWidth = static_cast<int>(canvas.right - canvas.left);
    const int canvasHeight = static_cast<int>(canvas.bottom - canvas.top);
    if (!g_arrayEditMode) {
        origin.x = static_cast<int>(canvas.left) + std::max(18, (canvasWidth - static_cast<int>(std::lround(metrics.contentWidth * previewScale))) / 2);
        origin.y = static_cast<int>(canvas.top) + std::max(36, (canvasHeight - static_cast<int>(std::lround(metrics.contentHeight * previewScale))) / 2);
    } else {
        origin.x = static_cast<int>(canvas.left) + 18;
        origin.y = static_cast<int>(canvas.top) + 48;
    }

    for (const KeyPlacement& placement : metrics.placements) {
        RECT rect = ScaleRectForPreview(placement.rect, origin, previewScale);
        if (PtInRect(&rect, clientPoint)) {
            outVk = placement.vk;
            outOffset = POINT{clientPoint.x - rect.left, clientPoint.y - rect.top};
            return true;
        }
    }
    return false;
}

void UpdateDraggedKeyPosition(POINT clientPoint) {
    if (g_draggingKey == 0 || !g_config.useCustomArrayLayout) {
        return;
    }

    RECT client{};
    GetClientRect(g_settingsUi.previewPanel, &client);
    const KeyLayoutMetrics metrics = BuildKeyLayoutMetrics();
    const float previewScale = ComputePreviewScale(client, metrics);
    POINT origin = ComputeLayoutOrigin(client, metrics, true);
    const RECT canvas = ComputePreviewCanvasRect(client);
    origin.x = static_cast<int>(canvas.left) + 18;
    origin.y = static_cast<int>(canvas.top) + 48;

    POINT newPoint{
        static_cast<LONG>(std::lround((clientPoint.x - origin.x - g_dragOffset.x) / previewScale)),
        static_cast<LONG>(std::lround((clientPoint.y - origin.y - g_dragOffset.y) / previewScale))
    };
    const LONG maxX = std::max<LONG>(0, static_cast<LONG>(std::lround(((canvas.right - canvas.left) - 36) / previewScale)) - metrics.keyWidth);
    const LONG maxY = std::max<LONG>(0, static_cast<LONG>(std::lround(((canvas.bottom - canvas.top) - 72) / previewScale)) - metrics.keyHeight);
    newPoint.x = std::max<LONG>(0, std::min<LONG>(newPoint.x, maxX));
    newPoint.y = std::max<LONG>(0, std::min<LONG>(newPoint.y, maxY));
    g_config.customLayoutPositions[g_draggingKey] = newPoint;
    NotifyConfigChanged();
}

bool RegisterRawKeyboardInput(bool enable) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
    rid.hwndTarget = enable ? g_mainWindow : nullptr;
    const BOOL success = RegisterRawInputDevices(&rid, 1, sizeof(rid));
    if (!success) {
        return false;
    }
    g_rawInputRegistered = enable;
    return true;
}

void RemoveOverlay() {
    if (g_overlayWindow != nullptr) {
        DestroyWindow(g_overlayWindow);
        g_overlayWindow = nullptr;
    }
}

void UpdateOverlayLayout() {
    if (g_overlayWindow == nullptr) {
        return;
    }
    const RECT rc = ComputeOverlayRect();
    SetLayeredWindowAttributes(g_overlayWindow, RGB(255, 0, 255), static_cast<BYTE>(g_config.activeAlpha), LWA_COLORKEY | LWA_ALPHA);
    SetWindowPos(
        g_overlayWindow,
        HWND_TOPMOST,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
}

void StartOverlay() {
    if (g_state == AppState::Active) {
        return;
    }
    if (CurrentSelectedKeys().empty()) {
        MessageBoxW(nullptr, L"Select at least one key in Settings before starting the overlay.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!RegisterRawKeyboardInput(true)) {
        MessageBoxW(nullptr, L"Failed to register Raw Input devices.", kAppName, MB_OK | MB_ICONERROR);
        return;
    }

    g_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayClassName,
        L"",
        WS_POPUP,
        0,
        0,
        320,
        120,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr
    );

    if (g_overlayWindow == nullptr) {
        RegisterRawKeyboardInput(false);
        MessageBoxW(nullptr, L"Failed to create overlay window.", kAppName, MB_OK | MB_ICONERROR);
        return;
    }

    SetLayeredWindowAttributes(g_overlayWindow, RGB(255, 0, 255), static_cast<BYTE>(g_config.activeAlpha), LWA_COLORKEY | LWA_ALPHA);
    UpdateOverlayLayout();
    ShowWindow(g_overlayWindow, SW_SHOWNOACTIVATE);
    g_state = AppState::Active;
    UpdateSettingsActionControls();
}

void StopOverlay() {
    if (g_rawInputRegistered) {
        RegisterRawKeyboardInput(false);
    }
    g_pressed.clear();
    RemoveOverlay();
    g_state = AppState::Stopped;
    UpdateSettingsActionControls();
}

bool AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    wcscpy_s(nid.szTip, L"Secure Key Overlay");
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        if (nid.hIcon != nullptr) {
            DestroyIcon(nid.hIcon);
        }
        return false;
    }
    if (nid.hIcon != nullptr) {
        DestroyIcon(nid.hIcon);
    }

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    g_trayIconCreated = true;
    SetTimer(hwnd, kTrayHealthTimerId, kTrayHealthCheckMs, nullptr);
    return true;
}

void RemoveTrayIcon(HWND hwnd) {
    KillTimer(hwnd, kTrayHealthTimerId);

    if (!g_trayIconCreated) {
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayIconCreated = false;
}

void ShowTrayContextMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_state == AppState::Active ? MF_GRAYED : 0), IDM_TRAY_START, L"Start Overlay");
    AppendMenuW(menu, MF_STRING | (g_state == AppState::Active ? 0 : MF_GRAYED), IDM_TRAY_STOP, L"Stop Overlay");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_SETTINGS, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, owner, nullptr);
    DestroyMenu(menu);
}

void OpenSettingsWindow() {
    if (g_settingsWindow != nullptr) {
        ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_settingsWindow);
        UpdateSettingsActionControls();
        return;
    }

    g_settingsWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        kSettingsClassName,
        L"Secure Key Overlay - Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1120,
        700,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr
    );
    ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
}

void CreateSettingsControls(HWND hwnd) {
    const int margin = 18;
    const int headerTop = margin;
    const int sectionTop = 62;

    auto createLabel = [&](const wchar_t* text, int x, int y, int w, int h) -> HWND {
        HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd, nullptr, g_hInstance, nullptr);
        SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return label;
    };

    auto createGroup = [&](const wchar_t* text, int x, int y, int w, int h) -> HWND {
        HWND group = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, x, y, w, h, hwnd, nullptr, g_hInstance, nullptr);
        SendMessageW(group, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return group;
    };

    auto createCheck = [&](const wchar_t* text, int x, int y, int id) -> HWND {
        HWND check = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, 78, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInstance, nullptr);
        SendMessageW(check, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return check;
    };

    createLabel(L"Secure Key Overlay", margin, headerTop, 240, 22);
    createLabel(L"Keyboard-aligned default layout with separated controls for safer adjustment.", margin, headerTop + 22, 560, 18);

    createGroup(L"1) Key Selection", margin, sectionTop, 430, 264);
    const int keySectionX = margin + 18;
    const int keySectionY = sectionTop + 36;
    g_settingsUi.keyChecks[0] = createCheck(L"Tab", keySectionX, keySectionY, IDC_KEY_TAB);
    g_settingsUi.keyChecks[1] = createCheck(L"Q", keySectionX + 84, keySectionY, IDC_KEY_Q);
    g_settingsUi.keyChecks[2] = createCheck(L"W", keySectionX + 152, keySectionY, IDC_KEY_W);
    g_settingsUi.keyChecks[3] = createCheck(L"E", keySectionX + 220, keySectionY, IDC_KEY_E);
    g_settingsUi.keyChecks[4] = createCheck(L"R", keySectionX + 288, keySectionY, IDC_KEY_R);
    g_settingsUi.keyChecks[5] = createCheck(L"Shift", keySectionX, keySectionY + 36, IDC_KEY_SHIFT);
    g_settingsUi.keyChecks[6] = createCheck(L"A", keySectionX + 108, keySectionY + 36, IDC_KEY_A);
    g_settingsUi.keyChecks[7] = createCheck(L"S", keySectionX + 176, keySectionY + 36, IDC_KEY_S);
    g_settingsUi.keyChecks[8] = createCheck(L"D", keySectionX + 244, keySectionY + 36, IDC_KEY_D);
    g_settingsUi.keyChecks[9] = createCheck(L"F", keySectionX + 312, keySectionY + 36, IDC_KEY_F);
    g_settingsUi.keyChecks[10] = createCheck(L"Ctrl", keySectionX, keySectionY + 72, IDC_KEY_CTRL);
    g_settingsUi.keyChecks[11] = createCheck(L"Alt", keySectionX + 132, keySectionY + 72, IDC_KEY_ALT);

    createLabel(L"Add overlay key", keySectionX, keySectionY + 118, 110, 24);
    g_settingsUi.customAddButton = CreateWindowExW(0, L"BUTTON", L"Capture Key", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, keySectionX + 118, keySectionY + 116, 118, 28, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_ADD), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customAddButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    g_settingsUi.captureStatus = createLabel(L"Press the key you want to overlay", keySectionX, keySectionY + 148, 280, 20);
    g_settingsUi.customList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, keySectionX, keySectionY + 176, 208, 60, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_LIST), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customList, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    g_settingsUi.customRemoveButton = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, keySectionX + 220, keySectionY + 176, 92, 28, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_REMOVE), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customRemoveButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int positionX = 466;
    createGroup(L"2) Layout & Position", positionX, sectionTop, 188, 264);
    auto createRadio = [&](const wchar_t* text, int x, int y, int id, bool first = false) -> HWND {
        const DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (first ? WS_GROUP : 0);
        HWND radio = CreateWindowExW(0, L"BUTTON", text, style, x, y, 126, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInstance, nullptr);
        SendMessageW(radio, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return radio;
    };
    const int positionInnerX = positionX + 18;
    const int positionInnerY = sectionTop + 34;
    createLabel(L"Placed apart from key toggles.", positionInnerX, positionInnerY, 150, 18);
    g_settingsUi.positionRadios[0] = createRadio(L"Top-left", positionInnerX, positionInnerY + 28, IDC_POS_TOP_LEFT, true);
    g_settingsUi.positionRadios[1] = createRadio(L"Top-right", positionInnerX, positionInnerY + 54, IDC_POS_TOP_RIGHT);
    g_settingsUi.positionRadios[2] = createRadio(L"Bottom-left", positionInnerX, positionInnerY + 80, IDC_POS_BOTTOM_LEFT);
    g_settingsUi.positionRadios[3] = createRadio(L"Bottom-right", positionInnerX, positionInnerY + 106, IDC_POS_BOTTOM_RIGHT);
    g_settingsUi.positionRadios[4] = createRadio(L"Custom", positionInnerX, positionInnerY + 132, IDC_POS_CUSTOM);
    createLabel(L"Custom X", positionInnerX, positionInnerY + 164, 70, 20);
    g_settingsUi.customXEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, positionInnerX, positionInnerY + 184, 68, 26, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_X_EDIT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customXEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    createLabel(L"Custom Y", positionInnerX + 82, positionInnerY + 164, 70, 20);
    g_settingsUi.customYEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, positionInnerX + 82, positionInnerY + 184, 68, 26, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_Y_EDIT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customYEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int appearanceX = margin;
    const int appearanceTop = 344;
    createGroup(L"3) Appearance", appearanceX, appearanceTop, 636, 204);
    createLabel(L"Key size", appearanceX + 18, appearanceTop + 38, 120, 20);
    g_settingsUi.keySizeSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, appearanceX + 110, appearanceTop + 32, 418, 34, hwnd, reinterpret_cast<HMENU>(IDC_SIZE_SLIDER), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.keySizeSlider, TBM_SETRANGE, TRUE, MAKELONG(36, 120));

    createLabel(L"Spacing", appearanceX + 18, appearanceTop + 90, 120, 20);
    g_settingsUi.spacingSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, appearanceX + 110, appearanceTop + 84, 418, 34, hwnd, reinterpret_cast<HMENU>(IDC_SPACING_SLIDER), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.spacingSlider, TBM_SETRANGE, TRUE, MAKELONG(2, 36));

    createLabel(L"Surface colors", appearanceX + 18, appearanceTop + 138, 120, 20);
    g_settingsUi.idleColorButton = CreateWindowExW(0, L"BUTTON", L"Idle Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 18, appearanceTop + 162, 132, 32, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_IDLE), g_hInstance, nullptr);
    g_settingsUi.activeColorButton = CreateWindowExW(0, L"BUTTON", L"Active Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 164, appearanceTop + 162, 132, 32, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_ACTIVE), g_hInstance, nullptr);
    g_settingsUi.textColorButton = CreateWindowExW(0, L"BUTTON", L"Text Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 310, appearanceTop + 162, 132, 32, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_TEXT), g_hInstance, nullptr);
    g_settingsUi.arrayButton = CreateWindowExW(0, L"BUTTON", L"Array", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 456, appearanceTop + 162, 90, 32, hwnd, reinterpret_cast<HMENU>(IDC_ARRAY_BUTTON), g_hInstance, nullptr);
    createLabel(L"Fill opacity", appearanceX + 456, appearanceTop + 38, 100, 20);
    g_settingsUi.activeAlphaSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, appearanceX + 456, appearanceTop + 60, 164, 34, hwnd, reinterpret_cast<HMENU>(IDC_ACTIVE_ALPHA_SLIDER), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.activeAlphaSlider, TBM_SETRANGE, TRUE, MAKELONG(72, 255));
    SendMessageW(g_settingsUi.idleColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.activeColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.textColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.arrayButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int previewX = 674;
    const int previewTop = sectionTop;
    createGroup(L"4) Preview", previewX, previewTop, 402, 486);
    createLabel(L"Live geometry preview.", previewX + 18, previewTop + 28, 180, 18);
    g_settingsUi.previewPanel = CreateWindowExW(WS_EX_CLIENTEDGE, kPreviewClassName, L"", WS_CHILD | WS_VISIBLE, previewX + 18, previewTop + 54, 366, 408, hwnd, reinterpret_cast<HMENU>(IDC_PREVIEW_PANEL), g_hInstance, nullptr);

    g_settingsUi.statusLabel = createLabel(L"", margin, 576, 520, 24);
    g_settingsUi.startButton = CreateWindowExW(0, L"BUTTON", L"Start Overlay", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 760, 572, 122, 36, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_START), g_hInstance, nullptr);
    g_settingsUi.stopButton = CreateWindowExW(0, L"BUTTON", L"Stop Overlay", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 892, 572, 122, 36, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_STOP), g_hInstance, nullptr);
    g_settingsUi.closeButton = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 1024, 572, 72, 36, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_CLOSE), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.startButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.stopButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.closeButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    UpdateSettingsActionControls();
}

void ResetSettingsUiHandles() {
    g_settingsUi = SettingsUi{};
}

void UpdateSettingsActionControls() {
    if (g_settingsUi.statusLabel != nullptr) {
        const wchar_t* statusText = g_state == AppState::Active
            ? L"Overlay is active. Press the selected keys to highlight them."
            : L"Overlay is stopped. Click Start Overlay to show it on screen.";
        SetWindowTextW(g_settingsUi.statusLabel, statusText);
    }

    if (g_settingsUi.startButton != nullptr) {
        EnableWindow(g_settingsUi.startButton, g_state != AppState::Active);
    }
    if (g_settingsUi.stopButton != nullptr) {
        EnableWindow(g_settingsUi.stopButton, g_state == AppState::Active);
    }
}

LRESULT CALLBACK PreviewWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    switch (message) {
        case WM_LBUTTONDOWN: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            UINT vk = 0;
            POINT offset{};
            if (HitTestPreviewKey(pt, vk, offset)) {
                g_draggingKey = vk;
                g_dragOffset = offset;
                SetCapture(hwnd);
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (g_draggingKey != 0 && GetCapture() == hwnd) {
                UpdateDraggedKeyPosition(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (g_draggingKey != 0) {
                ReleaseCapture();
                g_draggingKey = 0;
                return 0;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HBRUSH background = CreateSolidBrush(RGB(238, 242, 246));
            FillRect(hdc, &client, background);
            DeleteObject(background);

            SelectObject(hdc, g_uiFont);
            DrawKeys(hdc, client, true);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CAPTURECHANGED:
            g_draggingKey = 0;
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    (void)lParam;
    switch (message) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HBRUSH transparentBrush = CreateSolidBrush(RGB(255, 0, 255));
            FillRect(hdc, &client, transparentBrush);
            DeleteObject(transparentBrush);

            SelectObject(hdc, g_uiFont);
            DrawKeys(hdc, client, false);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void HandleRawInput(LPARAM lParam) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0) {
        return;
    }
    if (size == 0) {
        return;
    }

    std::vector<BYTE> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
        return;
    }

    const RAWINPUT* rawInput = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (rawInput->header.dwType != RIM_TYPEKEYBOARD) {
        return;
    }
    if (g_state != AppState::Active) {
        return;
    }

    const RAWKEYBOARD& keyboard = rawInput->data.keyboard;
    const UINT vk = NormalizeRawVKey(keyboard);
    if (vk == 0 || !IsVkSelected(vk)) {
        return;
    }

    const bool pressed = (keyboard.Flags & RI_KEY_BREAK) == 0;
    const bool previous = g_pressed[vk];
    g_pressed[vk] = pressed;

    if (previous != pressed && g_overlayWindow != nullptr) {
        InvalidateRect(g_overlayWindow, nullptr, FALSE);
    }
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (message) {
        case WM_CREATE: {
            CreateSettingsControls(hwnd);
            SyncSettingsControlsFromConfig();
            return 0;
        }

        case WM_COMMAND: {
            const int controlId = LOWORD(wParam);
            const int code = HIWORD(wParam);

            for (size_t i = 0; i < kDefaultKeyDefs.size(); ++i) {
                if (controlId == kDefaultKeyDefs[i].controlId && code == BN_CLICKED) {
                    g_config.enabledDefaults[i] = SendMessageW(g_settingsUi.keyChecks[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
                    NotifyConfigChanged();
                    return 0;
                }
            }

            switch (controlId) {
                case IDC_CUSTOM_KEY_ADD: {
                    if (code == BN_CLICKED) {
                        g_captureCustomKey = true;
                        SetCapturePrompt(L"Press the key you want to overlay...");
                        SetFocus(hwnd);
                    }
                    return 0;
                }

                case IDC_CUSTOM_KEY_REMOVE: {
                    if (code == BN_CLICKED) {
                        const LRESULT selectedIndex = SendMessageW(g_settingsUi.customList, LB_GETCURSEL, 0, 0);
                        if (selectedIndex != LB_ERR) {
                            const size_t index = static_cast<size_t>(selectedIndex);
                            if (index < g_config.customKeys.size()) {
                                g_config.customKeys.erase(g_config.customKeys.begin() + static_cast<std::ptrdiff_t>(index));
                                RefreshCustomKeyList();
                                NotifyConfigChanged();
                            }
                        }
                    }
                    return 0;
                }

                case IDC_POS_TOP_LEFT:
                case IDC_POS_TOP_RIGHT:
                case IDC_POS_BOTTOM_LEFT:
                case IDC_POS_BOTTOM_RIGHT:
                case IDC_POS_CUSTOM: {
                    if (code == BN_CLICKED) {
                        if (controlId == IDC_POS_TOP_LEFT) {
                            g_config.preset = PositionPreset::TopLeft;
                        } else if (controlId == IDC_POS_TOP_RIGHT) {
                            g_config.preset = PositionPreset::TopRight;
                        } else if (controlId == IDC_POS_BOTTOM_LEFT) {
                            g_config.preset = PositionPreset::BottomLeft;
                        } else if (controlId == IDC_POS_BOTTOM_RIGHT) {
                            g_config.preset = PositionPreset::BottomRight;
                        } else {
                            g_config.preset = PositionPreset::Custom;
                        }
                        NotifyConfigChanged();
                    }
                    return 0;
                }

                case IDC_CUSTOM_X_EDIT:
                case IDC_CUSTOM_Y_EDIT: {
                    if (code == EN_CHANGE) {
                        wchar_t xBuffer[32] = {};
                        wchar_t yBuffer[32] = {};
                        GetWindowTextW(g_settingsUi.customXEdit, xBuffer, static_cast<int>(std::size(xBuffer)));
                        GetWindowTextW(g_settingsUi.customYEdit, yBuffer, static_cast<int>(std::size(yBuffer)));
                        g_config.customPos.x = _wtoi(xBuffer);
                        g_config.customPos.y = _wtoi(yBuffer);
                        if (g_config.preset == PositionPreset::Custom) {
                            NotifyConfigChanged();
                        }
                    }
                    return 0;
                }

                case IDC_COLOR_IDLE:
                    if (code == BN_CLICKED) {
                        PickColor(hwnd, g_config.idleColor);
                    }
                    return 0;
                case IDC_COLOR_ACTIVE:
                    if (code == BN_CLICKED) {
                        PickColor(hwnd, g_config.activeColor);
                    }
                    return 0;
                case IDC_COLOR_TEXT:
                    if (code == BN_CLICKED) {
                        PickColor(hwnd, g_config.textColor);
                    }
                    return 0;
                case IDC_ARRAY_BUTTON:
                    if (code == BN_CLICKED) {
                        g_arrayEditMode = !g_arrayEditMode;
                        g_config.useCustomArrayLayout = g_arrayEditMode || g_config.useCustomArrayLayout;
                        if (g_config.useCustomArrayLayout) {
                            EnsureCustomArrayLayoutSeeded();
                        }
                        if (g_settingsUi.arrayButton != nullptr) {
                            SetWindowTextW(g_settingsUi.arrayButton, g_arrayEditMode ? L"Done" : L"Array");
                        }
                        NotifyConfigChanged();
                    }
                    return 0;
                case IDC_SETTINGS_START:
                    if (code == BN_CLICKED) {
                        StartOverlay();
                    }
                    return 0;
                case IDC_SETTINGS_STOP:
                    if (code == BN_CLICKED) {
                        StopOverlay();
                    }
                    return 0;

                case IDC_SETTINGS_CLOSE:
                    if (code == BN_CLICKED) {
                        RequestAppExit();
                    }
                    return 0;
            }
            break;
        }

        case WM_HSCROLL: {
            const HWND source = reinterpret_cast<HWND>(lParam);
            if (source == g_settingsUi.keySizeSlider) {
                g_config.keySize = static_cast<int>(SendMessageW(g_settingsUi.keySizeSlider, TBM_GETPOS, 0, 0));
                if (g_config.useCustomArrayLayout) {
                    EnsureCustomArrayLayoutSeeded();
                }
                NotifyConfigChanged();
                return 0;
            }
            if (source == g_settingsUi.spacingSlider) {
                g_config.spacing = static_cast<int>(SendMessageW(g_settingsUi.spacingSlider, TBM_GETPOS, 0, 0));
                NotifyConfigChanged();
                return 0;
            }
            if (source == g_settingsUi.activeAlphaSlider) {
                g_config.activeAlpha = static_cast<int>(SendMessageW(g_settingsUi.activeAlphaSlider, TBM_GETPOS, 0, 0));
                NotifyConfigChanged();
                return 0;
            }
            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (g_captureCustomKey) {
                const UINT vk = NormalizeCapturedVKey(static_cast<UINT>(wParam));
                if (vk == VK_ESCAPE) {
                    g_captureCustomKey = false;
                    SetCapturePrompt(L"Press the key you want to overlay");
                    return 0;
                }
                if (TryAddCustomKey(vk)) {
                    g_captureCustomKey = false;
                    SetCapturePrompt(VkToLabel(vk).c_str());
                }
                return 0;
            }
            break;

        case WM_CLOSE:
            RequestAppExit();
            return 0;

        case WM_DESTROY:
            g_settingsWindow = nullptr;
            ResetSettingsUiHandles();
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreatedMessage != 0 && message == g_taskbarCreatedMessage) {
        DestroyWindow(hwnd);
        return 0;
    }

    switch (message) {
        case WM_CREATE:
            if (!AddTrayIcon(hwnd)) {
                MessageBoxW(hwnd, L"Failed to create the tray icon. The app will now exit.", kAppName, MB_OK | MB_ICONERROR);
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            OpenSettingsWindow();
            return 0;

        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_LBUTTONUP) {
                ShowTrayContextMenu(hwnd);
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_TRAY_START:
                    StartOverlay();
                    return 0;
                case IDM_TRAY_STOP:
                    StopOverlay();
                    return 0;
                case IDM_TRAY_SETTINGS:
                    OpenSettingsWindow();
                    return 0;
                case IDM_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;

        case WM_INPUT:
            HandleRawInput(lParam);
            return 0;

        case WM_TIMER:
            if (wParam == kTrayHealthTimerId && !IsTrayHostAvailable()) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_DESTROY:
            if (g_settingsWindow != nullptr) {
                DestroyWindow(g_settingsWindow);
            }
            StopOverlay();
            RemoveTrayIcon(hwnd);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterWindowClasses() {
    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.hInstance = g_hInstance;
    mainClass.lpszClassName = kMainClassName;
    mainClass.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXICON));
    mainClass.hIconSm = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    if (!RegisterClassExW(&mainClass)) {
        return false;
    }

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.lpfnWndProc = OverlayWindowProc;
    overlayClass.hInstance = g_hInstance;
    overlayClass.lpszClassName = kOverlayClassName;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    overlayClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    if (!RegisterClassExW(&overlayClass)) {
        return false;
    }

    WNDCLASSEXW settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.hInstance = g_hInstance;
    settingsClass.lpszClassName = kSettingsClassName;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&settingsClass)) {
        return false;
    }

    WNDCLASSEXW previewClass{};
    previewClass.cbSize = sizeof(previewClass);
    previewClass.lpfnWndProc = PreviewWindowProc;
    previewClass.hInstance = g_hInstance;
    previewClass.lpszClassName = kPreviewClassName;
    previewClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    previewClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&previewClass)) {
        return false;
    }

    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_hInstance = instance;
    g_config.enabledDefaults.fill(false);
    g_config.enabledDefaults[0] = true;   // Tab
    g_config.enabledDefaults[1] = true;   // Q
    g_config.enabledDefaults[2] = true;   // W
    g_config.enabledDefaults[3] = true;   // E
    g_config.enabledDefaults[4] = true;   // R
    g_config.enabledDefaults[5] = true;   // Shift
    g_config.enabledDefaults[6] = true;   // A
    g_config.enabledDefaults[7] = true;   // S
    g_config.enabledDefaults[8] = true;   // D
    g_config.enabledDefaults[9] = true;   // F
    g_config.enabledDefaults[10] = true;  // Ctrl
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        g_uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
    }

    if (!RegisterWindowClasses()) {
        MessageBoxW(nullptr, L"Failed to register window classes.", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_mainWindow = CreateWindowExW(
        0,
        kMainClassName,
        kAppName,
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr
    );

    if (g_mainWindow == nullptr) {
        MessageBoxW(nullptr, L"Failed to initialize application window.", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_mainWindow, SW_HIDE);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_uiFont != nullptr) {
        DeleteObject(g_uiFont);
        g_uiFont = nullptr;
    }
    return static_cast<int>(msg.wParam);
}
