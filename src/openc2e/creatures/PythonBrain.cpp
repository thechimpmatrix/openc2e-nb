/*
 *  PythonBrain.cpp
 *  openc2e — NORNBRAIN integration
 *
 *  Delegates c2eBrain::tick() to a Python CfC/NCP brain module via pybind11.
 *  Falls back to SVRule brain on any Python error.
 */

#include "PythonBrain.h"
#include "c2eCreature.h"
#include "Agent.h"
#include "World.h"
#include "historyManager.h"
#include "common/NornLog.h"
#include <pybind11/stl.h>
#include <fmt/core.h>
#include <filesystem>

namespace fs = std::filesystem;

PythonBrain::PythonBrain(c2eCreature* p, const std::string& module_path)
	: c2eBrain(p), module_path_(module_path) {
	NORN_INF(CREATURE, fmt::format("PythonBrain: creating with module '{}'", module_path_));
}

PythonBrain::~PythonBrain() {
	// Release Python objects before interpreter teardown.
	// Must hold the GIL since pybind11 ref-counts are modified.
	// Guard against interpreter already being finalized.
	if (!Py_IsInitialized()) return;
	try {
		py::gil_scoped_acquire gil;
		tick_fn_ = py::none();
		init_fn_ = py::none();
		brain_module_ = py::module_();
	} catch (...) {
		// Interpreter may already be finalised — nothing we can do.
	}
}

void PythonBrain::processGenes() {
	// We MUST call the base implementation so that lobe and tract data structures
	// are created from genome genes. PythonBrain simply won't run SVRules on them.
	c2eBrain::processGenes();
}

void PythonBrain::init() {
	// Initialise lobe neurons via base class (runs init SVRules, sets up tracts).
	c2eBrain::init();

	try {
		py::gil_scoped_acquire gil;

		// Add the module's parent directory to sys.path so Python can find it.
		fs::path mod_path(module_path_);
		std::string parent_dir = mod_path.parent_path().string();
		std::string stem = mod_path.stem().string();

		py::module_ sys = py::module_::import("sys");
		py::list path = sys.attr("path").cast<py::list>();

		// Avoid duplicate entries
		bool found = false;
		for (auto& entry : path) {
			if (entry.cast<std::string>() == parent_dir) {
				found = true;
				break;
			}
		}
		if (!found) {
			path.insert(0, parent_dir);
		}

		// Import the brain module
		brain_module_ = py::module_::import(stem.c_str());
		NORN_INF(CREATURE, fmt::format("PythonBrain: imported module '{}'", stem));

		// Get function handles
		tick_fn_ = brain_module_.attr("tick");
		init_fn_ = brain_module_.attr("init");

		// Build lobe_info: { "lobe_id": neuron_count, ... }
		py::dict lobe_info;
		for (auto& pair : lobes) {
			lobe_info[py::str(pair.first)] = pair.second->getNoNeurons();
		}

		// Call Python init(lobe_info)
		init_fn_(lobe_info);
		NORN_INF(CREATURE, fmt::format("PythonBrain: Python init() complete, {} lobes", lobes.size()));

		python_ready_ = true;

	} catch (py::error_already_set& e) {
		NORN_ERR(CREATURE, fmt::format("PythonBrain: Python init failed: {}", e.what()));
		e.restore();  // Clear Python error indicator
		python_ready_ = false;
	}
}

bool PythonBrain::should_retry_python() const {
	// If we've had too many consecutive errors, wait for cooldown
	if (consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS) {
		return (tick_count_ - last_error_tick_) >= ERROR_COOLDOWN_TICKS;
	}
	return true;
}

void PythonBrain::tick() {
	if (!python_ready_) {
		// Fall back to SVRule brain
		c2eBrain::tick();
		return;
	}

	// Error recovery: if we've hit the error threshold, use SVRule temporarily
	if (consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS && !should_retry_python()) {
		c2eBrain::tick();
		tick_count_++;
		return;
	}

	try {
		py::gil_scoped_acquire gil;

		py::dict inputs = gather_inputs();
		py::object result_obj = tick_fn_(inputs);
		py::dict result = result_obj.cast<py::dict>();
		apply_outputs(result);

		// Success — reset error counter
		if (consecutive_errors_ > 0) {
			NORN_INF(CREATURE, fmt::format("PythonBrain: recovered after {} errors", consecutive_errors_));
		}
		consecutive_errors_ = 0;
		tick_count_++;

	} catch (py::error_already_set& e) {
		consecutive_errors_++;
		total_errors_++;
		last_error_tick_ = tick_count_;
		tick_count_++;

		if (consecutive_errors_ <= 3 || consecutive_errors_ % 50 == 0) {
			NORN_ERR(CREATURE, fmt::format("PythonBrain: tick() error #{} (total {}): {}",
				consecutive_errors_, total_errors_, e.what()));
		}

		// Clear Python's internal error indicator so next call can succeed.
		// Without this, subsequent Python calls get corrupted SystemError.
		e.restore();

		if (consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS) {
			NORN_ERR(CREATURE, fmt::format(
				"PythonBrain: {} consecutive errors, falling back to SVRule for {} ticks",
				consecutive_errors_, ERROR_COOLDOWN_TICKS));
		}

		// Fall back to SVRule for this tick (but don't permanently disable)
		c2eBrain::tick();
	}
}

void PythonBrain::pushStimulus(int noun_id, int verb_id, float strength) {
	pending_stimuli_.push_back({noun_id, verb_id, strength});
}

py::dict PythonBrain::gather_inputs() {
	py::dict inputs;

	// Pending stimulus events (drained each tick)
	py::list stim_list;
	for (auto& s : pending_stimuli_) {
		py::dict sd;
		sd["noun"] = s.noun_id;
		sd["verb"] = s.verb_id;
		sd["strength"] = s.strength;
		stim_list.append(sd);
	}
	pending_stimuli_.clear();
	inputs["stimuli"] = stim_list;

	// Lobe neuron states: { "lobe_id": [var0, var0, ...], ... }
	py::dict lobe_data;
	for (auto& pair : lobes) {
		const std::string& id = pair.first;
		c2eLobe* lobe = pair.second.get();
		if (!lobe) continue;  // Skip null lobes

		unsigned int n = lobe->getNoNeurons();
		py::list neuron_values;
		for (unsigned int i = 0; i < n; i++) {
			c2eNeuron* neuron = lobe->getNeuron(i);
			if (!neuron) {
				neuron_values.append(0.0f);
				continue;
			}
			// Use neuron->input (set by Sensory Faculty) not variables[0]
			// (which is only updated by SVRule processing, which we skip)
			float val = neuron->input;
			// Fall back to variables[0] if input is zero (some lobes
			// don't receive sensory input and store state in variables)
			if (val == 0.0f) {
				val = neuron->variables[0];
			}
			// Clamp to valid range
			if (val != val) val = 0.0f;  // NaN check
			neuron_values.append(val);
		}
		lobe_data[py::str(id)] = neuron_values;
	}
	inputs["lobes"] = lobe_data;

	// All 256 chemicals
	c2eCreature* creature = getParent();
	py::list chem_list;
	if (creature) {
		for (int i = 0; i < 256; i++) {
			float chem = creature->getChemical(static_cast<unsigned char>(i));
			if (chem != chem) chem = 0.0f;  // NaN check
			chem_list.append(chem);
		}
	} else {
		for (int i = 0; i < 256; i++) {
			chem_list.append(0.0f);
		}
	}
	inputs["chemicals"] = chem_list;

	// Tick counter
	inputs["tick"] = tick_count_;

	// Creature identity and position
	if (creature) {
		Agent* agent = creature->getParentAgent();
		if (agent) {
			// UNID: unique integer per agent (stable for lifetime)
			inputs["unid"] = agent->getUNID();
			// Classifier
			inputs["family"] = agent->family;
			inputs["genus"] = agent->genus;
			inputs["species"] = agent->species;
			// Position
			inputs["posx"] = agent->x;
			inputs["posy"] = agent->y;
		} else {
			inputs["genus"] = creature->getGenus();
		}
		// Moniker from history (for name lookup) — may be empty
		auto genome = creature->getGenome();
		if (genome) {
			std::string moniker = world.history->findMoniker(genome);
			if (!moniker.empty()) {
				inputs["moniker"] = moniker;
				// Also pass the creature's name if set
				monikerData& md = world.history->getMoniker(moniker);
				if (!md.name.empty()) {
					inputs["creature_name"] = md.name;
				}
			}
		}
	}

	return inputs;
}

void PythonBrain::apply_outputs(const py::dict& result) {
	// result["attention"] -> int (winning neuron index in "attn" lobe)
	// result["decision"]  -> int (winning neuron index in "decn" lobe)

	if (result.contains("attention")) {
		int winner = result["attention"].cast<int>();
		c2eLobe* attn = getLobeById("attn");
		if (attn) {
			unsigned int n = attn->getNoNeurons();
			for (unsigned int i = 0; i < n; i++) {
				attn->getNeuron(i)->variables[0] = (static_cast<int>(i) == winner) ? 1.0f : 0.0f;
			}
			if (winner >= 0 && static_cast<unsigned int>(winner) < n) {
				attn->setSpare(static_cast<unsigned int>(winner));
			}
		}
	}

	if (result.contains("decision")) {
		int winner = result["decision"].cast<int>();
		c2eLobe* decn = getLobeById("decn");
		if (decn) {
			unsigned int n = decn->getNoNeurons();
			for (unsigned int i = 0; i < n; i++) {
				decn->getNeuron(i)->variables[0] = (static_cast<int>(i) == winner) ? 1.0f : 0.0f;
			}
			if (winner >= 0 && static_cast<unsigned int>(winner) < n) {
				decn->setSpare(static_cast<unsigned int>(winner));
			}
		}
	}
}
