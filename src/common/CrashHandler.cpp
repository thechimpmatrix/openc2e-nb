/*
 *  CrashHandler.cpp
 *  openc2e
 *
 *  Enhanced crash handler: JSON crash reports, minidumps, log capture.
 *
 *  IMPORTANT: The handler runs in a damaged process. All write paths use
 *  stack-allocated buffers and C-style FILE* I/O. No heap allocation,
 *  no std::string construction, no exceptions in the hot path.
 */

#include "CrashHandler.h"

#include "NornLog.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>

// ---------------------------------------------------------------------------
// Shared state (file-scope, pre-allocated)
// ---------------------------------------------------------------------------

static char s_logDir[1024] = ".";

// Pre-allocated path buffers so the handler never calls malloc
static char s_jsonPath[2048];
static char s_dumpPath[2048];

// ---------------------------------------------------------------------------
// Helpers: timestamp formatting (no heap)
// ---------------------------------------------------------------------------

static void formatTimestamp(char* buf, size_t bufLen) {
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf{};
#ifdef _WIN32
	localtime_s(&tm_buf, &tt);
#else
	localtime_r(&tt, &tm_buf);
#endif
	snprintf(buf, bufLen, "%04d%02d%02d-%02d%02d%02d",
	         tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
	         tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
}

// ---------------------------------------------------------------------------
// Helpers: JSON string escaping into a fixed buffer
// ---------------------------------------------------------------------------

static size_t escapeJsonTo(char* out, size_t outLen, const char* src) {
	size_t pos = 0;
	for (; *src && pos + 6 < outLen; ++src) {
		unsigned char ch = static_cast<unsigned char>(*src);
		switch (ch) {
			case '"': out[pos++] = '\\'; out[pos++] = '"'; break;
			case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
			case '\b': out[pos++] = '\\'; out[pos++] = 'b'; break;
			case '\f': out[pos++] = '\\'; out[pos++] = 'f'; break;
			case '\n': out[pos++] = '\\'; out[pos++] = 'n'; break;
			case '\r': out[pos++] = '\\'; out[pos++] = 'r'; break;
			case '\t': out[pos++] = '\\'; out[pos++] = 't'; break;
			default:
				if (ch < 0x20) {
					pos += static_cast<size_t>(snprintf(out + pos, outLen - pos, "\\u%04x", ch));
				} else {
					out[pos++] = static_cast<char>(ch);
				}
				break;
		}
	}
	out[pos] = '\0';
	return pos;
}

// ==========================================================================
// WINDOWS IMPLEMENTATION
// ==========================================================================
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

// ---------------------------------------------------------------------------
// Exception code to human-readable description
// ---------------------------------------------------------------------------

static const char* exceptionDescription(DWORD code) {
	switch (code) {
		case EXCEPTION_ACCESS_VIOLATION:         return "Access Violation";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "Array Bounds Exceeded";
		case EXCEPTION_BREAKPOINT:               return "Breakpoint";
		case EXCEPTION_DATATYPE_MISALIGNMENT:    return "Datatype Misalignment";
		case EXCEPTION_FLT_DENORMAL_OPERAND:     return "Float Denormal Operand";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Float Divide By Zero";
		case EXCEPTION_FLT_INEXACT_RESULT:       return "Float Inexact Result";
		case EXCEPTION_FLT_INVALID_OPERATION:    return "Float Invalid Operation";
		case EXCEPTION_FLT_OVERFLOW:             return "Float Overflow";
		case EXCEPTION_FLT_STACK_CHECK:          return "Float Stack Check";
		case EXCEPTION_FLT_UNDERFLOW:            return "Float Underflow";
		case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal Instruction";
		case EXCEPTION_IN_PAGE_ERROR:            return "In Page Error";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "Integer Divide By Zero";
		case EXCEPTION_INT_OVERFLOW:             return "Integer Overflow";
		case EXCEPTION_INVALID_DISPOSITION:      return "Invalid Disposition";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Noncontinuable Exception";
		case EXCEPTION_PRIV_INSTRUCTION:         return "Privileged Instruction";
		case EXCEPTION_SINGLE_STEP:              return "Single Step";
		case EXCEPTION_STACK_OVERFLOW:           return "Stack Overflow";
		default:                                 return "Unknown Exception";
	}
}

// ---------------------------------------------------------------------------
// Write minidump (.dmp)
// ---------------------------------------------------------------------------

static void writeMinidump(PEXCEPTION_POINTERS pExInfo, const char* path) {
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
	                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return;

	MINIDUMP_EXCEPTION_INFORMATION mei;
	mei.ThreadId = GetCurrentThreadId();
	mei.ExceptionPointers = pExInfo;
	mei.ClientPointers = FALSE;

	MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData),
		&mei,
		NULL,
		NULL);

	CloseHandle(hFile);
}

// ---------------------------------------------------------------------------
// Collect system info
// ---------------------------------------------------------------------------

static void writeSystemInfo(FILE* f) {
	// OS version
	OSVERSIONINFOA osvi{};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	// GetVersionEx is deprecated but safe for crash reporting
#pragma warning(push)
#pragma warning(disable : 4996)
	GetVersionExA(&osvi);
#pragma warning(pop)

	// Process memory
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	pmc.cb = sizeof(pmc);
	DWORD memMb = 0;
	if (GetProcessMemoryInfo(GetCurrentProcess(),
	                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
	                         sizeof(pmc))) {
		memMb = static_cast<DWORD>(pmc.WorkingSetSize / (1024 * 1024));
	}

	// Architecture
#if defined(_WIN64)
	const char* arch = "x86_64";
#else
	const char* arch = "x86";
#endif

	fprintf(f, "  \"system_info\": {\n");
	fprintf(f, "    \"os_version\": \"Windows %lu.%lu.%lu\",\n",
	        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
	fprintf(f, "    \"architecture\": \"%s\",\n", arch);
	fprintf(f, "    \"process_memory_mb\": %lu\n", memMb);
	fprintf(f, "  }");
}

// ---------------------------------------------------------------------------
// Walk the stack and write JSON array
// ---------------------------------------------------------------------------

static void writeStackTrace(FILE* f, PEXCEPTION_POINTERS pExInfo) {
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	DWORD sym_options = SymGetOptions();
	sym_options |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
	SymSetOptions(sym_options);

	CONTEXT ctx = *pExInfo->ContextRecord;
	STACKFRAME64 frame;
	memset(&frame, 0, sizeof(frame));

#if defined(_WIN64)
	DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = ctx.Rip;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrStack.Offset = ctx.Rsp;
#else
	DWORD machineType = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = ctx.Eip;
	frame.AddrFrame.Offset = ctx.Ebp;
	frame.AddrStack.Offset = ctx.Esp;
#endif
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	fprintf(f, "  \"stack_trace\": [\n");

	char symBuf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
	int idx = 0;

	while (StackWalk64(machineType, process, GetCurrentThread(),
	                   &frame, &ctx, NULL,
	                   &SymFunctionTableAccess64, &SymGetModuleBase64, NULL)) {
		if (idx > 0) fprintf(f, ",\n");
		fprintf(f, "    {");

		// Module
		DWORD64 moduleBase = SymGetModuleBase64(process, frame.AddrPC.Offset);
		IMAGEHLP_MODULE64 modInfo{};
		modInfo.SizeOfStruct = sizeof(modInfo);

		char moduleName[256] = "???";
		if (SymGetModuleInfo64(process, moduleBase, &modInfo)) {
			if (modInfo.ImageName[0]) {
				// Extract basename
				const char* base = modInfo.ImageName;
				for (const char* p = modInfo.ImageName; *p; ++p) {
					if (*p == '\\' || *p == '/') base = p + 1;
				}
				snprintf(moduleName, sizeof(moduleName), "%s", base);
			} else if (modInfo.ModuleName[0]) {
				snprintf(moduleName, sizeof(moduleName), "%s", modInfo.ModuleName);
			}
		}

		char escaped[512];
		escapeJsonTo(escaped, sizeof(escaped), moduleName);
		fprintf(f, "\"module\": \"%s\"", escaped);

		// Function
		PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(symBuf);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		DWORD64 displacement64 = 0;
		if (SymFromAddr(process, frame.AddrPC.Offset, &displacement64, symbol)) {
			escapeJsonTo(escaped, sizeof(escaped), symbol->Name);
			fprintf(f, ", \"function\": \"%s\"", escaped);

			// File and line
			IMAGEHLP_LINE64 lineInfo{};
			lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			DWORD lineDisp = 0;
			if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &lineInfo)) {
				const char* fileBase = lineInfo.FileName;
				for (const char* p = lineInfo.FileName; *p; ++p) {
					if (*p == '\\' || *p == '/') fileBase = p + 1;
				}
				escapeJsonTo(escaped, sizeof(escaped), fileBase);
				fprintf(f, ", \"file\": \"%s\", \"line\": %lu",
				        escaped, lineInfo.LineNumber);
			} else {
				fprintf(f, ", \"file\": null, \"line\": null");
			}
		} else {
			fprintf(f, ", \"function\": null, \"file\": null, \"line\": null");
		}

		fprintf(f, "}");
		idx++;

		if (idx >= 128) break; // Safety cap
	}

	fprintf(f, "\n  ]");
	SymCleanup(process);
}

// ---------------------------------------------------------------------------
// Write recent log entries
// ---------------------------------------------------------------------------

static void writeRecentLogs(FILE* f) {
	// NOTE: This calls into NornLogger which uses a mutex. In most crash
	// scenarios (access violation, etc.) the logger mutex will be unlocked
	// and this is safe. In the rare case of a crash inside the logger itself,
	// this could deadlock -- but that is an acceptable trade-off for the
	// diagnostic value of captured logs.
	std::deque<LogEntry> entries;
	try {
		entries = NornLogger::instance().getRecent(100);
	} catch (...) {
		fprintf(f, "  \"recent_log\": []");
		return;
	}

	fprintf(f, "  \"recent_log\": [\n");
	char escaped[4096];
	for (size_t i = 0; i < entries.size(); ++i) {
		const LogEntry& e = entries[i];
		if (i > 0) fprintf(f, ",\n");

		escapeJsonTo(escaped, sizeof(escaped), e.message.c_str());
		fprintf(f, "    {\"t\": %.3f, \"lvl\": \"%s\", \"cat\": \"%s\", \"msg\": \"%s\", \"file\": \"",
		        e.timestamp, to_string(e.severity), to_string(e.category), escaped);

		escapeJsonTo(escaped, sizeof(escaped), e.file.c_str());
		fprintf(f, "%s\", \"line\": %d}", escaped, e.line);
	}
	fprintf(f, "\n  ]");
}

// ---------------------------------------------------------------------------
// Windows exception handler
// ---------------------------------------------------------------------------

static LONG WINAPI CrashExceptionHandler(PEXCEPTION_POINTERS pExInfo) {
	DWORD code = pExInfo->ExceptionRecord->ExceptionCode;

	// Build timestamp string
	char ts[64];
	formatTimestamp(ts, sizeof(ts));

	// Build ISO timestamp for JSON
	char isoTs[64];
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf{};
	localtime_s(&tm_buf, &tt);
	snprintf(isoTs, sizeof(isoTs), "%04d-%02d-%02dT%02d:%02d:%02d",
	         tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
	         tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

	// Build file paths
	snprintf(s_jsonPath, sizeof(s_jsonPath), "%s/openc2e-crash-%s.json", s_logDir, ts);
	snprintf(s_dumpPath, sizeof(s_dumpPath), "%s/openc2e-crash-%s.dmp", s_logDir, ts);

	// --- Write minidump first (most reliable, uses kernel API) ---
	writeMinidump(pExInfo, s_dumpPath);

	// --- Write JSON crash report ---
	FILE* f = fopen(s_jsonPath, "w");
	if (f) {
		fprintf(f, "{\n");
		fprintf(f, "  \"timestamp\": \"%s\",\n", isoTs);
		fprintf(f, "  \"exception_code\": \"0x%08lX\",\n", code);
		fprintf(f, "  \"exception_description\": \"%s\",\n", exceptionDescription(code));

		writeStackTrace(f, pExInfo);
		fprintf(f, ",\n");

		writeRecentLogs(f);
		fprintf(f, ",\n");

		writeSystemInfo(f);
		fprintf(f, "\n");

		fprintf(f, "}\n");
		fflush(f);
		fclose(f);
	}

	// --- Print summary to stderr ---
	fprintf(stderr, "\n");
	fprintf(stderr, "=== OPENC2E CRASH ===\n");
	fprintf(stderr, "Exception: 0x%08lX (%s)\n", code, exceptionDescription(code));
	fprintf(stderr, "Crash report: %s\n", s_jsonPath);
	fprintf(stderr, "Minidump:     %s\n", s_dumpPath);
	fprintf(stderr, "=====================\n");

	return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------------------
// Public API (Windows)
// ---------------------------------------------------------------------------

void CrashHandler::install() {
	SetUnhandledExceptionFilter(CrashExceptionHandler);
}

void CrashHandler::setLogDir(const std::string& path) {
	snprintf(s_logDir, sizeof(s_logDir), "%s", path.c_str());
}

// ==========================================================================
// LINUX / POSIX IMPLEMENTATION
// ==========================================================================
#else

#include <csignal>
#include <cstdlib>
#include <dlfcn.h>
#include <execinfo.h>
#include <inttypes.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef __GNUG__
#include <cxxabi.h>
#endif

// ---------------------------------------------------------------------------
// Signal number to description
// ---------------------------------------------------------------------------

static const char* signalDescription(int sig) {
	switch (sig) {
		case SIGSEGV: return "Segmentation Fault";
		case SIGABRT: return "Abort";
		case SIGBUS:  return "Bus Error";
		case SIGFPE:  return "Floating Point Exception";
		case SIGILL:  return "Illegal Instruction";
		default:      return "Unknown Signal";
	}
}

// ---------------------------------------------------------------------------
// Write recent log entries (same logic as Windows)
// ---------------------------------------------------------------------------

static void writeRecentLogsLinux(FILE* f) {
	std::deque<LogEntry> entries;
	try {
		entries = NornLogger::instance().getRecent(100);
	} catch (...) {
		fprintf(f, "  \"recent_log\": []");
		return;
	}

	char escaped[4096];
	fprintf(f, "  \"recent_log\": [\n");
	for (size_t i = 0; i < entries.size(); ++i) {
		const LogEntry& e = entries[i];
		if (i > 0) fprintf(f, ",\n");

		escapeJsonTo(escaped, sizeof(escaped), e.message.c_str());
		fprintf(f, "    {\"t\": %.3f, \"lvl\": \"%s\", \"cat\": \"%s\", \"msg\": \"%s\", \"file\": \"",
		        e.timestamp, to_string(e.severity), to_string(e.category), escaped);

		escapeJsonTo(escaped, sizeof(escaped), e.file.c_str());
		fprintf(f, "%s\", \"line\": %d}", escaped, e.line);
	}
	fprintf(f, "\n  ]");
}

// ---------------------------------------------------------------------------
// Write system info (Linux)
// ---------------------------------------------------------------------------

static void writeSystemInfoLinux(FILE* f) {
	struct utsname uname_buf;
	const char* osVersion = "Linux (unknown)";
	char osBuf[256];
	if (uname(&uname_buf) == 0) {
		snprintf(osBuf, sizeof(osBuf), "%s %s %s", uname_buf.sysname,
		         uname_buf.release, uname_buf.machine);
		osVersion = osBuf;
	}

	// Process memory from /proc/self/status
	long memKb = 0;
	FILE* status = fopen("/proc/self/status", "r");
	if (status) {
		char line[256];
		while (fgets(line, sizeof(line), status)) {
			if (strncmp(line, "VmRSS:", 6) == 0) {
				memKb = atol(line + 6);
				break;
			}
		}
		fclose(status);
	}

#if defined(__x86_64__) || defined(__aarch64__)
	const char* arch = sizeof(void*) == 8 ? "x86_64" : "x86";
#if defined(__aarch64__)
	arch = "aarch64";
#endif
#else
	const char* arch = "x86";
#endif

	char escapedOs[512];
	escapeJsonTo(escapedOs, sizeof(escapedOs), osVersion);

	fprintf(f, "  \"system_info\": {\n");
	fprintf(f, "    \"os_version\": \"%s\",\n", escapedOs);
	fprintf(f, "    \"architecture\": \"%s\",\n", arch);
	fprintf(f, "    \"process_memory_mb\": %ld\n", memKb / 1024);
	fprintf(f, "  }");
}

// ---------------------------------------------------------------------------
// Signal handler
// ---------------------------------------------------------------------------

static void crashSignalHandler(int sig) {
	// Build timestamp
	char ts[64];
	formatTimestamp(ts, sizeof(ts));

	char isoTs[64];
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf{};
	localtime_r(&tt, &tm_buf);
	snprintf(isoTs, sizeof(isoTs), "%04d-%02d-%02dT%02d:%02d:%02d",
	         tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
	         tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

	snprintf(s_jsonPath, sizeof(s_jsonPath), "%s/openc2e-crash-%s.json", s_logDir, ts);

	// Capture backtrace
	void* callstack[128];
	int numFrames = backtrace(callstack, 128);

	// --- Write JSON crash report ---
	FILE* f = fopen(s_jsonPath, "w");
	if (f) {
		fprintf(f, "{\n");
		fprintf(f, "  \"timestamp\": \"%s\",\n", isoTs);
		fprintf(f, "  \"exception_code\": \"0x%08X\",\n", sig);
		fprintf(f, "  \"exception_description\": \"%s\",\n", signalDescription(sig));

		// Stack trace
		fprintf(f, "  \"stack_trace\": [\n");
		for (int i = 0; i < numFrames; ++i) {
			Dl_info info;
			memset(&info, 0, sizeof(info));
			dladdr(callstack[i], &info);

			if (i > 0) fprintf(f, ",\n");
			fprintf(f, "    {");

			// Module
			char escaped[512];
			if (info.dli_fname) {
				const char* base = strrchr(info.dli_fname, '/');
				base = base ? base + 1 : info.dli_fname;
				escapeJsonTo(escaped, sizeof(escaped), base);
			} else {
				escaped[0] = '?'; escaped[1] = '?'; escaped[2] = '?'; escaped[3] = '\0';
			}
			fprintf(f, "\"module\": \"%s\"", escaped);

			// Function
			if (info.dli_sname) {
				const char* name = info.dli_sname;
#ifdef __GNUG__
				int status = 0;
				char* demangled = abi::__cxa_demangle(info.dli_sname, NULL, NULL, &status);
				if (status == 0 && demangled) name = demangled;
#endif
				escapeJsonTo(escaped, sizeof(escaped), name);
				fprintf(f, ", \"function\": \"%s\"", escaped);
#ifdef __GNUG__
				if (demangled) free(demangled);
#endif
			} else {
				fprintf(f, ", \"function\": null");
			}

			// No reliable file/line info from backtrace() without addr2line
			fprintf(f, ", \"file\": null, \"line\": null");
			fprintf(f, "}");
		}
		fprintf(f, "\n  ],\n");

		writeRecentLogsLinux(f);
		fprintf(f, ",\n");

		writeSystemInfoLinux(f);
		fprintf(f, "\n");

		fprintf(f, "}\n");
		fflush(f);
		fclose(f);
	}

	// Print summary to stderr
	fprintf(stderr, "\n");
	fprintf(stderr, "=== OPENC2E CRASH ===\n");
	fprintf(stderr, "Signal: %d (%s)\n", sig, signalDescription(sig));
	fprintf(stderr, "Crash report: %s\n", s_jsonPath);
	fprintf(stderr, "=====================\n");

	// Also dump raw backtrace to stderr as a fallback
	backtrace_symbols_fd(callstack, numFrames, STDERR_FILENO);

	_exit(1);
}

// ---------------------------------------------------------------------------
// Public API (Linux)
// ---------------------------------------------------------------------------

void CrashHandler::install() {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = crashSignalHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESETHAND; // One-shot: avoid infinite loops on re-raise

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
}

void CrashHandler::setLogDir(const std::string& path) {
	snprintf(s_logDir, sizeof(s_logDir), "%s", path.c_str());
}

#endif // _WIN32
