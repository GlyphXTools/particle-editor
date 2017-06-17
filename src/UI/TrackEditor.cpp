#include <cfloat>
#include "UI.h"
using namespace std;

struct TrackControl
{
	HWND hWnd;
	HWND hDialog;
    bool editable;
	int  iTrack;
    ParticleSystem::Emitter::Track*     trackContents;
    ParticleSystem::Emitter::Track**    tracks;
    ParticleSystem::Emitter::Track::Key selectionStart;
};

static void NotifyParent(TrackControl* control, UINT code)
{
	NMTECHANGE nmtec;
	nmtec.hdr.code     = code;
	nmtec.hdr.hwndFrom = control->hWnd;
	nmtec.hdr.idFrom   = GetDlgCtrlID(control->hWnd);
    nmtec.track = control->iTrack;
	SendMessage(GetParent(control->hWnd), WM_NOTIFY, (WPARAM)nmtec.hdr.idFrom, (LPARAM)&nmtec );
}

static void RefreshInterplation(TrackControl* control)
{
    HWND hToolbar = GetDlgItem(control->hDialog, IDC_TOOLBAR1);
    const ParticleSystem::Emitter::Track* track = control->tracks[control->iTrack];
    SendMessage(hToolbar, TB_CHECKBUTTON, 3, track->interpolation == ParticleSystem::Emitter::Track::IT_LINEAR);
    SendMessage(hToolbar, TB_CHECKBUTTON, 4, track->interpolation == ParticleSystem::Emitter::Track::IT_SMOOTH);
    SendMessage(hToolbar, TB_CHECKBUTTON, 5, track->interpolation == ParticleSystem::Emitter::Track::IT_STEP);
}

INT_PTR CALLBACK TrackDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TrackControl* control = (TrackControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			control = (TrackControl*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);

			//
			// Set spinner properties
			//
			SPINNER_INFO si;
			si.IsFloat     = true;
			si.Mask        = SPIF_ALL;
			si.f.Increment =   0.1f;
			si.f.MinValue  =   0.0f;
			si.f.MaxValue  = 100.0f;
			si.f.Value     =   0.0f;
			Spinner_SetInfo(GetDlgItem(hWnd, IDC_SPINNER1), &si);

			HWND hEditor = GetDlgItem(hWnd, IDC_EDITOR1);
			CurveEditor_SetHorzRange(hEditor, 0.0f, 100.0f, true);

			switch (control->iTrack)
			{
				case 0: case 1: case 2: case 3: 
					// Red, Green, Blue, Alpha
					si.f.MinValue = 0.0f;
					si.f.MaxValue = 1.0f;
					CurveEditor_SetVertRange(hEditor, 0.0f, 1.0f, true);
					break;

				case 4: case 5:
					// Scale, Index
					si.f.MinValue = 0.0f;
					si.f.MaxValue = FLT_MAX;
					CurveEditor_SetVertRange(hEditor, 0.0f, FLT_MAX);
					break;

				case 6:
					// Rotation speed
					si.f.MinValue = -FLT_MAX;
					si.f.MaxValue =  FLT_MAX;
					CurveEditor_SetVertRange(hEditor, -FLT_MAX, FLT_MAX);
					break;
			}

			Spinner_SetInfo(GetDlgItem(hWnd, IDC_SPINNER2), &si);

			//
			// Fill "lock to" combobox
			//
			HWND hCombo = GetDlgItem(hWnd, IDC_COMBO1);
			static const UINT texts[7][5] = {
                {IDS_LABEL_TRACK_NONE, 0},
                {IDS_LABEL_TRACK_NONE, IDS_LABEL_TRACK_RED, 0},
                {IDS_LABEL_TRACK_NONE, IDS_LABEL_TRACK_RED, IDS_LABEL_TRACK_GREEN, 0},
                {IDS_LABEL_TRACK_NONE, IDS_LABEL_TRACK_RED, IDS_LABEL_TRACK_GREEN, IDS_LABEL_TRACK_BLUE, 0},
                {IDS_LABEL_TRACK_NONE, 0},
                {IDS_LABEL_TRACK_NONE, 0},
                {IDS_LABEL_TRACK_NONE, 0}
			};

			size_t i = 0;
			for (i = 0; texts[control->iTrack][i] != 0; i++)
			{
				const wstring text = LoadString(texts[control->iTrack][i]);
				SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)text.c_str());
			}
			SendMessage(hCombo, CB_SETCURSEL, 0, 0);
			if (i <= 1)
			{
				EnableWindow(hCombo, FALSE);
			}

			//
			// Initialize toolbar
			//
			HWND hToolbar = GetDlgItem(hWnd, IDC_TOOLBAR1);
			HINSTANCE hInstance = GetModuleHandle(NULL);
			HIMAGELIST hList = ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK, 0, 32);
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_SELECT)));
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_INSERT)));
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_LINEAR)));
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_SMOOTH)));
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_STEP)));
			ImageList_AddIcon(hList, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDITOR_DELETE)));

			SendMessage(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hList);
			TBBUTTON buttons[10] = {
				{0, 0, TBSTATE_ENABLED, BTNS_SEP},
				{0, 1, TBSTATE_ENABLED | TBSTATE_CHECKED, BTNS_CHECKGROUP},
				{1, 2, TBSTATE_ENABLED, BTNS_CHECKGROUP},
				{0, 0, TBSTATE_ENABLED, BTNS_SEP},
				{2, 3, TBSTATE_ENABLED | TBSTATE_CHECKED, BTNS_CHECKGROUP},
				{3, 4, TBSTATE_ENABLED, BTNS_CHECKGROUP},
				{4, 5, TBSTATE_ENABLED, BTNS_CHECKGROUP},
				{0, 0, TBSTATE_ENABLED, BTNS_SEP},
				{5, 6, 0, BTNS_BUTTON},
				{0, 0, TBSTATE_ENABLED, BTNS_SEP},
			};
			SendMessage(hToolbar, TB_ADDBUTTONS, 10, (LPARAM)buttons);

			break;
		}

		case WM_SIZE:
		{
			HWND hEditor = GetDlgItem(hWnd, IDC_EDITOR1);
			RECT rect;
			GetWindowRect(hEditor, &rect);
			POINT pos = {rect.left, rect.top};
			ScreenToClient(hWnd, &pos);
			MoveWindow(hEditor, 0, pos.y, LOWORD(lParam), HIWORD(lParam) - pos.y, TRUE);
			break;
		}

        case WM_COMMAND:
			if (lParam != NULL)
			{
				HWND hCtrl = (HWND)lParam;
				switch (HIWORD(wParam))
				{
					case BN_CLICKED:
						if (hCtrl == GetDlgItem(hWnd, IDC_TOOLBAR1))
						{
							// A toolbar button has been clicked
							HWND hEditor = GetDlgItem(hWnd, IDC_EDITOR1);
							switch (LOWORD(wParam))
							{
								case 1: CurveEditor_SetCursorMode(hEditor, CM_SELECT); break;
								case 2: CurveEditor_SetCursorMode(hEditor, CM_INSERT); break;
								case 3: CurveEditor_SetInterpolationType(hEditor, ParticleSystem::Emitter::Track::IT_LINEAR); NotifyParent(control, TE_CHANGE); break;
								case 4: CurveEditor_SetInterpolationType(hEditor, ParticleSystem::Emitter::Track::IT_SMOOTH); NotifyParent(control, TE_CHANGE); break;
								case 5: CurveEditor_SetInterpolationType(hEditor, ParticleSystem::Emitter::Track::IT_STEP);   NotifyParent(control, TE_CHANGE); break;
								case 6: CurveEditor_DeleteSelection(hEditor); NotifyParent(control, TE_CHANGE); break;
							}
						}
						break;

					case CBN_SELCHANGE:
					{
						// A combobox has changed
						int sel = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
						HWND hEditor = GetDlgItem(hWnd, IDC_EDITOR1);
                        control->tracks[control->iTrack] = &control->trackContents[(sel > 0) ? sel - 1 : control->iTrack];
                        control->editable = (sel == 0);
                        CurveEditor_SetTrack(hEditor, control->tracks[control->iTrack], control->editable);

                        HWND hToolbar = GetDlgItem(hWnd, IDC_TOOLBAR1);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 1, control->editable);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 2, control->editable);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 3, control->editable);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 4, control->editable);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 5, control->editable);
                        SendMessage(hToolbar, TB_ENABLEBUTTON, 6, control->editable && !CurveEditor_GetSelection(hEditor)->empty());
                        RefreshInterplation(control);

                        NotifyParent(control, TE_CHANGE);
						break;
					}

					case SN_CHANGE:
					{
						// A spinner has changed, move the selection
						HWND hEditor   = GetDlgItem(hWnd, IDC_EDITOR1);
                        HWND hSpinner1 = GetDlgItem(hWnd, IDC_SPINNER1);
                        HWND hSpinner2 = GetDlgItem(hWnd, IDC_SPINNER2);

						SPINNER_INFO si;
						si.Mask = SPIF_VALUE;
						Spinner_GetInfo(hSpinner1, &si);
                        float dTime = si.f.Value - control->selectionStart.time;
						Spinner_GetInfo(hSpinner2, &si);
                        float dValue = si.f.Value - control->selectionStart.value;
					    CurveEditor_MoveSelection(hEditor, dTime, dValue);

						NotifyParent(control, TE_CHANGE);
						break;
					}
				}
			}
			break;

		case WM_NOTIFY:
		{
			NMHDR* hdr = (NMHDR*)lParam;
			switch (hdr->code)
			{
				case TTN_GETDISPINFO:
				{
					// Toolbar wants tooltips
					NMTTDISPINFO* nmdi = (NMTTDISPINFO*)hdr;
					static const UINT tooltips[] =
					{
						IDS_TOOLTIP_KEYS_SELECT,
						IDS_TOOLTIP_KEYS_INSERT,
						IDS_TOOLTIP_INTERPOLATE_LINEAR,
						IDS_TOOLTIP_INTERPOLATE_SMOOTH,
						IDS_TOOLTIP_INTERPOLATE_STEP,
						IDS_TOOLTIP_KEYS_DELETE,
					};
                    nmdi->hinst    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
					nmdi->lpszText = MAKEINTRESOURCE(tooltips[hdr->idFrom - 1]);
					break;
				}

				case CEN_MODECHANGE:
				{
					// A different mode was selected
					NMCURVEEDITOR* nmce = (NMCURVEEDITOR*)lParam;
					HWND hToolbar = GetDlgItem(hWnd, IDC_TOOLBAR1);
					SendMessage(hToolbar, TB_CHECKBUTTON, 1, (nmce->mode == CM_SELECT));
					SendMessage(hToolbar, TB_CHECKBUTTON, 2, (nmce->mode == CM_INSERT));
					break;
				}

				case CEN_SELCHANGE:
				{
					// Different keys were selected
                    const SelectedKeys* selection = CurveEditor_GetSelection(hdr->hwndFrom);
                    if (selection != NULL)
                    {
	    				HWND hSpinnerTime  = GetDlgItem(hWnd, IDC_SPINNER1);
    					HWND hSpinnerValue = GetDlgItem(hWnd, IDC_SPINNER2);

                        // Calculate average over selection
                        control->selectionStart = ParticleSystem::Emitter::Track::Key(0,0);

                        bool editable = false;
                        if (!selection->empty())
                        {
					        int nKeys = CurveEditor_GetKeyCount(hdr->hwndFrom);
                            for (SelectedKeys::const_iterator p = selection->begin(); p != selection->end(); p++)
                            {
                                if (p->index != 0 && p->index != nKeys - 1)
                                {
                                    // The selection contains something else than the first or last key,
                                    // we can edit it
                                    editable = true;
                                }
                                ParticleSystem::Emitter::Track::Key key;
                                CurveEditor_GetKey(hdr->hwndFrom, p->index, &key);
                                control->selectionStart.time  += key.time;
                                control->selectionStart.value += key.value;
                            }
                            control->selectionStart.time  /= selection->size();
                            control->selectionStart.value /= selection->size();
				        }

                        SPINNER_INFO si;
					    si.IsFloat = true;
					    si.Mask    = SPIF_VALUE;
                        si.f.Value = control->selectionStart.time;  Spinner_SetInfo(hSpinnerTime, &si);
				        si.f.Value = control->selectionStart.value; Spinner_SetInfo(hSpinnerValue, &si);
				        EnableWindow(hSpinnerTime,  editable);
                        EnableWindow(hSpinnerValue, !selection->empty());

				        // Change delete key state
				        SendMessage(GetDlgItem(hWnd, IDC_TOOLBAR1), TB_ENABLEBUTTON, 6, editable && !selection->empty());
                    }
					break;
				}

                case CEN_KEYSCHANGED:
                {
                    // The position of the selected keys changed
                    const SelectedKeys* selection = CurveEditor_GetSelection(hdr->hwndFrom);
                    if (selection != NULL)
                    {
                        // Calculate average over selection
                        ParticleSystem::Emitter::Track::Key average(0,0);
                        for (SelectedKeys::const_iterator p = selection->begin(); p != selection->end(); p++)
                        {
                            ParticleSystem::Emitter::Track::Key key;
                            CurveEditor_GetKey(hdr->hwndFrom, p->index, &key);
                            average.time  += key.time;
                            average.value += key.value;
                        }

                        // Show average
	    				HWND hSpinnerTime  = GetDlgItem(hWnd, IDC_SPINNER1);
    					HWND hSpinnerValue = GetDlgItem(hWnd, IDC_SPINNER2);

                        SPINNER_INFO si;
					    si.IsFloat = true;
					    si.Mask    = SPIF_VALUE;
                        si.f.Value = average.time  / selection->size(); Spinner_SetInfo(hSpinnerTime, &si);
				        si.f.Value = average.value / selection->size(); Spinner_SetInfo(hSpinnerValue, &si);
    			        NotifyParent(control, TE_CHANGE);
                    }
                    break;
                }

				case CEN_KBINPUT:
				{
					NMCURVEEDITORKEY* nmcek = (NMCURVEEDITORKEY*)lParam;
					if (nmcek->uMsg == WM_KEYDOWN)
					{
						switch (nmcek->wParam)
						{
						case VK_DELETE:
							// Delete currently selected keys
							CurveEditor_DeleteSelection(hdr->hwndFrom);
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

LRESULT CALLBACK TrackWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TrackControl* control = (TrackControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			control = new TrackControl;
            control->trackContents = NULL;
            control->tracks        = NULL;
            control->editable = false;
			control->iTrack   = (LONG)(LONG_PTR)pcs->lpCreateParams;
			control->hWnd     = hWnd;
			control->hDialog  = CreateDialogParam(pcs->hInstance, MAKEINTRESOURCE(IDD_TRACK_EDITOR), hWnd, TrackDialogProc, (LPARAM)control);
			ShowWindow(control->hDialog, SW_SHOW);

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);
			break;
		}

		case WM_NOTIFY:
		{
			NMHDR* hdr = (NMHDR*)lParam;
			if (hdr->code == TE_CHANGE)
			{
				NotifyParent(control, TE_CHANGE);
			}
			break;
		}

		case WM_DESTROY:
			delete control;
			break;

		case WM_SIZE:
			MoveWindow(control->hDialog, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;

        case WM_SHOWWINDOW:
            if (wParam && control->tracks != NULL)
            {
                // Update interpolation state to possibly reflect shared state
                RefreshInterplation(control);
            }
            break;
    }
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void TrackEditor_SetTrack(HWND hWnd, ParticleSystem::Emitter::Track* trackContents, ParticleSystem::Emitter::Track** tracks)
{
	TrackControl* control = (TrackControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (control != NULL)
	{	
        HWND hCombo   = GetDlgItem(control->hDialog, IDC_COMBO1);
		HWND hEditor  = GetDlgItem(control->hDialog, IDC_EDITOR1);
		HWND hToolbar = GetDlgItem(control->hDialog, IDC_TOOLBAR1);
        control->trackContents = trackContents;
        control->tracks        = tracks;
        control->editable      = (tracks[control->iTrack] == &trackContents[control->iTrack]);

		CurveEditor_SetTrack(hEditor, tracks[control->iTrack], control->editable);

        if (!control->editable)
        {
            // Set the combo box to the shared track
            SendMessage(hCombo, CB_SETCURSEL, 1 + tracks[control->iTrack] - trackContents, 0);
        }

		UINT id = 0;
		switch (tracks[control->iTrack]->interpolation)
		{
			case ParticleSystem::Emitter::Track::IT_LINEAR:	id = 3; break;
			case ParticleSystem::Emitter::Track::IT_SMOOTH:	id = 4; break;
			case ParticleSystem::Emitter::Track::IT_STEP:	id = 5; break;
		}
        SendMessage(hToolbar, TB_ENABLEBUTTON, 1, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 2, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 3, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 4, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 5, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 6, false);
		SendMessage(hToolbar, TB_CHECKBUTTON, id, TRUE);
	}
}

void TrackEditor_EnableTrack(HWND hWnd, bool enabled)
{
	TrackControl* control = (TrackControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (control != NULL && control->editable != enabled)
	{
        HWND hEditor  = GetDlgItem(control->hDialog, IDC_EDITOR1);
		HWND hToolbar = GetDlgItem(control->hDialog, IDC_TOOLBAR1);

        control->editable = enabled;
        CurveEditor_SetTrack(hEditor, control->tracks[control->iTrack], control->editable);

        SendMessage(hToolbar, TB_ENABLEBUTTON, 1, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 2, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 3, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 4, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 5, control->editable);
        SendMessage(hToolbar, TB_ENABLEBUTTON, 6, false);
    }
}

bool TrackEditor_Initialize( HINSTANCE hInstance )
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = TrackWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = NULL;
	wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"TrackEditor";
	wcx.hIconSm       = NULL;
	
	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

	return true;
}
