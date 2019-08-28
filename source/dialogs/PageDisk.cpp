/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "StdAfx.h"
#include "disk/Disk.h"    // Drive_e, Disk_Status_e
#include "video/Frame.h"
#include "state/Registry.h"
#include "PageDisk.h"
#include "PropertySheetHelper.h"

PageDisk * PageDisk::ms_this = 0;  // reinit'd in ctor

const TCHAR PageDisk::m_discchoices[] =
    TEXT("Authentic Speed\0")
    TEXT("Enhanced Speed\0");

const TCHAR PageDisk::m_defaultDiskOptions[] =
    TEXT("Select Disk...\0")
    TEXT("Eject Disk\0");

const TCHAR PageDisk::m_defaultHDDOptions[] =
    TEXT("Select Hard Disk Image...\0")
    TEXT("Unplug Hard Disk Image\0");

BOOL CALLBACK PageDisk::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // Switch from static func to our instance
    return PageDisk::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL PageDisk::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_NOTIFY:
    {
        // Property Sheet notifications

        switch (((LPPSHNOTIFY)lparam)->hdr.code) {
        case PSN_SETACTIVE:
            // About to become the active page
            m_PropertySheetHelper.SetLastPage(m_Page);
            InitOptions(hWnd);
            break;
        case PSN_KILLACTIVE:
            SetWindowLong(hWnd, DWLP_MSGRESULT, FALSE);         // Changes are valid
            break;
        case PSN_APPLY:
            DlgOK(hWnd);
            SetWindowLong(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);    // Changes are valid
            break;
        case PSN_QUERYCANCEL:
            // Can use this to ask user to confirm cancel
            break;
        case PSN_RESET:
            DlgCANCEL(hWnd);
            break;
        }
    }
    break;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDC_COMBO_DISK1:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                HandleDiskCombo(hWnd, DRIVE_1, LOWORD(wparam));
                FrameRefreshStatus(DRAW_BUTTON_DRIVES);
            }
            break;
        case IDC_COMBO_DISK2:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                HandleDiskCombo(hWnd, DRIVE_2, LOWORD(wparam));
                FrameRefreshStatus(DRAW_BUTTON_DRIVES);
            }
            break;
        case IDC_COMBO_HDD1:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                HandleHDDCombo(hWnd, HARDDISK_1, LOWORD(wparam));
            }
            break;
        case IDC_COMBO_HDD2:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                HandleHDDCombo(hWnd, HARDDISK_2, LOWORD(wparam));
            }
            break;
        case IDC_HDD_ENABLE:
            EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));
            break;
        case IDC_HDD_SWAP:
            HandleHDDSwap(hWnd);
            break;
        }
        break;

    case WM_INITDIALOG:
    {
        m_PropertySheetHelper.FillComboBox(hWnd, IDC_DISKTYPE, m_discchoices, sg_Disk2Card.GetEnhanceDisk() ? 1 : 0);
        m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_DISK1, m_defaultDiskOptions, -1);
        m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_DISK2, m_defaultDiskOptions, -1);

        if (strlen(sg_Disk2Card.GetFullName(DRIVE_1)) > 0) {
            SendDlgItemMessage(hWnd, IDC_COMBO_DISK1, CB_INSERTSTRING, 0, (LPARAM)sg_Disk2Card.GetFullName(DRIVE_1));
            SendDlgItemMessage(hWnd, IDC_COMBO_DISK1, CB_SETCURSEL, 0, 0);
        }

        if (strlen(sg_Disk2Card.GetFullName(DRIVE_2)) > 0) {
            SendDlgItemMessage(hWnd, IDC_COMBO_DISK2, CB_INSERTSTRING, 0, (LPARAM)sg_Disk2Card.GetFullName(DRIVE_2));
            SendDlgItemMessage(hWnd, IDC_COMBO_DISK2, CB_SETCURSEL, 0, 0);
        }

        InitComboHDD(hWnd);

        CheckDlgButton(hWnd, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

        EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));

        InitOptions(hWnd);

        break;
    }

    }

    return FALSE;
}

void PageDisk::InitComboHDD(HWND hWnd) {
    m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD1, m_defaultHDDOptions, -1);
    m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD2, m_defaultHDDOptions, -1);

    if (strlen(HD_GetFullName(HARDDISK_1)) > 0) {
        SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
        SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_SETCURSEL, 0, 0);
    }

    if (strlen(HD_GetFullName(HARDDISK_2)) > 0) {
        SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
        SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_SETCURSEL, 0, 0);
    }
}

void PageDisk::DlgOK(HWND hWnd) {
    const bool bNewEnhanceDisk = SendDlgItemMessage(hWnd, IDC_DISKTYPE, CB_GETCURSEL, 0, 0) ? true : false;
    if (bNewEnhanceDisk != sg_Disk2Card.GetEnhanceDisk()) {
        sg_Disk2Card.SetEnhanceDisk(bNewEnhanceDisk);
        REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), (DWORD)bNewEnhanceDisk);
    }

    const bool bNewHDDIsEnabled = IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE) ? true : false;
    if (bNewHDDIsEnabled != HD_CardIsEnabled()) {
        m_PropertySheetHelper.GetConfigNew().m_bEnableHDD = bNewHDDIsEnabled;
    }

    RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, HD_GetFullPathName(HARDDISK_1));
    RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, HD_GetFullPathName(HARDDISK_2));

    m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void PageDisk::InitOptions(HWND hWnd) {
    // Nothing to do:
    // - no changes made on any other pages affect this page
}

void PageDisk::EnableHDD(HWND hWnd, BOOL bEnable) {
    EnableWindow(GetDlgItem(hWnd, IDC_COMBO_HDD1), bEnable);
    EnableWindow(GetDlgItem(hWnd, IDC_COMBO_HDD2), bEnable);
    EnableWindow(GetDlgItem(hWnd, IDC_HDD_SWAP), bEnable);
}

void PageDisk::EnableDisk(HWND hWnd, BOOL bEnable) {
    EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK1), bEnable);
    EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK2), bEnable);
}

void PageDisk::HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected) {
    if (!IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE))
        return;

    // Search from "select hard drive"
    DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)& m_defaultHDDOptions[0]);
    DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

    SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);   // Set to "empty" item

    if (dwComboSelection == dwOpenDialogIndex) {
        EnableHDD(hWnd, FALSE); // Prevent multiple Selection dialogs to be triggered
        bool bRes = HD_Select(driveSelected);
        EnableHDD(hWnd, TRUE);

        if (!bRes) {
            if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)    // If there's already a HDD...
                SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);        // then reselect it in the ComboBox
            return;
        }

        // Add hard drive name as item 0 and select it
        if (dwOpenDialogIndex > 0) {
            // Remove old item first
            SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
        }

        SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(driveSelected));
        SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

        // If the HD was in the other combo, remove now
        DWORD comboOther = (comboSelected == IDC_COMBO_HDD1) ? IDC_COMBO_HDD2 : IDC_COMBO_HDD1;

        DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)HD_GetFullName(driveSelected));
        if (duplicated != CB_ERR) {
            SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
            SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
        }
    }
    else if (dwComboSelection == (dwOpenDialogIndex + 1)) {
        if (dwComboSelection > 1) {
            UINT uCommand = (driveSelected == 0) ? IDC_COMBO_HDD1 : IDC_COMBO_HDD2;
            if (RemovalConfirmation(uCommand)) {
                // Unplug selected disk
                HD_Unplug(driveSelected);
                // Remove drive from list
                SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
            }
            else {
                SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
            }
        }
    }
}

void PageDisk::HandleDiskCombo(HWND hWnd, UINT driveSelected, UINT comboSelected) {
    // Search from "select floppy drive"
    DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)& m_defaultDiskOptions[0]);
    DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

    SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);   // Set to "empty" item

    if (dwComboSelection == dwOpenDialogIndex) {
        EnableDisk(hWnd, FALSE);    // Prevent multiple Selection dialogs to be triggered
        bool bRes = sg_Disk2Card.UserSelectNewDiskImage(driveSelected);
        EnableDisk(hWnd, TRUE);

        if (!bRes) {
            if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)    // If there's already a disk...
                SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);        // then reselect it in the ComboBox
            return;
        }

        // Add floppy drive name as item 0 and select it
        if (dwOpenDialogIndex > 0) {
            //Remove old item first
            SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
        }

        SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)sg_Disk2Card.GetFullName(driveSelected));
        SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

        // If the FD was in the other combo, remove now
        DWORD comboOther = (comboSelected == IDC_COMBO_DISK1) ? IDC_COMBO_DISK2 : IDC_COMBO_DISK1;

        DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)sg_Disk2Card.GetFullName(driveSelected));
        if (duplicated != CB_ERR) {
            SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
            SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
        }
    }
    else if (dwComboSelection == (dwOpenDialogIndex + 1)) {
        if (dwComboSelection > 1) {
            UINT uCommand = (driveSelected == 0) ? IDC_COMBO_DISK1 : IDC_COMBO_DISK2;
            if (RemovalConfirmation(uCommand)) {
                // Eject selected disk
                sg_Disk2Card.EjectDisk(driveSelected);
                // Remove drive from list
                SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
            }
            else {
                SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
            }
        }
    }
}

void PageDisk::HandleHDDSwap(HWND hWnd) {
    if (!RemovalConfirmation(IDC_HDD_SWAP))
        return;

    if (!HD_ImageSwap())
        return;

    InitComboHDD(hWnd);
}

UINT PageDisk::RemovalConfirmation(UINT uCommand) {
    TCHAR szText[100];
    const size_t strLen = sizeof(szText) - 1;
    bool bMsgBox = true;

    if (uCommand == IDC_COMBO_DISK1 || uCommand == IDC_COMBO_DISK2)
        _snprintf(szText, strLen, "Do you really want to eject the disk in drive-%c ?", '1' + uCommand - IDC_COMBO_DISK1);
    else if (uCommand == IDC_COMBO_HDD1 || uCommand == IDC_COMBO_HDD2)
        _snprintf(szText, strLen, "Do you really want to unplug harddisk-%c ?", '1' + uCommand - IDC_COMBO_HDD1);
    else if (uCommand == IDC_HDD_SWAP)
        _snprintf(szText, strLen, "Do you really want to swap the harddisk images?");
    else
        bMsgBox = false;

    szText[strLen] = 0;

    if (bMsgBox) {
        int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Eject/Unplug Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
        if (nRes == IDNO)
            uCommand = 0;
    }

    return uCommand;
}