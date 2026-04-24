#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

enum class LogSeverity : uint8_t {
	TRACE = 0,
	DBG = 1,
	INF = 2,
	WRN = 3,
	ERR = 4,
	FATAL = 5,
};

enum class LogCategory : uint8_t {
	ENGINE = 0,
	CAOS,
	AGENT,
	WORLD,
	RENDER,
	AUDIO,
	FILEIO,
	MEMORY,
	PERF,
	CREATURE,
	NET,
};

struct LogEntry {
	double timestamp; // seconds since epoch, with fractional part
	LogSeverity severity;
	LogCategory category;
	std::string message;
	std::string file;
	int line;
};

struct PerfSnapshot {
	double tick_ms;
	double frame_ms;
	int agents;
	double fps;
};

const char* to_string(LogSeverity s);
const char* to_string(LogCategory c);

template <typename T>
class RingBuffer {
  public:
	explicit RingBuffer(size_t capacity)
		: capacity_(capacity) {}

	void push(const T& item) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (buf_.size() >= capacity_) {
			buf_.pop_front();
		}
		buf_.push_back(item);
	}

	std::deque<T> getRecent(size_t n) const {
		std::lock_guard<std::mutex> lock(mutex_);
		if (n >= buf_.size()) {
			return buf_;
		}
		return std::deque<T>(buf_.end() - static_cast<ptrdiff_t>(n), buf_.end());
	}

	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return buf_.size();
	}

	void clear() {
		std::lock_guard<std::mutex> lock(mutex_);
		buf_.clear();
	}

  private:
	size_t capacity_;
	std::deque<T> buf_;
	mutable std::mutex mutex_;
};

class NornLogger {
  public:
	static NornLogger& instance();

	// Initialize file + UDP output. Call once at startup.
	// logDir: directory for .jsonl files. udpPort: localhost port for UDP broadcast.
	void init(const std::string& logDir, uint16_t udpPort = 9999);

	// Set minimum severity level. Entries below this are discarded.
	void setMinSeverity(LogSeverity s);

	// Log a message. file/line are source location of the call site.
	void log(LogSeverity severity, LogCategory category, const std::string& message,
	         const char* file = "", int line = 0);

	// Log a performance snapshot.
	void logPerf(const PerfSnapshot& perf);

	// Get recent log entries from the ring buffer.
	std::deque<LogEntry> getRecent(size_t n = 100) const;

	// Flush file and close socket.
	void shutdown();

	~NornLogger();

	// Non-copyable
	NornLogger(const NornLogger&) = delete;
	NornLogger& operator=(const NornLogger&) = delete;

  private:
	NornLogger();

	void writeToFile(const std::string& json);
	void sendUdp(const std::string& json);
	std::string escapeJson(const std::string& s);
	std::string basenameOf(const char* path);

	bool initialized_ = false;
	LogSeverity minSeverity_ = LogSeverity::DBG;
	RingBuffer<LogEntry> ring_{2048};

	// File output
	FILE* file_ = nullptr;

	// UDP output
#ifdef _WIN32
	uintptr_t sock_ = ~static_cast<uintptr_t>(0); // INVALID_SOCKET
#else
	int sock_ = -1;
#endif
	uint16_t udpPort_ = 9999;

	std::mutex writeMutex_;
};

// Convenience macros
#define NORN_LOG(sev, cat, msg) \
	NornLogger::instance().log(LogSeverity::sev, LogCategory::cat, msg, __FILE__, __LINE__)

#define NORN_DBG(cat, msg) NORN_LOG(DBG, cat, msg)
#define NORN_INF(cat, msg) NORN_LOG(INF, cat, msg)
#define NORN_WRN(cat, msg) NORN_LOG(WRN, cat, msg)
#define NORN_ERR(cat, msg) NORN_LOG(ERR, cat, msg)
