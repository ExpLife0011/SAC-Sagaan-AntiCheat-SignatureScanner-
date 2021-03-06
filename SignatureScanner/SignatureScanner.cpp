#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <string>
#include <Windows.h>
#include <Psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <tlhelp32.h>

#define INRANGE(x,a,b)		(x >= a && x <= b) 
#define GET_BITS( x )		(INRANGE(x,'0','9') ? (x - '0') : ((x&(~0x20)) - 'A' + 0xa))
#define GET_BYTE( x )		(GET_BITS(x[0]) << 4 | GET_BITS(x[1]))

inline bool is_match(const uint8_t* addr, const uint8_t* pat, const uint8_t* msk)
{
	size_t n = 0;
	while (addr[n] == pat[n] || msk[n] == (uint8_t)'?') {
		if (!msk[++n]) {
			return true;
		}
	}
	return false;
}

inline uint8_t* find_pattern(uint8_t* range_start, const uint32_t len, const char* pattern)
{
	size_t l = strlen(pattern);
	uint8_t* patt_base = static_cast<uint8_t*>(_alloca(l >> 1));
	uint8_t* msk_base = static_cast<uint8_t*>(_alloca(l >> 1));
	uint8_t* pat = patt_base;
	uint8_t* msk = msk_base;
	l = 0;
	while (*pattern) {
		if (*(uint8_t*)pattern == (uint8_t)'\?') {
			*pat++ = 0;
			*msk++ = '?';
			pattern += ((*(uint16_t*)pattern == (uint16_t)'\?\?') ? 3 : 2);
		}
		else {
			*pat++ = GET_BYTE(pattern);
			*msk++ = 'x';
			pattern += 3;
		}
		l++;
	}
	*msk = 0;
	pat = patt_base;
	msk = msk_base;
	for (uint32_t n = 0; n < (len - l); ++n)
	{
		if (is_match(range_start + n, patt_base, msk_base)) {
			return range_start + n;
		}
	}
	return nullptr;
}

inline void open_binary_file(const std::string & file, std::vector<uint8_t>& data)
{
	std::ifstream file_stream(file, std::ios::binary);
	file_stream.unsetf(std::ios::skipws);
	file_stream.seekg(0, std::ios::end);

	const auto file_size = file_stream.tellg();

	file_stream.seekg(0, std::ios::beg);
	data.reserve(static_cast<uint32_t>(file_size));
	data.insert(data.begin(), std::istream_iterator<uint8_t>(file_stream), std::istream_iterator<uint8_t>());
}

DWORD GetProcId(char* ProcName)
{
	PROCESSENTRY32   pe32;
	HANDLE         hSnapshot = NULL;

	pe32.dwSize = sizeof(PROCESSENTRY32);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (Process32First(hSnapshot, &pe32))
	{
		do {
			if (strcmp(pe32.szExeFile, ProcName) == 0)
				break;
		} while (Process32Next(hSnapshot, &pe32));
	}

	if (hSnapshot != INVALID_HANDLE_VALUE)
		CloseHandle(hSnapshot);

	return pe32.th32ProcessID;
}

const std::vector<std::pair<std::string, const char*>> signatures = {
	{ "chams material",				"89 47 10 8B C7 83 FA 10 72 02 8B 07 53 FF 75 08 03 F0" },
	{ "interfaces loaded",			"49 6E 74 65 72 66 61 63 65 73 20 4C 6F 61 64 65 64 00" },
	{ "xml shit",					"58 4D 4C 5F 53 55 43 43 45" },
	{ "reflective loader",			"52 65 66 6C 65 63 74 69 76 65 4C 6F 61 64" },
	{ "chams shit",					"22 25 73 22 09 09 0A 7B 09 09 0A 09 22 24 62 61 73 65 74 65 78 74 75 72 65 22 20 22 76 67 75" },
	{ "bomb esp",					"42 6F 6D 62 3A 20 25 2E 31 66 00" },
	{ "perfecthook entry point",	"55 8B EC E8 ? ? ? ? 8B 0D ? ? ? ? E8" }
};

int main(const int argc, char** argv)
{
	while (1)
	{
		std::vector<uint8_t> data;

		HMODULE hMods[1024];
		HANDLE hProcess;
		DWORD cbNeeded;
		unsigned int i;


		// Get a handle to the process.

		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
			PROCESS_VM_READ,
			FALSE, GetProcId("csgo.exe"));

		if (NULL == hProcess)
			return 1;

		// Get a list of all the modules in this process.

		if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
		{
			for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
			{
				TCHAR szModName[MAX_PATH];

				// Get the full path to the module's file.

				if (GetModuleFileNameEx(hProcess, hMods[i], szModName,
					sizeof(szModName) / sizeof(TCHAR)))
				{
					// Print the module name and handle value.a

					open_binary_file(szModName, data);

			

					auto hack_features = 0;

					for (auto & entry : signatures)
					{
						const auto offset = find_pattern(data.data(), data.size(), entry.second);
						if (offset != nullptr)
						{
							printf("[!] found %s\n", entry.first.c_str());
							hack_features++;
						}
					}

					printf("[+] %i/%i hack traits detected\n", hack_features, signatures.size());

					if (hack_features)
					{
						system("pause");
					}

					std::cout << szModName << std::endl;
				}
				Sleep(3);
			}
		}

		// Release the handle to the process.

		CloseHandle(hProcess);

		system("pause");
	}
}