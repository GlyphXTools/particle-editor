#include <cfloat>
#include "UI.h"
using namespace std;

struct RandomParamControl
{
	HWND hDialog;
	ParticleSystem::Emitter::Group* group;
};

static void NotifyParent(HWND hWnd, UINT code)
{
	NMHDR hdr;
	hdr.code     = code;
	hdr.hwndFrom = hWnd;
	hdr.idFrom   = GetDlgCtrlID(hWnd);
	SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr );
}

static void RandomParameters_ShowControls(HWND hWnd, int type)
{
	static const int visibleList1[] = {
		IDC_STATIC6,  IDC_STATIC7,  IDC_STATIC8,  IDC_STATIC9,
		IDC_SPINNER7, IDC_SPINNER8, IDC_SPINNER9,
		-1};
	static const int visibleList2[] = {
		IDC_STATIC1,  IDC_STATIC2,  IDC_STATIC3,  IDC_STATIC4,  IDC_STATIC5,
		IDC_SPINNER1, IDC_SPINNER2, IDC_SPINNER3, IDC_SPINNER4, IDC_SPINNER5, IDC_SPINNER6,
		-1};
	static const int visibleList3[] = {IDC_STATIC10, IDC_SPINNER10, -1};
	static const int visibleList4[] = {IDC_STATIC11, IDC_SPINNER11, IDC_CHECK1, -1};
	static const int visibleList5[] = {
		IDC_STATIC12,  IDC_STATIC13,
		IDC_SPINNER12, IDC_SPINNER13,
		IDC_CHECK2,
		-1};
	static const int* visibleList[] = {visibleList1, visibleList2, visibleList3, visibleList4, visibleList5};

	for (int i = 0; i < 5; i++)
	{
		int nCmdShow = (i == type) ? SW_SHOW : SW_HIDE;
		for (int j = 0; visibleList[i][j] != -1; j++)
		{
			ShowWindow(GetDlgItem(hWnd, visibleList[i][j]), nCmdShow);
		}
	}
}

static INT_PTR CALLBACK RandomParamDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RandomParamControl* control = (RandomParamControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			control = (RandomParamControl*)lParam;
			SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG)(LONG_PTR)control);

			SPINNER_INFO si;
			si.Mask        = SPIF_ALL;
			si.IsFloat     = true;
			si.f.MinValue  = -FLT_MAX;
			si.f.MaxValue  =  FLT_MAX;
			si.f.Increment = 0.1f;
			si.f.Value     = 0.0f;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER1), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER2), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER3), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER4), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER5), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER6), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER7), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER8), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER9), &si);

			si.f.MinValue  = 0;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER10), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER11), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER12), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER13), &si);

			// Fill type list
			HWND hList = GetDlgItem(hWnd, IDC_COMBO1);
			static const UINT listEntries[] = {
                IDS_PARAMTYPE_EXACT,
                IDS_PARAMTYPE_BOX,
                IDS_PARAMTYPE_CUBE,
                IDS_PARAMTYPE_SPHERE,
                IDS_PARAMTYPE_CYLINDER
            };

			for (int i = 0; i < 5; i++)
			{
                const wstring name = LoadString(listEntries[i]);
				SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)name.c_str());
			}

			SendMessage(hList, CB_SETCURSEL, 0, 0);
			RandomParameters_ShowControls(hWnd, 0);
			break;
		}

		case WM_COMMAND:
			if (control->group != NULL)
			{
				UINT uCtrlID = LOWORD(wParam);
				HWND hCtrl   = (HWND)lParam;

				switch (HIWORD(wParam))
				{
					case CBN_SELCHANGE:
						// "Type" list changed
						control->group->type = (int)SendMessage(GetDlgItem(hWnd, IDC_COMBO1), CB_GETCURSEL, 0, 0);
						RandomParameters_ShowControls(hWnd, control->group->type);
						NotifyParent(hWnd, RP_CHANGE);
						break;

					case BN_CLICKED:
					case SN_CHANGE:
						// A spinner or checkbox has changed
						switch (uCtrlID)
						{
							case IDC_SPINNER1:	control->group->minX = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER2:  control->group->minY = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER3:  control->group->minZ = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER4:  control->group->maxX = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER5:  control->group->maxY = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER6:  control->group->maxZ = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER7:  control->group->valX = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER8:  control->group->valY = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER9:  control->group->valZ = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER10: control->group->sideLength     = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER11: control->group->sphereRadius   = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER12: control->group->cylinderRadius = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_SPINNER13: control->group->cylinderHeight = GetUIFloat(hWnd, uCtrlID); break;
							case IDC_CHECK1:	control->group->sphereEdge   = (IsDlgButtonChecked(hWnd, uCtrlID) == BST_CHECKED); break;
							case IDC_CHECK2:	control->group->cylinderEdge = (IsDlgButtonChecked(hWnd, uCtrlID) == BST_CHECKED); break;
						}
						NotifyParent(hWnd, RP_CHANGE);
						break;
				}
			}
			break;

		case WM_ENABLE:
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO1), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER1), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER2), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER3), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER4), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER5), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER6), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER7), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER8), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER9), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER10), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER11), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER12), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_SPINNER13), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK1), wParam == TRUE);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK2), wParam == TRUE);
			break;
	}
	return FALSE;
}

static LRESULT CALLBACK RandomParamWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RandomParamControl* control = (RandomParamControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			control = new RandomParamControl;
			control->group = NULL;
			SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG)(LONG_PTR)control);

			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			if ((control->hDialog = CreateDialogParam(pcs->hInstance, MAKEINTRESOURCE(IDD_RANDOM_PARAMETERS), hWnd, RandomParamDlgProc, (LPARAM)control)) == NULL)
			{
				delete control;
				return false;
			}
			ShowWindow(control->hDialog, SW_SHOW);

			break;
		}

		case WM_ENABLE:
			EnableWindow(control->hDialog, wParam == TRUE);
			break;

		case WM_NOTIFY:
			// Pass notification on to parent
			SendMessage(GetParent(hWnd), WM_NOTIFY, wParam, lParam);
			break;

		case WM_SETFOCUS:
			SetFocus(control->hDialog);
			break;

		case WM_DESTROY:
		{
			DestroyWindow(control->hDialog);
			delete control;
			break;
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void RandomParam_SetGroup(HWND hWnd, ParticleSystem::Emitter::Group* group )
{
	RandomParamControl* control = (RandomParamControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWL_USERDATA);
	if (control != NULL)
	{
		HWND hDialog = control->hDialog;
		control->group = group;
		
		// Set type combobox
		int type = min(5, group->type) % 5; // Anything above 4 becomes 0
		SendMessage(GetDlgItem(hDialog, IDC_COMBO1), CB_SETCURSEL, type, 0);

		// Set floats
		SetUIFloat(hDialog, IDC_SPINNER1,  group->minX);
		SetUIFloat(hDialog, IDC_SPINNER2,  group->minY);
		SetUIFloat(hDialog, IDC_SPINNER3,  group->minZ);
		SetUIFloat(hDialog, IDC_SPINNER4,  group->maxX);
		SetUIFloat(hDialog, IDC_SPINNER5,  group->maxY);
		SetUIFloat(hDialog, IDC_SPINNER6,  group->maxZ);
		SetUIFloat(hDialog, IDC_SPINNER7,  group->valX);
		SetUIFloat(hDialog, IDC_SPINNER8,  group->valY);
		SetUIFloat(hDialog, IDC_SPINNER9,  group->valZ);
		SetUIFloat(hDialog, IDC_SPINNER10, group->sideLength);
		SetUIFloat(hDialog, IDC_SPINNER11, group->sphereRadius);
		SetUIFloat(hDialog, IDC_SPINNER12, group->cylinderRadius);
		SetUIFloat(hDialog, IDC_SPINNER13, group->cylinderHeight);

		// Set bools
		CheckDlgButton(hDialog, IDC_CHECK1, group->sphereEdge   != 0 ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDialog, IDC_CHECK2, group->cylinderEdge != 0 ? BST_CHECKED : BST_UNCHECKED);

		RandomParameters_ShowControls(hDialog, type);
	}
}

bool RandomParam_Initialize( HINSTANCE hInstance )
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = RandomParamWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = NULL;
	wcx.hCursor       = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"RandomParam";
	wcx.hIconSm       = NULL;
	
	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

	return true;
}
