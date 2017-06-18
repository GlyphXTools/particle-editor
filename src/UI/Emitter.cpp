#include <windows.h>
#include "UI.h"
#include "utils.h"
using namespace std;

// The property pages
static const int  N_PROPERTY_PAGES = 3;
static const UINT PropertyPageLabels[N_PROPERTY_PAGES] = {IDS_PROPPAGE_BASIC, IDS_PROPPAGE_APPEARANCE, IDS_PROPPAGE_PHYSICS};
static const UINT PropertyPageIDs   [N_PROPERTY_PAGES] = {IDD_EMITTER_PROPS1, IDD_EMITTER_PROPS2, IDD_EMITTER_PROPS3};

// The shader blend modes
struct BLEND_MODE
{
    int  index;
    UINT idName;
};

static const int	    NUM_BLEND_MODES = 10;
static const BLEND_MODE BlendModes[NUM_BLEND_MODES] = {
    { 0, IDS_BLEND_NONE},
    { 1, IDS_BLEND_ADDITIVE},
    { 2, IDS_BLEND_TRANSPARENT},
    { 3, IDS_BLEND_INVERSE},
    { 4, IDS_BLEND_DEPTH_ADDITIVE},
    { 5, IDS_BLEND_DEPTH_TRANSPARENT},
    { 6, IDS_BLEND_DEPTH_INVERSE},
    { 7, IDS_BLEND_DIFFUSE_TRANSPARENT},
    {11, IDS_BLEND_BUMPMAP},
    {12, IDS_BLEND_DECAL_BUMPMAP}
};

// The ground interaction behavior
static const int  NUM_GROUND_BEHAVIORS = 4;
static const UINT GroundBehaviors[NUM_GROUND_BEHAVIORS] = {
    IDS_GROUND_BEHAVIOR_NONE,
    IDS_GROUND_BEHAVIOR_DISAPPEAR,
    IDS_GROUND_BEHAVIOR_BOUNCE,
    IDS_GROUND_BEHAVIOR_STICK
};

// Emit modes
static const int  NUM_EMIT_MODES = 4;
static const UINT EmitModes[NUM_EMIT_MODES] = {
    IDS_EMITMODE_DISABLED,
    IDS_EMITMODE_RANDOM_VERTEX,
    IDS_EMITMODE_RANDOM_MESH,
    IDS_EMITMODE_EVERY_VERTEX
};

static void NotifyParent(HWND hWnd, UINT code)
{
	NMHDR hdr;
	hdr.code     = code;
	hdr.hwndFrom = hWnd;
	hdr.idFrom   = GetDlgCtrlID(hWnd);
	SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr );
}

struct EmitterPropsControl
{
	ParticleSystem::Emitter* emitter;
    bool                     allowNotify;
	HWND				  	 hTabs;
	HWND					 hPages[N_PROPERTY_PAGES];
};

static void LoadTexture(HWND hWnd, string& texture)
{
	// Query for the  file
	char filename[MAX_PATH];
	filename[0] = '\0';

	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = hWnd;
	ofn.hInstance    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
	ofn.lpstrFilter  = "Texture files (*.tga;*.dds)\0*.TGA;*.DDS\0All files (*.*)\0*.*\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile    = filename;
	ofn.nMaxFile     = MAX_PATH;
	ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileNameA(&ofn) != 0)
	{
        char* sep = strrchr(filename, '\\');
        texture = (sep != NULL) ? sep + 1 : filename;
	}
}

static void EnableControls(EmitterPropsControl* control)
{
	const ParticleSystem::Emitter* emitter = control->emitter;

	HWND hPage1 = control->hPages[0];
	HWND hPage2 = control->hPages[1];
	HWND hPage3 = control->hPages[2];

	//
	// First page
	//

    const bool isDeathEmitter = (emitter->parent != NULL && emitter->parent->spawnOnDeath == emitter->index);
	const bool isLifeEmitter = (emitter->parent != NULL && emitter->parent->spawnDuringLife == emitter->index);
	const bool isChildEmitter = isDeathEmitter || isLifeEmitter;

	// Timing
	EnableWindow(GetDlgItem(hPage1, IDC_SPINNER28), !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage1, IDC_SPINNER27), !emitter->isWeatherParticle);
	// Generation
    EnableWindow(GetDlgItem(hPage1, IDC_RADIO1),   !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_RADIO2),   !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_RADIO3),   !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC10), !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC11), !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC13), !isDeathEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC17), !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC18), !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC19), !isLifeEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC27), !isChildEmitter);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER6),  !isChildEmitter && !emitter->isWeatherParticle &&  emitter->useBursts);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER3),  !isChildEmitter && !emitter->isWeatherParticle &&  emitter->useBursts && emitter->nBursts != 1);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER10), !isLifeEmitter  && !emitter->isWeatherParticle &&  emitter->useBursts);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER7),  !isDeathEmitter && !emitter->isWeatherParticle && !emitter->useBursts);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER15), !isChildEmitter &&  emitter->isWeatherParticle);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER35), !isChildEmitter &&  emitter->isWeatherParticle);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER31), !isChildEmitter &&  emitter->isWeatherParticle);
	// Connection
    EnableWindow(GetDlgItem(hPage1, IDC_CHECK2),    !emitter->isWeatherParticle);
    EnableWindow(GetDlgItem(hPage1, IDC_COMBO1),    !emitter->isWeatherParticle);
    EnableWindow(GetDlgItem(hPage1, IDC_STATIC25),  !emitter->isWeatherParticle && emitter->emitFromMesh != ParticleSystem::EMIT_DISABLE);
    EnableWindow(GetDlgItem(hPage1, IDC_SPINNER8),  !emitter->isWeatherParticle && emitter->emitFromMesh != ParticleSystem::EMIT_DISABLE);

	//
	// Second page
	//

	// Random color addition
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER19), true);
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER24), !emitter->doColorAddGrayscale);
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER25), !emitter->doColorAddGrayscale);
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER26), !emitter->doColorAddGrayscale);
	
	// Tail
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER34), emitter->hasTail);

	// Rotation
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER16), emitter->randomRotation);
	EnableWindow(GetDlgItem(hPage2, IDC_SPINNER17), emitter->randomRotation);

    // Rendering
    bool forceFace = (emitter->blendMode == ParticleSystem::BLEND_BUMP);
    EnableWindow(GetDlgItem(hPage2, IDC_CHECK16), !forceFace);

    //
	// Third page
	//

	// Position
	EnableWindow(GetDlgItem(hPage3, IDC_PARAMS1), !emitter->isWeatherParticle);

    // Speed
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER18), !emitter->isWeatherParticle);

    // Acceleration
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER12), !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER22), !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER23), !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER4),  !emitter->isWeatherParticle);
    EnableWindow(GetDlgItem(hPage3, IDC_SPINNER9),  !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage3, IDC_CHECK9),    !emitter->isWeatherParticle);

	// Ground interaction
	EnableWindow(GetDlgItem(hPage3, IDC_COMBO2),    !emitter->isWeatherParticle);
	EnableWindow(GetDlgItem(hPage3, IDC_SPINNER21), !emitter->isWeatherParticle && emitter->groundBehavior == 2);
}

static INT_PTR WINAPI DlgEmitterPropsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EmitterPropsControl* control = (EmitterPropsControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			control = (EmitterPropsControl*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);

			SPINNER_INFO si;
			si.Mask        = SPIF_ALL;

			// Positive-only floats
			si.IsFloat     = true;
			si.f.MinValue  = 0.0f;
			si.f.MaxValue  = FLT_MAX;
			si.f.Increment = 0.1f;
			si.f.Value     = 0.0f;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER1), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER5), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER27), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER28), &si);

            // Burst delay should not be 0
            si.f.MinValue  = 0.01f;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER3), &si);

			// Unsigned ints (0 or higher)
			si.IsFloat     = false;
			si.i.MaxValue  = INT_MAX;
			si.i.Increment = 1;
			si.i.MinValue  = 0;
			si.i.Value     = 0;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER6), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER10), &si);

			// Unsigned ints (1 or higher)
			si.i.MinValue  = 1;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER2), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER7), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER15), &si);

			// Percentages
			si.i.MinValue  = 0;
			si.i.MaxValue  = 100;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER13), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER14), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER18), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER19), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER24), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER25), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER26), &si);

			// Degrees
			si.i.MinValue  = -180;
			si.i.MaxValue  =  180;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER16), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER17), &si);

			// Any floats
			si.IsFloat     = true;
			si.f.MinValue  = -FLT_MAX;
			si.f.MaxValue  =  FLT_MAX;
			si.f.Increment = 0.1f;
			si.f.Value     = 0.0f;
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER4),  &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER9),  &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER8),  &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER11), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER12), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER21), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER22), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER23), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER31), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER34), &si);
			Spinner_SetInfo( GetDlgItem(hWnd, IDC_SPINNER35), &si);

			HWND hBlendMode = GetDlgItem(hWnd, IDC_COMBO3);
			if (hBlendMode != NULL)
			{
				for (int i = 0; i < NUM_BLEND_MODES; i++)
				{
                    const wstring name = LoadString(BlendModes[i].idName);
                    SendMessage(hBlendMode, CB_ADDSTRING, i, (LPARAM)name.c_str());
				}
				SendMessage(hBlendMode, CB_SETCURSEL, 0, 0);
			}

			HWND hBehavior = GetDlgItem(hWnd, IDC_COMBO2);
			if (hBehavior != NULL)
			{
				for (int i = 0; i < NUM_GROUND_BEHAVIORS; i++)
				{
                    const wstring name = LoadString(GroundBehaviors[i]);
                    SendMessage(hBehavior, CB_ADDSTRING, i, (LPARAM)name.c_str());
				}
				SendMessage(hBehavior, CB_SETCURSEL, 0, 0);
			}

            HWND hEmitModes = GetDlgItem(hWnd, IDC_COMBO1);
			if (hEmitModes != NULL)
			{
				for (int i = 0; i < NUM_EMIT_MODES; i++)
				{
                    const wstring name = LoadString(EmitModes[i]);
                    SendMessage(hEmitModes, CB_ADDSTRING, i, (LPARAM)name.c_str());
				}
				SendMessage(hEmitModes, CB_SETCURSEL, 0, 0);
			}
            break;
		}
		
		case WM_COMMAND:
			if (control != NULL && lParam != 0 && control->emitter != NULL)
			{
				ParticleSystem::Emitter& emitter = *control->emitter;

				UINT uCtrlID = LOWORD(wParam);
				HWND hCtrl   = (HWND)lParam;
				switch (HIWORD(wParam))
				{
					case BN_CLICKED:
					{
						bool enable = true;
						switch (uCtrlID)
						{
							case IDC_RADIO1:
							case IDC_RADIO2:
							case IDC_RADIO3: 
								emitter.useBursts         = GetUIBool(hWnd, IDC_RADIO1);
								emitter.isWeatherParticle = GetUIBool(hWnd, IDC_RADIO3); 
								break;

                            case IDC_CHECK2:  emitter.linkToSystem            = GetUIBool(hWnd, IDC_CHECK2); break;
							case IDC_CHECK7:  emitter.doColorAddGrayscale     = GetUIBool(hWnd, uCtrlID);    break;
							case IDC_CHECK12: emitter.hasTail                 = GetUIBool(hWnd, uCtrlID);    break;
							case IDC_CHECK5:  emitter.randomRotationDirection = GetUIBool(hWnd, uCtrlID);  break;
							case IDC_CHECK15:
								emitter.randomRotation = GetUIBool(hWnd, uCtrlID);
								EnableWindow(GetDlgItem(hWnd, IDC_SPINNER16), emitter.randomRotation);
								EnableWindow(GetDlgItem(hWnd, IDC_SPINNER17), emitter.randomRotation);
								break;

							case IDC_CHECK16: emitter.isWorldOriented         = !GetUIBool(hWnd, uCtrlID); break;
							case IDC_CHECK10: emitter.isHeatParticle          = GetUIBool(hWnd, uCtrlID);  break;
							case IDC_CHECK14: emitter.noDepthTest             = GetUIBool(hWnd, uCtrlID);  break;
							case IDC_CHECK8:  emitter.affectedByWind          = GetUIBool(hWnd, uCtrlID);  break;
							case IDC_CHECK9:  emitter.objectSpaceAcceleration = GetUIBool(hWnd, uCtrlID);  break;
							
							// Load textures
							case IDC_BUTTON1:
                                LoadTexture(hWnd, emitter.colorTexture);
                                SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), AnsiToWide(emitter.colorTexture).c_str());
                                break;

							case IDC_BUTTON2:
                                LoadTexture(hWnd, emitter.normalTexture);
                                SetWindowText(GetDlgItem(hWnd, IDC_EDIT3), AnsiToWide(emitter.normalTexture).c_str());
                                break;

							default:
								enable = false;
						}

						if (enable)
						{
							EnableControls(control);
						}

                        if (control->allowNotify)
                        {
    						NotifyParent(hWnd, EP_CHANGE);
                        }
						break;
					}

					case EN_CHANGE:
						switch (uCtrlID)
						{
                        case IDC_EDIT1: emitter.name          = WideToAnsi(GetWindowStr(GetDlgItem(hWnd, uCtrlID))); break;
						case IDC_EDIT2: emitter.colorTexture  = WideToAnsi(GetWindowStr(GetDlgItem(hWnd, uCtrlID))); break;
						case IDC_EDIT3: emitter.normalTexture = WideToAnsi(GetWindowStr(GetDlgItem(hWnd, uCtrlID))); break;
						}

                        if (control->allowNotify)
                        {
						    NotifyParent(hWnd, EP_CHANGE);
                        }
						break;

					case SN_CHANGE:
						switch (uCtrlID)
						{
						case IDC_SPINNER1:  emitter.initialDelay = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER28: emitter.skipTime     = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER27: emitter.freezeTime           = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER6:  emitter.nBursts              = GetUIInteger(hWnd, uCtrlID); EnableControls(control); break;
						case IDC_SPINNER3:  emitter.burstDelay           = GetUIFloat  (hWnd, uCtrlID); break;
						case IDC_SPINNER10: emitter.nParticlesPerBurst   = GetUIInteger(hWnd, uCtrlID); break;
						case IDC_SPINNER7:  if (!emitter.isWeatherParticle) SetUIInteger(hWnd, IDC_SPINNER15, emitter.nParticlesPerSecond = GetUIInteger(hWnd, uCtrlID)); break;
						case IDC_SPINNER15: if ( emitter.isWeatherParticle) SetUIInteger(hWnd, IDC_SPINNER7,  emitter.nParticlesPerSecond = GetUIInteger(hWnd, uCtrlID)); break;
						case IDC_SPINNER5:  emitter.lifetime             = GetUIFloat  (hWnd, uCtrlID); break;
						case IDC_SPINNER14: emitter.randomLifetimePerc   = (100 - GetUIInteger(hWnd, uCtrlID)) / 100.0f; break;
						case IDC_SPINNER18: emitter.parentLinkStrength   = GetUIInteger(hWnd, uCtrlID) / 100.0f; break;
						case IDC_SPINNER35: emitter.weatherCubeDistance  = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER31: emitter.weatherCubeSize      = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER2:  emitter.textureSize          = GetUIInteger(hWnd, uCtrlID); break;
						case IDC_SPINNER13: emitter.randomScalePerc      = (100 - GetUIInteger(hWnd, uCtrlID)) / 100.0f; break;
						case IDC_SPINNER19: emitter.randomColors[0]      = GetUIInteger(hWnd, uCtrlID) / 100.0f; break;
						case IDC_SPINNER24: emitter.randomColors[1]		 = GetUIInteger(hWnd, uCtrlID) / 100.0f; break;
						case IDC_SPINNER25: emitter.randomColors[2]		 = GetUIInteger(hWnd, uCtrlID) / 100.0f; break;
						case IDC_SPINNER26: emitter.randomColors[3]		 = GetUIInteger(hWnd, uCtrlID) / 100.0f; break;
						case IDC_SPINNER34: emitter.tailSize		     = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER16: emitter.randomRotationAverage  = GetUIInteger(hWnd, uCtrlID) / 360.0f; break;
						case IDC_SPINNER17: emitter.randomRotationVariance = GetUIInteger(hWnd, uCtrlID) / 360.0f; break;
						case IDC_SPINNER11: emitter.inwardSpeed          = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER12: emitter.acceleration[0]      = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER22: emitter.acceleration[1]      = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER23: emitter.acceleration[2]      = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER4:  emitter.gravity              = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER9:  emitter.inwardAcceleration   = GetUIFloat(hWnd, uCtrlID); break;
						case IDC_SPINNER21: emitter.bounciness           = GetUIFloat(hWnd, uCtrlID); break;
						}
                        
                        if (control->allowNotify)
                        {
    						NotifyParent(hWnd, EP_CHANGE);
                        }
						break;

					case CBN_SELCHANGE:
						switch (uCtrlID)
						{
                            case IDC_COMBO3:
                            {
                                HWND hFaceCamera  = GetDlgItem(hWnd, IDC_CHECK16);
                                emitter.blendMode = BlendModes[(int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0)].index;
                                bool forceFace = (emitter.blendMode == ParticleSystem::BLEND_BUMP);
                                EnableWindow(hFaceCamera, !forceFace);
                                if (forceFace)
                                {
                                    CheckDlgButton(hWnd, IDC_CHECK16, BST_CHECKED);
                                    emitter.isWorldOriented = false;
                                }
                                break;
                            }

						    case IDC_COMBO2:
							    emitter.groundBehavior = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
                                EnableWindow(GetDlgItem(hWnd, IDC_SPINNER21), emitter.groundBehavior == ParticleSystem::GROUND_BOUNCE);
							    break;

                            case IDC_COMBO1:
                                emitter.emitFromMesh = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
                                EnableWindow(GetDlgItem(hWnd, IDC_SPINNER8), emitter.emitFromMesh != ParticleSystem::EMIT_DISABLE);
							    break;
                        }
                        
                        if (control->allowNotify)
                        {
						    NotifyParent(hWnd, EP_CHANGE);
                        }
						break;
				}
			}
			break;

		case WM_NOTIFY:
			if (control != NULL && lParam != 0 && control->emitter != NULL)
			{
				ParticleSystem::Emitter& emitter = *control->emitter;
				NMHDR* hdr = (NMHDR*)lParam;
				switch (hdr->code)
				{
					case RP_CHANGE:
						// A RandomParameter control has changed
                        if (control->allowNotify)
                        {
    						NotifyParent(hWnd, EP_CHANGE);
                        }
						break;
				}
			}
			break;
	}
	return FALSE;
}

static EmitterPropsControl* CreateEmitterPropsControl(HWND hOwner, HINSTANCE hInstance)
{
	EmitterPropsControl* control = new EmitterPropsControl;
	if (control != NULL)
	{
		control->emitter     = NULL;
        control->allowNotify = true;

		//
		// Create the property tab window
		//
		if ((control->hTabs = CreateWindowEx(WS_EX_CONTROLPARENT, WC_TABCONTROL, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | TCS_FOCUSNEVER | WS_TABSTOP,
			4, 4, 286, 514, hOwner, NULL, hInstance, NULL)) == NULL)
		{
			delete control;
			return NULL;
		}

		for (int i = 0; i < N_PROPERTY_PAGES; i++)
		{
            const wstring title = LoadString(PropertyPageLabels[i]);
			TCITEM item;
			item.mask    = TCIF_TEXT;
            item.pszText = (LPWSTR)title.c_str();
			SendMessage(control->hTabs, TCM_INSERTITEM, i, (LPARAM)&item);
		}

		//
		// Create the property windows
		//
		for (int i = 0; i < N_PROPERTY_PAGES; i++)
		{
			if ((control->hPages[i] = CreateDialogParam(hInstance, MAKEINTRESOURCE(PropertyPageIDs[i]), control->hTabs, DlgEmitterPropsProc, (LPARAM)control)) == NULL)
			{
				for (int j = 0; j < i; j++)
				{
					DestroyWindow(control->hPages[j]);
				}
				DestroyWindow(control->hTabs);
				delete control;
				return NULL;
			}
		}
		ShowWindow(control->hPages[0], SW_SHOW);
	}
	return control;
}

static LRESULT CALLBACK EmitterPropsWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EmitterPropsControl* control = (EmitterPropsControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			control = CreateEmitterPropsControl(hWnd, pcs->hInstance);
			if (control == NULL)
			{
				return FALSE;
			}
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);
			break;
		}

		case WM_DESTROY:
			for (int i = 0; i < N_PROPERTY_PAGES; i++)
			{
				DestroyWindow(control->hPages[i]);
			}
			DestroyWindow(control->hTabs);
			break;

		case WM_NOTIFY:
		{
			NMHDR* hdr = (NMHDR*)lParam;
			switch (hdr->code)
			{
				case TCN_SELCHANGING:
					ShowWindow(control->hPages[TabCtrl_GetCurSel(hdr->hwndFrom)], SW_HIDE);
					break;

				case TCN_SELCHANGE:
					ShowWindow(control->hPages[TabCtrl_GetCurSel(hdr->hwndFrom)], SW_SHOW);
					break;

				default:
					// Pass notification on
					SendMessage(GetParent(hWnd), WM_NOTIFY, wParam, lParam);
					break;
			}
			break;
		}

		case WM_SIZE:
		{
			RECT tabs;
			MoveWindow(control->hTabs, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			GetClientRect(control->hTabs, &tabs);
			TabCtrl_AdjustRect(control->hTabs, FALSE, &tabs);
			for (int i = 0; i < N_PROPERTY_PAGES; i++)
			{
				MoveWindow(control->hPages[i], 4 + tabs.left, tabs.top, tabs.right - tabs.left - 4, tabs.bottom - tabs.top, TRUE);
			}
			break;
		}

		case WM_SETFONT:
			SendMessage(control->hTabs, WM_SETFONT, wParam, lParam);
			break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool EmitterProps_Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = EmitterPropsWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = NULL;
	wcx.hCursor       = NULL;
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"EmitterProps";
	wcx.hIconSm       = NULL;
	
	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

	return true;
}

void EmitterProps_SetEmitter(HWND hWnd, ParticleSystem::Emitter* emitter)
{
	EmitterPropsControl* control = (EmitterPropsControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
		control->emitter = emitter;
        control->allowNotify = false;

		HWND hPage1 = control->hPages[0];
		HWND hPage2 = control->hPages[1];
		HWND hPage3 = control->hPages[2];

		//
		// Fill first page
		//

		// Name
		SetDlgItemTextA(hPage1, IDC_EDIT1, emitter->name.c_str());
		// Timing
		SetUIFloat(hPage1, IDC_SPINNER1,  emitter->initialDelay);
		SetUIFloat(hPage1, IDC_SPINNER28, emitter->skipTime);
		SetUIFloat(hPage1, IDC_SPINNER27, emitter->freezeTime);

		// Generation
		CheckDlgButton(hPage1, IDC_RADIO1, !emitter->isWeatherParticle &&  emitter->useBursts ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hPage1, IDC_RADIO2, !emitter->isWeatherParticle && !emitter->useBursts ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hPage1, IDC_RADIO3,  emitter->isWeatherParticle ? BST_CHECKED : BST_UNCHECKED);
		SetUIInteger(hPage1, IDC_SPINNER6,  emitter->nBursts);

		SetUIFloat  (hPage1, IDC_SPINNER3,  emitter->burstDelay);
		SetUIInteger(hPage1, IDC_SPINNER10, emitter->nParticlesPerBurst);
		SetUIInteger(hPage1, IDC_SPINNER7,  emitter->nParticlesPerSecond);
		SetUIInteger(hPage1, IDC_SPINNER15, emitter->nParticlesPerSecond);
		SetUIFloat  (hPage1, IDC_SPINNER5,  emitter->lifetime);
		SetUIInteger(hPage1, IDC_SPINNER14, 100 - (int)(emitter->randomLifetimePerc * 100) );
		// Connection
        SendMessage(GetDlgItem(hPage1, IDC_COMBO1), CB_SETCURSEL, emitter->emitFromMesh, 0);
        SetUIBool (hPage1, IDC_CHECK2,    emitter->linkToSystem);
		SetUIFloat(hPage1, IDC_SPINNER8,  emitter->emitFromMeshOffset);
		// Weather
		SetUIFloat(hPage1, IDC_SPINNER35, emitter->weatherCubeDistance);
		SetUIFloat(hPage1, IDC_SPINNER31, emitter->weatherCubeSize);

		//
		// Fill second page
		//

		// Textures
		SetDlgItemTextA(hPage2, IDC_EDIT2, emitter->colorTexture.c_str());
		SetDlgItemTextA(hPage2, IDC_EDIT3, emitter->normalTexture.c_str());
		SetUIInteger(hPage2, IDC_SPINNER2,  emitter->textureSize );
		SetUIInteger(hPage2, IDC_SPINNER13, 100 - (int)(emitter->randomScalePerc * 100) );
		
		// Random color addition
		SetUIInteger(hPage2, IDC_SPINNER19, (int)(emitter->randomColors[0] * 100) );
		SetUIInteger(hPage2, IDC_SPINNER24, (int)(emitter->randomColors[1] * 100) );
		SetUIInteger(hPage2, IDC_SPINNER25, (int)(emitter->randomColors[2] * 100) );
		SetUIInteger(hPage2, IDC_SPINNER26, (int)(emitter->randomColors[3] * 100) );
		SetUIBool(hPage2, IDC_CHECK7, emitter->doColorAddGrayscale);
		
		// Tail
		SetUIBool (hPage2, IDC_CHECK12,   emitter->hasTail);
		SetUIFloat(hPage2, IDC_SPINNER34, emitter->tailSize);

		// Rotation
		SetUIBool   (hPage2, IDC_CHECK5,    emitter->randomRotationDirection);
		SetUIBool   (hPage2, IDC_CHECK15,   emitter->randomRotation);
		SetUIInteger(hPage2, IDC_SPINNER16, (int)(emitter->randomRotationAverage  * 360) );
		SetUIInteger(hPage2, IDC_SPINNER17, (int)(emitter->randomRotationVariance * 360) );

		// Rendering
		SetUIBool   (hPage2, IDC_CHECK16, !emitter->isWorldOriented);
		SetUIBool   (hPage2, IDC_CHECK10,  emitter->isHeatParticle);
		SetUIBool   (hPage2, IDC_CHECK14,  emitter->noDepthTest);
        for (int i = 0; i < NUM_BLEND_MODES; i++)
        {
            if (BlendModes[i].index == emitter->blendMode)
            {
		        SendMessage(GetDlgItem(hPage2, IDC_COMBO3), CB_SETCURSEL, i, 0);
                break;
            }
        }

		//
		// Fill third page
		//

		// Initial position
		RandomParam_SetGroup(GetDlgItem(hPage3, IDC_PARAMS1), &emitter->groups[ParticleSystem::GROUP_POSITION]);

		// Initial speed
		RandomParam_SetGroup(GetDlgItem(hPage3, IDC_PARAMS2), &emitter->groups[ParticleSystem::GROUP_SPEED]);
		SetUIFloat(hPage3, IDC_SPINNER11, emitter->inwardSpeed);
		SetUIInteger(hPage3, IDC_SPINNER18, (int)(emitter->parentLinkStrength * 100));

		// Acceleration
		SetUIFloat(hPage3, IDC_SPINNER12, emitter->acceleration[0]);
		SetUIFloat(hPage3, IDC_SPINNER22, emitter->acceleration[1]);
		SetUIFloat(hPage3, IDC_SPINNER23, emitter->acceleration[2]);
		SetUIFloat(hPage3, IDC_SPINNER4,  emitter->gravity );
		SetUIFloat(hPage3, IDC_SPINNER9,  emitter->inwardAcceleration );
		SetUIBool (hPage3, IDC_CHECK8,    emitter->affectedByWind);
		SetUIBool (hPage3, IDC_CHECK9,    emitter->objectSpaceAcceleration);

		// Ground interaction
		SendMessage(GetDlgItem(hPage3, IDC_COMBO2), CB_SETCURSEL, emitter->groundBehavior, 0);
		SetUIFloat(hPage3, IDC_SPINNER21, emitter->bounciness );

		EnableControls(control);
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);

        control->allowNotify = true;
    }
}