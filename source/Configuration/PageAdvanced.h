#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class PropertySheetHelper;

class PageAdvanced : private IPropertySheetPage {
public:
    PageAdvanced(PropertySheetHelper & PropertySheetHelper) :
        m_Page(PG_ADVANCED),
        m_PropertySheetHelper(PropertySheetHelper),
        m_uTheFreezesF8Rom(0) {
        PageAdvanced::ms_this = this;
    }
    virtual ~PageAdvanced() {
    }

    static BOOL CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

    UINT GetTheFreezesF8Rom(void) {
        return m_uTheFreezesF8Rom;
    }
    void SetTheFreezesF8Rom(UINT uValue) {
        m_uTheFreezesF8Rom = uValue;
    }

protected:
    // IPropertySheetPage
    virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
    virtual void DlgOK(HWND hWnd);
    virtual void DlgCANCEL(HWND hWnd) {
    }

private:
    void InitOptions(HWND hWnd);
    void InitFreezeDlgButton(HWND hWnd);

    static PageAdvanced * ms_this;

    const PAGETYPE m_Page;
    PropertySheetHelper & m_PropertySheetHelper;
    UINT m_uTheFreezesF8Rom;
};
