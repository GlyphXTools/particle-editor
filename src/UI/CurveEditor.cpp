#include <iomanip>
#include <sstream>
#include <cmath>
#include <cfloat>
#include <map>
#include "UI.h"
using namespace std;

static const int KEY_SIZE  = 4;
static const int TICK_SIZE = 3;

// Registered clipboard format
static UINT CF_TRACK_KEYS = 0;

struct Range
{
	float xMin, xMax;
	float yMin, yMax;
};

struct CurveControl
{
	HWND hEditor;

	Range data;
	Range view;
	bool  viewRangeHorzLocked;
	bool  viewRangeVertLocked;
	bool  dragging;
    POINT dragStart;
    POINT dragCurrent;
    int   draggingKey;

	// Calculated render parameters
	int   xStep;
	int   yStep;
	int   xLabelHeight;
	int   yLabelWidth;

	// Data
	CursorMode						mode;
	SelectedKeys	                selection;
	ParticleSystem::Emitter::Track* track;
	bool							editable;

	// Resources
	HCURSOR hCursor;
	HDC     hBackBufferDC;
	HBITMAP hBackBufferBitmap;
	HFONT   hFont;

	HPEN    hGridPen;
	HPEN    hBorderPen;
	HPEN    hLabelPen;
	HPEN    hLinePen;
	HPEN    hLinePenStep;
	HPEN    hKeyPen;
	HBRUSH	hBrushNonSelected;
	HBRUSH	hBrushSelected;
};

static POINT Curve_KeyToWindowCoords(HWND hWnd, const CurveControl* control, const ParticleSystem::Emitter::Track::Key& key)
{
	POINT p;
	RECT area;
	GetClientRect(hWnd, &area);

	float xRange = control->view.xMax - control->view.xMin;
	float yRange = control->view.yMax - control->view.yMin;

	p.x = control->yLabelWidth                + (long)(((key.time  - control->view.xMin) / xRange) * (area.right  - control->yLabelWidth ));
	p.y = area.bottom - control->xLabelHeight - (long)(((key.value - control->view.yMin) / yRange) * (area.bottom - control->xLabelHeight));
	return p;
}

static bool Curve_WindowCoordsToKey(HWND hWnd, const CurveControl* control, const POINT& pos, ParticleSystem::Emitter::Track::Key* key)
{
	RECT area;
	GetClientRect(hWnd, &area);
	int x = max(min(pos.x, area.right - 1), control->yLabelWidth);
	int y = max(min(pos.y, area.bottom - control->xLabelHeight), 0);

	float xRange = control->view.xMax - control->view.xMin;
	float yRange = control->view.yMax - control->view.yMin;

	key->time  = (x - control->yLabelWidth               ) * xRange / (area.right  - control->yLabelWidth) + control->view.xMin;
	key->value = (area.bottom - control->xLabelHeight - y) * yRange / (area.bottom - control->xLabelHeight)  + control->view.yMin;
	return true;
}

// Create reasonable values from possibly weird limits
// i.e. transform 107.46238f into 110.0f etc.
// The maximum is extended away from zero, the minimum is extended
// towards zero.
// Returns the base of the numbers.
static float Curve_Sanitize_Limits(float& minVal, float& maxVal)
{
	float signMin = 0.0f,     signMax = 0.0f;
	float logMin  = -FLT_MAX, logMax  = -FLT_MAX;

	if (minVal == maxVal)
	{
		maxVal = minVal + 1.0f;
	}

	if (minVal != 0.0f)
	{
		signMin = minVal / fabs(minVal);
		minVal = fabs(minVal);
		logMin = floorf(log10f(minVal));// - 1;
	}

	if (maxVal != 0.0f)
	{
		signMax = maxVal / fabs(maxVal);
		maxVal = fabs(maxVal);
		logMax = floorf(log10f(maxVal));// - 1;
	}

	// The base is determined from the biggest number (logarithm wise)
	float base = powf(10, max(logMin, logMax));

	maxVal = (signMax > 0.0f) ? ceilf(maxVal / base) : floorf(maxVal / base);
	minVal = (signMin < 0.0f) ? ceilf(minVal / base) : floorf(minVal / base);
	maxVal = signMax * maxVal * base;
	minVal = signMin * minVal * base;
	return base / 10;
}

static int Curve_CalculateStep(float& min, float& max, int size, int minStepSize)
{
	float base = Curve_Sanitize_Limits(min, max);

	// And now it's "magically" an integer problem
	// And we can calculate divisors!
	int step, range = (int)floor((max - min) / base + 0.5);
	for (step = range; step > 0; step--)
	{
		if (range % step == 0 && size / step >= minStepSize)
		{
			break;
		}
	}
	return step;
}

static void Curve_CalculatePaintParameters(HWND hWnd, HDC hDC, CurveControl* control)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	TEXTMETRIC tm;
	GetTextMetrics(hDC, &tm);

	// Calculate Horz labels width
	control->xLabelHeight = tm.tmHeight + TICK_SIZE + 2;
	control->yStep = Curve_CalculateStep(control->view.yMin, control->view.yMax, rect.bottom - rect.top - control->xLabelHeight,  25);
	
	// Calculate Vert labels width
	control->yLabelWidth = 0;
	float yRange = control->view.yMax - control->view.yMin;
	for (int i = 0; i <= control->yStep; i++)
	{
        wstringstream ss;
        ss << yRange * i / control->yStep;
        wstring s = ss.str();
		SIZE size;
        GetTextExtentPoint32(hDC, s.c_str(), (int)s.length(), &size);
		control->yLabelWidth = max(control->yLabelWidth, size.cx);
	}
	control->yLabelWidth += TICK_SIZE + 3;
	control->xStep = Curve_CalculateStep(control->view.xMin, control->view.xMax, rect.right  - rect.left - control->yLabelWidth, 50);
}

static void Curve_PaintAxes(HWND hWnd, HDC hDC, CurveControl* control)
{
	RECT area;
	GetClientRect(hWnd, &area);

	TEXTMETRIC tm;
	GetTextMetrics(hDC, &tm);

    // Draw label lines
	SelectObject(hDC, control->hBorderPen);
	MoveToEx(hDC, control->yLabelWidth, 0, NULL);
	LineTo(hDC, control->yLabelWidth, area.bottom - control->xLabelHeight);
	MoveToEx(hDC, control->yLabelWidth, area.bottom - control->xLabelHeight, NULL);
	LineTo(hDC, area.right, area.bottom - control->xLabelHeight);

	// Draw labels and grid
	float xRange = control->view.xMax - control->view.xMin;
	float yRange = control->view.yMax - control->view.yMin;

	SetBkMode(hDC, TRANSPARENT);
	SetTextAlign(hDC, TA_TOP | TA_CENTER);
	for (int i = 0; i <= control->xStep; i++)
	{
		wstringstream ss;
		float val = xRange * i / control->xStep + control->view.xMin;
		long x = (long)(((val - control->view.xMin) / xRange) * (area.right - control->yLabelWidth)) + control->yLabelWidth;
		long y = area.bottom - control->xLabelHeight;
        ss << val;
        wstring s = ss.str();

		if (i == control->xStep)
		{
			SetTextAlign(hDC, TA_TOP | TA_RIGHT);
		}

		SelectObject(hDC, control->hLabelPen);
        TextOut(hDC, x, y + TICK_SIZE, s.c_str(), (int)s.length());

		SelectObject(hDC, control->hGridPen);
		MoveToEx(hDC, x, 0, NULL);
		LineTo(hDC, x, y);

		SelectObject(hDC, control->hBorderPen);
		LineTo(hDC, x, y + TICK_SIZE);
	}

	SetTextAlign(hDC, TA_RIGHT | TA_BOTTOM);
	for (int i = 0; i <= control->yStep; i++)
	{
		wstringstream ss;
		float val = yRange * i / control->yStep + control->view.yMin;
		long x = area.left + control->yLabelWidth;
		long y = area.bottom - control->xLabelHeight - (long)(((val - control->view.yMin) / yRange) * (area.bottom - control->xLabelHeight));
		ss << val;
		wstring s = ss.str();

		SelectObject(hDC, control->hGridPen);
		MoveToEx(hDC, area.right, y, NULL);
		LineTo(hDC, x, y);

		SelectObject(hDC, control->hBorderPen);
		LineTo(hDC, x - TICK_SIZE, y);

		y += tm.tmAscent / 2;
		if (i == control->yStep)
		{
			y += tm.tmAscent / 2;
		}

		SelectObject(hDC, control->hLabelPen);
		TextOut(hDC, x - 1 - TICK_SIZE, y, s.c_str(), (int)s.length());
	}
}

static void Curve_Paint(HWND hWnd, HDC hDC, CurveControl* control)
{
	RECT area;
	GetClientRect(hWnd, &area);
    FillRect(hDC, &area, GetSysColorBrush( (control->editable ? COLOR_WINDOW : COLOR_BTNFACE) ));

	SelectObject(hDC, control->hFont);

    Curve_CalculatePaintParameters(hWnd, hDC, control);
	Curve_PaintAxes(hWnd, hDC, control);

	if (control->track != NULL)
	{
		const ParticleSystem::Emitter::Track& track = *control->track;

		switch (track.interpolation)
		{
			case ParticleSystem::Emitter::Track::IT_LINEAR:
				SelectObject(hDC, control->hLinePen);
				for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = track.keys.begin(); p != track.keys.end(); p++)
				{
					POINT cur = Curve_KeyToWindowCoords(hWnd, control, *p);
					if (p != track.keys.begin()) LineTo(hDC, cur.x, cur.y);
					else MoveToEx(hDC, cur.x, cur.y, NULL);
				}
				break;

			case ParticleSystem::Emitter::Track::IT_SMOOTH:
			{
				// A bezier curve with control points at 1/4 and 3/4 between the points very closely
				// resembles the cubic interpolation used by the renderer.
				if (track.keys.size() > 0)
				{
					vector<POINT> points(1 + 3 * (track.keys.size() - 1));
					ParticleSystem::Emitter::Track::KeyMap::const_iterator p = track.keys.begin();
					points[0] = Curve_KeyToWindowCoords(hWnd, control, *p++);
					for (int i = 0; p != track.keys.end(); p++, i++)
					{
						int j = 1 + 3 * i;
						points[j+2] = Curve_KeyToWindowCoords(hWnd, control, *p);
						points[j+0].x = (points[j+2].x - points[j-1].x) * 1 / 4 + points[j-1].x;
						points[j+0].y = points[j-1].y;
						points[j+1].x = (points[j+2].x - points[j-1].x) * 3 / 4 + points[j-1].x;
						points[j+1].y = points[j+2].y;
					}
					SelectObject(hDC, control->hLinePen);
					PolyBezier(hDC, &points[0], (DWORD)points.size());
				}
				break;
			}

			case ParticleSystem::Emitter::Track::IT_STEP:
			{
				POINT prev;
				for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = track.keys.begin(); p != track.keys.end(); p++)
				{
					POINT cur = Curve_KeyToWindowCoords(hWnd, control, *p);
					if (p != track.keys.begin())
					{
						SelectObject(hDC, control->hLinePen);
						LineTo(hDC, cur.x, prev.y);
						SelectObject(hDC, control->hLinePenStep);
						LineTo(hDC, cur.x, cur.y);
					}
					else
					{
						MoveToEx(hDC, cur.x, cur.y, NULL);
					}
					prev = cur;
				}
				break;
			}
		}

		SelectObject(hDC, control->hKeyPen);
		ParticleSystem::Emitter::Track::KeyMap::const_iterator p;
		int i;
		for (i = 0, p = track.keys.begin(); p != track.keys.end(); p++, i++)
		{
			POINT cur = Curve_KeyToWindowCoords(hWnd, control, *p);
            SelectObject(hDC, (control->selection.find(i) != control->selection.end()) ? control->hBrushSelected : control->hBrushNonSelected);
			Rectangle(hDC, cur.x - KEY_SIZE, cur.y - KEY_SIZE, cur.x + KEY_SIZE, cur.y + KEY_SIZE);
		}
	}

    if (control->dragging && control->selection.empty())
    {
        // We're dragging a selection rectangle
        RECT selection;
        selection.left   = min(control->dragStart.x, control->dragCurrent.x);
        selection.right  = max(control->dragStart.x, control->dragCurrent.x);
        selection.top    = min(control->dragStart.y, control->dragCurrent.y);
        selection.bottom = max(control->dragStart.y, control->dragCurrent.y);
        DrawFocusRect(hDC, &selection);
    }
}

static void Curve_OnModeChange(HWND hWnd, CurveControl* control)
{
	// Send mode change notification to parent
	NMCURVEEDITOR nmce;
	nmce.hdr.code     = CEN_MODECHANGE;
	nmce.hdr.hwndFrom = hWnd;
	nmce.hdr.idFrom   = GetDlgCtrlID(hWnd);
	nmce.mode         = control->mode;
	SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)nmce.hdr.idFrom, (LPARAM)&nmce);
}

static void Curve_OnSelectionChanged(HWND hWnd, CurveControl* control)
{
	// Send selection change notification to parent
	NMCURVEEDITOR nmce;
	nmce.hdr.code     = CEN_SELCHANGE;
	nmce.hdr.hwndFrom = hWnd;
	nmce.hdr.idFrom   = GetDlgCtrlID(hWnd);
	nmce.mode         = control->mode;
	SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)nmce.hdr.idFrom, (LPARAM)&nmce);

	// Redraw window
	RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

static void Curve_OnKeysChanged(HWND hWnd, CurveControl* control)
{
	// Send key change notification to parent
	NMCURVEEDITOR nmce;
	nmce.hdr.code     = CEN_KEYSCHANGED;
	nmce.hdr.hwndFrom = hWnd;
	nmce.hdr.idFrom   = GetDlgCtrlID(hWnd);
	nmce.mode         = control->mode;
	SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)nmce.hdr.idFrom, (LPARAM)&nmce);

	// Redraw window
	RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void CurveEditor_AutoScale(CurveControl* control)
{
	if (control->track != NULL && !control->dragging && (!control->viewRangeHorzLocked || !control->viewRangeVertLocked))
	{
		float xMin = FLT_MAX, xMax = -FLT_MAX;
		float yMin = FLT_MAX, yMax = -FLT_MAX;

		for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = control->track->keys.begin(); p != control->track->keys.end(); p++)
		{
			xMin = min(xMin, p->time);
			xMax = max(xMax, p->time);
			yMin = min(yMin, p->value);
			yMax = max(yMax, p->value);
		}

		if (!control->viewRangeHorzLocked)
		{
			control->view.xMin = xMin;
			control->view.xMax = xMax;
		}

		if (!control->viewRangeVertLocked)
		{
			control->view.yMin = yMin;
			control->view.yMax = yMax;
		}
	}
}

static bool CopyKeys(HWND hWnd, CurveControl* control)
{
    if (!control->selection.empty())
    {
        HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, control->selection.size() * sizeof(ParticleSystem::Emitter::Track::Key) ); 
        if (hMemory == NULL) 
        { 
            return false;
        }

        // Lock the handle and copy the text to the buffer. 
        ParticleSystem::Emitter::Track::Key* keys = (ParticleSystem::Emitter::Track::Key*)GlobalLock(hMemory); 
        for (SelectedKeys::const_iterator p = control->selection.begin(); p != control->selection.end(); p++, keys++)
        {
            *keys = *p;
        }
        GlobalUnlock(hMemory); 

        // Place the handle on the clipboard. 
        OpenClipboard(hWnd);
        SetClipboardData(CF_TRACK_KEYS, hMemory); 
        CloseClipboard();
    }
    return true;
}

static bool PasteKeys(HWND hWnd, CurveControl* control)
{
    OpenClipboard(hWnd);
    HANDLE hMemory = GetClipboardData(CF_TRACK_KEYS);
    if (hMemory == NULL)
    {
        CloseClipboard();
        return false;
    }

    // Get the keys and order them
    ParticleSystem::Emitter::Track::KeyMap keys;
    ParticleSystem::Emitter::Track::Key* pKeys = (ParticleSystem::Emitter::Track::Key*)GlobalLock(hMemory); 
    size_t nKeys = GlobalSize(hMemory) / sizeof(ParticleSystem::Emitter::Track::Key);
    for (size_t i = 0; i < nKeys; i++)
    {
        keys.insert(pKeys[i]);
    }
    GlobalUnlock(hMemory); 
    CloseClipboard();

    // Then add and select them
    control->selection.clear();
    for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = keys.begin(); p != keys.end(); p++)
    {
        control->selection.insert(SelectedKey(CurveEditor_AddKey(hWnd, *p, false), *p));
    }
    RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    Curve_OnSelectionChanged(hWnd, control);
    return true;
}

LRESULT CALLBACK CurveWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			control = new CurveControl;
			SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG)(LONG_PTR)control);

			control->editable    = false;
			control->track       = NULL;
			control->hEditor     = hWnd;
			control->data.xMin   =   0.0f;
			control->data.xMax   = 100.0f;
			control->data.yMin   =   0.0f;
			control->data.yMax   =   1.0f;
			control->view = control->data;
			control->viewRangeHorzLocked = false;
			control->viewRangeVertLocked = false;
			control->dragging            = false;

			CurveEditor_SetCursorMode(hWnd, CM_SELECT);
			CurveEditor_SetInterpolationType(hWnd, ParticleSystem::Emitter::Track::IT_LINEAR);

			//
			// Create backbuffer
			//
			HDC hDC = GetDC(hWnd);
			
			RECT area;
			GetClientRect(hWnd, &area);

			if ((control->hBackBufferDC = CreateCompatibleDC(hDC)) == NULL)
			{
				delete control;
				return -1;
			}

			if ((control->hBackBufferBitmap = CreateCompatibleBitmap(control->hBackBufferDC, area.right, area.bottom)) == NULL)
			{
				DeleteDC(control->hBackBufferDC);
				delete control;
				return -1;
			}
			SelectObject(control->hBackBufferDC, control->hBackBufferBitmap);

			//
			// Create pens, brushes, etc
			//
			control->hGridPen     = CreatePen(PS_SOLID, 0, RGB(192,192,192));
			control->hBorderPen   = CreatePen(PS_SOLID, 2, RGB(0,0,0));
			control->hLabelPen    = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_WINDOWTEXT));
			control->hLinePen     = CreatePen(PS_SOLID, 2, RGB(192,0,0));
			control->hLinePenStep = CreatePen(PS_DOT,   0, RGB(192,0,0));
			control->hKeyPen      = (HPEN)GetStockObject(BLACK_PEN);
			control->hBrushNonSelected = (HBRUSH)GetStockObject(GRAY_BRUSH);
			control->hBrushSelected    = (HBRUSH)GetStockObject(WHITE_BRUSH);
			break;
		}

        case WM_DESTROY:
			DeleteObject(control->hGridPen);
			DeleteObject(control->hBorderPen);
			DeleteObject(control->hLabelPen);
			DeleteObject(control->hLinePenStep);
			DeleteObject(control->hLinePen);
			DeleteObject(control->hBackBufferBitmap);
			DeleteDC(control->hBackBufferDC);
			delete control;
			break;

		case WM_PAINT:
		{
			// Paint into backbuffer
			Curve_Paint(hWnd, control->hBackBufferDC, control);

			// Blit backbuffer
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(hWnd, &ps);
			
			BitBlt(hDC, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, control->hBackBufferDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY );

			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_SETFONT:
			control->hFont = (HFONT)wParam;
			break;

		case WM_RBUTTONDOWN:
			SetFocus(hWnd);
			switch (control->mode)
			{
				case CM_SELECT:
					// In Select mode we deselect it
                    if (!control->selection.empty())
					{
                        control->selection.empty();
						Curve_OnSelectionChanged(hWnd, control);
					}
					break;

				case CM_INSERT:
					// In Insert mode we return to select mode (without deselecting)
					CurveEditor_SetCursorMode(hWnd, CM_SELECT);
					Curve_OnModeChange(hWnd, control);
					break;
			}
			break;

        case WM_CHAR:
            // Handle Ctrl+[C,V,X] chars
            if (GetKeyState(VK_CONTROL) & 0x80000000)
            {
                switch (wParam)
                {
                    case 1 + 'C' - 'A': SendMessage(hWnd, WM_COPY, 0, 0); return 0;
                    case 1 + 'X' - 'A': SendMessage(hWnd, WM_CUT,  0, 0); return 0;
                    case 1 + 'V' - 'A': SendMessage(hWnd, WM_PASTE, 0, 0); return 0;
                }
            }

        case WM_KEYDOWN:
		case WM_KEYUP:
			// Notify parent
			NMCURVEEDITORKEY nmcek;
			nmcek.hdr.code     = CEN_KBINPUT;
			nmcek.hdr.hwndFrom = hWnd;
			nmcek.hdr.idFrom   = GetDlgCtrlID(hWnd);
			nmcek.lParam = lParam;
			nmcek.wParam = wParam;
			nmcek.uMsg   = uMsg;
			SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)nmcek.hdr.idFrom, (LPARAM)&nmcek);
			break;

        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;

        case WM_COPY:  CopyKeys(hWnd, control);       break;
        case WM_CUT:   if (!CopyKeys(hWnd, control))  break;
        case WM_CLEAR: CurveEditor_DeleteSelection(hWnd);  break;
        case WM_PASTE: PasteKeys(hWnd, control);      break;

		case WM_LBUTTONUP:
			if (control->dragging)
			{
				control->dragging = false;
                if (!control->selection.empty())
                {
                    // We were dragging keys
				    CurveEditor_AutoScale(control);
                }
                else
                {
                    // We were dragging the selection rectangle
                    RECT rect;
                    rect.left   = min(control->dragStart.x, control->dragCurrent.x);
                    rect.right  = max(control->dragStart.x, control->dragCurrent.x);
                    rect.top    = min(control->dragStart.y, control->dragCurrent.y);
                    rect.bottom = max(control->dragStart.y, control->dragCurrent.y);
                    int i = 0;
                    for (ParticleSystem::Emitter::Track::KeyMap::const_iterator key = control->track->keys.begin(); key != control->track->keys.end(); key++, i++)
                    {
                        POINT p = Curve_KeyToWindowCoords(hWnd, control, *key);
                        if (p.x >= rect.left && p.x <= rect.right && p.y >= rect.top && p.y <= rect.bottom)
                        {
                            control->selection.insert(SelectedKey(i, *key));
                        }
                    }
                    Curve_OnSelectionChanged(hWnd, control);
                }
                ReleaseCapture();
      		    RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			}
			break;

		case WM_MOUSEMOVE:
			if (control->dragging)
			{
    		    POINT mouse = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
                control->dragCurrent = mouse;
                if (!control->selection.empty())
                {
                    // We're dragging keys
                    ParticleSystem::Emitter::Track::Key key;
				    if (Curve_WindowCoordsToKey(hWnd, control, mouse, &key))
				    {
                        SelectedKeys::const_iterator p = control->selection.find(control->draggingKey);
                        CurveEditor_MoveSelection(hWnd, key.time - p->time, key.value - p->value);                       
                        Curve_OnKeysChanged(hWnd, control);
				    }
                }
                else
                {
                    // We're dragging a selection, repaint
                    RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                }
			}
			break;

		case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);
			if (control->track != NULL && control->editable)
			{
				RECT area;
				GetClientRect(hWnd, &area);

				POINT mouse = {LOWORD(lParam), HIWORD(lParam)};

				// See if we hit a key
				int selectedKey = -1;
				ParticleSystem::Emitter::Track& track = *control->track;
				ParticleSystem::Emitter::Track::KeyMap::const_iterator p;
				int i;
				for (i = 0, p = track.keys.begin(); p != track.keys.end(); p++, i++)
				{
					POINT pos = Curve_KeyToWindowCoords(hWnd, control, *p);
					if (mouse.x >= pos.x - KEY_SIZE && mouse.x <= pos.x + KEY_SIZE &&
						mouse.y >= pos.y - KEY_SIZE && mouse.y <= pos.y + KEY_SIZE)
					{
						selectedKey = (int)i;
						break;
					}
				}

				switch (control->mode)
				{
					case CM_SELECT:
                        // In Select mode we select it or start dragging a selection rectangle
                        if (selectedKey == -1)
                        {
                            control->selection.clear();
                        }
                        else
                        {
                            if (control->selection.find(selectedKey) == control->selection.end())
						    {
                                control->selection.clear();
                                control->selection.insert(SelectedKey(selectedKey, *p));
       					        Curve_OnSelectionChanged(hWnd, control);
                            }
                        }
						
						SetCapture(hWnd);
                        control->draggingKey = selectedKey;
                        control->dragStart   = mouse;
                        control->dragCurrent = mouse;
						control->dragging    = true;
                        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
						break;

					case CM_INSERT:
					{
						// In Insert mode we only insert if we didn't hit a key
						ParticleSystem::Emitter::Track::Key key;
						if (selectedKey == -1 && Curve_WindowCoordsToKey(hWnd, control, mouse, &key))
						{
                            control->selection.clear();
                            control->selection.insert(SelectedKey(CurveEditor_AddKey(hWnd, key), key));
							Curve_OnSelectionChanged(hWnd, control);
                            Curve_OnKeysChanged(hWnd, control);
						}
						break;
					}
				}
			}
			break;
		}

		case WM_SIZE:
		{
			// Resize back buffer
			HBITMAP hBitmap;
			HDC hDC = GetDC(hWnd);
			if ((hBitmap = CreateCompatibleBitmap(hDC, LOWORD(lParam), HIWORD(lParam))) != NULL)
			{
				DeleteObject(control->hBackBufferBitmap);
				control->hBackBufferBitmap = hBitmap;
				SelectObject(control->hBackBufferDC, control->hBackBufferBitmap);
			}
			ReleaseDC(hWnd, hDC);
			break;
		}

		case WM_SETCURSOR:
			SetCursor(control->hCursor);
			return TRUE;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void CurveEditor_SetCursorMode(HWND hWnd, CursorMode mode)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		control->mode = mode;
		LPCTSTR lpCursor;
		switch (mode)
		{
			case CM_INSERT:	lpCursor = IDC_CROSS; break;
			case CM_SELECT: 
			default:		lpCursor = IDC_ARROW; break;
		}
		control->hCursor = LoadCursor(NULL, lpCursor);
		
        if (mode == CM_INSERT && !control->selection.empty())
		{
            control->selection.clear();
			Curve_OnSelectionChanged(hWnd, control);
			RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		}
	}
}

void CurveEditor_SetInterpolationType(HWND hWnd, ParticleSystem::Emitter::Track::InterpolationType type)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL && control->track != NULL)
	{
		control->track->interpolation = type;
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void CurveEditor_SetHorzRange(HWND hWnd, float xMin, float xMax, bool lock)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		control->data.xMin = xMin;
		control->data.xMax = xMax;
		control->viewRangeHorzLocked = lock;
		if (lock)
		{
			control->view.xMin = xMin;
			control->view.xMax = xMax;
		}

		if (control->track != NULL)
		{
			// Limit keys
			ParticleSystem::Emitter::Track::KeyMap keys;
			for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = control->track->keys.begin(); p != control->track->keys.end(); p++)
			{
				ParticleSystem::Emitter::Track::Key key;
				key.time  = min(control->data.xMax, max(control->data.xMin, p->time));
				key.value = min(control->data.yMax, max(control->data.yMin, p->value));
				keys.insert(key);
			}
			control->track->keys = keys;
		}

		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void CurveEditor_SetVertRange(HWND hWnd, float yMin, float yMax, bool lock)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		control->data.yMin = yMin;
		control->data.yMax = yMax;
		control->viewRangeVertLocked = lock;
		if (lock)
		{
			control->view.yMin = yMin;
			control->view.yMax = yMax;
		}

		if (control->track != NULL)
		{
			// Limit keys
			ParticleSystem::Emitter::Track::KeyMap keys;
			for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = control->track->keys.begin(); p != control->track->keys.end(); p++)
			{
				ParticleSystem::Emitter::Track::Key key;
				key.time  = min(control->data.xMax, max(control->data.xMin, p->time));
				key.value = min(control->data.yMax, max(control->data.yMin, p->value));
				keys.insert(key);
			}
			control->track->keys = keys;
		}

		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

const SelectedKeys* CurveEditor_GetSelection(HWND hWnd)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		return &control->selection;
	}
	return NULL;
}

int CurveEditor_GetKeyCount(HWND hWnd)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		return (int)control->track->keys.size();
	}
	return 0;
}

void CurveEditor_GetKey(HWND hWnd, int index, ParticleSystem::Emitter::Track::Key* key)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = control->track->keys.begin(); p != control->track->keys.end(); p++, index--)
		{
			if (index == 0)
			{
				*key = *p;
				break;
			}
		}
	}
}

int CurveEditor_AddKey(HWND hWnd, ParticleSystem::Emitter::Track::Key key, bool isBorder)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
        // Bound the values
		key.time  = min(control->data.xMax, max(control->data.xMin, key.time));
		key.value = min(control->data.yMax, max(control->data.yMin, key.value));

        // Add the key and adjust graph scale
        ParticleSystem::Emitter::Track::KeyMap::iterator p = control->track->keys.insert(key);
		CurveEditor_AutoScale(control);

        // Find the index of the added key
		int i;
		ParticleSystem::Emitter::Track::KeyMap::iterator q;
		for (i = 0, q = control->track->keys.begin(); q != control->track->keys.end(); q++, i++)
		{
			if (p == q)
			{
				return i;
			}
		}
	}
	return -1;
}

void CurveEditor_MoveSelection(HWND hWnd, float dTime, float dValue)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
        typedef multimap<ParticleSystem::Emitter::Track::Key,SelectedKey> SelectionChangeMap;

        int   numKeys     = (int)control->track->keys.size();
        int   draggingKey = control->draggingKey;
        float delta       = (control->data.xMax - control->data.xMin) / 10000.0f;

        ParticleSystem::Emitter::Track::Key first = *control->track->keys.begin();
        ParticleSystem::Emitter::Track::Key last  = *control->track->keys.rbegin();

        // First delete the entire selection (backwards because of the indices)
        for (SelectedKeys::const_reverse_iterator p = control->selection.rbegin(); p != control->selection.rend(); p++)
        {
            ParticleSystem::Emitter::Track::KeyMap::iterator q = control->track->keys.begin();
            for (int i = 0; i < p->index; i++, q++);
            control->track->keys.erase(q);
        }

        // Construct new selection
        SelectionChangeMap selection;
        for (SelectedKeys::const_iterator p = control->selection.begin(); p != control->selection.end(); p++)
        {
            // First and last key must remain so
            bool isBorder = (p->index == 0 || p->index == numKeys - 1);
            ParticleSystem::Emitter::Track::Key key;
            key.time  = p->time  + (isBorder ? 0 : dTime);
            key.value = p->value + dValue;
            if (!isBorder)
            {
                if (key.time <= first.time) key.time = first.time + delta;
                if (key.time >= last.time)  key.time = last.time  - delta;
            }
            selection.insert(make_pair(key, *p));
        }

        control->selection.clear();
        for (SelectionChangeMap::const_iterator p = selection.begin(); p != selection.end(); p++)
        {
            int i = CurveEditor_AddKey(hWnd, p->first);
            if (draggingKey == p->second.index)
            {
                control->draggingKey = i;
            }
            control->selection.insert(SelectedKey(i, p->second));
        }
        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void CurveEditor_DeleteSelection(HWND hWnd)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
        int numKeys = (int)control->track->keys.size();
        // Delete the entire selection (backwards because of the indices)
        for (SelectedKeys::const_reverse_iterator p = control->selection.rbegin(); p != control->selection.rend(); p++)
        {
            // Don't touch first and last keys
            if (p->index > 0 && p->index < numKeys - 1)
            {
                ParticleSystem::Emitter::Track::KeyMap::iterator q = control->track->keys.begin();
                for (int i = 0; i < p->index; i++, q++);
                control->track->keys.erase(q);
            }
        }
        control->selection.clear();
        Curve_OnKeysChanged(hWnd, control);
    	Curve_OnSelectionChanged(hWnd, control);
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void CurveEditor_SetTrack(HWND hWnd, ParticleSystem::Emitter::Track *track, bool editable)
{
	CurveControl* control = (CurveControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		control->track    = track;
		control->editable = editable;
        control->selection.clear();
		CurveEditor_AutoScale(control);
        Curve_OnSelectionChanged(hWnd, control);
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

bool CurveEditor_Initialize( HINSTANCE hInstance )
{
    // Register the UI class
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = CurveWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = NULL;
	wcx.hCursor       = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"CurveEditor";
	wcx.hIconSm       = NULL;
	
	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

    // Register clipboard format
    CF_TRACK_KEYS = RegisterClipboardFormat(L"Alamo_EmitterTrackKeys");

    return true;
}
