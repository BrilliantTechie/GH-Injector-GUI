#include "pch.h"

#include "InjectionLib.h"

//macros xd

#define LOAD_INJECTION_FUNCTION(mod, name) m_##name = reinterpret_cast<f_##name>(GetProcAddress(mod, #name))
#define IS_VALID_FUNCTION_POINTER(name, v) if (m_##name == nullptr) \
{ \
	g_print("Failed to resolve %s\n", L#name); \
	return v; \
} \

InjectionLib::InjectionLib()
{
	m_hInjectionMod = NULL;

	m_InjectA						= nullptr;
	m_InjectW						= nullptr;
	m_ValidateInjectionFunctions	= nullptr;
	m_RestoreInjectionFunctions		= nullptr;
	m_GetVersionA					= nullptr;
	m_GetVersionW					= nullptr;
	m_GetSymbolState				= nullptr;
	m_GetDownloadProgress			= nullptr;
	m_SetRawPrintCallback			= nullptr;

	memset(m_HookInfo, 0, sizeof(m_HookInfo));

	m_Err		= 0;
	m_Win32Err	= 0;
	m_CountOut	= 0;
	m_Changed	= 0;
	m_TargetPID = 0;
}

InjectionLib::~InjectionLib()
{
	FreeLibrary(m_hInjectionMod);
}

bool InjectionLib::Init()
{
	m_hInjectionMod = LoadLibrary(GH_INJ_MOD_NAME);

	if (m_hInjectionMod == NULL)
	{
		g_print("Failed to load injection module: %08X\n", GetLastError());

		return false;
	}
	else
	{
		g_print("Injection module loaded at %p\n", m_hInjectionMod);
	}

	LOAD_INJECTION_FUNCTION(m_hInjectionMod, InjectA);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, InjectW);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, ValidateInjectionFunctions);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, RestoreInjectionFunctions);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, GetVersionA);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, GetVersionW);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, GetSymbolState);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, GetDownloadProgress);
	LOAD_INJECTION_FUNCTION(m_hInjectionMod, SetRawPrintCallback);

	return LoadingStatus();
}

bool InjectionLib::LoadingStatus()
{
	if (m_hInjectionMod == NULL)
	{
		return false;
	}

	IS_VALID_FUNCTION_POINTER(InjectA,						false);
	IS_VALID_FUNCTION_POINTER(InjectW,						false);
	IS_VALID_FUNCTION_POINTER(ValidateInjectionFunctions,	false);
	IS_VALID_FUNCTION_POINTER(RestoreInjectionFunctions,	false);
	IS_VALID_FUNCTION_POINTER(GetVersionA,					false);
	IS_VALID_FUNCTION_POINTER(GetVersionW,					false);
	IS_VALID_FUNCTION_POINTER(GetSymbolState,				false);
	IS_VALID_FUNCTION_POINTER(GetDownloadProgress,			false);
	IS_VALID_FUNCTION_POINTER(SetRawPrintCallback,			false);

	return true;
}

void InjectionLib::Unload()
{
	FreeLibrary(m_hInjectionMod);
}

DWORD InjectionLib::InjectA(INJECTIONDATAA * pData)
{
	IS_VALID_FUNCTION_POINTER(InjectA, (DWORD)-1);

	return m_InjectA(pData);
}

DWORD InjectionLib::InjectW(INJECTIONDATAW * pData)
{
	IS_VALID_FUNCTION_POINTER(InjectW, (DWORD)-1);

	return m_InjectW(pData);
}

bool InjectionLib::ValidateInjectionFunctions(int PID, std::vector<std::string> & hList)
{
	IS_VALID_FUNCTION_POINTER(ValidateInjectionFunctions, false);
	
	m_TargetPID = PID;

	auto val_ret = m_ValidateInjectionFunctions(m_TargetPID, m_Err, m_Win32Err, m_HookInfo, m_HookCount, &m_CountOut);

	if (!val_ret)
	{
		g_print("ValidateInjectionFUnctions failed:\n");
		g_print(" error code: %08X\n", m_Err);
		g_print(" win32 error: %08X\n", m_Win32Err);

		return false;
	}

	for (UINT i = 0; i != m_CountOut; ++i)
	{
		if (m_HookInfo[i].ChangeCount && !m_HookInfo[i].ErrorCode)
		{
			hList.push_back(std::string(m_HookInfo[i].ModuleName) + "->" + std::string(m_HookInfo[i].FunctionName));

			++m_Changed;
		}
	}

	return true;
}

bool InjectionLib::RestoreInjectionFunctions(std::vector<int> & IndexList)
{
	IS_VALID_FUNCTION_POINTER(RestoreInjectionFunctions, false);

	if (!IndexList.size())
	{
		g_print("Hook list is empty\n");

		return false;
	}

	HookInfo to_restore[m_HookCount]{ 0 };
	UINT counter = 0;

	for (auto i : IndexList)
	{
		to_restore[counter++] = m_HookInfo[i];
		g_print("Restoring (%d) %s\n", i, m_HookInfo[i].FunctionName);
	}

	auto res_ret = m_RestoreInjectionFunctions(m_TargetPID, m_Err, m_Win32Err, to_restore, m_CountOut, &m_CountOut);

	if (!res_ret)
	{
		g_print("RestoreInjectionFunctions failed:\n");
		g_print(" error code: %08X\n", m_Err);
		g_print(" win32 error: %08X\n", m_Win32Err);

		return false;
	}

	return true;
}

bool InjectionLib::GetSymbolState()
{
	IS_VALID_FUNCTION_POINTER(GetSymbolState, false);

	return (m_GetSymbolState() == 0);
}

float InjectionLib::GetDownloadProgress(bool bWow64)
{
	IS_VALID_FUNCTION_POINTER(GetDownloadProgress, 0.0f);

	return m_GetDownloadProgress(bWow64);
}

std::string InjectionLib::GetVersionA()
{
	IS_VALID_FUNCTION_POINTER(GetVersionA, std::string("0.0"));

	char szVersion[32]{ 0 };
	if (FAILED(m_GetVersionA(szVersion, sizeof(szVersion))))
	{
		return std::string("0.0");
	}

	return std::string(szVersion);
}

std::wstring InjectionLib::GetVersionW()
{
	IS_VALID_FUNCTION_POINTER(GetVersionW, std::wstring(L"0.0"));

	wchar_t szVersion[32]{ 0 };
	if (FAILED(m_GetVersionW(szVersion, sizeof(szVersion))))
	{
		return std::wstring(L"0.0");
	}

	return std::wstring(szVersion);
}

DWORD InjectionLib::SetRawPrintCallback(f_raw_print_callback callback)
{
	IS_VALID_FUNCTION_POINTER(SetRawPrintCallback, (DWORD)-1);

	return m_SetRawPrintCallback(callback);
}