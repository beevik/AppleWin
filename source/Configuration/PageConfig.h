#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class PropertySheetHelper;

class PageConfig : private IPropertySheetPage {
public:
    PageConfig(PropertySheetHelper & PropertySheetHelper) :
        m_Page(PG_CONFIG),
        m_PropertySheetHelper(PropertySheetHelper) {
        PageConfig::ms_this = this;
    }
    virtual ~PageConfig() {
    }

    static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

protected:
    // IPropertySheetPage
    virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
    virtual void DlgOK(HWND hWnd);
    virtual void DlgCANCEL(HWND hWnd) {
    }

private:
    void InitOptions(HWND hWnd);
    eApple2Type GetApple2Type(DWORD NewMenuItem);
    void EnableTrackbar(HWND hWnd, BOOL enable);
    bool IsOkToBenchmark(HWND hWnd, const bool bConfigChanged);

    static PageConfig * ms_this;
    static const TCHAR m_ComputerChoices[];

    const PAGETYPE m_Page;
    PropertySheetHelper & m_PropertySheetHelper;
};
