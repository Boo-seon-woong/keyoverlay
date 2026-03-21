#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
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

constexpr int IDC_PREVIEW_PANEL = 2301;
constexpr int IDC_SETTINGS_CLOSE = 2302;
constexpr int IDC_SETTINGS_START = 2303;
constexpr int IDC_SETTINGS_STOP = 2304;
constexpr int IDC_SETTINGS_STATUS = 2305;

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

constexpr std::array<KeyDefinition, 7> kDefaultKeyDefs{{
    {L"W", 'W', IDC_KEY_W},
    {L"A", 'A', IDC_KEY_A},
    {L"S", 'S', IDC_KEY_S},
    {L"D", 'D', IDC_KEY_D},
    {L"Ctrl", VK_CONTROL, IDC_KEY_CTRL},
    {L"Shift", VK_SHIFT, IDC_KEY_SHIFT},
    {L"Alt", VK_MENU, IDC_KEY_ALT},
}};

struct AppConfig {
    std::array<bool, kDefaultKeyDefs.size()> enabledDefaults{};
    std::vector<UINT> customKeys;
    PositionPreset preset = PositionPreset::TopLeft;
    POINT customPos{100, 100};
    int keySize = 64;
    int spacing = 10;
    COLORREF idleColor = RGB(105, 105, 105);
    COLORREF activeColor = RGB(56, 176, 0);
    COLORREF textColor = RGB(255, 255, 255);
};

struct SettingsUi {
    std::array<HWND, kDefaultKeyDefs.size()> keyChecks{};
    HWND customEdit = nullptr;
    HWND customAddButton = nullptr;
    HWND customList = nullptr;
    HWND customRemoveButton = nullptr;
    std::array<HWND, 5> positionRadios{};
    HWND customXEdit = nullptr;
    HWND customYEdit = nullptr;
    HWND keySizeSlider = nullptr;
    HWND spacingSlider = nullptr;
    HWND idleColorButton = nullptr;
    HWND activeColorButton = nullptr;
    HWND textColorButton = nullptr;
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

void UpdateOverlayLayout();
void UpdateSettingsActionControls();

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

    RefreshCustomKeyList();
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
    const auto selectedKeys = CurrentSelectedKeys();
    const int count = static_cast<int>(selectedKeys.size());
    const int padding = 12;
    const int keySize = g_config.keySize;
    const int spacing = g_config.spacing;
    const int width = count > 0 ? (padding * 2 + (count * keySize) + ((count - 1) * spacing)) : 240;
    const int height = padding * 2 + keySize;
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

void DrawKeys(HDC hdc, const RECT& bounds, bool previewMode) {
    const auto selectedKeys = CurrentSelectedKeys();
    const int padding = 12;
    const int keySize = g_config.keySize;
    const int spacing = g_config.spacing;
    int x = bounds.left + padding;
    const int y = bounds.top + padding;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_config.textColor);

    if (selectedKeys.empty()) {
        RECT messageRect = bounds;
        DrawTextW(hdc, L"No keys selected", -1, &messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    HBRUSH borderBrush = CreateSolidBrush(RGB(25, 25, 25));
    for (size_t index = 0; index < selectedKeys.size(); ++index) {
        const UINT vk = selectedKeys[index];
        const bool isPressed = previewMode ? (index % 2 == 0) : (g_pressed[vk]);
        RECT keyRect{x, y, x + keySize, y + keySize};

        const COLORREF color = isPressed ? g_config.activeColor : g_config.idleColor;
        HBRUSH keyBrush = CreateSolidBrush(color);
        FillRect(hdc, &keyRect, keyBrush);
        FrameRect(hdc, &keyRect, borderBrush);
        DeleteObject(keyBrush);

        const std::wstring label = VkToLabel(vk);
        DrawTextW(hdc, label.c_str(), -1, &keyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += keySize + spacing;
    }
    DeleteObject(borderBrush);
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

    SetLayeredWindowAttributes(g_overlayWindow, RGB(255, 0, 255), 255, LWA_COLORKEY);
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
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Secure Key Overlay");
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        return false;
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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        760,
        610,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr
    );
    ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
}

void CreateSettingsControls(HWND hwnd) {
    const int margin = 16;
    const int sectionTop = margin;

    auto createLabel = [&](const wchar_t* text, int x, int y, int w, int h) -> HWND {
        HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd, nullptr, g_hInstance, nullptr);
        SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return label;
    };

    auto createCheck = [&](const wchar_t* text, int x, int y, int id) -> HWND {
        HWND check = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInstance, nullptr);
        SendMessageW(check, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return check;
    };

    createLabel(L"1) Key Selection", margin, sectionTop, 220, 20);
    g_settingsUi.keyChecks[0] = createCheck(L"W", margin, sectionTop + 28, IDC_KEY_W);
    g_settingsUi.keyChecks[1] = createCheck(L"A", margin + 70, sectionTop + 28, IDC_KEY_A);
    g_settingsUi.keyChecks[2] = createCheck(L"S", margin + 140, sectionTop + 28, IDC_KEY_S);
    g_settingsUi.keyChecks[3] = createCheck(L"D", margin + 210, sectionTop + 28, IDC_KEY_D);
    g_settingsUi.keyChecks[4] = createCheck(L"Ctrl", margin, sectionTop + 56, IDC_KEY_CTRL);
    g_settingsUi.keyChecks[5] = createCheck(L"Shift", margin + 90, sectionTop + 56, IDC_KEY_SHIFT);
    g_settingsUi.keyChecks[6] = createCheck(L"Alt", margin + 190, sectionTop + 56, IDC_KEY_ALT);

    createLabel(L"Custom key:", margin, sectionTop + 92, 90, 24);
    g_settingsUi.customEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, margin + 90, sectionTop + 90, 100, 24, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_EDIT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    g_settingsUi.customAddButton = CreateWindowExW(0, L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, margin + 198, sectionTop + 90, 60, 24, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_ADD), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customAddButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    g_settingsUi.customList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, margin, sectionTop + 120, 170, 100, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_LIST), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customList, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    g_settingsUi.customRemoveButton = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, margin + 178, sectionTop + 120, 80, 24, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_KEY_REMOVE), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customRemoveButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int positionX = 300;
    createLabel(L"2) Position", positionX, sectionTop, 180, 20);
    auto createRadio = [&](const wchar_t* text, int x, int y, int id, bool first = false) -> HWND {
        const DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (first ? WS_GROUP : 0);
        HWND radio = CreateWindowExW(0, L"BUTTON", text, style, x, y, 140, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInstance, nullptr);
        SendMessageW(radio, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        return radio;
    };
    g_settingsUi.positionRadios[0] = createRadio(L"Top-left", positionX, sectionTop + 28, IDC_POS_TOP_LEFT, true);
    g_settingsUi.positionRadios[1] = createRadio(L"Top-right", positionX, sectionTop + 52, IDC_POS_TOP_RIGHT);
    g_settingsUi.positionRadios[2] = createRadio(L"Bottom-left", positionX, sectionTop + 76, IDC_POS_BOTTOM_LEFT);
    g_settingsUi.positionRadios[3] = createRadio(L"Bottom-right", positionX, sectionTop + 100, IDC_POS_BOTTOM_RIGHT);
    g_settingsUi.positionRadios[4] = createRadio(L"Custom", positionX, sectionTop + 124, IDC_POS_CUSTOM);
    createLabel(L"X:", positionX + 24, sectionTop + 150, 18, 20);
    g_settingsUi.customXEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, positionX + 44, sectionTop + 146, 70, 24, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_X_EDIT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customXEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    createLabel(L"Y:", positionX + 124, sectionTop + 150, 18, 20);
    g_settingsUi.customYEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, positionX + 144, sectionTop + 146, 70, 24, hwnd, reinterpret_cast<HMENU>(IDC_CUSTOM_Y_EDIT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.customYEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int appearanceX = 16;
    const int appearanceTop = 260;
    createLabel(L"3) Appearance", appearanceX, appearanceTop, 220, 20);
    createLabel(L"Key size", appearanceX, appearanceTop + 32, 120, 20);
    g_settingsUi.keySizeSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, appearanceX + 70, appearanceTop + 26, 220, 34, hwnd, reinterpret_cast<HMENU>(IDC_SIZE_SLIDER), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.keySizeSlider, TBM_SETRANGE, TRUE, MAKELONG(36, 120));

    createLabel(L"Spacing", appearanceX, appearanceTop + 68, 120, 20);
    g_settingsUi.spacingSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, appearanceX + 70, appearanceTop + 62, 220, 34, hwnd, reinterpret_cast<HMENU>(IDC_SPACING_SLIDER), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.spacingSlider, TBM_SETRANGE, TRUE, MAKELONG(2, 36));

    g_settingsUi.idleColorButton = CreateWindowExW(0, L"BUTTON", L"Idle Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX, appearanceTop + 110, 100, 28, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_IDLE), g_hInstance, nullptr);
    g_settingsUi.activeColorButton = CreateWindowExW(0, L"BUTTON", L"Active Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 110, appearanceTop + 110, 100, 28, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_ACTIVE), g_hInstance, nullptr);
    g_settingsUi.textColorButton = CreateWindowExW(0, L"BUTTON", L"Text Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, appearanceX + 220, appearanceTop + 110, 100, 28, hwnd, reinterpret_cast<HMENU>(IDC_COLOR_TEXT), g_hInstance, nullptr);
    SendMessageW(g_settingsUi.idleColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.activeColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    SendMessageW(g_settingsUi.textColorButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

    const int previewX = 360;
    const int previewTop = 260;
    createLabel(L"4) Preview (simulated)", previewX, previewTop, 220, 20);
    g_settingsUi.previewPanel = CreateWindowExW(WS_EX_CLIENTEDGE, kPreviewClassName, L"", WS_CHILD | WS_VISIBLE, previewX, previewTop + 26, 360, 230, hwnd, reinterpret_cast<HMENU>(IDC_PREVIEW_PANEL), g_hInstance, nullptr);

    g_settingsUi.statusLabel = createLabel(L"", margin, 546, 390, 24);
    g_settingsUi.startButton = CreateWindowExW(0, L"BUTTON", L"Start Overlay", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 430, 540, 100, 30, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_START), g_hInstance, nullptr);
    g_settingsUi.stopButton = CreateWindowExW(0, L"BUTTON", L"Stop Overlay", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 535, 540, 100, 30, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_STOP), g_hInstance, nullptr);
    g_settingsUi.closeButton = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 640, 540, 90, 30, hwnd, reinterpret_cast<HMENU>(IDC_SETTINGS_CLOSE), g_hInstance, nullptr);
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
    (void)lParam;
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HBRUSH background = CreateSolidBrush(RGB(246, 248, 250));
            FillRect(hdc, &client, background);
            DeleteObject(background);

            SelectObject(hdc, g_uiFont);
            DrawKeys(hdc, client, true);
            EndPaint(hwnd, &ps);
            return 0;
        }
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
                        wchar_t text[64] = {};
                        GetWindowTextW(g_settingsUi.customEdit, text, static_cast<int>(std::size(text)));
                        UINT vk = 0;
                        if (!ParseKeyText(text, vk)) {
                            MessageBoxW(hwnd, L"Unknown key. Try examples: Q, F5, Space, Enter, Left.", kAppName, MB_OK | MB_ICONINFORMATION);
                            return 0;
                        }
                        if (!IsVkSelected(vk)) {
                            g_config.customKeys.push_back(vk);
                            RefreshCustomKeyList();
                            SetWindowTextW(g_settingsUi.customEdit, L"");
                            NotifyConfigChanged();
                        }
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
                        DestroyWindow(hwnd);
                    }
                    return 0;
            }
            break;
        }

        case WM_HSCROLL: {
            const HWND source = reinterpret_cast<HWND>(lParam);
            if (source == g_settingsUi.keySizeSlider) {
                g_config.keySize = static_cast<int>(SendMessageW(g_settingsUi.keySizeSlider, TBM_GETPOS, 0, 0));
                NotifyConfigChanged();
                return 0;
            }
            if (source == g_settingsUi.spacingSlider) {
                g_config.spacing = static_cast<int>(SendMessageW(g_settingsUi.spacingSlider, TBM_GETPOS, 0, 0));
                NotifyConfigChanged();
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
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
    mainClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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
    g_config.enabledDefaults.fill(true);
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
