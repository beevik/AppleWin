#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "../Joystick.h"
class PropertySheetHelper;
class ConfigNeedingRestart;

class PageInput : private IPropertySheetPage {
public:
    PageInput(PropertySheetHelper & PropertySheetHelper) :
        m_Page(PG_INPUT),
        m_PropertySheetHelper(PropertySheetHelper),
        m_uScrollLockToggle(0),
        m_uCursorControl(1),
        m_uCenteringControl(JOYSTICK_MODE_CENTERING),
        m_bmAutofire(0),
        m_uMouseShowCrosshair(0),
        m_uMouseRestrictToWindow(0) {
        PageInput::ms_this = this;
    }
    virtual ~PageInput() {
    }

    static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    UINT GetScrollLockToggle(void) {
        return m_uScrollLockToggle;
    }
    void SetScrollLockToggle(UINT uValue) {
        m_uScrollLockToggle = uValue;
    }
    UINT GetJoystickCursorControl(void) {
        return m_uCursorControl;
    }
    void SetJoystickCursorControl(UINT uValue) {
        m_uCursorControl = uValue;
    }
    UINT GetJoystickCenteringControl(void) {
        return m_uCenteringControl;
    }
    void SetJoystickCenteringControl(UINT uValue) {
        m_uCenteringControl = uValue;
    }
    UINT GetAutofire(UINT uButton) {
        return (m_bmAutofire >> uButton) & 1;
    }    // Get a specific button
    void SetAutofire(UINT uValue) {
        m_bmAutofire = uValue;
    }                    // Set all buttons
    UINT GetMouseShowCrosshair(void) {
        return m_uMouseShowCrosshair;
    }
    void SetMouseShowCrosshair(UINT uValue) {
        m_uMouseShowCrosshair = uValue;
    }
    UINT GetMouseRestrictToWindow(void) {
        return m_uMouseRestrictToWindow;
    }
    void SetMouseRestrictToWindow(UINT uValue) {
        m_uMouseRestrictToWindow = uValue;
    }

protected:
    // IPropertySheetPage
    virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
    virtual void DlgOK(HWND hWnd);
    virtual void DlgCANCEL(HWND hWnd) {
    }

private:
    void InitOptions(HWND hWnd);
    void InitJoystickChoices(HWND hWnd, int nJoyNum, int nIdcValue);
    void InitSlotOptions(HWND hWnd);

    static PageInput * ms_this;
    static const UINT MaxMenuChoiceLen = 40;

    static const TCHAR m_szJoyChoice0[];
    static const TCHAR m_szJoyChoice1[];
    static const TCHAR m_szJoyChoice2[];
    static const TCHAR m_szJoyChoice3[];
    static const TCHAR m_szJoyChoice4[];
    static const TCHAR m_szJoyChoice5[];
    static const TCHAR m_szJoyChoice6[];
    static const TCHAR * const m_pszJoy0Choices[J0C_MAX];
    static const TCHAR * const m_pszJoy1Choices[J1C_MAX];

    int m_nJoy0ChoiceTranlationTbl[J0C_MAX];
    TCHAR m_joystick0choices[J0C_MAX * MaxMenuChoiceLen];
    int m_nJoy1ChoiceTranlationTbl[J1C_MAX];
    TCHAR m_joystick1choices[J1C_MAX * MaxMenuChoiceLen];

    const PAGETYPE m_Page;
    PropertySheetHelper & m_PropertySheetHelper;

    UINT m_uScrollLockToggle;
    UINT m_uCursorControl;      // 1 = Allow AppleII to read cursor keys from $C000 (when using keyboard for joystick emu)
    UINT m_uCenteringControl;   // 1 = Centering, 0=Floating (when using keyboard for joystick emu)
    UINT m_bmAutofire;          // bitmask b2:0
    UINT m_uMouseShowCrosshair;
    UINT m_uMouseRestrictToWindow;
};
