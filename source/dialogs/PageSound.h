#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class PropertySheetHelper;

class PageSound : private IPropertySheetPage {
public:
    PageSound(PropertySheetHelper & PropertySheetHelper) :
        m_Page(PG_SOUND),
        m_PropertySheetHelper(PropertySheetHelper),
        m_NewCardType(CT_Empty),
        m_nCurrentIDCheckButton(0) {
        PageSound::ms_this = this;
    }
    virtual ~PageSound() {
    }

    static BOOL CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

    DWORD GetVolumeMax(void) {
        return VOLUME_MAX;
    }

protected:
    // IPropertySheetPage
    virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
    virtual void DlgOK(HWND hWnd);
    virtual void DlgCANCEL(HWND hWnd) {
    }

private:
    void InitOptions(HWND hWnd);
    bool NewSoundcardConfigured(HWND hWnd, WPARAM wparam, SS_CARDTYPE NewCardType);

    static PageSound * ms_this;

    const PAGETYPE m_Page;
    PropertySheetHelper & m_PropertySheetHelper;

    static const UINT VOLUME_MIN = 0;
    static const UINT VOLUME_MAX = 59;
    static const TCHAR m_soundchoices[];

    SS_CARDTYPE m_NewCardType;
    int m_nCurrentIDCheckButton;
};
