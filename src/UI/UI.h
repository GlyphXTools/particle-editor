#ifndef UIELEMENTS_H
#define UIELEMENTS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <afxres.h>
#include <string>
#include "exceptions.h"
#include "resource.h"
#include "types.h"
#include "ParticleSystem.h"

/*
 * Spinner control
 */

// Spinner Flags
static const int SPIF_VALUE     = 1;
static const int SPIF_RANGE     = 2;
static const int SPIF_INCREMENT = 4;
static const int SPIF_ALL       = SPIF_VALUE | SPIF_RANGE | SPIF_INCREMENT;

// Messages
#define SN_CHANGE		(WM_APP + 1)	// Spinner value has changed. Sent as WM_COMMAND.
#define SN_KILLFOCUS	(WM_APP + 2)	// Spinner has lost focus. Sent as WM_COMMAND.

// Structure with spinner info
struct SPINNER_INFO
{
	int  Mask;
	bool IsFloat;
	union
	{
		struct
		{
			float Value;
			float MaxValue;
			float MinValue;
			float Increment;
		} f;
		struct
		{
			long Value;
			long MaxValue;
			long MinValue;
			long Increment;
		} i;
	};
};

bool Spinner_Initialize(HINSTANCE hInstance);
void Spinner_SetInfo(HWND hWnd, const SPINNER_INFO* psi);
void Spinner_GetInfo(HWND hWnd, SPINNER_INFO* psi);

/*
 * Curve Editor control
 */
// CEN_SELCHANGE:
//	CurveEditor selection has changed. Sent as WM_NOTIFY message to the parent control.
//	wParam = handle to the control
//	lParam = pointer to NMCURVEEDITOR structure
//	
#define CEN_SELCHANGE	(WM_APP + 3)
#define CEN_MODECHANGE	(WM_APP + 4)
#define CEN_KBINPUT		(WM_APP + 5)
#define CEN_KEYSCHANGED	(WM_APP + 6)

enum CursorMode
{
	CM_SELECT,
	CM_INSERT,
};

struct NMCURVEEDITOR
{
	NMHDR      hdr;
	CursorMode mode; // Current cursor mode
};

struct NMCURVEEDITORKEY
{
	NMHDR  hdr;
	UINT   uMsg;
	WPARAM wParam;
	LPARAM lParam;
};

struct SelectedKey : public ParticleSystem::Emitter::Track::Key
{
    int index;

    bool operator < (const SelectedKey& sel) const {
        return index < sel.index;
    }

    SelectedKey(int i) : index(i) { }
    SelectedKey(int i, const ParticleSystem::Emitter::Track::Key& key)
        : ParticleSystem::Emitter::Track::Key(key), index(i) { }
};

typedef std::set<SelectedKey> SelectedKeys;

bool CurveEditor_Initialize(HINSTANCE hInstance);
void CurveEditor_SetCursorMode(HWND hWnd, CursorMode mode);
void CurveEditor_SetTrack(HWND hWnd, ParticleSystem::Emitter::Track *track, bool editable);
void CurveEditor_SetInterpolationType(HWND hWnd, ParticleSystem::Emitter::Track::InterpolationType type);
void CurveEditor_SetHorzRange(HWND hWnd, float xMin, float xMax, bool lock = false);
void CurveEditor_SetVertRange(HWND hWnd, float yMin, float yMax, bool lcok = false);
const SelectedKeys* CurveEditor_GetSelection(HWND hWnd);
int  CurveEditor_GetKeyCount(HWND hWnd);
void CurveEditor_GetKey(HWND hWnd, int index, ParticleSystem::Emitter::Track::Key* key);
int  CurveEditor_AddKey(HWND hWnd, ParticleSystem::Emitter::Track::Key key, bool isBorder = false);
void CurveEditor_MoveSelection(HWND hWnd, float dTime, float dValue);
void CurveEditor_DeleteSelection(HWND hWnd);

/*
 * Track Editor control
 */
struct NMTECHANGE
{
	NMHDR hdr;
	int   track;
};

#define TE_CHANGE	(WM_APP + 8)

bool TrackEditor_Initialize(HINSTANCE hInstance);
void TrackEditor_SetTrack(HWND hWnd, ParticleSystem::Emitter::Track* trackContents, ParticleSystem::Emitter::Track** tracks);
void TrackEditor_EnableTrack(HWND hWnd, bool enabled);

/*
 * Random Parameters control
 */
#define RP_CHANGE	(WM_APP + 9)

bool RandomParam_Initialize(HINSTANCE hInstance);
void RandomParam_SetGroup(HWND hWnd, ParticleSystem::Emitter::Group* group );

/*
 * Emitter Properties
 */
#define EP_CHANGE	 (WM_APP + 10)

bool  EmitterProps_Initialize(HINSTANCE hInstance);
void  EmitterProps_SetEmitter(HWND hWnd, ParticleSystem::Emitter* emitter);

/*
 * Emitter List
 */
#define ELN_SELCHANGED  (WM_APP + 11)
#define ELN_LISTCHANGED (WM_APP + 12)

bool EmitterList_Initialize(HINSTANCE hInstance);
void EmitterList_SetParticleSystem(HWND hWnd, ParticleSystem* system);
void EmitterList_SelectionChanged(HWND hWnd);
void EmitterList_AddRootEmitter    (HWND hWnd, const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
void EmitterList_AddLifetimeEmitter(HWND hWnd, const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
void EmitterList_AddDeathEmitter   (HWND hWnd, const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
void EmitterList_ToggleEmitterVisibility(HWND hWnd, HTREEITEM hItem = NULL);
void EmitterList_SetAllEmitterVisibility(HWND hWnd, bool visible);
void EmitterList_DeleteEmitter(HWND hWnd);
void EmitterList_RenameEmitter(HWND hWnd);
bool EmitterList_HasFocus(HWND hWnd);
ParticleSystem::Emitter* EmitterList_GetSelection(HWND hWnd);
/*
 * ColorButton control
 */

// Messages
#define CBN_CHANGE		(WM_APP + 13)	// Color button value has changed. Sent as WM_COMMAND.

bool ColorButton_Initialize(HINSTANCE hInstance);
void ColorButton_SetColor(HWND hWnd, COLORREF color);
COLORREF ColorButton_GetColor(HWND hWnd);

/* 
 * Global UI functions
 */
static bool UI_Initialize( HINSTANCE hInstance )
{
	InitCommonControls();
	return (
			Spinner_Initialize(hInstance) &&
			CurveEditor_Initialize(hInstance) &&
			TrackEditor_Initialize(hInstance) &&
			RandomParam_Initialize(hInstance) &&
			EmitterProps_Initialize(hInstance) &&
            EmitterList_Initialize(hInstance) &&
            ColorButton_Initialize(hInstance)
			);
}

static void SetUIFloat(HWND hDlg, int nIDDlgItem, float value)
{
	SPINNER_INFO si;
	si.IsFloat = true;
	si.Mask    = SPIF_VALUE;
	si.f.Value = value;
	Spinner_SetInfo(GetDlgItem(hDlg, nIDDlgItem), &si);
}

static void SetUIInteger(HWND hDlg, int nIDDlgItem, long value)
{
	SPINNER_INFO si;
	si.IsFloat = false;
	si.Mask    = SPIF_VALUE;
	si.i.Value = value;
	Spinner_SetInfo(GetDlgItem(hDlg, nIDDlgItem), &si);
}

static void SetUIBool(HWND hDlg, int nIDDlgItem, bool value)
{
	CheckDlgButton(hDlg, nIDDlgItem, value ? BST_CHECKED : BST_UNCHECKED);
}

static float GetUIFloat(HWND hDlg, int nIDDlgItem)
{
	SPINNER_INFO si;
	si.IsFloat = true;
	si.Mask    = SPIF_VALUE;
	Spinner_GetInfo(GetDlgItem(hDlg, nIDDlgItem), &si);
	return si.f.Value;
}

static long GetUIInteger(HWND hDlg, int nIDDlgItem)
{
	SPINNER_INFO si;
	si.IsFloat = false;
	si.Mask    = SPIF_VALUE;
	Spinner_GetInfo(GetDlgItem(hDlg, nIDDlgItem), &si);
	return si.i.Value;
}

static bool GetUIBool(HWND hDlg, int nIDDlgItem)
{
	return IsDlgButtonChecked(hDlg, nIDDlgItem) == BST_CHECKED;
}

#endif