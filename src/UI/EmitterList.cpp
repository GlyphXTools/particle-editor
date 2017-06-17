#include "UI/UI.h"
#include "utils.h"
#include "Rescale.h"
using namespace std;

// Registered clipboard format
static UINT CF_PARTICLE_EMITTER = 0;

struct EmitterListControl
{
	HWND			hDialog;
    HMENU           hNewEmitterMenu;
    HMENU           hEmitterContextMenu;
    HWND            hTree;
    HWND            hToolbar;
	ParticleSystem* system;
    ParticleSystem::Emitter* selection;
};

static void NotifyParent(EmitterListControl* control, UINT code)
{
    if (code == ELN_SELCHANGED)
    {
        // Enable buttons on toolbar
        SendMessage(control->hToolbar, TB_ENABLEBUTTON, ID_DELETE_EMITTER,            control->selection != NULL);
        SendMessage(control->hToolbar, TB_ENABLEBUTTON, ID_TOGGLE_EMITTER_VISIBILITY, control->selection != NULL);
    }

	NMHDR hdr;
	hdr.code     = code;
    hdr.hwndFrom = control->hDialog;
    hdr.idFrom   = GetDlgCtrlID(hdr.hwndFrom);
    SendMessage(GetParent(hdr.hwndFrom), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr );
}

static bool CopyEmitter(HWND hWnd, EmitterListControl* control)
{
    if (control->selection != NULL)
    {
        // Copy the emitter
        MemoryFile* memfile = new MemoryFile;
        try
        {
            ChunkWriter writer(memfile);
            control->selection->copy(writer);

            HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, memfile->size()); 
            if (hMemory == NULL) 
            { 
                return false;
            } 
     
            // Lock the handle and copy the text to the buffer. 
            void* data = GlobalLock(hMemory); 
            memfile->seek(0);
            memfile->read(data, memfile->size());
            GlobalUnlock(hMemory); 
     
            // Place the handle on the clipboard. 
            OpenClipboard(hWnd);
            SetClipboardData(CF_PARTICLE_EMITTER, hMemory); 
            memfile->Release();
            CloseClipboard();
        }
        catch (...)
        {
            memfile->Release();
            CloseClipboard();
            MessageBox(NULL, LoadString(IDS_ERROR_EMITTER_COPY).c_str(), NULL, MB_OK | MB_ICONHAND);
            return false;
        }
    }
    return true;
}

static bool PasteEmitter(HWND hWnd, EmitterListControl* control, void (*func)(HWND, const ParticleSystem::Emitter&) = &EmitterList_AddRootEmitter)
{
    // Paste an emitter
    OpenClipboard(hWnd);
    HANDLE hMemory = GetClipboardData(CF_PARTICLE_EMITTER);
    if (hMemory == NULL)
    {
        CloseClipboard();
        return false;
    }

    MemoryFile* memfile = new MemoryFile();
    try
    {
        void* data = GlobalLock(hMemory); 
        memfile->write(data, (unsigned long)GlobalSize(hMemory));
        memfile->seek(0);
        GlobalUnlock(hMemory); 

        // Create the emitter
        ChunkReader reader(memfile);
        ParticleSystem::Emitter emitter(reader);
        (*func)(hWnd, emitter);
        memfile->Release();
        CloseClipboard();
    }
    catch (...)
    {
        memfile->Release();
        CloseClipboard();
        MessageBox(NULL, LoadString(IDS_ERROR_EMITTER_PASTE).c_str(), NULL, MB_OK | MB_ICONHAND);
        return false;
    }
    return true;
}

static int GetTreeNodeIcon(const ParticleSystem* system, size_t iEmitter)
{
    const ParticleSystem::Emitter& emitter = system->getEmitter(iEmitter);
    int base = (emitter.visible ? 0 : 4);
    if (emitter.parent != NULL && emitter.parent->spawnOnDeath == emitter.index)
    {
        // Spawn-on-death type particle
        return base + 1;
    }
    
    if (emitter.useBursts && emitter.nBursts > 0)
    {
        // Finite amount of particles
        return base + 2;
    }

    // Infinite (weather or normal)
    return base + (emitter.isWeatherParticle ? 3 : 0);
}

static HTREEITEM InsertTreeItem(EmitterListControl* control, HTREEITEM hParent, ParticleSystem::Emitter* emitter)
{
    wstring name = AnsiToWide(emitter->name);
    TVINSERTSTRUCT tvis;
    tvis.hParent        = hParent;
    tvis.hInsertAfter   = TVI_LAST;
    tvis.item.mask      = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_TEXT;
    tvis.item.pszText   = (LPWSTR)name.c_str();
    tvis.item.lParam    = (LPARAM)emitter;
    tvis.item.cChildren = 1;
    tvis.item.iImage    = GetTreeNodeIcon(control->system, emitter->index);
    tvis.item.iSelectedImage = tvis.item.iImage;
    HTREEITEM hItem = TreeView_InsertItem(control->hTree, &tvis);
    if (hItem != NULL)
    {
        TreeView_Expand(control->hTree, hParent, TVE_EXPAND);
    }
    return hItem;
}

// Window procedure for subclassed edit box during treeview label edit
// Workaround for bug in Knowledge Base item Q130691.
static LRESULT WINAPI LabelEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
    }
    WNDPROC wndProc = (WNDPROC)GetProp(hWnd, L"Old_WindowProc");
    return CallWindowProc(wndProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EmitterTreeViewWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
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
            break;

        case WM_COPY:  CopyEmitter(hWnd, control);       break;
        case WM_CUT:   if (!CopyEmitter(hWnd, control))  break;
        case WM_CLEAR: EmitterList_DeleteEmitter(hWnd);  break;
        case WM_PASTE: PasteEmitter(hWnd, control);      break;
    }
    WNDPROC wndProc = (WNDPROC)GetProp(hWnd, L"Old_WindowProc");
    return CallWindowProc(wndProc, hWnd, uMsg, wParam, lParam);
}

static INT_PTR WINAPI DlgEmitterListProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			control = (EmitterListControl*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);

            HINSTANCE hInstance = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
            
            //
            // Initialize treeview
            //
            control->hTree     = GetDlgItem(hWnd, IDC_TREE1);
            HIMAGELIST hImgList = ImageList_LoadImage(hInstance, MAKEINTRESOURCE(IDB_EMITTER_LIST), 12, 0, RGB(0,128,128), IMAGE_BITMAP, 0);
    		TreeView_SetImageList(control->hTree, hImgList, TVSIL_NORMAL);

            // Subclass window proc for Cut/Copy/Paste operations
            WNDPROC wndProc = (WNDPROC)(LONG_PTR)GetWindowLongPtr(control->hTree, GWLP_WNDPROC);
            SetProp(control->hTree, L"Old_WindowProc", (HANDLE)wndProc);
            SetWindowLongPtr(control->hTree, GWLP_USERDATA, (LONG)(LONG_PTR)control);
            SetWindowLongPtr(control->hTree, GWLP_WNDPROC,  (LONG)(LONG_PTR)EmitterTreeViewWindowProc);

			//
			// Initialize toolbar
			//
			control->hToolbar = GetDlgItem(hWnd, IDC_TOOLBAR1);
			hImgList = ImageList_LoadImage(hInstance, MAKEINTRESOURCE(IDR_EMITTER_TOOLBAR), 16, 0, RGB(0,128,128), IMAGE_BITMAP, 0);
            SendMessage(control->hToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
			SendMessage(control->hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImgList);

			TBBUTTON buttons[10] = {
				{0, ID_NEW_EMITTER_ROOT,          TBSTATE_ENABLED, BTNS_DROPDOWN},
				{0, 0,                            TBSTATE_ENABLED, BTNS_SEP},
				{1, ID_DELETE_EMITTER,            TBSTATE_ENABLED, BTNS_BUTTON},
				{2, ID_TOGGLE_EMITTER_VISIBILITY, TBSTATE_ENABLED, BTNS_BUTTON},
				{0, 0,                            TBSTATE_ENABLED, BTNS_SEP},
                {3, ID_SHOW_ALL_EMITTERS, TBSTATE_ENABLED, BTNS_BUTTON},
                {4, ID_HIDE_ALL_EMITTERS, TBSTATE_ENABLED, BTNS_BUTTON},
			};
			SendMessage(control->hToolbar, TB_ADDBUTTONS, 7, (LPARAM)buttons);

            // Load resources
            control->hNewEmitterMenu     = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_NEW_EMITTER_MENU));
            control->hEmitterContextMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_EMITTER_CONTEXT_MENU));
            break;
        }

        case WM_DESTROY:
            DestroyMenu(control->hEmitterContextMenu);
            DestroyMenu(control->hNewEmitterMenu);
            break;

		case WM_COMMAND:
			if (lParam != NULL)
			{
				HWND hCtrl = (HWND)lParam;
				switch (HIWORD(wParam))
				{
					case BN_CLICKED:
                        if (hCtrl == control->hToolbar)
						{
							// A toolbar button has been clicked
							switch (LOWORD(wParam))
							{
                                case ID_NEW_EMITTER_ROOT:          EmitterList_AddRootEmitter(hWnd); break;
                                case ID_NEW_EMITTER_LIFETIME:      EmitterList_AddLifetimeEmitter(hWnd); break;
                                case ID_NEW_EMITTER_DEATH:         EmitterList_AddDeathEmitter(hWnd); break;
                                case ID_TOGGLE_EMITTER_VISIBILITY: EmitterList_ToggleEmitterVisibility(hWnd); break;
                                case ID_DELETE_EMITTER:            EmitterList_DeleteEmitter(hWnd); break;
                                case ID_SHOW_ALL_EMITTERS:         EmitterList_SetAllEmitterVisibility(hWnd, true);  break;
                                case ID_HIDE_ALL_EMITTERS:         EmitterList_SetAllEmitterVisibility(hWnd, false); break;
                            }
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
                        UINT id;
                        UINT text;
                    }
                    tooltips[] =
					{
                        {ID_NEW_EMITTER_ROOT,          IDS_TOOLTIP_EMITTER_NEW},
                        {ID_DELETE_EMITTER,            IDS_TOOLTIP_EMITTER_DELETE},
                        {ID_TOGGLE_EMITTER_VISIBILITY, IDS_TOOLTIP_EMITTER_TOGGLE},
                        {ID_SHOW_ALL_EMITTERS,         IDS_TOOLTIP_EMITTERS_SHOW},
                        {ID_HIDE_ALL_EMITTERS,         IDS_TOOLTIP_EMITTERS_HIDE},
                        {0, NULL}
					};

                    for (int i = 0; tooltips[i].text != NULL; i++)
                    {
                        if (tooltips[i].id == hdr->idFrom)
                        {
                            nmdi->hinst    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
					        nmdi->lpszText = MAKEINTRESOURCE(tooltips[i].text);
                            break;
                        }
                    }

                    break;
				}

                case NM_CLICK:
                    if (hdr->hwndFrom == control->hTree)
                    {
                        // Get item under cursor
                        TVHITTESTINFO tvht;
                        GetCursorPos(&tvht.pt);
                        ScreenToClient(control->hTree, &tvht.pt);
                        TreeView_HitTest(control->hTree, &tvht);
                        
                        if (tvht.hItem != NULL && tvht.flags & TVHT_ONITEMICON)
                        {
                            // User clicked an emitter's icon; toggle visibility
                            EmitterList_ToggleEmitterVisibility(hWnd, tvht.hItem);
                        }
                    }
                    break;

                case NM_RCLICK:
                    if (hdr->hwndFrom == control->hTree)
                    {
                        POINT cursor;
                        GetCursorPos(&cursor);

                        // Get item under cursor
                        TVHITTESTINFO tvht;
                        tvht.pt = cursor;
                        ScreenToClient(control->hTree, &tvht.pt);
                        TreeView_HitTest(control->hTree, &tvht);
                        TreeView_SelectItem(control->hTree, tvht.hItem);

                        HMENU hPopupMenu = GetSubMenu(control->hEmitterContextMenu, 0);
                        EnableMenuItem(hPopupMenu, ID_EDIT_COPY,       MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_EDIT_CUT ,       MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_EDIT_DELETE,     MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_EDIT_PASTE,      MF_BYCOMMAND | (IsClipboardFormatAvailable(CF_PARTICLE_EMITTER) ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_EMITTER_RENAME,  MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_EMITTER_RESCALE, MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_NEW_EMITTER_LIFETIME, MF_BYCOMMAND | (control->selection != NULL && control->selection->spawnDuringLife == -1 ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_NEW_EMITTER_DEATH,    MF_BYCOMMAND | (control->selection != NULL && control->selection->spawnOnDeath    == -1 ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_PASTEAS_LIFETIME, MF_BYCOMMAND | (control->selection != NULL && control->selection->spawnDuringLife == -1 ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_PASTEAS_DEATH,    MF_BYCOMMAND | (control->selection != NULL && control->selection->spawnOnDeath    == -1 ? MF_ENABLED : MF_GRAYED));
                        EnableMenuItem(hPopupMenu, ID_TOGGLE_EMITTER_VISIBILITY, MF_BYCOMMAND | (control->selection != NULL ? MF_ENABLED : MF_GRAYED));

                        INT id = TrackPopupMenuEx(hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, cursor.x, cursor.y, hWnd, NULL);
                        switch (id)
                        {
                            case ID_NEW_EMITTER_ROOT:
                            case ID_NEW_EMITTER_LIFETIME:
                            case ID_NEW_EMITTER_DEATH:
                                SendMessage(hWnd, WM_COMMAND, MAKELONG(id, BN_CLICKED), (LPARAM)control->hToolbar);
                                break;

                            case ID_EDIT_CUT:    SendMessage(control->hTree, WM_CUT,   0, 0); break;
                            case ID_EDIT_COPY:   SendMessage(control->hTree, WM_COPY,  0, 0); break;
                            case ID_EDIT_DELETE: SendMessage(control->hTree, WM_CLEAR, 0, 0); break;
                            case ID_EDIT_PASTE:  SendMessage(control->hTree, WM_PASTE, 0, 0); break;
                            case ID_PASTEAS_LIFETIME: PasteEmitter(hWnd, control, &EmitterList_AddLifetimeEmitter); break;
                            case ID_PASTEAS_DEATH:    PasteEmitter(hWnd, control, &EmitterList_AddDeathEmitter); break;
                            case ID_EMITTER_RENAME:   TreeView_EditLabel(control->hTree, tvht.hItem); break;
                            case ID_TOGGLE_EMITTER_VISIBILITY: EmitterList_ToggleEmitterVisibility(hWnd); break;
                            case ID_EMITTER_RESCALE:
                                if (RescaleEmitter(hWnd, control->selection))
                                {
                                    NotifyParent(control, ELN_LISTCHANGED);
                                }
                                break;
                        }
                    }
                    break;

                case TVN_SELCHANGING:
                {
					NMTREEVIEW* nmtv = (NMTREEVIEW*)lParam;
                    if (nmtv->action == TVC_BYMOUSE)
                    {
                        // Get item under cursor
                        TVHITTESTINFO tvht;
                        GetCursorPos(&tvht.pt);
                        ScreenToClient(control->hTree, &tvht.pt);
                        TreeView_HitTest(control->hTree, &tvht);

                        if (tvht.flags & TVHT_ONITEMICON)
                        {
                            // Don't change selection
                            SetWindowLongPtr(hWnd, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                    }
                    break;
                }

				case TVN_SELCHANGED:
                {
					NMTREEVIEW* nmtv   = (NMTREEVIEW*)lParam;
                    control->selection = (nmtv->itemNew.hItem != NULL) ? (ParticleSystem::Emitter*)nmtv->itemNew.lParam : NULL;
                    
                	NotifyParent(control, ELN_SELCHANGED);
					break;
                }

                case TBN_DROPDOWN:
                {
                    NMTOOLBAR* nmtb = (NMTOOLBAR*)lParam;

                    // Enable items as applicable
                    HMENU hPopupMenu = GetSubMenu(control->hNewEmitterMenu, 0);
                    UINT nEnable1 = (control->selection != NULL && control->selection->spawnDuringLife == -1) ? MF_ENABLED : MF_GRAYED;
                    UINT nEnable2 = (control->selection != NULL && control->selection->spawnOnDeath    == -1) ? MF_ENABLED : MF_GRAYED;
                    EnableMenuItem(hPopupMenu, ID_NEW_EMITTER_LIFETIME, nEnable1 | MF_BYCOMMAND);
                    EnableMenuItem(hPopupMenu, ID_NEW_EMITTER_DEATH,    nEnable2 | MF_BYCOMMAND);

                    // Get position and show popup
                    POINT pnt = {nmtb->rcButton.left, nmtb->rcButton.bottom};
                    ClientToScreen(control->hToolbar, &pnt);
                    INT id = TrackPopupMenuEx(hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pnt.x, pnt.y, hWnd, NULL);
                    switch (id)
                    {
                        case ID_NEW_EMITTER_ROOT:
                        case ID_NEW_EMITTER_LIFETIME:
                        case ID_NEW_EMITTER_DEATH:
                            SendMessage(hWnd, WM_COMMAND, MAKELONG(id, BN_CLICKED), (LPARAM)control->hToolbar);
                            break;
                    }
                    return TBDDRET_DEFAULT;
                }

                case TVN_BEGINLABELEDIT:
                {
                    // Workaround for bug in Knowledge Base item Q130691; subclass the edit control
                    HWND hEdit = TreeView_GetEditControl(control->hTree);
                    WNDPROC wndProc = (WNDPROC)(LONG_PTR)GetWindowLongPtr(hEdit, GWLP_WNDPROC);
                    SetProp(hEdit, L"Old_WindowProc", (HANDLE)wndProc);
                    SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG)(LONG_PTR)LabelEditProc);
                    break;
                }

                case TVN_ENDLABELEDIT:
                {
                    NMTVDISPINFO* nmtvdi = (NMTVDISPINFO*)lParam;
                    if (nmtvdi->item.pszText != NULL)
                    {
                        control->selection->name = WideToAnsi(nmtvdi->item.pszText);
                        TreeView_SetItem(control->hTree, &nmtvdi->item);
                        NotifyParent(control, ELN_LISTCHANGED);
                        NotifyParent(control, ELN_SELCHANGED);
                    }
                    break;
                }

				case TVN_KEYDOWN:
				{
					NMTVKEYDOWN* pnkd = (NMTVKEYDOWN*)lParam;
					switch (pnkd->wVKey)
					{
						case VK_F2:     EmitterList_RenameEmitter(hWnd); return 0;
						case VK_DELETE: EmitterList_DeleteEmitter(hWnd); return 0;
					}
					break;
                }

                default:
			        // Pass notification on
			        SendMessage(GetParent(hWnd), WM_NOTIFY, wParam, lParam);
                    break;
            }
			break;
		}

        case WM_SIZE:
        {
            RECT toolbar;
            GetClientRect(control->hToolbar, &toolbar);
            MoveWindow(control->hToolbar, 0, HIWORD(lParam) - toolbar.bottom, LOWORD(lParam), toolbar.bottom, TRUE);
            MoveWindow(control->hTree,    0, 0, LOWORD(lParam), HIWORD(lParam) - toolbar.bottom, TRUE);
            break;
        }
    }
    return FALSE;
}

static EmitterListControl* CreateEmitterListControl(HWND hOwner, HINSTANCE hInstance)
{
	EmitterListControl* control = new EmitterListControl;
	if (control != NULL)
	{
        control->selection = NULL;
		control->system    = NULL;
        control->hDialog   = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_EMITTER_LIST), hOwner, DlgEmitterListProc, (LPARAM)control);
        if (control->hDialog == NULL)
        {
            delete control;
            return NULL;
        }
        ShowWindow(control->hDialog, SW_SHOW);
	}
	return control;
}

static LRESULT CALLBACK EmitterListWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			control = CreateEmitterListControl(hWnd, pcs->hInstance);
			if (control == NULL)
			{
				return FALSE;
			}
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)control);
			break;
		}

		case WM_DESTROY:
			break;

		case WM_NOTIFY:
			// Pass notification on
			SendMessage(GetParent(hWnd), WM_NOTIFY, wParam, lParam);
			break;

		case WM_SIZE:
            MoveWindow(control->hDialog, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;

		case WM_SETFONT:
            SendMessage(control->hDialog, uMsg, wParam, lParam);
			break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void OnParticleSystemChange_AddChildren(HWND hTree, ParticleSystem* system, HTREEITEM hNode, const ParticleSystem::Emitter* emitter, set<size_t>& index)
{
	if (emitter->spawnOnDeath != -1 || emitter->spawnDuringLife != -1)
	{
		TVINSERTSTRUCT tvis;
		tvis.hParent        = hNode;
		tvis.hInsertAfter   = TVI_LAST;
		tvis.item.mask      = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvis.item.cChildren = 1;
		tvis.item.stateMask = TVIS_SELECTED;

		if (emitter->spawnDuringLife != -1)
		{
			index.erase(emitter->spawnDuringLife);
			const ParticleSystem::Emitter& onLife  = system->getEmitter(emitter->spawnDuringLife);
			wstring text = AnsiToWide(onLife.name);

            tvis.item.iImage  = GetTreeNodeIcon(system, onLife.index);
			tvis.item.pszText = (LPWSTR)text.c_str();
			tvis.item.lParam  = (LPARAM)&onLife;
			tvis.item.state   = 0;
            tvis.item.iSelectedImage = tvis.item.iImage;
			HTREEITEM hChild = TreeView_InsertItem(hTree, &tvis);
			OnParticleSystemChange_AddChildren(hTree, system, hChild, &onLife, index);
			TreeView_Expand(hTree, hChild, TVE_EXPAND);
		}

        if (emitter->spawnOnDeath != -1)
		{
			index.erase(emitter->spawnOnDeath);
			const ParticleSystem::Emitter& onDeath = system->getEmitter(emitter->spawnOnDeath);
			wstring text = AnsiToWide(onDeath.name);

            tvis.item.iImage  = GetTreeNodeIcon(system, onDeath.index);
			tvis.item.pszText = (LPWSTR)text.c_str();
            tvis.item.lParam  = (LPARAM)&onDeath;
			tvis.item.state   = 0;
            tvis.item.iSelectedImage = tvis.item.iImage;
			HTREEITEM hChild = TreeView_InsertItem(hTree, &tvis);
			OnParticleSystemChange_AddChildren(hTree, system, hChild, &onDeath, index);
			TreeView_Expand(hTree, hChild, TVE_EXPAND);
		}
    }
}

static void OnParticleSystemChange(EmitterListControl* control, ParticleSystem* system)
{
	// Fill the emitter list
	TreeView_DeleteAllItems(control->hTree);
    control->selection = NULL;
    if (system != NULL)
	{
		TVINSERTSTRUCT tvis;
		tvis.hParent        = NULL;
		tvis.hInsertAfter   = TVI_ROOT;
		tvis.item.mask      = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

		const std::vector<ParticleSystem::Emitter*>& emitters = system->getEmitters();
		std::set<size_t> index;
		for (size_t i = 0; i < emitters.size(); i++)
		{
			index.insert(i);
		}

		while (!index.empty())
		{
			size_t i = *index.begin();
            if (emitters[i]->parent == NULL)
            {
			    wstring name = AnsiToWide(emitters[i]->name);
                tvis.item.iImage    = GetTreeNodeIcon(system, i);
			    tvis.item.cChildren = 1;
			    tvis.item.pszText   = (LPWSTR)name.c_str();
			    tvis.item.lParam    = (LPARAM)emitters[i];
			    tvis.item.state     = (i == 0) ? TVIS_SELECTED : 0;
                tvis.item.iSelectedImage = tvis.item.iImage;

			    HTREEITEM hChild = TreeView_InsertItem(control->hTree, &tvis);
			    OnParticleSystemChange_AddChildren(control->hTree, system, hChild, emitters[i], index);
			    TreeView_Expand(control->hTree, hChild, TVE_EXPAND);

                if (control->selection == NULL)
                {
                    control->selection = emitters[i];
                    TreeView_SelectItem(control->hTree, hChild);
                }
            }
 			index.erase(index.begin());
		}
	}
}


void EmitterList_SetParticleSystem(HWND hWnd, ParticleSystem* system)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
        control->system = NULL;
        OnParticleSystemChange(control, system);
        control->system = system;
    }
}

void EmitterList_AddRootEmitter(HWND hWnd, const ParticleSystem::Emitter& emitter)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
    {
        ParticleSystem::Emitter* pEmitter = control->system->addRootEmitter(emitter);
        if (pEmitter != NULL)
        {
            HTREEITEM hItem = InsertTreeItem(control, NULL, pEmitter);
            control->selection = pEmitter;
            NotifyParent(control, ELN_LISTCHANGED);
            TreeView_SelectItem(control->hTree, hItem);
        }
    }
}

void EmitterList_AddLifetimeEmitter(HWND hWnd, const ParticleSystem::Emitter& emitter)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL && control->selection != NULL)
    {
        ParticleSystem::Emitter* pEmitter = control->system->addLifetimeEmitter(control->selection, emitter);
        if (pEmitter != NULL)
        {
            HTREEITEM hItem = InsertTreeItem(control, TreeView_GetSelection(control->hTree), pEmitter);
            control->selection = pEmitter;
            NotifyParent(control, ELN_LISTCHANGED);
            TreeView_SelectItem(control->hTree, hItem);
        }
    }
}

void EmitterList_AddDeathEmitter(HWND hWnd, const ParticleSystem::Emitter& emitter)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL && control->selection != NULL)
    {
        ParticleSystem::Emitter* pEmitter = control->system->addDeathEmitter(control->selection, emitter);
        if (pEmitter != NULL)
        {
            HTREEITEM hItem = InsertTreeItem(control, TreeView_GetSelection(control->hTree), pEmitter);
            control->selection = pEmitter;
            NotifyParent(control, ELN_LISTCHANGED);
            TreeView_SelectItem(control->hTree, hItem);
        }
    }
}

void EmitterList_DeleteEmitter(HWND hWnd)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL && control->selection != NULL)
    {
        control->system->deleteEmitter(control->selection);
        TreeView_DeleteItem(control->hTree, TreeView_GetSelection(control->hTree));
        if (control->system->getEmitters().empty())
        {
            control->selection = NULL;
        }
        NotifyParent(control, ELN_LISTCHANGED);
        NotifyParent(control, ELN_SELCHANGED);
    }
}

void EmitterList_RenameEmitter(HWND hWnd)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL && control->selection != NULL)
    {
        TreeView_EditLabel(control->hTree, TreeView_GetSelection(control->hTree));
    }
}

ParticleSystem::Emitter* EmitterList_GetSelection(HWND hWnd)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
        return control->selection;
    }
    return NULL;
}

static void EmitterList_SetAllEmitterVisibility(HWND hWnd, HTREEITEM hItem, bool visible)
{
    while (hItem != NULL)
    {
        // Get item
        TVITEM item;
        item.hItem = hItem;
        item.mask  = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        TreeView_GetItem(hWnd, &item);

        ParticleSystem::Emitter* emitter = (ParticleSystem::Emitter*)item.lParam;
        emitter->visible = visible;

        // Set new image
        item.iSelectedImage = item.iImage = (item.iImage % 4) + (emitter->visible ? 0 : 4);
        TreeView_SetItem(hWnd, &item);

        EmitterList_SetAllEmitterVisibility(hWnd, TreeView_GetChild(hWnd, hItem), visible);
        hItem = TreeView_GetNextSibling(hWnd, hItem);
    }
}

void EmitterList_SetAllEmitterVisibility(HWND hWnd, bool visible)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
        EmitterList_SetAllEmitterVisibility(control->hTree, TreeView_GetRoot(control->hTree), visible);
    }
}

void EmitterList_ToggleEmitterVisibility(HWND hWnd, HTREEITEM hItem)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
        if (hItem == NULL)
        {
            hItem = TreeView_GetSelection(control->hTree);
        }

        if (hItem != NULL)
        {
            // Get item
            TVITEM item;
            item.hItem = hItem;
            item.mask  = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
            TreeView_GetItem(control->hTree, &item);

            ParticleSystem::Emitter* emitter = (ParticleSystem::Emitter*)item.lParam;
            emitter->visible = !emitter->visible;

            // Set new image
            item.iSelectedImage = item.iImage = (item.iImage % 4) + (emitter->visible ? 0 : 4);
            TreeView_SetItem(control->hTree, &item);
        }
    }
}

void EmitterList_SelectionChanged(HWND hWnd)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
	{
        wstring name = AnsiToWide(control->system->getEmitter(control->selection->index).name);

		TVITEM item;
        item.hItem   = TreeView_GetSelection(control->hTree);
        item.mask    = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
        item.pszText = (LPWSTR)name.c_str();
        item.iImage  = GetTreeNodeIcon(control->system, control->selection->index);
        item.iSelectedImage = item.iImage;
        TreeView_SetItem(control->hTree, &item);
    }
}

bool EmitterList_HasFocus(HWND hWnd)
{
	EmitterListControl* control = (EmitterListControl*)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (control != NULL)
    {
        return GetFocus() == control->hTree;
    }
    return false;
}

bool EmitterList_Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof(WNDCLASSEX);
	wcx.style         = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc   = EmitterListWindowProc;
	wcx.cbClsExtra    = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = NULL;
	wcx.hCursor       = NULL;
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"EmitterList";
	wcx.hIconSm       = NULL;
	
	if (!RegisterClassEx(&wcx))
	{
		return false;
	}

    // Register clipboard format
    CF_PARTICLE_EMITTER = RegisterClipboardFormat(L"Alamo_ParticleEmitter");

	return true;
}
