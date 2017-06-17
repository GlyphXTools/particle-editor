#define _WIN32_WINNT 0x0501
#include <cmath>
#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cfloat>
#include <sstream>
#include <queue>

#include "exceptions.h"
#include "UI/UI.h"
#include "utils.h"
#include "engine.h"
#include "ParticleSystemInstance.h"
#include "Rescale.h"
#include "resource.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commdlg.h>
using namespace std;

static const int VERSION_MAJOR = 1;
static const int VERSION_MINOR = 5;

// Show up to this amount of files in the File menu
static const int NUM_HISTORY_ITEMS = 9;

static const int N_TRACKS          = 7;
static const int MIN_WINDOW_WIDTH  = 860;
static const int MIN_WINDOW_HEIGHT = 750;

//
// A class to measure the FPS
//
class FPSMeasurer
{
    static const int MAX_FRAMES = 32;

    float  m_frames[MAX_FRAMES];
    size_t m_iFrame;
    size_t m_nFrames;
    size_t m_lastFrame;
    size_t m_firstFrame;

public:
    float getFPS()
    {
        if (m_nFrames > 0)
        {
            float diff = (m_frames[m_lastFrame] - m_frames[m_firstFrame]);
            if (diff > 0.0f)
            {
                return m_nFrames / diff;
            }
        }
        return 0.0f;
    }

    void measure()
    {
        m_lastFrame = m_iFrame;
        m_frames[m_iFrame] = GetTickCount() / 1000.0f;
        m_nFrames   = min(m_nFrames + 1, MAX_FRAMES);
        m_iFrame    = (m_iFrame + 1) % MAX_FRAMES;
        if (m_iFrame == m_firstFrame)
        {
            m_firstFrame = (m_firstFrame + 1) % MAX_FRAMES;
        }
    }

    FPSMeasurer()
    {
        m_firstFrame = 0;
        m_lastFrame  = 0;
        m_iFrame     = 0;
        m_nFrames    = 0;
    }
};

class TextureManager : public ITextureManager
{
	typedef map<string,IDirect3DTexture9*> TextureMap;

	TextureMap			textures;
	string				basePath;
	IFileManager*		fileManager;
	IDirect3DTexture9*  pDefaultTexture;

	static IDirect3DTexture9* createTexture(IDirect3DDevice9* pDevice, IFile* file)
	{
		IDirect3DTexture9* pTexture = NULL;
		unsigned long size = file->size();
		char* data = new char[ size ];
		file->read( (void*)data, size );
		if (D3DXCreateTextureFromFileInMemory( pDevice, (void*)data, size, &pTexture ) != D3D_OK)
		{
            delete[] data;
			return NULL;
		}
		delete[] data;
		return pTexture;
	}

	IDirect3DTexture9* load(IDirect3DDevice9* pDevice, const string& filename)
	{
		TextureMap::iterator p = textures.find(filename);
		if (p != textures.end())
		{
			// Texture has already been loaded
			return p->second;
		}

		IFile* file = fileManager->getFile( basePath + filename );
		if (file == NULL)
		{
			return NULL;
		}
		return createTexture(pDevice, file);
	}

public:
	IDirect3DTexture9* getTexture(IDirect3DDevice9* pDevice, string filename)
	{
		size_t pos;
		transform(filename.begin(), filename.end(), filename.begin(), toupper);
		
		IDirect3DTexture9* pTexture = NULL;

		// See if the file exists as specified
		try
		{
			IFile* file = new PhysicalFile(AnsiToWide(filename));
			pTexture = createTexture(pDevice, file);
			delete file;
		}
		catch (FileNotFoundException&)
		{
		}

		if (pTexture == NULL)
		{
			// Use the part after the (back)slash, if any
            if (filename.find_first_of(":") != string::npos && (pos = filename.find_last_of("\\/")) != string::npos)
			{
				filename = filename.substr(pos + 1);
			}

			pTexture = load(pDevice, filename);
		}

		if (pTexture == NULL)
		{
			string name = filename;
			if ((pos = filename.rfind('.')) != string::npos)
			{
				name = name.substr(0, pos) + ".DDS";
			}
		
			pTexture = load(pDevice, name);
			if (pTexture == NULL)
			{
				// Load and return default placeholder texture
				if (pDefaultTexture == NULL)
				{
					D3DXCreateTextureFromResource( pDevice, GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_MISSING), &pDefaultTexture );
				}

				if (pDefaultTexture != NULL)
				{
					pTexture = pDefaultTexture;
					pDefaultTexture->AddRef();
				}
			}
		}

		if (pTexture != NULL)
		{
			textures.insert(make_pair(filename, pTexture));
			pTexture->AddRef();
		}

		return pTexture;
	}

	TextureManager(IFileManager* fileManager, const std::string& basePath)
	{
		this->basePath		  = basePath;
		this->fileManager	  = fileManager;
		this->pDefaultTexture = NULL;
	}

	~TextureManager()
	{
		SAFE_RELEASE(pDefaultTexture);
		for (TextureMap::iterator p = textures.begin(); p != textures.end(); p++)
		{
			SAFE_RELEASE(p->second);
		}
	}
};

class ShaderManager : public IShaderManager
{
	typedef map<string,Effect*> ShaderMap;

	ShaderMap	  shaders;
	string		  basePath;
	IFileManager* fileManager;
	Effect*       pDefaultShader;

	static Effect* createShader(IDirect3DDevice9* pDevice, IFile* file)
	{
		ID3DXEffect* pShader = NULL;
		unsigned long size = file->size();
		char* data = new char[ size ];
		file->read( (void*)data, size );
		if (FAILED(D3DXCreateEffect( pDevice, (void*)data, size, NULL, NULL, D3DXFX_NOT_CLONEABLE, NULL, &pShader, NULL )))
		{
            delete[] data;
			return NULL;
		}
		delete[] data;
        
        D3DXHANDLE technique;
        pShader->FindNextValidTechnique(NULL, &technique);
        pShader->SetTechnique(technique);

		Effect* pEffect = new Effect(pShader);
        SAFE_RELEASE(pShader);
        return pEffect;
	}

	Effect* load(IDirect3DDevice9* pDevice, const string& filename)
	{
		ShaderMap::iterator p = shaders.find(filename);
		if (p != shaders.end())
		{
			// Texture has already been loaded
			return p->second;
		}

		IFile* file = fileManager->getFile( basePath + filename );
		if (file == NULL)
		{
			return NULL;
		}
		return createShader(pDevice, file);
	}

public:
	Effect* getShader(IDirect3DDevice9* pDevice, string filename)
	{
		size_t pos;
		transform(filename.begin(), filename.end(), filename.begin(), toupper);
		Effect* pShader = NULL;

		// See if the file exists as specified
		try
		{
			IFile* file = new PhysicalFile(AnsiToWide(filename));
			pShader = createShader(pDevice, file);
			delete file;
		}
		catch (FileNotFoundException&)
		{
		}

		if (pShader == NULL)
		{
			// Use the part after the (back)slash, if any
            if (filename.find_first_of(":") != string::npos && (pos = filename.find_last_of("\\/")) != string::npos)
			{
				filename = filename.substr(pos + 1);
			}

			pShader = load(pDevice, filename);
		}

		if (pShader == NULL)
		{
			string name = filename;
			if ((pos = filename.rfind('.')) != string::npos)
			{
				name = name.substr(0, pos) + ".FXO";
			}
		
			pShader = load(pDevice, name);
			if (pShader == NULL)
			{
				// Load and return default placeholder texture
				if (pDefaultShader == NULL)
				{
                    ID3DXEffect* pDefaultEffect;
					if (SUCCEEDED(D3DXCreateEffectFromResource( pDevice, GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_DEFAULT_SHADER), NULL, NULL, D3DXFX_NOT_CLONEABLE, NULL, &pDefaultEffect, NULL)))
                    {
                        pDefaultShader = new Effect(pDefaultEffect);
                        SAFE_RELEASE(pDefaultEffect);
                    }
				}

				if (pDefaultShader != NULL)
				{
					pShader = pDefaultShader;
					pDefaultShader->AddRef();
				}
			}
		}

		if (pShader != NULL)
		{
			shaders.insert(make_pair(filename, pShader));
			pShader->AddRef();
		}

		return pShader;
	}

	ShaderManager(IFileManager* fileManager, const std::string& basePath)
	{
		this->basePath		 = basePath;
		this->fileManager	 = fileManager;
		this->pDefaultShader = NULL;
	}

	~ShaderManager()
	{
		SAFE_RELEASE(pDefaultShader);
		for (ShaderMap::iterator p = shaders.begin(); p != shaders.end(); p++)
		{
			SAFE_RELEASE(p->second);
		}
	}
};

class MouseCursor : public Object3D
{
    D3DXVECTOR3   m_oldPosition;
    LARGE_INTEGER m_updated;
    LARGE_INTEGER m_frequency;

public:
	void SetPosition(const D3DXVECTOR3& position)
	{
	    m_position = position;
    }

  	void UpdateVelocity()
    {
        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);

        D3DXVECTOR3 dx = m_position - m_oldPosition;
        float       dt = (float)(time.QuadPart - m_updated.QuadPart) / (float)m_frequency.QuadPart;
        m_velocity = dx / dt;

        m_oldPosition = m_position;
        m_updated     = time;
    }

    MouseCursor() : Object3D(NULL, D3DXVECTOR3(0,0,0))
	{
        QueryPerformanceFrequency(&m_frequency);
        m_oldPosition = D3DXVECTOR3(0,0,0);
	}
};

static INT_PTR CALLBACK AboutProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            HWND hVersion = GetDlgItem(hWnd, IDC_VERSION);
            wstring text = GetWindowStr(hVersion);
            text = FormatString(text.c_str(), VERSION_MAJOR, VERSION_MINOR);
            SetWindowText(hVersion, text.c_str());
            
            HWND hBuildDate = GetDlgItem(hWnd, IDC_BUILDDATE);
            text = GetWindowStr(hBuildDate);
            const char* s = __DATE__;
            text = FormatString(text.c_str(), s);
            SetWindowText(hBuildDate, text.c_str());

            wstring copyright = LoadString(IDS_EXPAT_COPYRIGHT);
            SetWindowText(GetDlgItem(hWnd, IDC_EXPAT_COPYRIGHT), copyright.c_str());

            wstring disclaimer = LoadString(IDS_DISCLAIMER);
            SetWindowText(GetDlgItem(hWnd, IDC_DISCLAIMER), disclaimer.c_str());

            return TRUE;
        }

        case WM_COMMAND:
		{
			WORD code = HIWORD(wParam);
			WORD id   = LOWORD(wParam);
			if (code == BN_CLICKED && id == IDOK || id == IDCANCEL)
			{
                EndDialog(hWnd, 0);
            }
            break;
        }
    }
    return FALSE;
}

void ShowAboutDialog(HWND hWndParent)
{
    DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUT), hWndParent, AboutProc);
}

struct APPLICATION_INFO
{
	HINSTANCE hInstance;
	HWND      hMainWnd;
	HWND      hRenderWnd;
	bool	  isMinimized;

	map<ULONGLONG, wstring> history;

    HWND      hLeaveParticles;
    HWND      hBackgroundLabel;
    HWND      hBackgroundBtn;
    HWND      hEmitterList;
	HWND      hPropertyTabs;
	HWND      hRebar;
	HWND	  hToolbar;
	HWND	  hStatusBar;
	HWND	  hTrackTabs;
	HWND      hTrackEditors[N_TRACKS];

	Engine*         engine;
	MouseCursor		mouseCursor;

	ParticleSystem*			 particleSystem;
    ParticleSystem::Emitter* selectedEmitter;
	ParticleSystemInstance*  attachedParticleSystem;

	wstring   filename;
	bool      changed;

	// Dragging
	enum { NONE, ROTATE, MOVE, ZOOM, OBJECT_Z } dragmode;
	long			xstart;
	long			ystart;
	Engine::Camera	startCam;
	bool            dragged;
    D3DXVECTOR3     dragStartPosition;
};

static void GetHistory(map<ULONGLONG, wstring>& history)
{
	history.clear();

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\AloParticleEditor", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
	{
		LONG error;
		for (int i = 0;; i++)
		{
			TCHAR  name[256] = {'\0'};
			DWORD length = 255;
			DWORD type, size;
			if ((error = RegEnumValue(hKey, i, name, &length, NULL, &type, NULL, &size)) != ERROR_SUCCESS)
			{
				break;
			}

			if (type == REG_BINARY && size == sizeof(FILETIME))
			{
				FILETIME filetime;
				if (RegQueryValueEx(hKey, name, NULL, &type, (BYTE*)&filetime, &size) != ERROR_SUCCESS)
				{
					break;
				}
				ULARGE_INTEGER largeint;
				largeint.LowPart  = filetime.dwLowDateTime;
				largeint.HighPart = filetime.dwHighDateTime;
				history.insert(make_pair(largeint.QuadPart, name));
			}
		}

		if (error == ERROR_NO_MORE_ITEMS)
		{
			// Graceful loop end, now delete everything older than the X-th oldest item
			map<ULONGLONG,wstring>::const_reverse_iterator p = history.rbegin();
			for (int j = 0; p != history.rend() && j < NUM_HISTORY_ITEMS; p++, j++);

			// Now start deleting
			for (; p != history.rend(); p++)
			{
				RegDeleteValue(hKey, p->second.c_str());
			}
		}

		RegCloseKey(hKey);
	}
}

// Adds the Alo Viewer history to the file menu
static bool AppendHistory(APPLICATION_INFO* info, HWND hWnd)
{
	// Get the history (timestamp, filename pairs)
	GetHistory(info->history);

	HMENU hMenu = GetMenu(hWnd);
	hMenu = GetSubMenu(hMenu, 0);

	MENUITEMINFO mii;
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask  = MIIM_TYPE;
	mii.cch    = 0;

	// Find the first seperator
	int i = 0;
	do {
		if (!GetMenuItemInfo(hMenu, i++, true, &mii))
		{
			return false;
		}
	} while (mii.fType != MFT_SEPARATOR);

	// Delete everything after it until only Exit's left
	mii.fMask = MIIM_ID;
	while (GetMenuItemInfo(hMenu, i, true, &mii) && mii.wID != ID_FILE_EXIT)
	{
		DeleteMenu(hMenu, i, MF_BYPOSITION);
	}

	if (!info->history.empty())
	{
		int j = 0;
		for (map<ULONGLONG,wstring>::const_reverse_iterator p = info->history.rbegin(); p != info->history.rend() && j < NUM_HISTORY_ITEMS; p++, i++, j++)
		{
			wstring name = p->second.c_str();

			HDC hDC = GetDC(info->hMainWnd);
			SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
			PathCompactPath(hDC, (LPTSTR)name.c_str(), 400);
			ReleaseDC(info->hMainWnd, hDC);

			if (j < 9)
			{
				name = wstring(L"&") + (TCHAR)(L'1' + j) + L" " + name;
			}
			InsertMenu(hMenu, i, MF_BYPOSITION | MF_STRING, ID_FILE_HISTORY_0 + j, name.c_str());
		}

		// Finally, add the seperator
		InsertMenu(hMenu, i, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
	}
	return true;
}

// Adds this filename to the history
static void AddToHistory(const wstring& name)
{
	// Get the current date & time
	FILETIME   filetime;
	SYSTEMTIME systime;
	GetSystemTime(&systime);
	SystemTimeToFileTime(&systime, &filetime);

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\AloParticleEditor", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		RegSetValueEx(hKey, name.c_str(), 0, REG_BINARY, (BYTE*)&filetime, sizeof(FILETIME));
		RegCloseKey(hKey);
	}
}

static void SetEmitterInfo(APPLICATION_INFO* info)
{
	bool show = (info->particleSystem != NULL && info->selectedEmitter != NULL);

    if (show)
    {
        EmitterProps_SetEmitter(info->hPropertyTabs, info->selectedEmitter);
	    for (int i = 0; i < ParticleSystem::NUM_TRACKS; i++)
	    {
            TrackEditor_SetTrack(info->hTrackEditors[i], info->selectedEmitter->trackContents, info->selectedEmitter->tracks);
	    }

        TrackEditor_EnableTrack(info->hTrackEditors[ParticleSystem::TRACK_ROTATION_SPEED], !info->selectedEmitter->randomRotation);
    }

	ShowWindow(info->hPropertyTabs, show ? SW_SHOW : SW_HIDE);
	ShowWindow(info->hTrackTabs,    show ? SW_SHOW : SW_HIDE);
	for (int i = 0; i < N_TRACKS; i++)
	{
		ShowWindow(info->hTrackEditors[i], (show && TabCtrl_GetCurSel(info->hTrackTabs) == i) ? SW_SHOW : SW_HIDE);
	}
}

static void SetFileChanged(APPLICATION_INFO* info, bool changed)
{
    info->changed = changed;

	// Load the proper name in the title bar
	wstring name = GetWindowStr(info->hMainWnd);
	size_t pos = name.find_first_of('-');
	if (pos != wstring::npos)
	{
		name = name.substr(0, pos - 1);
	}

	if (system != NULL)
	{
		name += L" - [" + (info->filename == L"" ? LoadString(IDS_TITLE_NEW_FILE) : info->filename);
		if (info->changed)
		{
			name += L"*";
		}
		name += L"]";
	}
	SetWindowText(info->hMainWnd, name.c_str());
}

static void OnFileChange(APPLICATION_INFO* info, ParticleSystem* system)
{
    SetFileChanged(info, false);
    
    // Update the emitter list
    EmitterList_SetParticleSystem(info->hEmitterList, system);

    // Set the emitter property panel
    info->particleSystem = system;

    if (info->particleSystem != NULL)
    {
        // Set the global particle system info
        SendMessage(info->hLeaveParticles, BM_SETCHECK, info->particleSystem->getLeaveParticles() ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    // Set the selected emitter info
    SetEmitterInfo(info);
}

static bool LoadFile(APPLICATION_INFO* info, const wstring& filename)
{
	// Delete old particle system
	if (info->engine != NULL)
	{
		info->engine->Clear();
	}
	delete info->particleSystem;
	info->particleSystem = NULL;

	PhysicalFile* file = new PhysicalFile(filename);
    ParticleSystem* system = NULL;
	try
	{
		system = new ParticleSystem(file);
		info->filename = filename;
		file->Release();
	}
	catch (wexception& e)
	{
        system = NULL;
		file->Release();
		MessageBox(info->hMainWnd, LoadString(IDS_ERROR_FILE_OPEN, e.what()).c_str(), NULL, MB_OK | MB_ICONERROR );
	}

    if (system != NULL)
    {
	    // Add it to the history
        AddToHistory(info->filename);
        AppendHistory(info, info->hMainWnd);
    }

    OnFileChange(info, system);
	return (system != NULL);
}

static void OpenHistoryFile(APPLICATION_INFO* info, int idx)
{
	// Find the correct entry
	map<ULONGLONG, wstring>::const_reverse_iterator p = info->history.rbegin();
	for (int j = 0; p != info->history.rend() && idx > 0; idx--, p++);
	if (p != info->history.rend())
	{
		LoadFile(info, p->second);
	}
}

static void DoNewFile(APPLICATION_INFO* info)
{
	info->particleSystem  = NULL;
	info->selectedEmitter = NULL;
    ParticleSystem* system = new ParticleSystem();
    system->addRootEmitter();
    OnFileChange(info, system);
}

static bool DoOpenFile(APPLICATION_INFO* info)
{
	// Query for the  file
	TCHAR filename[MAX_PATH];
	filename[0] = L'\0';

    wstring filter = LoadString(IDS_FILES_ALO) + wstring(L" (*.alo)\0*.ALO\0", 15)
                   + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = info->hMainWnd;
	ofn.hInstance    = info->hInstance;
    ofn.lpstrFilter  = filter.c_str();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile    = filename;
	ofn.nMaxFile     = MAX_PATH;
	ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileName(&ofn) == 0)
	{
		return false;
	}

    return LoadFile(info, filename);
}

static bool DoSaveFile(APPLICATION_INFO* info, bool saveas = false)
{
	if (info->filename == L"")
	{
		saveas = true;
	}

	if (saveas)
	{
		// Query for the filename
		TCHAR filename[MAX_PATH];
		filename[0] = L'\0';

        wstring filter = LoadString(IDS_FILES_ALO) + wstring(L" (*.alo)\0*.ALO\0", 15)
                       + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

        OPENFILENAME ofn;
		memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize  = sizeof(OPENFILENAME);
		ofn.hwndOwner    = info->hMainWnd;
		ofn.hInstance    = info->hInstance;
        ofn.lpstrFilter  = filter.c_str();
        ofn.lpstrDefExt  = L"alo";
		ofn.nFilterIndex = 1;
		ofn.lpstrFile    = filename;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
		if (GetSaveFileName( &ofn ) == 0)
		{
			return false;
		}
		info->filename = filename;
	}

	PhysicalFile* file = new PhysicalFile(info->filename, PhysicalFile::WRITE);
	try
	{
		// Create particleSystem name from filename
		wstring name = info->filename;

		size_t pos = name.find_last_of('\\');
		if (pos != wstring::npos) name = name.substr(pos + 1);
		pos = name.find_last_of('.');
		if (pos != wstring::npos) name = name.substr(0, pos);
		transform(name.begin(), name.end(), name.begin(), tolower);

		info->particleSystem->setName(WideToAnsi(name,"_"));
		info->particleSystem->write(file);
		file->Release();
	}
	catch (wexception& e)
	{
		file->Release();
		MessageBox(info->hMainWnd, LoadString(IDS_ERROR_FILE_SAVE, e.what()).c_str(), NULL, MB_OK | MB_ICONERROR );
	}
    SetFileChanged(info, false);
	return true;
}

static bool DoCheckChanges(APPLICATION_INFO* info)
{
	if (info->particleSystem != NULL)
	{
		if (info->changed)
		{
			switch (MessageBox(info->hMainWnd, LoadString(IDS_QUERY_SAVE_CHANGES).c_str(), LoadString(IDS_WARNING).c_str(), MB_YESNOCANCEL | MB_ICONQUESTION))
			{
				case IDYES:    return DoSaveFile(info);
				case IDCANCEL: return false;
			}
		}
	}
	return true;
}

static bool DoCloseFile(APPLICATION_INFO* info)
{
	if (info->particleSystem != NULL)
	{
		if (!DoCheckChanges(info))
		{
			return false;
		}

        if (info->engine != NULL)
        {
		    info->engine->Clear();
        }
		delete info->particleSystem;
		info->particleSystem         = NULL;
		info->attachedParticleSystem = NULL;
	}
	info->filename   = L"";
	info->selectedEmitter = NULL;
    OnFileChange(info, NULL);
	return true;
}

static void DoMenuInit(HMENU hMenu, APPLICATION_INFO* info)
{
    EnableMenuItem(hMenu, ID_EDIT_CLEARALLPARTICLES, MF_BYCOMMAND | (info->engine == NULL || info->engine->GetNumInstances() > 0 ? MF_ENABLED : MF_GRAYED ));

    EnableMenuItem(hMenu, ID_NEW_EMITTER_LIFETIME,      MF_BYCOMMAND | (info->selectedEmitter != NULL && info->selectedEmitter->spawnDuringLife == -1 ? MF_ENABLED : MF_GRAYED ));
    EnableMenuItem(hMenu, ID_NEW_EMITTER_DEATH,         MF_BYCOMMAND | (info->selectedEmitter != NULL && info->selectedEmitter->spawnOnDeath    == -1 ? MF_ENABLED : MF_GRAYED ));
    EnableMenuItem(hMenu, ID_EMITTER_RENAME,            MF_BYCOMMAND | (info->selectedEmitter != NULL ? MF_ENABLED : MF_GRAYED ));
    EnableMenuItem(hMenu, ID_EMITTER_RESCALE,           MF_BYCOMMAND | (info->selectedEmitter != NULL ? MF_ENABLED : MF_GRAYED ));
    EnableMenuItem(hMenu, ID_TOGGLE_EMITTER_VISIBILITY, MF_BYCOMMAND | (info->selectedEmitter != NULL ? MF_ENABLED : MF_GRAYED ));

    CheckMenuItem (hMenu, ID_VIEW_SHOWGROUND, MF_BYCOMMAND | (info->engine != NULL && info->engine->GetGround()     ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem (hMenu, ID_VIEW_DEBUGHEAT,  MF_BYCOMMAND | (info->engine != NULL && info->engine->GetHeatDebug()  ? MF_CHECKED : MF_UNCHECKED));
}

static bool DoMenuItem(APPLICATION_INFO* info, UINT id)
{
	switch (id)
	{
		case ID_FILE_NEW:     if (DoCloseFile   (info)) DoNewFile(info); break;
		case ID_FILE_OPEN:	  if (DoCheckChanges(info)) DoOpenFile(info); break;
		case ID_FILE_EXIT:    if (DoCheckChanges(info)) DestroyWindow(info->hMainWnd); break;
		case ID_FILE_SAVE:    DoSaveFile(info); break;
		case ID_FILE_SAVE_AS: DoSaveFile(info, true); break;

        case ID_EDIT_COPY:    SendMessage(GetFocus(), WM_COPY,  0, 0); break;
        case ID_EDIT_CUT:     SendMessage(GetFocus(), WM_CUT,   0, 0); break;
        case ID_EDIT_PASTE:   SendMessage(GetFocus(), WM_PASTE, 0, 0); break;
        case ID_EDIT_DELETE:  SendMessage(GetFocus(), WM_CLEAR, 0, 0); break;
        case ID_EDIT_RESCALE:
            if (RescaleParticleSystem(info->hMainWnd, info->particleSystem))
            {
                SetEmitterInfo(info);
                SetFileChanged(info, true);
            }
            break;

        case ID_EDIT_CLEARALLPARTICLES:
            if (info->engine != NULL)
            {
                info->engine->Clear();
            }
            break;

        case ID_NEW_EMITTER_ROOT:          EmitterList_AddRootEmitter(info->hEmitterList); break;
        case ID_NEW_EMITTER_LIFETIME:      EmitterList_AddLifetimeEmitter(info->hEmitterList); break;
        case ID_NEW_EMITTER_DEATH:         EmitterList_AddDeathEmitter(info->hEmitterList); break;
        case ID_EMITTER_RENAME:            EmitterList_RenameEmitter(info->hEmitterList); break;
        case ID_TOGGLE_EMITTER_VISIBILITY: EmitterList_ToggleEmitterVisibility(info->hEmitterList); break;
        case ID_SHOW_ALL_EMITTERS:         EmitterList_SetAllEmitterVisibility(info->hEmitterList, true);  break;
        case ID_HIDE_ALL_EMITTERS:         EmitterList_SetAllEmitterVisibility(info->hEmitterList, false); break;
        case ID_EMITTERS_RESCALE:
            if (info->selectedEmitter != NULL)
            {
                if (RescaleEmitter(info->hMainWnd, info->selectedEmitter))
                {
                    SetEmitterInfo(info);
                    SetFileChanged(info, true);
                }
            }
            break;

		case ID_VIEW_SHOWGROUND:
            if (info->engine != NULL)
            {
			    info->engine->SetGround(!info->engine->GetGround());
			    SendMessage(info->hToolbar, TB_CHECKBUTTON, id, MAKELONG(info->engine->GetGround(), 0));
            }
			break;

		case ID_VIEW_DEBUGHEAT:
            if (info->engine != NULL)
            {
			    info->engine->SetHeatDebug(!info->engine->GetHeatDebug());
			    SendMessage(info->hToolbar, TB_CHECKBUTTON, id, MAKELONG(info->engine->GetHeatDebug(), 0));
            }
			break;

        case ID_VIEW_RESETCAMERA:
            if (info->engine != NULL)
            {
                Engine::Camera camera = 
                {
                    D3DXVECTOR3(0,-250,125),
                    D3DXVECTOR3(0,0,0),
                    D3DXVECTOR3(0,0,1)
                };
                info->engine->SetCamera(camera);
            }
            break;

        case ID_HELP_ABOUT:
            ShowAboutDialog(info->hMainWnd);
			break;
    }
	return true;
}

static void Render(APPLICATION_INFO* info)
{
    static FPSMeasurer measurer;

	// Update and Render!
	info->engine->Update();
    info->engine->Render();
    measurer.measure();

    const D3DXVECTOR3 cursor = info->mouseCursor.GetPosition();
    info->mouseCursor.UpdateVelocity();

    // Update status bar
    SendMessage(info->hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadString(IDS_STATUS_INSTANCES, info->engine->GetNumInstances(), info->engine->GetNumEmitters()).c_str());
    SendMessage(info->hStatusBar, SB_SETTEXT, 1, (LPARAM)LoadString(IDS_STATUS_PARTICLES, info->engine->GetNumParticles()).c_str());
    SendMessage(info->hStatusBar, SB_SETTEXT, 2, (LPARAM)LoadString(IDS_STATUS_FPS,       (int)measurer.getFPS()).c_str());
}

static LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static int SIDEBAR_WIDTH = 310;

	APPLICATION_INFO* info = (APPLICATION_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			info = (APPLICATION_INFO*)pcs->lpCreateParams;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);

			HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

			//
			// Create the emitter list
			//
            if ((info->hEmitterList = CreateWindow(L"EmitterList", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				4, 4, SIDEBAR_WIDTH, 200, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
            {
                return -1;
            }
			SendMessage(info->hEmitterList, WM_SETFONT, (WPARAM)hFont, FALSE);

			//
			// Create the property tab window
			//
			if ((info->hPropertyTabs = CreateWindowEx(WS_EX_CONTROLPARENT, L"EmitterProps", NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | TCS_FOCUSNEVER | WS_TABSTOP,
				4, 4, SIDEBAR_WIDTH, 514, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}
			SendMessage(info->hPropertyTabs, WM_SETFONT, (WPARAM)hFont, FALSE);

			//
			// Create the tool bar
			//
			if ((info->hToolbar = CreateWindow(TOOLBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_TOOLTIPS,
				0, 0, 0, 0, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}

			HIMAGELIST hImgList = ImageList_LoadImage(pcs->hInstance, MAKEINTRESOURCE(IDR_TOOLBAR1), 16, 0, RGB(0,128,128), IMAGE_BITMAP, 0);
			SendMessage(info->hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImgList);

			TBBUTTON buttons[] = {
				{0, 0, 0, BTNS_SEP},
				{0, ID_FILE_NEW,  TBSTATE_ENABLED, BTNS_BUTTON},
				{1, ID_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON},
				{2, ID_FILE_SAVE, TBSTATE_ENABLED, BTNS_BUTTON},
				{0, 0, 0, BTNS_SEP},
				{3, ID_VIEW_SHOWGROUND, TBSTATE_ENABLED | TBSTATE_CHECKED, BTNS_CHECK},
				{4, ID_VIEW_DEBUGHEAT,  TBSTATE_ENABLED, BTNS_CHECK},
			};
			SendMessage(info->hToolbar, TB_ADDBUTTONS, 7, (LPARAM)&buttons);
			
			if ((info->hRebar = CreateWindow(REBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
				0, 0, 0, 0, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}

			SIZE size;
			SendMessage(info->hToolbar, TB_GETMAXSIZE, 0, (LPARAM)&size);

			REBARBANDINFO rbbi;
			rbbi.cbSize     = sizeof(REBARBANDINFO);
			rbbi.fMask      = RBBIM_STYLE | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE;
			rbbi.fStyle     = RBBS_NOGRIPPER;
			rbbi.hwndChild  = info->hToolbar;
			rbbi.cxMinChild = size.cx;
			rbbi.cyMinChild = size.cy + 2;
			rbbi.cx         = rbbi.cxMinChild;
			SendMessage(info->hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi);

			//
			// Create the status bar
			//
			if ((info->hStatusBar = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0, 0, 0, 0, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}

            INT widths[] = {140, 230, 280, 475, -1};
            SendMessage(info->hStatusBar, SB_SETPARTS, 5, (LPARAM)widths);
            SendMessage(info->hStatusBar, SB_SETTEXT, 4, (LPARAM)LoadString(IDS_STATUS_SHIFT_TO_SPAWN).c_str());

			//
			// Create the track tab window
			//
			if ((info->hTrackTabs = CreateWindowEx(WS_EX_CONTROLPARENT, WC_TABCONTROL, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | TCS_FOCUSNEVER, 4, 4, 300, 175, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}
			SendMessage(info->hTrackTabs, WM_SETFONT, (WPARAM)hFont, FALSE);

			const UINT trackLabels[N_TRACKS] = {
                IDS_LABEL_TRACK_RED, IDS_LABEL_TRACK_GREEN, IDS_LABEL_TRACK_BLUE, IDS_LABEL_TRACK_ALPHA,
                IDS_LABEL_TRACK_SCALE, IDS_LABEL_TRACK_INDEX, IDS_LABEL_TRACK_RPS};

			for (int i = 0; i < N_TRACKS; i++)
			{
                wstring label = LoadString(trackLabels[i]);

				TCITEM item;
				item.mask    = TCIF_TEXT;
				item.pszText = (LPWSTR)label.c_str();
				SendMessage(info->hTrackTabs, TCM_INSERTITEM, i, (LPARAM)&item);
			}

			//
			// Create the track editors
			//
			for (int i = 0; i < N_TRACKS; i++)
			{
				if ((info->hTrackEditors[i] = CreateWindowEx(WS_EX_CONTROLPARENT, L"TrackEditor", NULL, WS_CHILD | WS_CLIPCHILDREN | WS_TABSTOP, 0, 0, 100, 100, info->hTrackTabs, NULL, pcs->hInstance, (LPVOID)(LONG_PTR)i)) == NULL)
				{
					return -1;
				}
			}

            // Create the background control
			if ((info->hBackgroundLabel = CreateWindowEx(0, L"STATIC", LoadString(IDS_LABEL_BACKGROUND).c_str(), WS_CHILD | WS_VISIBLE,
				0, 0, 65, 16, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}
			SendMessage(info->hBackgroundLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
			
			if ((info->hBackgroundBtn = CreateWindowEx(0, L"ColorButton", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
				0, 0, 24, 24, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
			{
				return -1;
			}

            // Create the "leave particles" check box
            if ((info->hLeaveParticles = CreateWindow(L"BUTTON", LoadString(IDS_LABEL_LEAVE_PARTICLES).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                0, 0, 300, 16, hWnd, NULL, pcs->hInstance, NULL)) == NULL)
            {
                return -1;
            }
            SendMessage(info->hLeaveParticles, WM_SETFONT, (WPARAM)hFont, FALSE);

			SetEmitterInfo(info);
            AppendHistory(info, hWnd);
			ShowWindow(info->hTrackEditors[0], SW_SHOW);
            SetFocus(info->hEmitterList);
			break;
		}

        case WM_CLOSE:
            if (DoCloseFile(info))
            {
                DestroyWindow(hWnd);
            }
            return 0;

		case WM_DESTROY:
            if (info->engine != NULL)
            {
			    info->engine->Clear();
            }
			delete info->particleSystem;
			info->particleSystem = NULL;
			PostQuitMessage(0);
			break;

		case WM_INITMENU:
			DoMenuInit((HMENU)wParam, info);
			break;

		case WM_COMMAND:
			if (info != NULL)
			{
				// Menu and control notifications
				WORD code     = HIWORD(wParam);
				WORD id       = LOWORD(wParam);
				HWND hControl = (HWND)lParam;

                if (hControl == NULL)
                {
				    // Menu or accelerator
                    if (id >= ID_FILE_HISTORY_0 && id < ID_FILE_HISTORY_0 + min(9,NUM_HISTORY_ITEMS))
		            {
			            // It's a history item
                        if (DoCheckChanges(info))
                        {
			                OpenHistoryFile(info, id - ID_FILE_HISTORY_0);
                        }
		            }
    		        else DoMenuItem(info, id);
                }
				else if (code == CBN_CHANGE)
				{
					if (hControl == info->hBackgroundBtn && info->engine != NULL)
					{
						// The background color has changed
						info->engine->SetBackground(ColorButton_GetColor(hControl));
                        RedrawWindow(info->hRenderWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
					}
				}
                else if (code == BN_CLICKED)
                {
                    if (hControl == info->hLeaveParticles && info->particleSystem != NULL)
                    {
                        info->particleSystem->setLeaveParticles(SendMessage(hControl, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }
                    else if (hControl == info->hToolbar)
                    {
                        DoMenuItem(info, id);
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
					static struct
                    {
                        UINT_PTR idFrom;
                        UINT     idStr;
                    }
                    tooltips[] =
					{
                        {ID_FILE_NEW,        IDS_TOOLTIP_FILE_NEW},
                        {ID_FILE_OPEN,       IDS_TOOLTIP_FILE_OPEN},
                        {ID_FILE_SAVE,       IDS_TOOLTIP_FILE_SAVE},
                        {ID_VIEW_SHOWGROUND, IDS_TOOLTIP_TOGGLE_GROUND},
                        {ID_VIEW_DEBUGHEAT,  IDS_TOOLTIP_DEBUG_HEAT},
                        {0}
					};

                    for (int i = 0; tooltips[i].idFrom != 0; i++)
                    {
                        if (tooltips[i].idFrom == hdr->idFrom)
                        {
                            nmdi->hinst    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
					        nmdi->lpszText = MAKEINTRESOURCE(tooltips[i].idStr);
                            break;
                        }
                    }

                    break;
				}

				case TCN_SELCHANGING:
					ShowWindow(info->hTrackEditors[TabCtrl_GetCurSel(hdr->hwndFrom)], SW_HIDE);
					break;

				case TCN_SELCHANGE:
					ShowWindow(info->hTrackEditors[TabCtrl_GetCurSel(hdr->hwndFrom)], SW_SHOW);
					break;

				case ELN_LISTCHANGED:
					SetFileChanged(info, true);
					break;

				case ELN_SELCHANGED:
                    info->selectedEmitter = EmitterList_GetSelection(info->hEmitterList);
					SetEmitterInfo(info);
					break;

				case EP_CHANGE:
                    TrackEditor_EnableTrack(info->hTrackEditors[ParticleSystem::TRACK_ROTATION_SPEED], !info->selectedEmitter->randomRotation);
                    EmitterList_SelectionChanged(info->hEmitterList);
                    SetFileChanged(info, true);
                    if (info->engine != NULL)
                    {
                        info->engine->OnParticleSystemChanged(-1);
                    }
					break;

				case TE_CHANGE:
					// A track has changed; update the affected tracks
                    if (info->engine != NULL)
                    {
                        NMTECHANGE* nmtec = (NMTECHANGE*)lParam;
					    for (int i = 0; i < ParticleSystem::NUM_TRACKS; i++)
					    {
                            if (i == nmtec->track || info->selectedEmitter->tracks[i] == &info->selectedEmitter->trackContents[nmtec->track])
						    {
							    info->engine->OnParticleSystemChanged(i);
						    }
					    }
                    }
                    SetFileChanged(info, true);
					break;
			}
			break;
		}

        case WM_SIZING:
		{
			RECT* size = (RECT*)lParam;
			if (size->right - size->left < MIN_WINDOW_WIDTH)
			{
				if (wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT)
					size->left = size->right - MIN_WINDOW_WIDTH;
				else
					size->right = size->left + MIN_WINDOW_WIDTH;
			}
			if (size->bottom - size->top < MIN_WINDOW_HEIGHT)
			{
				if (wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT)
					size->bottom = size->top + MIN_WINDOW_HEIGHT;
				else
					size->top = size->bottom - MIN_WINDOW_HEIGHT;
			}
			break;
		}

		case WM_SIZE:
		{
			info->isMinimized = (wParam == SIZE_MINIMIZED);
			if (!info->isMinimized)
			{
				RECT props, tabs, status;

				// Get toolbar height
				GetWindowRect(info->hRebar, &props);
				int top = props.bottom - props.top;

				// Move status bar and recalculate height
				MoveWindow(info->hStatusBar, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
				GetClientRect(info->hStatusBar, &status);
				lParam = MAKELONG(LOWORD(lParam), HIWORD(lParam) - status.bottom - top);

				GetClientRect(info->hPropertyTabs, &props);
				GetClientRect(info->hTrackTabs,    &tabs);

				// Move property
				MoveWindow(info->hEmitterList,  4, top + 4, props.right, HIWORD(lParam) - props.bottom - 8, TRUE);
				MoveWindow(info->hPropertyTabs, 4, top + HIWORD(lParam) - props.bottom, props.right, props.bottom, TRUE);

                // Move top bar 
                RECT checkbox;
                RECT label;
                GetClientRect(info->hLeaveParticles, &checkbox);
                GetClientRect(info->hBackgroundLabel, &label);
                int height = max(max(24, checkbox.bottom), label.bottom);
                MoveWindow(info->hLeaveParticles, props.right + 8, top + 4 + (height - checkbox.bottom) / 2, checkbox.right, label.bottom, TRUE);
				MoveWindow(info->hBackgroundBtn,   LOWORD(lParam) - 28, top + 4 + (height - 24) / 2, 24, 24, TRUE);
				MoveWindow(info->hBackgroundLabel, LOWORD(lParam) - 32 - label.right, top + 4 + (height - label.bottom) / 2, label.right, label.bottom, TRUE);

				// Move render window
				MoveWindow(info->hRenderWnd, props.right + 8, top + 32, LOWORD(lParam) - (props.right + 8), HIWORD(lParam) - tabs.bottom - 36, TRUE);

				// Move track tabs
				tabs.right = LOWORD(lParam) - (props.right + 8);
				MoveWindow(info->hTrackTabs, props.right + 8, top + HIWORD(lParam) - tabs.bottom, tabs.right, tabs.bottom, TRUE);
				TabCtrl_AdjustRect(info->hTrackTabs, FALSE, &tabs);
				for (int i = 0; i < N_TRACKS; i++)
				{
					MoveWindow(info->hTrackEditors[i], tabs.left, tabs.top, tabs.right - tabs.left, tabs.bottom - tabs.top, TRUE);
				}
			}
			return 0;
        }
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Calculates the 3D position of the intersection of the cursor with Z = 0
static void GetCursorPos3D(Engine* engine, short x, short y, D3DXVECTOR3& position)
{
	D3DXVECTOR3  front, back;
	D3DVIEWPORT9 viewport;
	D3DXMATRIX   world;
	D3DXMatrixIdentity(&world);
	engine->GetViewPort(&viewport);

	D3DXVec3Unproject(&front, &D3DXVECTOR3(x, y, 0.0f), &viewport, &engine->GetProjectionMatrix(), &engine->GetViewMatrix(), &world);
	D3DXVec3Unproject(&back,  &D3DXVECTOR3(x, y, 0.9f), &viewport, &engine->GetProjectionMatrix(), &engine->GetViewMatrix(), &world);

	D3DXPLANE plane(0,0,1,0);
	D3DXPlaneIntersectLine(&position, &plane, &front, &back);
}

static LRESULT CALLBACK RenderWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	APPLICATION_INFO* info = (APPLICATION_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			info = (APPLICATION_INFO*)pcs->lpCreateParams;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);
			break;
		}

		case WM_PAINT:
			Render(info);
			break;

		case WM_LBUTTONUP:
			if (info->attachedParticleSystem != NULL)
			{
				// We've placed the system here
				info->engine->DetachParticleSystem(info->attachedParticleSystem);
				info->attachedParticleSystem = NULL;
			}

        case WM_RBUTTONUP:
			// Stop dragging
			info->dragmode = APPLICATION_INFO::NONE;
			ReleaseCapture();
			break;

		case WM_LBUTTONDOWN:
			if (info->attachedParticleSystem != NULL)
    		{
                info->dragmode = APPLICATION_INFO::OBJECT_Z;
                info->dragStartPosition = info->attachedParticleSystem->GetRelativePosition();
            }
            else info->dragmode = (wParam & MK_CONTROL) ? APPLICATION_INFO::ZOOM : APPLICATION_INFO::MOVE;

		case WM_RBUTTONDOWN:
            if (uMsg == WM_RBUTTONDOWN)
            {
    			info->dragmode = (wParam & MK_CONTROL) ? APPLICATION_INFO::ZOOM : APPLICATION_INFO::ROTATE;
            }

            // Start dragging, remember start settings
			info->startCam = info->engine->GetCamera();
			info->xstart   = LOWORD(lParam);
			info->ystart   = HIWORD(lParam);
			SetCapture(hWnd);
			SetFocus(hWnd);
			break;

		case WM_KEYUP:
			if (wParam == VK_SHIFT && info->attachedParticleSystem != NULL)
			{
				// Shift released. Remove cursor-bound system
				info->engine->KillParticleSystem(info->attachedParticleSystem);
				info->attachedParticleSystem = NULL;
			}
			break;

		case WM_KEYDOWN:
			// Only react to Shift being pressed initially
			if (wParam == VK_SHIFT && (~lParam & 0x40000000) && info->particleSystem != NULL && info->attachedParticleSystem == NULL)
			{
				// Spawn cursor-bound particle system
				D3DXVECTOR3 position;
				GetCursorPos3D(info->engine, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam), position);
				info->attachedParticleSystem = info->engine->SpawnParticleSystem(*info->particleSystem, &info->mouseCursor);

                // Clear statusbar hint
                SendMessage(info->hStatusBar, SB_SETTEXT, 4, (LPARAM)L"");
			}
			break;

		case WM_MOUSEMOVE:
        {
            D3DXVECTOR3 cursor;
            if (info->dragmode == APPLICATION_INFO::OBJECT_Z)
			{
                // Move the attached object up or down
				long  y   = (short)HIWORD(lParam) - info->ystart;
                float len = D3DXVec3Length(&(info->startCam.Target - info->startCam.Position));
                cursor = info->mouseCursor.GetPosition();
                cursor.z = -y * len / 1000;
                info->mouseCursor.SetPosition(cursor);
        	    Render(info);
            }
            else
            {
				// Move cursor-bound particle system
				GetCursorPos3D(info->engine, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam), cursor);
				info->mouseCursor.SetPosition(cursor);

                if (info->dragmode != APPLICATION_INFO::NONE)
			    {
				    // Yay, math time!
				    long x = (short)LOWORD(lParam) - info->xstart;
				    long y = (short)HIWORD(lParam) - info->ystart;

				    Engine::Camera camera = info->startCam;
				    D3DXVECTOR3    orthVec, diff = info->startCam.Position - info->startCam.Target;
    				
				    // Get the orthogonal vector
				    D3DXVec3Cross( &orthVec, &diff, &camera.Up );
				    D3DXVec3Normalize( &orthVec, &orthVec );

				    if (info->dragmode == APPLICATION_INFO::ROTATE)
				    {
					    // Lets rotate
					    D3DXMATRIX rotateXY, rotateZ, rotate;
					    D3DXMatrixRotationZ( &rotateZ, -D3DXToRadian(x / 2.0f) );
					    D3DXMatrixRotationAxis( &rotateXY, &orthVec, D3DXToRadian(y / 2.0f) );
					    D3DXMatrixMultiply( &rotate, &rotateXY, &rotateZ );
					    D3DXVec3TransformCoord( &camera.Position, &diff, &rotate );
					    camera.Position += camera.Target;
				    }
				    else if (info->dragmode == APPLICATION_INFO::MOVE)
				    {
					    // Lets translate
					    D3DXVECTOR3 Up;
					    D3DXVec3Cross( &Up, &orthVec, &diff );
					    D3DXVec3Normalize( &Up, &Up );
    					
					    // The distance we move depends on the distance from the object
					    // Large distance: move a lot, small distance: move a little
					    float multiplier = D3DXVec3Length( &diff ) / 1000;

					    camera.Target  += (float)x * multiplier * orthVec;
					    camera.Target  += (float)y * multiplier * Up;
					    camera.Position = diff + camera.Target;
				    }
				    else if (info->dragmode == APPLICATION_INFO::ZOOM)
				    {
					    // Lets zoom
					    // The amount we scroll in and out depends on the distance.
					    float olddist = D3DXVec3Length( &diff );
					    float newdist = max(1.0f, olddist - sqrt(olddist) * -y);
					    D3DXVec3Scale( &camera.Position, &diff, newdist / olddist );
					    camera.Position += camera.Target;
				    }
				    info->dragged = true;
				    info->engine->SetCamera( camera );
    			    Render(info);
			    }
            }

            // Update statusbar
            SendMessage(info->hStatusBar, SB_SETTEXT, 3, (LPARAM)LoadString(IDS_STATUS_MOUSE, cursor.x, cursor.y, cursor.z).c_str());
            break;
        }

		case WM_MOUSEWHEEL:
			if (info->dragmode == APPLICATION_INFO::NONE)
			{
				Engine::Camera camera = info->engine->GetCamera();

				// The amount we scroll in and out depends on the distance.
				D3DXVECTOR3 diff = camera.Position - camera.Target;
				float olddist = D3DXVec3Length( &diff );
				float newdist = max(1.0f, olddist - sqrt(olddist) * (SHORT)HIWORD(wParam) / WHEEL_DELTA);
				D3DXVec3Scale( &camera.Position, &diff, newdist / olddist );
				camera.Position += camera.Target;

				info->engine->SetCamera(camera);
				Render(info);
			}
			break;

		case WM_SIZE:
			if (info->engine != NULL)
			{
				info->engine->Reset();
			}
			break;

		case WM_SETFOCUS:
			// Yes, we want focus please
			return 0;

        case WM_DROPFILES:
		{
			// User dropped a filename on the window
			HDROP hDrop = (HDROP)wParam;
			UINT nFiles = DragQueryFile(hDrop, -1, NULL, 0);
			for (UINT i = 0; i < nFiles; i++)
			{
				UINT size = DragQueryFile(hDrop, i, NULL, 0);
				wstring filename(size,L' ');
				DragQueryFile(hDrop, i, (LPTSTR)filename.c_str(), size + 1);
				if (LoadFile(info, filename))
                {
                    break;
                }
			}
			DragFinish(hDrop);
			break;
		}
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Returns the install path by querying the registry, or an empty string when failed.
static void getGamePath_Reg(vector<wstring>& strings)
{
	const TCHAR* paths[] = {
		L"Software\\LucasArts\\Star Wars Empire at War Forces of Corruption\\1.0",
		L"Software\\LucasArts\\Star Wars Empire at War Forces of Corruption Demo\\1.0",
		L"Software\\LucasArts\\Star Wars Empire at War\\1.0",
		L""
	};

	for (int i = 0; paths[i][0] != '\0'; i++)
	{
		HKEY hKey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, paths[i], 0, KEY_QUERY_VALUE, &hKey ) == ERROR_SUCCESS)
		{
			DWORD type, size = MAX_PATH;
			TCHAR path[MAX_PATH];
			if (RegQueryValueEx(hKey, L"ExePath", NULL, &type, (LPBYTE)path, &size) == ERROR_SUCCESS)
			{
				wstring str = path;
				size_t pos = str.find_last_of(L"\\");
				if (pos != string::npos)
				{
					str = str.substr(0, pos);
				}
				strings.push_back(str);
			}
			RegCloseKey(hKey);
		}
	}
}

// Adds the %PROGRAMFILES%\LucasArts\Star Wars Empire at War\GameData paths to the vector
static void getGamePath_Shell(vector<wstring>& strings)
{
	TCHAR path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, path )))
	{
		MessageBox(NULL, path, NULL, MB_OK );
		wstring str = path;
		if (*str.rbegin() != L'\\') str += L'\\';
		
		strings.push_back(str + L"LucasArts\\Star Wars Empire at War Forces of Corruption");
		strings.push_back(str + L"LucasArts\\Star Wars Empire at War Forces of Corruption Demo");
		strings.push_back(str + L"LucasArts\\Star Wars Empire at War\\GameData");
	}
}

static FileManager* createFileManager( HWND hWnd, const vector<wstring>& argv )
{
	// Search for the Empire at War path
	vector<wstring> EmpireAtWarPaths;
	if (argv.size() > 1)
	{
		// Override on the command line; use that
		for (size_t i = 1; i < argv.size(); i++)
		{
			if (PathIsDirectory(argv[i].c_str()))
			{
    			EmpireAtWarPaths.push_back(argv[i]);
            }
		}
	}

	if (EmpireAtWarPaths.empty())
	{
		// First try the registry
        TCHAR buffer[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, buffer);
        EmpireAtWarPaths.push_back(buffer);

        #if 0
        getGamePath_Reg(EmpireAtWarPaths);
		if (EmpireAtWarPaths.empty())
		{
			// Then try the shell
			getGamePath_Shell(EmpireAtWarPaths);
		}
        #endif
		
	}
	FileManager* fileManager = NULL;

	while (fileManager == NULL)
	{
		for (size_t i = 0; i < EmpireAtWarPaths.size(); i++)
		{
			if (*EmpireAtWarPaths[i].rbegin() != '\\') EmpireAtWarPaths[i] += '\\';
		}

		try
		{
			// Initialize the file manager
			fileManager = new FileManager( EmpireAtWarPaths );
		}
		catch (FileNotFoundException&)
		{
			// This path didn't work; ask the user to select a path
            const wstring title = LoadString(IDS_QUERY_DATA_PATH);

			BROWSEINFO bi;
			bi.hwndOwner      = hWnd;
			bi.pidlRoot       = NULL;
			bi.pszDisplayName = NULL;
			bi.lpszTitle      = title.c_str();
			bi.ulFlags        = BIF_RETURNONLYFSDIRS;
			bi.lpfn           = NULL;
			LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
			if (pidl == NULL)
			{
				fileManager = NULL;
				break;
			}

			TCHAR path[MAX_PATH];
			if (SHGetPathFromIDList( pidl, path ))
			{
				EmpireAtWarPaths.push_back(path);
			}
			CoTaskMemFree(pidl);
		}
	}
	return fileManager;
}

void main( APPLICATION_INFO* info, const vector<wstring>& argv )
{
	FileManager* fileManager = createFileManager( info->hMainWnd, argv );
	if (fileManager == NULL)
	{
		// No file manager, no play
		return;
	}

	try
	{
		// Initialize the other managers and engine
		TextureManager textureManager(fileManager, "Data\\Art\\Textures\\");
        ShaderManager  shaderManager (fileManager, "Data\\Art\\Shaders\\");

		// Create the rendering engine
        try
        {
		    info->engine = new Engine(info->hMainWnd, info->hRenderWnd, textureManager, shaderManager);
            ColorButton_SetColor(info->hBackgroundBtn, info->engine->GetBackground());
        }
        catch (exception&)
        {
            DestroyWindow(info->hRenderWnd);
        }
		
		DoCloseFile(info);

        bool loaded = false;
        if (info->engine != NULL)
        {
			// See if a file was specified
			for (size_t i = 1; i < argv.size(); i++)
			{
				if (PathFileExists(argv[i].c_str()) && !PathIsDirectory(argv[i].c_str()))
				{
					loaded = LoadFile(info, argv[i]);
					break;
				}
			}
        }

        if (!loaded)
        {
		    DoNewFile(info);
        }
		ShowWindow(info->hMainWnd, SW_SHOWNORMAL);

        HACCEL hAccel = LoadAccelerators( info->hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
		for (bool quit = false; !quit; )
		{
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (!TranslateAccelerator(info->hMainWnd, hAccel, &msg) && !IsDialogMessage(info->hMainWnd, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if (msg.message == WM_QUIT)
				{
					quit = true;
				}
			}

            if (!quit && (info->isMinimized || info->engine == NULL))
			{
				WaitMessage();
			}
			else if (info->engine != NULL)
			{
				Render(info);
			}
		}

		delete fileManager;
	}
	catch (...)
	{
		delete fileManager;
		throw;
	}
}

static bool InitializeWindows( APPLICATION_INFO* info )
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = MainWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = info->hInstance;
	wcx.hIcon         = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LOGO));
	wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wcx.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);
	wcx.lpszClassName = L"ParticleEditor";
	wcx.hIconSm       = NULL;

	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

	wcx.lpfnWndProc   = RenderWindowProc;
	wcx.hIcon         = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"ParticleEditorRenderer";

	if (!RegisterClassEx(&wcx))
	{
		UnregisterClass(L"ParticleEditor", info->hInstance);
		return false;
	}

	if ((info->hMainWnd = CreateWindow(L"ParticleEditor", L"Particle Editor", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_GROUP,
		50, 50, 1150, 850, NULL, NULL, info->hInstance, info)) == NULL)
	{
		UnregisterClass(L"ParticleEditorRenderer", info->hInstance);
		UnregisterClass(L"ParticleEditor", info->hInstance);
		return false;
	}

	if ((info->hRenderWnd = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_ACCEPTFILES
        , L"ParticleEditorRenderer", NULL, WS_CHILD | WS_VISIBLE | WS_GROUP,
		400, 4, 100, 100, info->hMainWnd, NULL, info->hInstance, info)) == NULL)
	{
		DestroyWindow(info->hMainWnd);
		UnregisterClass(L"ParticleEditorRenderer", info->hInstance);
		UnregisterClass(L"ParticleEditor", info->hInstance);
		return false;
	}

	return true;
}

static vector<wstring> parseCommandLine()
{
	vector<wstring> argv;
	TCHAR* cmdline = GetCommandLine();

	bool quoted = false;
	wstring arg;
	for (TCHAR* p = cmdline; p == cmdline || *(p - 1) != '\0'; p++)
	{
		if (*p == '\0' || (*p == ' ' && !quoted))
		{
			if (arg != L"")
			{
				argv.push_back(arg);
				arg = L"";
			}
		}
		else if (*p == '"') quoted = !quoted;
		else arg += *p;
	}
	return argv;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
#ifndef NDEBUG
	AllocConsole();
	freopen("conout$", "wb", stdout);
#endif
	int result = -1;

    APPLICATION_INFO info;
	info.hInstance				= hInstance;
	info.hMainWnd				= NULL;
	info.particleSystem			= NULL;
	info.attachedParticleSystem = NULL;
	info.engine					= NULL;
	info.dragmode				= APPLICATION_INFO::NONE;
	info.isMinimized			= false;

#ifdef NDEBUG
 	try
#endif
    {
		// Initialize UI classes and create windows
		if (!UI_Initialize(hInstance) || !InitializeWindows(&info))
		{
			//throw wruntime_error(LoadString(IDS_ERROR_UI_INITIALIZATION));
		}

		// Run the program
		result = main( &info, parseCommandLine() );
        DestroyWindow(info.hMainWnd);
        delete info.engine;
	}
#ifdef NDEBUG
	catch (wexception& e)
	{
        DestroyWindow(info.hMainWnd);
        delete info.engine;
		MessageBox(NULL, e.what(), NULL, MB_OK | MB_ICONHAND);
	}
	catch (exception& e)
	{
        DestroyWindow(info.hMainWnd);
        delete info.engine;
		MessageBoxA(NULL, e.what(), NULL, MB_OK | MB_ICONHAND);
	}
#endif

#ifndef NDEBUG
	FreeConsole();
#endif
	return result;
}