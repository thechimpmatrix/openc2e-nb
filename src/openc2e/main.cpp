/*
 *  main.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Wed 02 Jun 2004.
 *  Copyright (c) 2004-2008 Alyssa Milburn. All rights reserved.
 *  Copyright (c) 2005-2008 Bryan Donlan. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#include "Engine.h"
#include "common/CrashHandler.h"
#include "common/Exception.h"
#include "common/NornLog.h"
#include "common/backtrace.h"
#include "sdlbackend/SDLBackend.h"
#include "sdlbackend/SDLMixerBackend.h"
#include "version.h"

#include <pybind11/embed.h>
namespace py = pybind11;

#include <fmt/core.h>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

// SDL tries stealing main on some platforms, which we don't want.
#undef main

extern "C" int main(int argc, char* argv[]) {
	// Initialize logging and crash handling before anything else
	NornLogger::instance().init("logs", 9999);
	CrashHandler::setLogDir("logs");
	CrashHandler::install();
	NORN_INF(ENGINE, "openc2e starting up");

	install_backtrace_printer();

	// Initialize Python interpreter for brain embedding
	py::scoped_interpreter python_guard{};

	try {
		std::string version;
#ifdef DEV_BUILD
		version = "development build";
#else
		version = RELEASE_VERSION;
#endif
		fmt::print("openc2e ({}), built " __DATE__ " " __TIME__ "\nCopyright (c) 2004-2008 Alyssa Milburn and others\n\n", version);
		NORN_INF(ENGINE, fmt::format("openc2e version: {}", version));

		engine.addPossibleBackend("sdl", SDLBackend::get_instance());
		engine.addPossibleAudioBackend("sdlmixer", SDLMixerBackend::get_instance());

		// pass command-line flags to the engine, but do no other setup
		if (!engine.parseCommandLine(argc, argv))
			return 1;

		// get the engine to do all the startup (read catalogue, loading world, etc)
		if (!engine.initialSetup())
			return 0;

		get_backend()->run([] {
			engine.tick();
			if (engine.done) {
				return false;
			}
			engine.drawWorld();
			return true;
		});

		// we're done, be sure to shut stuff down
		engine.shutdown();

	} catch (Exception& e) {
		NORN_LOG(FATAL, ENGINE, fmt::format("Fatal engine exception: {}", e.what()));
		NORN_INF(ENGINE, "openc2e shutting down after fatal exception");
		NornLogger::instance().shutdown();
		return 1;
	} catch (std::exception& e) {
		NORN_LOG(FATAL, ENGINE, fmt::format("Fatal std::exception: {}", e.what()));
		NORN_INF(ENGINE, "openc2e shutting down after fatal exception");
		NornLogger::instance().shutdown();
		return 1;
	} catch (...) {
		NORN_LOG(FATAL, ENGINE, "Unknown fatal exception");
		NORN_INF(ENGINE, "openc2e shutting down after fatal exception");
		NornLogger::instance().shutdown();
		return 1;
	}

	NORN_INF(ENGINE, "openc2e shutting down");
	NornLogger::instance().shutdown();

	return 0;
}

/* vim: set noet: */
