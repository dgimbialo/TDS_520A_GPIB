// resource.h - Resource IDs
#pragma once

// Application
#define IDR_MAINFRAME           128

// Icons
#define IDI_APP_ICON            130
#define IDI_APP_SMALL           131

// Commands
#define ID_FILE_EXIT            32771
#define ID_SCOPE_CONNECT        32800
#define ID_SCOPE_DISCONNECT     32801
#define ID_SCOPE_STARTACQ       32802
#define ID_SCOPE_STOPACQ        32803
#define ID_SCOPE_SINGLESHOT     32804
#define ID_SCOPE_CH1            32810
#define ID_SCOPE_CH2            32811
#define ID_SCOPE_CH3            32812
#define ID_SCOPE_CH4            32813
#define ID_SERVER_START         32820
#define ID_SERVER_STOP          32821
#define ID_VIEW_SETTINGS        32830
#define ID_HELP_ABOUT           32890

// Status bar pane IDs
#define ID_STATUSBAR_PANE_GPIB  0xE801
#define ID_STATUSBAR_PANE_FPS   0xE802
#define ID_STATUSBAR_PANE_ACQ   0xE803
#define ID_STATUSBAR_PANE_HTTP  0xE804

// Dialogs
#define IDD_CONNECT_DIALOG      200

// Connect dialog controls
#define IDC_SPIN_BOARD          201
#define IDC_EDIT_BOARD          202
#define IDC_BTN_SCAN            203
#define IDC_LIST_DEVICES        204
#define IDC_BTN_CONNECT         205
#define IDC_EDIT_LOG            206
#define IDC_STATIC_STATUS       207
#define IDC_PROGRESS_SCAN       208
#define IDC_EDIT_START_ADDR     209
#define IDC_SPIN_START_ADDR     210

// Timer IDs
#define TIMER_RENDER            1
#define TIMER_STATUS            2
#define TIMER_CTRL_UPDATE       3

// Scope control panel dialog and controls
#define IDD_SCOPE_CONTROLS      300
#define IDC_BTN_RUN             301
#define IDC_BTN_STOP            302
#define IDC_BTN_SINGLE          303
#define IDC_COMBO_CHANNEL       304
#define IDC_STATIC_ACQRATE      305
#define IDC_STATIC_TIMEDIV      306
#define IDC_STATIC_VOLTDIV      307
#define IDC_STATIC_FREQ         308
#define IDC_STATIC_VMAX         309
#define IDC_STATIC_VMIN         310
#define IDC_STATIC_VPP          311
#define IDC_STATIC_CURSOR_DT    312
#define IDC_STATIC_CURSOR_DV    313
#define IDC_BTN_TIMEDIV_UP      314
#define IDC_BTN_TIMEDIV_DN      315
#define IDC_BTN_VOLTDIV_UP      316
#define IDC_BTN_VOLTDIV_DN      317
#define IDC_BTN_TRIGLVL_UP      318
#define IDC_BTN_TRIGLVL_DN      319
#define IDC_STATIC_TRIGLVL      320
#define IDC_BTN_CURSOR_TOGGLE   321
#define IDC_STATIC_CONN_STATUS  322
#define IDC_BTN_ACQRATE_UP      323
#define IDC_BTN_ACQRATE_DN      324
#define IDC_STATIC_ACQRATE_TGT  325
#define IDC_EDIT_ACQRATE        326
#define IDC_STATIC_QR_URL       327   // URL label below QR code
#define IDC_STATIC_QR_HINT      328   // "Scan for web UI" hint text
#define IDC_STATIC_QR_IMAGE     329   // owner-draw static for the QR bitmap

// String table
#define IDS_APP_TITLE           1
#define IDS_APP_VERSION         2

