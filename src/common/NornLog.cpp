/*
 *  NornLog.cpp
 *  openc2e
 *
 *  Structured logging with JSON Lines file output and UDP broadcast.
 *  Thread-safe, crash-safe (flush after every write), silent on failure.
 */

#include "NornLog.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

#include <fmt/core.h>

#ifdef _WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	pragma comment(lib, "ws2_32.lib")
#else
#	include <arpa/inet.h>
#	include <fcntl.h>
#	include <sys/socket.h>
#	include <sys/types.h>
#	include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Helpers: platform socket abstraction
// ---------------------------------------------------------------------------

#ifdef _WIN32
using socket_t = SOCKET;
static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
static void close_socket(socket_t s) {
	if (s != INVALID_SOCK) closesocket(s);
}
static void set_nonblocking(socket_t s) {
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}
#else
using socket_t = int;
static constexpr socket_t INVALID_SOCK = -1;
static void close_socket(socket_t s) {
	if (s != INVALID_SOCK) close(s);
}
static void set_nonblocking(socket_t s) {
	int flags = fcntl(s, F_GETFL, 0);
	if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
#endif

// ---------------------------------------------------------------------------
// Enum to_string
// ---------------------------------------------------------------------------

const char* to_string(LogSeverity s) {
	switch (s) {
		case LogSeverity::TRACE: return "TRC";
		case LogSeverity::DBG: return "DBG";
		case LogSeverity::INF: return "INF";
		case LogSeverity::WRN: return "WRN";
		case LogSeverity::ERR: return "ERR";
		case LogSeverity::FATAL: return "FTL";
	}
	return "???";
}

const char* to_string(LogCategory c) {
	switch (c) {
		case LogCategory::ENGINE: return "ENGINE";
		case LogCategory::CAOS: return "CAOS";
		case LogCategory::AGENT: return "AGENT";
		case LogCategory::WORLD: return "WORLD";
		case LogCategory::RENDER: return "RENDER";
		case LogCategory::AUDIO: return "AUDIO";
		case LogCategory::FILEIO: return "FILEIO";
		case LogCategory::MEMORY: return "MEMORY";
		case LogCategory::PERF: return "PERF";
		case LogCategory::CREATURE: return "CREATURE";
		case LogCategory::NET: return "NET";
	}
	return "???";
}

// ---------------------------------------------------------------------------
// Timestamp
// ---------------------------------------------------------------------------

static double now_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto epoch = now.time_since_epoch();
	auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(epoch);
	return secs.count();
}

// ---------------------------------------------------------------------------
// NornLogger singleton
// ---------------------------------------------------------------------------

NornLogger& NornLogger::instance() {
	static NornLogger inst;
	return inst;
}

NornLogger::NornLogger() = default;

NornLogger::~NornLogger() {
	try {
		shutdown();
	} catch (...) {
		// Never throw from destructor
	}
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void NornLogger::init(const std::string& logDir, uint16_t udpPort) {
	std::lock_guard<std::mutex> lock(writeMutex_);
	if (initialized_) return;

	udpPort_ = udpPort;

	// --- Build timestamped filename ---
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf{};
#ifdef _WIN32
	localtime_s(&tm_buf, &tt);
#else
	localtime_r(&tt, &tm_buf);
#endif

	std::string filename = fmt::format(
		"{}/openc2e-{:04d}-{:02d}-{:02d}-{:02d}{:02d}{:02d}.jsonl",
		logDir,
		tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
		tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

	// --- Open file ---
	file_ = std::fopen(filename.c_str(), "ab");
	// If fopen fails, file_ stays nullptr; writeToFile will silently skip.

	// --- Create UDP socket ---
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	sock_ = static_cast<uintptr_t>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
	if (sock_ != static_cast<uintptr_t>(INVALID_SOCKET)) {
		set_nonblocking(static_cast<socket_t>(sock_));
	}
#else
	sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_ != INVALID_SOCK) {
		set_nonblocking(sock_);
	}
#endif

	initialized_ = true;
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void NornLogger::shutdown() {
	std::lock_guard<std::mutex> lock(writeMutex_);
	if (!initialized_) return;

	if (file_) {
		std::fflush(file_);
		std::fclose(file_);
		file_ = nullptr;
	}

#ifdef _WIN32
	close_socket(static_cast<socket_t>(sock_));
	sock_ = ~static_cast<uintptr_t>(0);
	WSACleanup();
#else
	close_socket(sock_);
	sock_ = INVALID_SOCK;
#endif

	initialized_ = false;
}

// ---------------------------------------------------------------------------
// setMinSeverity
// ---------------------------------------------------------------------------

void NornLogger::setMinSeverity(LogSeverity s) {
	minSeverity_ = s;
}

// ---------------------------------------------------------------------------
// JSON string escaping
// ---------------------------------------------------------------------------

std::string NornLogger::escapeJson(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 8);
	for (unsigned char ch : s) {
		switch (ch) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (ch < 0x20) {
					out += fmt::format("\\u{:04x}", ch);
				} else {
					out += static_cast<char>(ch);
				}
				break;
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// basenameOf -- extract filename from full path
// ---------------------------------------------------------------------------

std::string NornLogger::basenameOf(const char* path) {
	if (!path || !*path) return "";
	const char* last = path;
	for (const char* p = path; *p; ++p) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	// If no separator was found, last still points to start of string
	if (last == path && *path != '/' && *path != '\\') return path;
	return last;
}

// ---------------------------------------------------------------------------
// File output
// ---------------------------------------------------------------------------

void NornLogger::writeToFile(const std::string& json) {
	if (!file_) return;
	std::fwrite(json.data(), 1, json.size(), file_);
	std::fputc('\n', file_);
	std::fflush(file_);
}

// ---------------------------------------------------------------------------
// UDP output
// ---------------------------------------------------------------------------

void NornLogger::sendUdp(const std::string& json) {
	struct sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(udpPort_);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

#ifdef _WIN32
	socket_t s = static_cast<socket_t>(sock_);
	if (s == INVALID_SOCKET) return;
#else
	socket_t s = sock_;
	if (s == INVALID_SOCK) return;
#endif

	// Fire-and-forget: ignore errors
	sendto(s, json.data(), static_cast<int>(json.size()), 0,
	       reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------

void NornLogger::log(LogSeverity severity, LogCategory category,
                     const std::string& message, const char* file, int line) {
	try {
		if (static_cast<uint8_t>(severity) < static_cast<uint8_t>(minSeverity_)) return;

		double ts = now_timestamp();

		LogEntry entry;
		entry.timestamp = ts;
		entry.severity = severity;
		entry.category = category;
		entry.message = message;
		entry.file = basenameOf(file);
		entry.line = line;

		ring_.push(entry);

		std::string json = fmt::format(
			R"({{"t":{:.3f},"lvl":"{}","cat":"{}","msg":"{}","file":"{}","line":{}}})",
			ts,
			to_string(severity),
			to_string(category),
			escapeJson(message),
			escapeJson(entry.file),
			line);

		std::lock_guard<std::mutex> lock(writeMutex_);
		writeToFile(json);
		sendUdp(json);
	} catch (...) {
		// Logging must never throw
	}
}

// ---------------------------------------------------------------------------
// logPerf
// ---------------------------------------------------------------------------

void NornLogger::logPerf(const PerfSnapshot& perf) {
	try {
		double ts = now_timestamp();

		LogEntry entry;
		entry.timestamp = ts;
		entry.severity = LogSeverity::INF;
		entry.category = LogCategory::PERF;
		entry.message = fmt::format("tick={:.1f}ms frame={:.1f}ms agents={} fps={:.1f}",
		                            perf.tick_ms, perf.frame_ms, perf.agents, perf.fps);
		entry.line = 0;

		ring_.push(entry);

		std::string json = fmt::format(
			R"({{"t":{:.3f},"lvl":"PERF","cat":"PERF",)"
			R"("tick_ms":{:.1f},"frame_ms":{:.1f},"agents":{},"fps":{:.1f}}})",
			ts, perf.tick_ms, perf.frame_ms, perf.agents, perf.fps);

		std::lock_guard<std::mutex> lock(writeMutex_);
		writeToFile(json);
		sendUdp(json);
	} catch (...) {
		// Logging must never throw
	}
}

// ---------------------------------------------------------------------------
// getRecent
// ---------------------------------------------------------------------------

std::deque<LogEntry> NornLogger::getRecent(size_t n) const {
	return ring_.getRecent(n);
}
