#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

#include "3rdParty/MinHook/MinHook.h"

#include "Utils.hpp"
#include "VersionHijack.hpp"

namespace Globals
{
    DWORD m_dProcesssID = 0;
    HMODULE m_hModule = 0;
    std::string m_sProcessPath(MAX_PATH, '\0');
    std::string m_sProcessName = "";

    HWND m_hWindow = 0;
}

namespace Program
{
    bool m_bWasManuallyStarted = true;
    bool m_bDisableNextWindowNameCheck = false;
    std::string m_sRestartedArg = "--ad_restart";

    void MediaPlayPause()
    {
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 1, 0);
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    }

    void RestartSpotify()
    {
        // Start New Process
        {
            STARTUPINFO m_sStartupInfo;
            ZeroMemory(&m_sStartupInfo, sizeof(m_sStartupInfo));
            m_sStartupInfo.cb = sizeof(STARTUPINFO);

            PROCESS_INFORMATION m_pProcessInfo;

            std::string m_sCommandLine = "\"" + Globals::m_sProcessPath + "\" " + m_sRestartedArg;
            if (!CreateProcessA(0, &m_sCommandLine[0], 0, 0, 0, CREATE_NEW_CONSOLE, 0, 0, &m_sStartupInfo, &m_pProcessInfo))
                return;

            CloseHandle(m_pProcessInfo.hProcess);
            CloseHandle(m_pProcessInfo.hThread);
        }

        TerminateProcess(GetCurrentProcess(), 0);
    }

    bool IsMainProcess()
    {
        std::string m_sCommandLine = GetCommandLineA();
        if (!m_sCommandLine.empty())
        {
            if (m_sCommandLine.find(m_sRestartedArg) != std::string::npos)
                m_bWasManuallyStarted = false;
            else
            {
                size_t m_sExecutablePos = m_sCommandLine.find(".exe");
                if (m_sExecutablePos != std::string::npos)
                {
                    size_t m_sCommandLineSizeDelta = m_sCommandLine.size() - m_sExecutablePos - 4;
                    if (m_sCommandLineSizeDelta > 3)
                        return false;
                }
            }
        }

        return true;
    }

    DWORD __stdcall AdvertisementRestart(void* m_pReserved)
    {
        Sleep(100);
        Program::MediaPlayPause();
        Sleep(500);

        Shell_NotifyIconW(NIM_DELETE, reinterpret_cast<PNOTIFYICONDATAW>(m_pReserved));

        Program::RestartSpotify();

        return 0;
    }
}

namespace HookFuncs
{
    void* m_pNotifyIconData = nullptr;

    typedef BOOL(__stdcall* m_tShell_NotifyIconW)(DWORD, void*); m_tShell_NotifyIconW m_oShell_NotifyIconW;
    BOOL __stdcall Shell_NotifyIconW_(DWORD m_dMessage, void* m_pData)
    {
        m_pNotifyIconData = m_pData;

        return m_oShell_NotifyIconW(m_dMessage, m_pData);
    }

    typedef BOOL(__stdcall* m_tSetForegroundWindow)(HWND); m_tSetForegroundWindow m_oSetForegroundWindow;
    BOOL __stdcall SetForegroundWindow_(HWND m_hWindow)
    {
        Globals::m_hWindow = m_hWindow;

        if (!Program::m_bWasManuallyStarted)
            return TRUE;

        return m_oSetForegroundWindow(m_hWindow);
    }

    typedef int(__stdcall* m_tSetWindowTextW)(HWND, LPCWSTR); m_tSetWindowTextW m_oSetWindowTextW;
    int __stdcall SetWindowTextW_(HWND m_hWindow, LPCWSTR m_pWideString)
    {
        if (wcscmp(m_pWideString, L"Advertisement") == 0)
        {
            if (Program::m_bDisableNextWindowNameCheck)
                Program::m_bDisableNextWindowNameCheck = false;
            else
                CreateThread(0, 0, Program::AdvertisementRestart, m_pNotifyIconData, 0, 0);
        }
        else if (!wcsstr(m_pWideString, L"-"))
            m_pWideString = L"Spotify & sneakyevil's AD Bypass";

        return m_oSetWindowTextW(m_hWindow, m_pWideString);
    }
}

namespace Hooks
{
    void AddExport(const char* m_pModule, const char* m_pName, void* m_pHook, void** m_pOriginal = nullptr)
    {
        HMODULE m_hModule = LoadLibraryA(m_pModule);
        if (!m_hModule)
            return;

        void* m_pFunction = reinterpret_cast<void*>(GetProcAddress(m_hModule, m_pName));
        if (m_pFunction)
            MH_CreateHook(m_pFunction, m_pHook, m_pOriginal);
    }

    void Init()
    {
        MH_Initialize();

        {
            AddExport("Shell32",        "Shell_NotifyIconW",        HookFuncs::Shell_NotifyIconW_,          (void**)&HookFuncs::m_oShell_NotifyIconW);
            AddExport("User32",         "SetForegroundWindow",      HookFuncs::SetForegroundWindow_,        (void**)&HookFuncs::m_oSetForegroundWindow);
            AddExport("User32",         "SetWindowTextW",           HookFuncs::SetWindowTextW_,             (void**)&HookFuncs::m_oSetWindowTextW);
        }

        MH_EnableHook(MH_ALL_HOOKS);
    }

    void Uninit()
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
}

DWORD __stdcall MainThread(void* m_pReserved)
{
    while (!Program::m_bWasManuallyStarted)
    {
        Sleep(500);
        if (Globals::m_hWindow)
        {
            Program::m_bDisableNextWindowNameCheck = true;
            Program::MediaPlayPause();
            break;
        }
    }

    return 0;
}

int __stdcall DllMain(HMODULE m_hModule, DWORD m_dReason, void* m_pReserved)
{
    if (m_dReason == DLL_PROCESS_ATTACH)
    {
        sneakyevil_DllHijack::Initialize();

        if (!Program::IsMainProcess())
            return 1;

        // Globals
        {
            Globals::m_dProcesssID = GetCurrentProcessId();
            Globals::m_hModule = m_hModule;
            Globals::m_sProcessPath.resize(GetModuleFileNameA(0, &Globals::m_sProcessPath[0], Globals::m_sProcessPath.size()));

            size_t m_sProcessPathLastDelimer = Globals::m_sProcessPath.find_last_of("\\/");
            if (m_sProcessPathLastDelimer != std::string::npos)
            {
                m_sProcessPathLastDelimer++;
                Globals::m_sProcessName = Globals::m_sProcessPath.substr(m_sProcessPathLastDelimer, Globals::m_sProcessPath.size() - m_sProcessPathLastDelimer);
            }
        }


        Hooks::Init();
        CreateThread(0, 0, MainThread, 0, 0, 0);
    }

    return 1;
}