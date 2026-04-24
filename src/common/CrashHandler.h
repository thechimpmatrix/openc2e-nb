#pragma once

/*
 *  CrashHandler.h
 *  openc2e
 *
 *  Enhanced crash handler that generates structured JSON crash reports,
 *  Windows minidumps, and captures recent log entries from NornLog's
 *  ring buffer. Wraps/replaces the basic backtrace.h handler.
 *
 *  Usage:
 *    CrashHandler::setLogDir("logs");   // optional, defaults to "."
 *    CrashHandler::install();           // call once at startup
 */

#include <string>

class CrashHandler {
  public:
	// Install the crash handler. Replaces any previously installed handler.
	// On Windows: SetUnhandledExceptionFilter
	// On Linux: signal handlers for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL
	static void install();

	// Set the directory where crash reports and minidumps are written.
	// Must be called before install() for the path to take effect.
	// If not called, defaults to the current working directory.
	static void setLogDir(const std::string& path);

  private:
	CrashHandler() = delete;
};
