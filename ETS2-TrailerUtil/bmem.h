#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <Windows.h>

#include <MinHook/MinHook.h>

#include "scs_logging.h"
using namespace scs_logging;

namespace bmem
{
	namespace
	{
		// custom byte type
		class pattern_byte
		{
		public:
			uint8_t byte;
			bool ignore;
		};

		inline uintptr_t moduleBase;
		inline uint64_t moduleSize;

		inline bool wasModuleSet = false;
	}

	static bool setModule(const char* module)
	{
		if (std::string(module) == "current")
		{
			if (!wasModuleSet)
				moduleBase = (uintptr_t)GetModuleHandleA(nullptr);
			else
				return true;
		}
		else
			moduleBase = (uintptr_t)GetModuleHandleA(module);


		if (!moduleBase)
		{
			printf("[BMEM] Failed to set module to: '%s'. Module not found! | Did you mean '%s.dll' or '%s.exe'?\n", module, module, module);
			return false;
		}

		const auto* header = (IMAGE_DOS_HEADER*)moduleBase;
		const auto* nt_header = (IMAGE_NT_HEADERS64*)((uint8_t*)header + header->e_lfanew);

		moduleSize = nt_header->OptionalHeader.SizeOfImage;

		printf("[BMEM] Module set to: '%s' (Base: 0x%llx, Size: %llu)\n", module, moduleBase, moduleSize);

		wasModuleSet = true;
		return true;
	}



	// A successful check is not proof of object identity or lifetime.
	static bool readable(uintptr_t address, size_t bytes)
	{
		if (!address || !bytes || address + bytes < address) return false;
		const uintptr_t end = address + bytes;
		while (address < end)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi))) return false;
			const DWORD p = mbi.Protect & 0xff;
			if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
				(p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY &&
				 p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY))
				return false;
			const auto next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
			if (next <= address) return false;
			address = next;
		}
		return true;
	}

	static uintptr_t patternScan(const char* patternSTR, const char* moduleToSet = "current")
	{
		if (!setModule(moduleToSet)) return 0;
		std::vector<pattern_byte> pattern;
		std::istringstream stream(patternSTR);
		std::string token;
		while (stream >> token)
		{
			pattern_byte pbyte{};
			pbyte.ignore = token == "?" || token == "??";
			if (!pbyte.ignore)
			{
				if (token.size() != 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return 0;
				unsigned int byte = 0;
				std::istringstream(token) >> std::hex >> byte;
				pbyte.byte = static_cast<uint8_t>(byte);
			}
			pattern.push_back(pbyte);
		}
		if (pattern.empty() || pattern.front().ignore) return 0;
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(moduleBase + dos->e_lfanew);
		const auto* section = IMAGE_FIRST_SECTION(nt);
		uintptr_t match = 0;
		for (unsigned int s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++section)
		{
			if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
			const auto rva = section->VirtualAddress;
			const auto size = section->Misc.VirtualSize;
			if (rva >= moduleSize || size > moduleSize - rva || size < pattern.size()) continue;
			const auto address = moduleBase + rva;
			if (!readable(address, size)) continue;
			const auto* data = reinterpret_cast<const uint8_t*>(address);
			for (size_t i = 0; i <= size - pattern.size(); ++i)
			{
				if (data[i] != pattern.front().byte) continue;
				size_t j = 1;
				while (j < pattern.size() && (pattern[j].ignore || data[i+j] == pattern[j].byte)) ++j;
				if (j != pattern.size()) continue;
				if (match)
				{
					scs_log(2, "Ambiguous signature: %s", patternSTR);
					return 0;
				}
				match = address + i;
			}
		}
		return match;
	}


	static uintptr_t relativeToAbsolute(uintptr_t address, int addressOffset, int instructionCount)
	{
		return (uintptr_t)(address + instructionCount + *reinterpret_cast<std::int32_t*>(address + addressOffset));
	}
	
	
	static bool isAddressValid(uintptr_t address, const char* moduleToSet = "current")
	{
		if (!moduleBase || address >= moduleBase + moduleSize)
			return false;

		if (address < moduleBase)
			return false;

		return true;
	}


	// returns: success | true = MH finished successfully | false = MH had a error
	static bool MH_Success(MH_STATUS result)
	{
		if (result == MH_OK) return true;

		if (result == MH_ERROR_ALREADY_INITIALIZED) scs_log(2, "[MinHook] MinHook is already initialized.");
		else if (result == MH_ERROR_NOT_INITIALIZED) scs_log(2, "[MinHook] MinHook is not initialized yet, or already uninitialized.");
		else if (result == MH_ERROR_ALREADY_CREATED) scs_log(2, "[MinHook] The hook for the specified target function is already created.");
		else if (result == MH_ERROR_NOT_CREATED) scs_log(2, "[MinHook] The hook for the specified target function is not created yet.");
		else if (result == MH_ERROR_ENABLED) scs_log(2, "[MinHook] The hook for the specified target function is already enabled.");
		else if (result == MH_ERROR_DISABLED) scs_log(2, "[MinHook] The hook for the specified target function is not enabled yet, or already disabled.");
		else if (result == MH_ERROR_NOT_EXECUTABLE) scs_log(2, "[MinHook] The specified pointer is invalid. It points the address of non-allocated and/or non-executable region.");
		else if (result == MH_ERROR_UNSUPPORTED_FUNCTION) scs_log(2, "[MinHook] The specified target function cannot be hooked.");
		else if (result == MH_ERROR_MEMORY_ALLOC) scs_log(2, "[MinHook] Failed to allocate memory.");
		else if (result == MH_ERROR_MEMORY_PROTECT) scs_log(2, "[MinHook] Failed to change the memory protection.");
		else if (result == MH_ERROR_MODULE_NOT_FOUND) scs_log(2, "[MinHook] The specified module is not loaded.");
		else if (result == MH_ERROR_FUNCTION_NOT_FOUND) scs_log(2, "[MinHook] The specified function is not found.");
		else if (result == MH_UNKNOWN) scs_log(2, "[MinHook] Unknown error.");

		return false;
	}
}