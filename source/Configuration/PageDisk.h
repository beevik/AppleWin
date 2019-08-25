#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class PropertySheetHelper;

class PageDisk : private IPropertySheetPage {
public:
    PageDisk(PropertySheetHelper & PropertySheetHelper) :
        m_Page(PG_DISK),
        m_PropertySheetHelper(PropertySheetHelper) {
        PageDisk::ms_this = this;
    }
    virtual ~PageDisk() {
    }

    static BOOL CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

protected:
    // IPropertySheetPage
    virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
    virtual void DlgOK(HWND hWnd);
    virtual void DlgCANCEL(HWND hWnd) {
    }

private:
    void InitOptions(HWND hWnd);
    void InitComboHDD(HWND hWnd);
    void EnableHDD(HWND hWnd, BOOL bEnable);
    void EnableDisk(HWND hWnd, BOOL bEnable);
    void HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected);
    void HandleDiskCombo(HWND hWnd, UINT driveSelected, UINT comboSelected);
    void HandleHDDSwap(HWND hWnd);
    UINT RemovalConfirmation(UINT uCommand);

    static PageDisk * ms_this;
    static const TCHAR m_discchoices[];
    static const TCHAR m_defaultDiskOptions[];
    static const TCHAR m_defaultHDDOptions[];

    const PAGETYPE m_Page;
    PropertySheetHelper & m_PropertySheetHelper;
};
