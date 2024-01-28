
#include <System.h>
#include <Globals.h>
#include <Resource.h>
#include <dwmapi.h>

#include <vector>
#include <commctrl.h>
#include "Control.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <Library/PlutoVGWrap.h>
#include <RegistryConfig.h>

//#pragma comment(lib, "mincore.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Comctl32")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "shlwapi.lib")

#if defined(_M_ARM64)
	#pragma comment(lib, "plutosvg-arm64.lib")
#elif defined(_M_ARM)
	#pragma comment(lib, "plutosvg-arm.lib")
#elif defined(_M_X64)
	#pragma comment(lib, "plutosvg-x64.lib")
#else
	#pragma comment(lib, "plutosvg-x86.lib")
#endif

using namespace Nilesoft;
using namespace Nilesoft::Windows::Forms;
using namespace Nilesoft::Shell;

//constexpr const char _DllGetClassObject[] = { 'D','l','l','G','e','t','C','l','a','s','s','O','b','j','e','c','t' };
constexpr const wchar_t dll_name[] = L"shell.dll";

//typedef HRESULT (__stdcall *fDllGetClassObject)(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR *ppv);

BOOL CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#define ID_MAINWINDOW 100

// define id for close, minimize & demo buttons
#define ID_CLOSE 0x001
#define ID_MINIMIZE 0x002

#define ID_REG 0x003
#define ID_UNREG 0x004
#define ID_RESTART 0x005
#define ID_WEB 0x006
#define ID_DONATE 0x007
#define ID_EMAIL 0x008
#define ID_BUG 0x009
#define ID_REDDIT 0x00a
#define ID_TWITTER 0x00b
#define ID_FACEBOOK 0x00c
#define ID_DOCS 0x00d

/////////
#define SetWindowStyle(hwnd, style)	 ::SetWindowLong((hwnd), GWL_STYLE, (style))
#define SetWindowExStyle(hwnd, style)	::SetWindowLong((hwnd), GWL_EXSTYLE, (style))

#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))  // windowsx.h
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))  // windowsx.h

int __stdcall btn_on_paint(UI::Control *s, int, WPARAM, LPARAM);

LRESULT __stdcall WindowProc(HWND, UINT, WPARAM, LPARAM);

bool is_elevated = false;

// Global Variables:
HINSTANCE _hInstance;    // current instance
Logger *_log;
HANDLE _fonthandle = nullptr;
HFONT _hfont_icon = nullptr;
HFONT _hfont_icon2 = nullptr;

UINT _dpi = 96;
/*
 if (m_hwnd)
        {
            // Because the SetWindowPos function takes its size in pixels, we
            // obtain the window's DPI, and use it to scale the window size.
            float dpi = GetDpiForWindow(m_hwnd);

            SetWindowPos(
                m_hwnd,
                NULL,
                NULL,
                NULL,
                static_cast<int>(ceil(640.f * dpi / 96.f)),
                static_cast<int>(ceil(480.f * dpi / 96.f)),
                SWP_NOMOVE);
            ShowWindow(m_hwnd, SW_SHOWNORMAL);
            UpdateWindow(m_hwnd);
        }
*/

// https://stackoverflow.com/questions/5689904/gracefully-exit-explorer-programmatically
BOOL ExitExplorer()
{
	auto hWndTray = ::FindWindowExW(nullptr, nullptr, L"Shell_TrayWnd", nullptr);
	return hWndTray ? ::PostMessageW(hWndTray, 0x5B4, 0, 0) : FALSE;
}

template<typename T = long>
T dpi(auto value) { return static_cast<T>((value * _dpi) / 96); }

void DrawPng(HDC hdc, HBITMAP hbitmap, int x, int y, int size = 16, int sx = 0, int sy = 0, int alpha = 255)
{
	if(hdc && hbitmap)
	{
		auto memDC = ::CreateCompatibleDC(hdc);
		if(memDC)
		{
			auto prev_bitmap = ::SelectObject(memDC, hbitmap);
			BLENDFUNCTION bf{};
			bf.BlendOp = AC_SRC_OVER;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = (byte)alpha;
			bf.AlphaFormat = AC_SRC_ALPHA;
			::GdiAlphaBlend(hdc, x, y, size, size, memDC, sx, sy, size, size, bf);
			// Clean up
			::SelectObject(memDC, prev_bitmap);
			::DeleteDC(memDC);
		}
	}
}

bool IsKeyDown(int key)
{
    return (((::GetKeyState(key) >> 8) & 0xff) != 0);
}

bool IsAsyncKeyDown(int key)
{
    return (((::GetAsyncKeyState(key) >> 8) & 0xff) != 0);
}

bool KeyboardIsKeyDown(int key) noexcept
{
    // the return value is a SHORT (16 bits), not a 32 bit value
    // (in other words, 0x80000000 is not a valid bit mask)
    return (::GetKeyState(key) & 0x8000) != 0;
}

void ColorToHtmlCode(COLORREF rgb, LPWSTR strg, size_t count)
{
    StringCchPrintf(strg, count, L"#%02X%02X%02X", (int)GetRValue(rgb), (int)GetGValue(rgb), (int)GetBValue(rgb));
}

void GetCurrentMonitorResolution(HWND hwnd, int *pCXScreen, int *pCYScreen)
{
    HMONITOR const hMonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    ::GetMonitorInfoW(hMonitor, &mi);
    *pCXScreen = (mi.rcMonitor.right - mi.rcMonitor.left);
    *pCYScreen = (mi.rcMonitor.bottom - mi.rcMonitor.top);
}

// FullHD? =>   0:'==',   -1:'<',   +1:'>'
inline int IsFullHD(HWND hwnd, int resX, int resY)
{
	int cxScreen{}, cyScreen{};
	GetCurrentMonitorResolution(hwnd, &cxScreen, &cyScreen);
    if(resX <= 0)
        resX = cxScreen;
    if(resY <= 0)
        resY = cyScreen;
    return ((resX == 1920) && (resY == 1080)) ? 0 : (((resX < 1920) || (resY < 1080)) ? -1 : +1);
}

/*int PointSizeToFontHeight(const float fPtHeight, const HDC hdc)
{
    return -MulDiv(float2int(fPtHeight * 100.0f), GetDeviceCaps(hdc, LOGPIXELSY), 72 * SC_FONT_SIZE_MULTIPLIER);
}*/

inline void StrReplChrW(WCHAR *pStrg, const WCHAR chSearch, const WCHAR chReplace)
{
    while(pStrg && *pStrg)
    {
        if(*pStrg == chSearch)
            *pStrg = chReplace;
        ++pStrg;
    }
}

/*Routine Description: This routine returns TRUE if the caller's
process is a member of the Administrators local group. Caller is NOT
expected to be impersonating anyone and is expected to be able to
open its own process and process token.
Arguments: None.
Return Value:
   TRUE - Caller has Administrators local group.
   FALSE - Caller does not have Administrators local group. --
*/
BOOL IsUserAdmin()
{
	BOOL b;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;
	b = ::AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup);

	if(b)
	{
		if(!::CheckTokenMembership(NULL, AdministratorsGroup, &b))
		{
			b = FALSE;
		}
		::FreeSid(AdministratorsGroup);
	}

	return(b);
}

RECT rc_window{};
RECT rc_reg{};
RECT rc_unreg{};

POINT p{};

UI::Window *main_window = nullptr;
HBITMAP hbitmap_logo = nullptr;
//0xAA0000FF
COLORREF color_background = 0xffffff;


string loadstring(UINT id, HMODULE hmodule = nullptr)
{
	string str(MAX_PATH);
	auto size = ::LoadStringW(hmodule, id, str.buffer(), MAX_PATH);
	if(size > MAX_PATH)
	{
		size = ::LoadStringW(hmodule, id, str.buffer(size + 1), size);
	}
	return str.release(size).move();
}

BOOL EnablePrivilege()
{
	BOOL result = FALSE;
	HANDLE hToken {};
	if(::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		LUID luid {};
		TOKEN_PRIVILEGES tp {};
		if(::LookupPrivilegeValueW(nullptr, SE_TAKE_OWNERSHIP_NAME, &luid))
		{
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = luid;
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			result = ::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
		}
		::CloseHandle(hToken);
	}
	return result == TRUE && (::GetLastError() == ERROR_SUCCESS);
}

bool SetPermissions(HKEY root, const wchar_t *subkey, REGSAM reg_view = KEY_WOW64_64KEY)
{
	bool res = false;
	if(!EnablePrivilege())
		return false;

	auto_regkey Key;

	if(ERROR_SUCCESS != Key.open(root, subkey, 0, WRITE_OWNER | reg_view, Key))
		return false;

	PACL pOldDACL{}, pNewDACL{};
	std::vector<PSID> sid;
	std::vector<EXPLICIT_ACCESS> ea;
	SID_IDENTIFIER_AUTHORITY sia{ SECURITY_NT_AUTHORITY };
	PSID pSid{};

	if(!::AllocateAndInitializeSid(&sia, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSid))
	   return false;

	sid.push_back(pSid);

	if(ERROR_SUCCESS == ::SetSecurityInfo(Key.get(), SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION, pSid, nullptr, nullptr, nullptr))
	{
		Key.close();

		if(ERROR_SUCCESS != Key.open(root, subkey, 0, WRITE_DAC| reg_view, Key))
			return false;

		PSECURITY_DESCRIPTOR sd{};

		if(ERROR_SUCCESS == ::GetSecurityInfo(Key.get(), SE_REGISTRY_KEY,
											  DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOldDACL, nullptr, &sd))
		{

			if(::AllocateAndInitializeSid(&sia, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSid))
				sid.push_back(pSid);

			if(::AllocateAndInitializeSid(&sia, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &pSid))
				sid.push_back(pSid);

			ea.resize(sid.size());

			for(size_t i = 0; i < sid.size(); i++)
			{
				::ZeroMemory(&ea[i], sizeof(EXPLICIT_ACCESS));
				ea[i].grfAccessMode = GRANT_ACCESS;
				ea[i].grfAccessPermissions = GENERIC_ALL;
				ea[i].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
				ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
				ea[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
				ea[i].Trustee.ptstrName = reinterpret_cast<wchar_t*>(sid[i]);
			}

			if(ERROR_SUCCESS == ::SetEntriesInAclW(static_cast<ULONG>(ea.size()), ea.data(), pOldDACL, &pNewDACL))
			{
				res = ERROR_SUCCESS == ::SetSecurityInfo(Key.get(), SE_REGISTRY_KEY,
														 DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDACL, nullptr);
				::LocalFree(pNewDACL);
			}
			::LocalFree(sd);
		}
	}

	for(auto &s : sid)
		::FreeSid(s);

	return res;
}

//RRF_RT_REG_SZ | RRF_RT_REG_MULTI_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND
void disable_modern(bool _register)
{
	string k;
	k.format(L"CLSID\\%s",string::ToString(IID_FileExplorerContextMenu, 2).c_str());
	auto setval = [=](auto reg)->LSTATUS
	{
		auto_regkey key{};
		auto res = ::RegOpenKeyExW(HKEY_CLASSES_ROOT, k, 0, KEY_CREATE_SUB_KEY | KEY_WOW64_64KEY, key);
		if(res == ERROR_SUCCESS)
		{
			auto treatAs = L"TreatAs";
			if(reg)
			{
				auto_regkey subkey;
				res = key.create_key(treatAs, subkey, KEY_SET_VALUE | KEY_WOW64_64KEY);
				if(res == ERROR_SUCCESS)
				{
					//IID_ExplorerHostCreator
					string treatas = CLS_ContextMenu;
					res = subkey.set_value(nullptr, REG_SZ,
										   reinterpret_cast<const uint8_t *>(treatas.c_str()),
										   treatas.length<int>() * sizeof(wchar_t));
				}
			}
			else
			{
				key.delete_key(treatAs);
			}
		}
		return res;
	};

	//E_ACCESSDENIED, OLE_E_NOTRUNNING, WS_E_ENDPOINT_ACCESS_DENIED
	if(setval(_register) == ERROR_ACCESS_DENIED)
	{
		if(SetPermissions(HKEY_CLASSES_ROOT, k.c_str()))
			setval(_register);
	}
	/*
	IID treatAs{};
	auto hres = ::CoGetTreatAsClass(IID_WindowsUIFileExplorer, &treatAs);
	if(_register && treatAs != IID_ContextMenu)
		hres = ::CoTreatAsClass(IID_WindowsUIFileExplorer, IID_ContextMenu);
	else if(S_OK == hres && treatAs == IID_ContextMenu)
		hres = ::CoTreatAsClass(IID_WindowsUIFileExplorer, GUID_NULL);
	*/
}

//printf("Please wait shell we'll process your command \n");
// r	register
// u	unregister
// s	silent
// i	init
// re	restart restart explorer
// shx	cmh integrated Shell Extensions ContextMenuHandlers
// Register the COM server and the context menu handler.
bool Registration(int freg)
{
	auto ver = &Windows::Version::Instance();

	try
	{
		bool _register = freg & REG_REGISTER;
		bool _unregister = freg & REG_UNREGISTER;
		bool _treat = freg & REG_TREAT;
		bool _silent = freg & REG_SILENT;
		bool _restart = freg & REG_RESTART;

		//string dir = IO::Path::Parent(IO::Path::Module(_hInstance));
		//string dll_path = IO::Path::Combine(dir, dll_name).move();

		string path = IO::Path::Module(_hInstance).move();
		
		if(_register)
		{
			string dir = IO::Path::Parent(path).move();
			if(!dir.empty())
			{
				Security::Permission::SetFile(dir.c_str());
			}

			_log->close();
			//logger->reset();
			//IO::Path::Delete(logger->path());

		}

		if(!ver->IsWindows7OrGreater())
		{
			//windows compatibility
			_log->error(string::Extract(IDS_WINDOWS_COMPATIBILITY));
			return false;
		}

		string msg;

		if(_register || _unregister)
		{
			if(!is_elevated)
			{
				// Missing administrative privileges!
				msg = string::Extract(IDS_ADMIN_PRIVILEGES).move();
				_log->warning(msg);
				if(!_silent)
				{
					//You will need to provide administrator permission to run this Shell
					MessageBox::Show(msg, APP_FULLNAME, MessageBoxIcon::Warning);
				}
				return false;
			}

			if(_register)
			{
				//logger->create();

				if(path.length() > 3)
					path.remove_left(3).append(L"dll");

				if(!RegistryConfig::Register(path, freg | REG_CONTEXTMENU | REG_ICONOVERLAY))
				{
					msg = string::Extract(IDS_REGISTER_NOT_SUCCESS).move();
					_log->error(msg);
					if(!_silent)
					{
						MessageBox::Show(msg, APP_FULLNAME,
										 MessageBoxIcon::Error, MessageBoxButtons::OK);
					}
					return false;
				}
				
// HKEY_CLASSES_ROOT\LibraryFolder\ShellEx\ContextMenuHandlers
// HKEY_CLASSES_ROOT\LibraryFolder\background\shellex\ContextMenuHandlers
				// is windows 11 or later
				if(ver->IsWindows11OrGreater() && _treat)
				{
					//SOFTWARE\\Classes\\CLSID"
					disable_modern(true);
				}

				msg = string::Extract(IDS_REGISTER_SUCCESS).move();
			}
			else
			{
				if(!RegistryConfig::IsRegistered())
					return false;

				if(!RegistryConfig::Unregister())
				{
					msg = string::Extract(IDS_UNREGISTER_NOT_SUCCESS).move();
					_log->error(msg);
					if(!_silent)
					{
						MessageBox::Show(msg, APP_FULLNAME,
										 MessageBoxIcon::Error, MessageBoxButtons::OK);
					}
					return false;
				}

				// is windows 11 or later
				if(ver->IsWindows11OrGreater() && _treat)
					disable_modern(false);

				msg = string::Extract(IDS_UNREGISTER_SUCCESS).move();
			}

			if(ver->IsWindows8OrGreater())
				_log->info(msg);

			if(_restart)
			{
				if(!_silent)
				{
					msg += L"\n\n";
					msg += string::Extract(IDS_RESTART_EXPLORERQ).move();
					_restart = MessageBox::Show(msg, APP_FULLNAME,
											   MessageBoxIcon::Information,
											   MessageBoxButtons::OKCancel) == DialogResult::OK;
				}
			}

			if(_restart)
			{
				if(ver->IsWindows8OrGreater())
					_log->info(string::Extract(IDS_RESTART_EXPLORER).move());
				Windows::Explorer::Restart();
			}
			else
			{
				// https://docs.microsoft.com/en-us/windows/desktop/shell/reg-shell-exts#predefined-shell-objects
				// If you do not call SHChangeNotify, the change might not be recognized until the system is rebooted.
				//::SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);
				//Windows::Explorer::Refresh();
			}
			return true;
		}
		else if(_restart)
		{
			if(ver->IsWindows8OrGreater())
				_log->info(string::Extract(IDS_RESTART_EXPLORER).move());
			return Windows::Explorer::Restart();
		}
	}
	catch(...)
	{
#ifdef _DEBUG
		logger->exception(__func__);
#endif
	}
	return false;
}

bool Register(int freg, HWND hwnd = nullptr)
{
	string path = IO::Path::Combine(IO::Path::Parent(IO::Path::Module(nullptr)), dll_name).move();
	DLL dll(path, true);

	if((freg & REG_REGISTER) || (freg & REG_UNREGISTER))
	{
		if(!is_elevated)
		{
			// Missing administrative privileges!
			string msg = loadstring(IDS_ADMIN_PRIVILEGES, dll).move();
			//You will need to provide administrator permission to run this Shell
			::MessageBoxW(hwnd, msg, APP_FULLNAME, MB_ICONWARNING);
			return false;
		}
	}
	
	if(!dll)
	{
		MessageBox::Show(L"shell.dll not found.", APP_FULLNAME, MessageBoxIcon::Warning);
		return false;
	}

	auto ret = Registration(freg);
	_log->close();

	return ret;
}

class Console
{
	FILE *_handle = nullptr;
	HWND _hwnd = nullptr;

	bool init()
	{
		if(!_handle)
		{
			if(::AttachConsole(ATTACH_PARENT_PROCESS))
			{
				_hwnd = ::GetConsoleWindow();
				if(_hwnd) if(0 == ::freopen_s(&_handle, "CONOUT$", "w", stdout))
					::setvbuf(stdout, nullptr, _IONBF, 0);
			}
		}
		return _handle;
	}

public:
	Console()
	{
		init();
	}

	~Console()
	{
		if(_handle)
		{
			if(_hwnd)
				::PostMessageW(_hwnd, WM_KEYUP, VK_RETURN, 0);
			::fflush(_handle);
			::fclose(_handle);
			::FreeConsole();
		}
	}

	Console &write(const wchar_t *buff = nullptr)
	{
		wprintf(buff);
		return *this;
	}

	Console &writel(const wchar_t *buff = nullptr)
	{
		wprintf(L"%s%c", buff, L'\n');
		return *this;
	}
};

void check()
{
	Console c;
	c.writel(L"\n\nNilesoft Shell version 1.8.1\n");

	_log->info(L"BEGIN CHECK");

	auto ver = &Windows::Version::Instance();

	auto clsid =[](const string &s)->string
	{
		return (L"CLSID\\" + s).move();
	};

	auto clsid_shex = [=](const string &s)->string
	{
		return (s + L"\\shellex\\ContextMenuHandlers\\nilesoft.shell").move();
	};

	auto exists_key = [=](HKEY k, const string &name)
	{
		if(!Registry::Exists(k, name))
		{
			wprintf(L"[warning] %s\n", name.c_str());
			_log->warning(name);
		}
	};

	auto exists_value_name = [=](HKEY k, const string &subkey, const string &name)
	{
		if(!Registry::ExistsValue(k, subkey, name))
		{
			wprintf(L"[warning] %s\\%s\n", name.c_str(), name.c_str());
			_log->warning(subkey + L"\\" + name);
		}
	};

	exists_key(HKCR, clsid(CLS_ContextMenu));
	exists_key(HKCR, clsid(CLS_FolderExtensions));
	exists_key(HKCR, clsid(CLS_IconOverlay));

	if(ver->IsWindows11OrGreater())
	{
		exists_key(HKCR, string().format(L"CLSID\\%s\\TreatAs", string::ToString(IID_FileExplorerContextMenu, 2).c_str()));
	}

	exists_key(HKCR, clsid_shex(L"*"));
	exists_key(HKCR, clsid_shex(L"Directory"));
	exists_key(HKCR, clsid_shex(L"Directory\\Background"));
	exists_key(HKCR, clsid_shex(L"Drive"));
	exists_key(HKCR, clsid_shex(L"DesktopBackground"));

	exists_value_name(HKLM, HKLM_APPROVED, CLS_ContextMenu);
	
	_log->info(L"END CHECK");
}

/*
BLENDFUNCTION func={AC_SRC_OVER,0,(BYTE)alpha,AC_SRC_ALPHA};

	HDC hSrc=CreateCompatibleDC(NULL);
	HGDIOBJ bmp0=SelectObject(hSrc,m_Bitmap);
	POINT srcPos={0,0};
	UpdateLayeredWindow(m_hWnd,NULL,NULL,&m_Size,hSrc,&srcPos,0,&func,ULW_ALPHA);
	SelectObject(hSrc,bmp0);
	DeleteDC(hSrc);
*/ 

struct Theme
{
	int mode = 1;

	struct 
	{
		uint32_t nor = 0xF1F1F1;
		uint32_t sel = 0xF1F1F1;
		uint32_t dis = 0x808080;
	}text;

	struct
	{
		uint32_t nor = 0x000000;
		uint32_t sel = 0x181818;
		uint32_t dis = 0x000000;
	}back;

	struct{
		uint32_t color = 0x000000;
		uint32_t border = 0x404040;
		uint32_t size = 1;
	}frame;
};

Theme m_theme;

HTHEME _hTheme = nullptr;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	//auto argc = __argc;
	//auto argv = __wargv;

	is_elevated = Security::Elevation::IsElevated();

	COM_INITIALIZER com_initializer(true);

    CommandLine cmdline;
    int freg{};

	_hInstance = hInstance;
	_log = &Logger::Instance();

	if(cmdline.ShowHelp)
	{
		//-config:register
	   // MessageBox::Show(usage, APP_NAME, MessageBoxIcon::Information);
		//::DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_CMDLINE), nullptr, (DLGPROC)WndProc, 0);
		auto hWnd = ::CreateDialogParamW(_hInstance, MAKEINTRESOURCEW(IDD_CMDLINE), NULL, (DLGPROC)WndProc, 0);
		if(hWnd)
		{
			// CenterWindow(hwnd);
			::ShowWindow(hWnd, SW_SHOW);

			MSG msg;
			BOOL bRet;
			while((bRet = ::GetMessageW(&msg, NULL, 0, 0)) != 0)
			{
				if(bRet == -1)
				{
					// Handle the error and possibly exit
					break;
				}
				else if(!IsWindow(hWnd) || !::IsDialogMessageW(hWnd, &msg))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}
			::DestroyWindow(hWnd);
		}
		_log->close();
		return 0;
	}
	else if(cmdline.empty())
    {
		auto shell_window = ::FindWindowExW(nullptr, nullptr, UI::WC_Window, APP_FULLNAME);
		if (shell_window)
		{
			::SetForegroundWindow(shell_window);
			::SetActiveWindow(shell_window);
			//::SwitchToThisWindow(shell_window, TRUE);
			return 0;
		}

		auto win = Windows::Version::Instance();

		if(win.IsWindowsVersionOrGreater(10, 0, 14393))
			_dpi = DLL::User32<uint32_t>("GetSystemDpiForProcess", ::GetCurrentProcess());
		else
			_dpi = 96;// DC(GetDesktopWindow()).GetDeviceCapsY();

		//_dpi = ::GetSystemDpiForProcess(::GetCurrentProcess());

		_hTheme = ::OpenThemeData(nullptr, L"MENU");
	//	RunGlyphs();
		//return 0;
        UI::App app(_hInstance, WindowProc, m_theme.frame.color,
                    LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_RPMICON)), CS_HREDRAW | CS_VREDRAW);

        app.Init();

        RECT rc_screen{};
        ::GetClientRect(::GetDesktopWindow(), &rc_screen);

        rc_window.right = dpi(430);
        rc_window.bottom = dpi(220);

        rc_window.left = (rc_screen.right - rc_window.right) / 2;
        rc_window.top = (rc_screen.bottom - rc_window.bottom) / 2;

        main_window = new UI::Window(APP_FULLNAME, rc_window, 0, nullptr, 0);

        if(!main_window || !main_window->Handle)
        {
            return FALSE;
        }

		string path = IO::Path::Module(hInstance);
		path = path.substr(0, path.length() - 3).append(L"dll");

		DLL d(path, true);
		if(d)
		{
			auto hRes = ::FindResourceW(d, L"FONTICON", RT_RCDATA);
			if(hRes)
			{
				auto cjSize = ::SizeofResource(d, hRes);
				auto hResData = ::LoadResource(d, hRes);
				if(hResData)
				{
					auto lpFileView = ::LockResource(hResData);
					if(lpFileView)
					{
						DWORD numFonts = 0;
						_fonthandle = ::AddFontMemResourceEx(lpFileView, cjSize, nullptr, &numFonts);
						if(_fonthandle)
						{
							_hfont_icon = ::CreateFontW(-dpi(20), 0, 0, 0, FW_NORMAL, 0, 0, 0,
														SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
														ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Nilesoft.Shell");
						}
						UnlockResource(lpFileView);
					}
					::FreeResource(hResData);
				}
			}
		}

        main_window->Color.Background = color_background;

        int btn_w = dpi(250), btn_h = dpi(30), left = dpi(150);
		int offset_2 = dpi(2);

        rc_reg.top = dpi(40);
        rc_unreg.top = rc_reg.top + offset_2 + btn_h;

        rc_reg.bottom = btn_h;
        rc_unreg.bottom = btn_h;

        rc_reg.right = btn_w;
        rc_unreg.right = btn_w;

        rc_reg.left = left;
        rc_unreg.left = rc_reg.left;
		
		std::string dd = "<svg viewBox='0 0 512 512' width='100%' height='100%'><path fill='#14DCFE' d='M200 100L300 0l106 106-100 100z'/><path fill='#14A0FF' d='M100 200l100-100 212 212-100 100z'/><path fill='#1465FF' d='M106 406l100-100 106 106-100 100z'/></svg>";
		PlutoVG pluto(dd.data(), static_cast<int>(dd.length()), dpi(96), dpi(96), 96);
		hbitmap_logo = pluto.tobitmap();

		auto btn_close = new UI::Button(L"\uE256", { rc_window.right - dpi(27), dpi(1), dpi(26), dpi(26) }, ID_CLOSE, main_window, BS_OWNERDRAW, _hfont_icon, L"Close");
        
		auto btn_reg = new UI::Button(L"Register\tCtrl+R", rc_reg, ID_REG, main_window, BS_OWNERDRAW);
        auto btn_unreg = new UI::Button(L"Unregister\tCtrl+U", rc_unreg, ID_UNREG, main_window, BS_OWNERDRAW);
        auto btn_restart = new UI::Button(L"Restart Explorer\tCtrl+X", { rc_reg.left, rc_unreg.top + offset_2 + btn_h, btn_w, btn_h }, ID_RESTART, main_window, BS_OWNERDRAW);

        auto tt = btn_restart->Rect.top + dpi(30) + btn_h + offset_2;
        auto tl = btn_restart->Rect.left;

        auto btn_web = new UI::Button(L"\uE11F", { tl, tt, btn_h, btn_h }, ID_WEB, main_window, BS_OWNERDRAW, _hfont_icon,  L"Website Ctrl+W");
       
        tl += btn_h + offset_2;
        auto btn_email = new UI::Button(L"\uE115", { tl, tt, btn_h, btn_h }, ID_EMAIL, main_window, BS_OWNERDRAW, _hfont_icon, L"Email Ctrl+E");
       
        tl += btn_h + offset_2;
        auto btn_bug = new UI::Button(L"\uE0D4", { tl, tt, btn_h, btn_h }, ID_BUG, main_window, BS_OWNERDRAW, _hfont_icon, L"Bugs Ctrl+B");

		tl += (dpi(50) - btn_h) + btn_h + dpi(12);
        auto btn_donate = new UI::Button(L"\uE1A8", { tl, tt, btn_h, btn_h }, ID_DONATE, main_window, BS_OWNERDRAW, _hfont_icon, L"Donate Ctrl+D");

		tl += btn_h + offset_2;
		auto btn_reddit = new UI::Button(L"\uE23F", { tl, tt, btn_h, btn_h }, ID_REDDIT, main_window, BS_OWNERDRAW, _hfont_icon, L"Reddit Ctrl+I");

		tl += btn_h + offset_2;
		auto btn_t = new UI::Button(L"\uE242", { tl, tt, btn_h, btn_h }, ID_TWITTER, main_window, BS_OWNERDRAW, _hfont_icon, L"Twitter Ctrl+T");

        tl += btn_h + offset_2;
        auto btn_f = new UI::Button(L"\uE244", { tl, tt, btn_h, btn_h }, ID_FACEBOOK, main_window, BS_OWNERDRAW, _hfont_icon, L"Facebook Ctrl+F");

        main_window->SetColor({ btn_reg, btn_unreg,btn_restart,btn_donate,btn_web,btn_email,btn_bug, btn_f, btn_t , btn_reddit }, 
							  m_theme.text.nor, m_theme.back.nor, m_theme.text.sel, m_theme.back.sel);//0xeeeee0
        main_window->SetColor({ btn_close }, 0xFFFFFF, m_theme.back.nor, m_theme.text.nor, 0x2311E8);//E81123

		btn_reg->OnDraw = btn_unreg->OnDraw = btn_restart->OnDraw = btn_on_paint;

        auto ret = app.Run(main_window);

		delete main_window; 
		_log->close();


		if(_hfont_icon)
			DeleteObject(_hfont_icon);

		if(_fonthandle)
			::RemoveFontMemResourceEx(_fonthandle);
		
		if(_hTheme)
		{
			::CloseThemeData(_hTheme);
			_hTheme = nullptr;
		}

		return ret;
    }
    else
    {
		bool _check = false;
		//bool runglyphs = false;
        string cmd;
        for(auto op : cmdline)
        {
            if(op->Kind == '-' || op->Kind == '/')
            {
				if(op->has_name({ L"g", L"glyphs" }))
				{
					//runglyphs = true;
					//break;
				}
				else  if(op->has_name({ L"r", L"register", L"e", L"enable" }))
				{
                    freg |= REG_REGISTER;
                }
                else if(op->has_name({ L"u", L"unregister", L"d", L"disable" }))
                {
					freg |= REG_UNREGISTER;
                }
                else if(op->has_name({ L"t", L"treat" }))
                {
					freg |= REG_TREAT;
                }
				else if(op->has_name({ L"f", L"force" }))
				{
					freg |= REG_FOLDEREXTENSIONS;
				}
                else if(op->has_name({ L"s", L"silent" }))
                {
					freg |= REG_SILENT;
                }
                else if(op->has_name({ L"re",  L"restart" }))
                {
					freg |= REG_RESTART;
                }
                else if(op->has_name(L"cmd"))
                {
                    cmd = op->Value;
                }
            }
			else if(op->has_name(L"check"))
			{
				_check = true;
			}
        }

		if(_check)
		{
			check();
			return 0;
		}

		/*if(runglyphs)
		{
			RunGlyphs();
			return FALSE;
		}*/

		if((freg & REG_REGISTER) || (freg & REG_UNREGISTER) || (freg & REG_RESTART) || (freg & REG_FOLDEREXTENSIONS))
		{
			return Register(freg);
		}
        else
        {
            if(!cmd.empty())
            {
                int sw = 1;
                if(auto o = cmdline[L"wd"])
                    sw = (int)string::ToInt(o);

                int run = 1;
                if(auto o = cmdline[L"runas"])
                    run = (int)string::ToInt(o);

                string verb = L"open";
                if(run > 1)
                    verb = L"runas";

                ::ShellExecuteW(nullptr,
                                verb,
                                cmd,
                                cmdline[L"args"],
                                cmdline[L"wd"],
                                SW_SHOWNORMAL);
            }
        }
    }

	_log->close();
    return 1;
}

HFONT FontSize(HFONT hFont, int size, int weight = 0, byte quality= ANTIALIASED_QUALITY)
{
    LOGFONT lf{};
	if(::GetObject(hFont, sizeof(LOGFONT), &lf))
	{
		lf.lfHeight = -size;
		lf.lfWidth = 0;
		if(weight > 0)
			lf.lfWeight = weight;
		lf.lfQuality = quality;
		return ::CreateFontIndirect(&lf);
	}
    return nullptr;
}

bool hover = false;
using namespace Nilesoft::Drawing;


void DrawString(HDC hdc, HFONT hFont, RECT *rc, COLORREF color, const wchar_t *text, int length, DWORD format, uint8_t opacity)
{
	if(!_hTheme || !hdc || !hFont) return;

	BLENDFUNCTION bf = { AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA };
	BP_PAINTPARAMS params = { sizeof(BP_PAINTPARAMS) };
	params.dwFlags = BPPF_ERASE | BPPF_NOCLIP;
	params.pBlendFunction = &bf;
	HDC hdcPaint = nullptr;
	auto hBufferedPaint = ::BeginBufferedPaint(hdc, rc, BPBF_TOPDOWNDIB, &params, &hdcPaint);
	if(hBufferedPaint)
	{
		if(hdcPaint)
		{
			::SetTextColor(hdcPaint, color);
			::SetBkMode(hdcPaint, TRANSPARENT);

			auto hFontOld = ::SelectObject(hdcPaint, hFont);
			DTTOPTS dttOpts = { sizeof(DTTOPTS),  DTT_COMPOSITED | DTT_TEXTCOLOR, color };
			::BufferedPaintClear(hBufferedPaint, rc);
			::DrawThemeTextEx(_hTheme, hdcPaint, 0, 0, text, length, format, rc, &dttOpts);
			::SelectObject(hdcPaint, hFontOld);
		}
		::EndBufferedPaint(hBufferedPaint, TRUE);
	}
}

int __stdcall btn_on_paint(UI::Control* s, int msg, [[maybe_unused]] WPARAM wp, [[maybe_unused]] LPARAM lp)
{
	if(msg == WM_MOUSEMOVE)
		return 1;

	RECT rect{};
	::GetClientRect(s->Handle, &rect);

	DC dc(s->Handle);

	auto color_txt = msg == WM_MOUSEHOVER ? s->Color.Select.Text : s->Color.Text;
	auto color_bg = msg == WM_MOUSEHOVER ? s->Color.Select.Background : s->Color.Background;

	if(msg == WM_MOUSEHOVER || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
	{
		//if(wp & MK_LBUTTON)
		{
			color_bg = s->Color.Select.Background;
		}
	}

	// if(color_bg == -1)
   //      color_bg = c->Parent->Color.Select.Background;

	if(color_bg != -1)
	{
		auto br = ::CreateSolidBrush(color_bg);
		::FillRect(dc, &rect, br);
		::DeleteObject(br);
	}

	if(!s->Text.empty())
	{
		auto ofont = s->Font.Handle;
		LOGFONT lf{};
		GetObject(ofont, sizeof(LOGFONT), &lf);

		lf.lfQuality = CLEARTYPE_QUALITY;
		//lf.lfWeight = FW_SEMIBOLD;
		lf.lfHeight = -dpi(14);
		auto cfont = ::CreateFontIndirect(&lf);
		auto tf = DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;

		auto pos = s->Text.index_of('\t');
		if(pos != string::npos)
		{
			string left = s->Text.substr(0, pos);
			string right = s->Text.substr(pos++);

			auto rc = rect;
			rc.left = dpi(20);
			DrawString(dc, cfont, &rc, color_txt, left, left.length<int>(), DT_LEFT | tf, 255);

			rc.right -= dpi(20);
			DrawString(dc, cfont, &rc, m_theme.text.dis, right, right.length<int>(), DT_RIGHT | tf, 128);
		}
		else
		{
			DrawString(dc, cfont, &rect, color_txt, s->Text, -1, s->Font.Format.Align | tf, 255);
		}
		::DeleteObject(cfont);
	}
    return 0;
};

void Open(HWND hWnd, const wchar_t* cmd)
{
	::ShellExecuteW(hWnd, L"open", cmd, nullptr, nullptr, SW_NORMAL);
}

//https://www.nilesoft.org/download/update/?version=1.1.0.0
/*
The latest Shell version is 1.1.0.0. You are using version 1.1.0.0. [Click here to download the latest version][https://www.nilesoft.org/download].

Found a bug or have a suggestion? Please report it here on the official Shell GitHub page.

//https://www.nilesoft.org/download/update/
The latest Notepad3 version is 5.21.227.1. You are using version Unknown. Click here to download the latest version.
*/ 
//hWindow

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
		case WM_CREATE:
		{
			::SendMessageW(hWnd, WM_SETFONT, (WPARAM)::GetStockObject(DEFAULT_GUI_FONT), TRUE);
			break;
		}
        case WM_ACTIVATE:// Extend the frame into the client area.
        {
           // MARGINS margins{ 0,0,0,1 };
            //::DwmExtendFrameIntoClientArea(hWnd, &margins);
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            DC dc = BeginPaint(hWnd, &ps);
			{
				RECT r;
				::GetWindowRect(hWnd, &r);
				::OffsetRect(&r, -r.left,- r.top);
				dc.draw_fillrect(r, m_theme.frame.color, m_theme.frame.border);

				BITMAP bmp{};
				GetObject(hbitmap_logo, sizeof(BITMAP), &bmp);
				//draw logo
				DrawPng(dc, hbitmap_logo, dpi(35), dpi(35), bmp.bmHeight, 0, 0, is_elevated ? 240 : 128);

				dc.set_back_mode();
				string text = APP_NAME L" ";
#ifdef APP_CANARY
				text += APP_CANARY L" ";
#endif
				text += APP_VERSION;

				dc.set_text(0x808080);

				auto ofont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
				auto cfont = FontSize(ofont, dpi(14), CLEARTYPE_NATURAL_QUALITY);
				ofont = dc.set_font(cfont);

				RECT rect = { 0, dpi(150 + 10), dpi(150), dpi(25 + 128 + 32) };

				DrawString(dc, cfont, &rect, 0x808080, text, text.length<int>(), DT_CENTER, 255);

				//dc.draw_text(text, text.length<int>(), rect, DT_CENTER);
				text.format(L"\xA9 %d %s", VERSION_YEAR, APP_COMPANY);
				rect = { 0, dpi(150 + 30), dpi(150), dpi(25 + 128 + 32 + 15) };
				//dc.set_text(0x808080);
				//dc.draw_text(text, text.length<int>(), rect, DT_CENTER);
				DrawString(dc, cfont, &rect, 0x808080, text, text.length<int>(), DT_CENTER, 255);

				::DeleteObject(::SelectObject(dc, ofont));
				//DrawPng(hdc, hbitmap_res, 38, 25 + 128 + 32 + 5, 16, 16 * 13, 0, 128);
			}
            ::EndPaint(hWnd, &ps);
            return TRUE;
        }
        //case WM_ERASEBKGND:
        //    return 0;
        case WM_NCCALCSIZE:
            return FALSE;
        case WM_NCHITTEST:
            return HTCAPTION;
		case WM_ERASEBKGND:
			return TRUE;
        case WM_COMMAND:
        {
            switch(wParam)
            {
                case ID_CLOSE:
                    ::DestroyWindow(hWnd);
                    break;
                case ID_MINIMIZE:
                    SendMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, lParam);
                    break;
                case ID_WEB:
					Open(hWnd, APP_WEBSITE);
                    break;
                case ID_DONATE:
					Open(hWnd, L"https://nilesoft.org/donate");
                    break;
                case ID_EMAIL:
					Open(hWnd, L"mailto:support@nilesoft.org");
                    break;
                case ID_BUG:
					Open(hWnd, L"https://nilesoft.org/bugs");
                    break;
				case ID_REDDIT:
					Open(hWnd, L"https://reddit.com/u/moudeygo");
					break;
				case ID_TWITTER:
					Open(hWnd, L"https://twitter.com/moudey");
					break;
                case ID_FACEBOOK:
					Open(hWnd, L"https://facebook.com/moudeygo");
                    break;
				case ID_RESTART:
				{
					Windows::Explorer::Restart();
					//for(auto &process : Diagnostics::Process::EnumInfo())
					//{
					//	auto hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, process.th32ProcessID);
					//	if(hProcess)
					//	{
					//		if(string::Equals(process.szExeFile, "explorer.exe")) continue;
					//		//if(process.szExeFile Diagnostics::Process::ModuleBaseName("explorer.exe"))
					//		//MB(process.szExeFile);
					//		for(auto &module : Diagnostics::Process::Modules(hProcess))
					//		{
					//			string dll = Diagnostics::Process::ModuleBaseName(hProcess, module).move();
					//			if(dll.equals(L"shell.dll"))
					//			{
					//				DWORD dwNewThreadId;
					//				LPVOID freeLibrary= FreeLibraryAndExitThread;
					//				HANDLE hRemoteThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)freeLibrary, module, 0, nullptr);
					//				if(hRemoteThread)
					//				{
					//					/*WaitForSingleObject(hRemoteThread, INFINITE);
					//					DWORD lpExitCode = 0;
					//					::GetExitCodeThread(hRemoteThread, &lpExitCode);
					//					CloseHandle(hRemoteThread);*/
					//				}
					//				else {
					//					::MessageBox(0, L"Fail to build the remote thread \"FreeLibraryAndExitThread\"", L"Error during injection", MB_ICONERROR);
					//				}

					//				/*LPVOID dllPathAddressInRemoteMemory =
					//					::VirtualAllocEx(hProcess, NULL, sizeof module, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
					//				
					//				if(dllPathAddressInRemoteMemory)
					//				{
					//					BOOL succeededWriting =
					//						::WriteProcessMemory(hProcess, dllPathAddressInRemoteMemory, module, sizeof module, NULL);
					//					
					//					if(succeededWriting)
					//					{
					//						LPVOID freeLibraryAndExitThreadAddress =
					//							(LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibraryAndExitThread");
					//						if(freeLibraryAndExitThreadAddress)
					//						{
					//							HANDLE remoteThread =
					//								CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)freeLibraryAndExitThreadAddress, module, NULL, NULL);

					//							if(remoteThread)
					//							{
					//								WaitForSingleObject(remoteThread, INFINITE);
					//								CloseHandle(remoteThread);
					//								MB(process.szExeFile);
					//							}
					//						}
					//					}
					//					else {
					//						MBF(L"not succeededWriting '%s'", process.szExeFile);
					//					}
					//					VirtualFreeEx(hProcess, dllPathAddressInRemoteMemory, sizeof module, MEM_RELEASE);
					//						
					//				}
					//				*/
					//				//MB(Diagnostics::Process::ModuleFileName(hProcess).c_str());
					//				//proc.append_format(L"%d %s\n", process.th32ProcessID, Diagnostics::Process::ModuleFileName(hProcess).c_str());
					//				break;
					//			}
					//		}
					//		::CloseHandle(hProcess);
					//	}
					//}
					//Windows::Explorer::Restart();
					//ExitExplorer();
					break;
				}
                case ID_REG:
				case ID_UNREG:
			//	case ID_RESTART:
				{
					int freg = 0;
					if(wParam == ID_REG)
						freg |= REG_REGISTER | REG_TREAT;
					else if(wParam == ID_UNREG)
						freg |= REG_UNREGISTER | REG_TREAT;
					//else if(wParam == ID_RESTART)
					//	reg.Restart = true;
					return Register(freg, hWnd);
				}
            }
            break;
        }
        case WM_SYSKEYUP:
        case WM_KEYUP:
      //      return ::DefWindowProc(hWnd, message, wParam, lParam);
     //   case WM_SYSKEYDOWN:
     //   case WM_KEYDOWN:
        {
            if(::GetAsyncKeyState(VK_CONTROL) < 0)
            {
                //  int count = 0;
                //  for(int i = 0; i < 256; i++)
                //      count += IsAsyncKeyDown(i);

                //  if(count == 2)
                {
                    auto vkCode = LOWORD(wParam); // virtual-key code
                    switch(vkCode)
                    {
                        case 'R':
                            main_window->SendCommand(ID_REG);
                            break;
                        case 'U':
                            main_window->SendCommand(ID_UNREG);
                            break;
                        case 'X':
                            main_window->SendCommand(ID_RESTART);
                            break;
                        case 'W':
                            main_window->SendCommand(ID_WEB);
                            break;
                        case 'E':
                            main_window->SendCommand(ID_EMAIL);
                            break;
                        case 'B':
                            main_window->SendCommand(ID_BUG);
                            break;
                        case 'D':
                            main_window->SendCommand(ID_DONATE);
                            break;
                        case 'F':
                            main_window->SendCommand(ID_FACEBOOK);
                            break;
                        case 'T':
                            main_window->SendCommand(ID_TWITTER);
                            break;
						case 'I':
							main_window->SendCommand(ID_REDDIT);
							break;
                    }
                }
            }
            break;
        }
        case WM_CLOSE:
            ::DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
        case WM_ENDSESSION:
            ::PostQuitMessage(0);
            break;
        default:
            return ::DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, [[maybe_unused]] LPARAM lParam)
{
    switch(message)
    {
		case WM_INITDIALOG:
        {
			//WCHAR szTitle[100];
			// LoadString(g_hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));
            auto usage = L"Command-Line Help\r\n\r\n"
                //L"shell.exe [-[r][u]] [-i] [-s] [-re]\n\n"
                L"-register\t\tRegistering.\r\n"
                L"-unregister\tUnregistering.\r\n"
                L"-treat\t\tDisable Windows 11 context menu.\r\n"
                L"-silent\t\tPrevents displaying messages.\r\n"
                L"-restart\t\tRestart Windows Explorer.\r\n\r\n"
                //L"-runas:N\t\tLaunch with elevated privileges.\r\n"
                //L"\t\tN=[admin | system | trustedinsaller]\r\n\r\n"
                L"-?\t\tDispay this help message.\r\n\r\n"
                L"Examples:\r\nshell.exe -register -treat\r\n"
              //  L"shell.exe -runas:admin -cmd:'cmd.exe' -args:\"/K echo Hello world!\"\r\n"
                ;

            ::SetDlgItemTextW(hwnd, IDC_CMDLINE_TEXT, usage);
           // SendDlgItemMessage(hwnd, IDC_CMDLINE_TEXT, EM_SETSEL, -1, -1);
            break;
        }
        case WM_CLOSE:
        {
            ::PostQuitMessage(0);
            break;
        }
        case WM_COMMAND:
        {   
            if(LOWORD(wParam) == IDOK)
                ::PostQuitMessage(0);
            break;
        }
       
    }
    return FALSE;
}
