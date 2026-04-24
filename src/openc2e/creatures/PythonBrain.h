#pragma once

#include "c2eBrain.h"
#include <pybind11/embed.h>
#include <string>
#include <vector>

namespace py = pybind11;

struct PendingStimulus {
	int noun_id;     // agent category that caused stimulus (-1 = none)
	int verb_id;     // action that was performed (-1 = none)
	float strength;  // stimulus strength (0.0–1.0)
};

class PythonBrain : public c2eBrain {
public:
	PythonBrain(c2eCreature* parent, const std::string& module_path);
	~PythonBrain() override;

	void processGenes() override;
	void tick() override;
	void init() override;

	// Called by c2eCreature::handleStimulus to deliver stimulus events to Python.
	void pushStimulus(int noun_id, int verb_id, float strength);

private:
	std::string module_path_;
	py::module_ brain_module_;
	py::object tick_fn_;
	py::object init_fn_;
	bool python_ready_ = false;
	unsigned int tick_count_ = 0;

	// Stimulus event queue — drained each tick by gather_inputs()
	std::vector<PendingStimulus> pending_stimuli_;

	// Error recovery: transient Python errors don't permanently kill the brain.
	// After max_consecutive_errors, fall back to SVRule until a cooldown period.
	unsigned int consecutive_errors_ = 0;
	unsigned int total_errors_ = 0;
	unsigned int last_error_tick_ = 0;
	static constexpr unsigned int MAX_CONSECUTIVE_ERRORS = 5;
	static constexpr unsigned int ERROR_COOLDOWN_TICKS = 50;  // ~2.5s at 20 tps

	py::dict gather_inputs();
	void apply_outputs(const py::dict& result);
	bool should_retry_python() const;
};
