#pragma once

namespace Utils
{
	std::string GetProcessName(unsigned int m_uPID)
	{
		std::string m_sReturn = "";

		HANDLE m_hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (m_hSnapshot)
		{
			PROCESSENTRY32 m_pEntry32;
			m_pEntry32.dwSize = sizeof(PROCESSENTRY32);
			if (Process32First(m_hSnapshot, &m_pEntry32))
			{
				while (Process32Next(m_hSnapshot, &m_pEntry32))
				{
					if (m_pEntry32.th32ProcessID == m_uPID)
					{
						m_sReturn = m_pEntry32.szExeFile;
						break;
					}
				}
			}

			CloseHandle(m_hSnapshot);
		}

		return m_sReturn;
	}
}