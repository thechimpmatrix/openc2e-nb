/*
 *  SVRuleTest.cpp
 *  openc2e
 *
 *  Unit tests for the c2eSVRule register machine and related brain structures.
 *
 *  The SVRule is a small register-machine interpreter: 16 instructions, each
 *  encoded as (opcode, operand_type, operand_data). It operates on an
 *  accumulator and four float[8] arrays (src neuron, neuron, spare neuron,
 *  dendrite). These tests exercise individual opcodes and small programs
 *  without requiring a full Creature or genome context.
 */

#include "openc2e/creatures/c2eBrain.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Helper: build a 48-byte rule data buffer from a list of (opcode, type, data)
// triples. Remaining slots are filled with opcode 0 (stop).
// ---------------------------------------------------------------------------
struct RuleEntry {
	uint8_t opcode;
	uint8_t operandtype;
	uint8_t operanddata;
};

static void buildRuleData(uint8_t out[48], const std::vector<RuleEntry>& entries) {
	std::memset(out, 0, 48); // opcode 0 = stop everywhere by default
	for (size_t i = 0; i < entries.size() && i < 16; i++) {
		out[i * 3 + 0] = entries[i].opcode;
		out[i * 3 + 1] = entries[i].operandtype;
		out[i * 3 + 2] = entries[i].operanddata;
	}
}

// Zero-initialised float[8] arrays for convenience.
static void zeroArray(float arr[8]) {
	for (int i = 0; i < 8; i++)
		arr[i] = 0.0f;
}

// ---------------------------------------------------------------------------
// Operand type constant precalculation tests (init-time)
// ---------------------------------------------------------------------------

TEST(SVRuleInit, ConstantZero) {
	// operandtype 9 = zero => operandvalue should be 0.0
	uint8_t rd[48];
	buildRuleData(rd, {{3, 9, 0}}); // load from zero
	c2eSVRule rule;
	rule.init(rd);
	// Execute: load from zero, then stop. Accumulator should remain 0.
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(999.0f, src, neuron, spare, dendrite, nullptr);
	// We can't directly read the accumulator, but we can store it.
	// Re-do with store.
}

TEST(SVRuleInit, ConstantOne) {
	// operandtype 10 = one => 1.0
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},   // load from one (acc = 1.0)
		{2, 3, 0},    // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleInit, ConstantValue) {
	// operandtype 11 = value => data * (1.0/248)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},  // load from value(124) => 124/248 = 0.5
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 124.0f / 248.0f);
}

TEST(SVRuleInit, ConstantNegativeValue) {
	// operandtype 12 = negative value => data * (-1.0/248)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 12, 248},  // load from neg value(248) => 248 * (-1/248) = -1.0
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], -1.0f);
}

TEST(SVRuleInit, ConstantValueTimes10) {
	// operandtype 13 = value * 10 => data * (10.0/248)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 13, 248},  // load => 248 * 10/248 = 10.0
		{2, 3, 0},     // store in neuron[0] — should be clamped to 1.0 by bindFloatValue
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	// store does bindFloatValue(accumulator) => clamp to [-1, 1]
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleInit, ConstantValueDiv10) {
	// operandtype 14 = value / 10 => data * (0.1/248)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 14, 248},  // load => 248 * 0.1/248 = 0.1
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.1f);
}

TEST(SVRuleInit, ConstantValueInteger) {
	// operandtype 15 = value integer => (float)data
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 15, 7},  // load => 7.0
		{2, 3, 0},   // store in neuron[0] — clamped to 1.0
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f); // clamped from 7.0
}

// ---------------------------------------------------------------------------
// Opcode tests: arithmetic
// ---------------------------------------------------------------------------

TEST(SVRuleOpcode, StopEndsExecution) {
	// A rule that is entirely stops should not crash and should not modify arrays.
	uint8_t rd[48];
	buildRuleData(rd, {}); // all zeros = all stop opcodes
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	bool is_spare = rule.runRule(0.5f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FALSE(is_spare);
	// Nothing should have been written
	for (int i = 0; i < 8; i++) {
		EXPECT_FLOAT_EQ(neuron[i], 0.0f);
	}
}

TEST(SVRuleOpcode, LoadAndStore) {
	// opcode 3 = load from, opcode 2 = store in
	// Load from neuron[2], store in dendrite[5]
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 3, 2},   // load from neuron[2]
		{2, 2, 5},   // store in dendrite[5]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	neuron[2] = 0.75f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(dendrite[5], 0.75f);
}

TEST(SVRuleOpcode, Blank) {
	// opcode 1 = blank (set operand pointer target to 0)
	uint8_t rd[48];
	buildRuleData(rd, {
		{1, 3, 0},  // blank neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	neuron[0] = 0.9f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f);
}

TEST(SVRuleOpcode, Add) {
	// opcode 16 = add operand to accumulator
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},  // load from one (acc = 1.0)
		{16, 11, 124}, // add value(124) => acc = 1.0 + 124/248 = 1.5
		{2, 3, 0},    // store in neuron[0] — clamped to 1.0
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f); // clamped
}

TEST(SVRuleOpcode, AddNoClamp) {
	// Add within range: acc = 0.25 + 0.25 = 0.5
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 62},   // load value(62) => 62/248 = 0.25
		{16, 11, 62},  // add value(62) => acc = 0.5
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 2.0f * 62.0f / 248.0f);
}

TEST(SVRuleOpcode, Subtract) {
	// opcode 17 = subtract
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0
		{17, 11, 124}, // subtract 124/248 = 0.5 => acc = 0.5
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

TEST(SVRuleOpcode, SubtractFrom) {
	// opcode 18 = subtract from: acc = operand - acc
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 62},   // load 62/248 = 0.25
		{18, 10, 0},   // subtract from one => acc = 1.0 - 0.25 = 0.75
		{2, 3, 0},     // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.75f);
}

TEST(SVRuleOpcode, Multiply) {
	// opcode 19 = multiply
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},   // load 124/248 = 0.5
		{19, 11, 124},  // multiply by 0.5 => acc = 0.25
		{2, 3, 0},      // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.25f);
}

TEST(SVRuleOpcode, DivideBy) {
	// opcode 20 = divide by
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},   // load 0.5
		{20, 11, 248},  // divide by 248/248 = 1.0 => acc = 0.5
		{2, 3, 0},      // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

TEST(SVRuleOpcode, DivideByZero) {
	// opcode 20 with zero divisor: should be no-op on accumulator
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},   // load 0.5
		{20, 9, 0},     // divide by zero => no-op
		{2, 3, 0},      // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f); // unchanged
}

TEST(SVRuleOpcode, DivideInto) {
	// opcode 21 = divide into: acc = operand / acc
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},   // load 0.5
		{21, 10, 0},    // divide into: acc = 1.0 / 0.5 = 2.0
		{2, 3, 0},      // store in neuron[0] — clamped to 1.0
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f); // clamped from 2.0
}

TEST(SVRuleOpcode, MinimumWith) {
	// opcode 22 = minimum
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},     // load 1.0
		{22, 11, 124},  // min(1.0, 0.5) = 0.5
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 124.0f / 248.0f);
}

TEST(SVRuleOpcode, MaximumWith) {
	// opcode 23 = maximum
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 9, 0},      // load 0.0
		{23, 11, 124},  // max(0.0, 0.5) = 0.5
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 124.0f / 248.0f);
}

TEST(SVRuleOpcode, LoadNegation) {
	// opcode 26 = load negation of
	uint8_t rd[48];
	buildRuleData(rd, {
		{26, 11, 124},  // acc = -(124/248) = -0.5
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], -0.5f);
}

TEST(SVRuleOpcode, LoadAbsOf) {
	// opcode 27 = load abs of
	uint8_t rd[48];
	buildRuleData(rd, {
		{27, 12, 124},  // load abs of negative value(124) => abs(-124/248) = 0.5
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

TEST(SVRuleOpcode, DistanceTo) {
	// opcode 28 = distance to: acc = abs(acc - operand)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},     // load 1.0
		{28, 11, 62},   // distance to 0.25 => abs(1.0 - 0.25) = 0.75
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.75f);
}

// ---------------------------------------------------------------------------
// Opcode tests: conditionals
// ---------------------------------------------------------------------------

TEST(SVRuleOpcode, IfEqualSkipsNext) {
	// opcode 4 = if equal. If false, skip next instruction.
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0 (acc = 1.0)
		{4, 9, 0},     // if acc == 0.0 => FALSE, skip next
		{2, 3, 0},     // store in neuron[0] — SKIPPED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f); // store was skipped
}

TEST(SVRuleOpcode, IfEqualDoesNotSkip) {
	// if equal and condition is true => execute next
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0 (acc = 1.0)
		{4, 10, 0},    // if acc == 1.0 => TRUE
		{2, 3, 0},     // store in neuron[0] — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleOpcode, IfGreaterThan) {
	// opcode 6: if acc > operand
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0
		{6, 11, 124},  // if 1.0 > 0.5 => TRUE
		{2, 3, 0},     // store in neuron[0] — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleOpcode, IfZero) {
	// opcode 10: if zero (tests operand, not accumulator)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0 into acc
		{10, 9, 0},    // if zero(0.0) => TRUE
		{2, 3, 0},     // store — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleOpcode, IfNonZero) {
	// opcode 11: if non-zero
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0 into acc
		{11, 10, 0},   // if non-zero(1.0) => TRUE
		{2, 3, 0},     // store — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

// ---------------------------------------------------------------------------
// Opcode tests: control flow
// ---------------------------------------------------------------------------

TEST(SVRuleOpcode, StopIfZero) {
	// opcode 46: stop if operand is zero
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0 into acc
		{46, 9, 0},    // stop if zero(0.0) => operand IS zero, STOP
		{2, 3, 0},     // store — NOT REACHED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f); // store not reached
}

TEST(SVRuleOpcode, StopIfNonZero) {
	// opcode 47: stop if operand is non-zero
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},    // load 1.0
		{47, 10, 0},   // stop if non-zero(1.0) => STOP
		{2, 3, 0},     // store — NOT REACHED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f);
}

TEST(SVRuleOpcode, GotoLine) {
	// opcode 52: goto line (1-based). Jump forward to line 4 to skip a store.
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},     // line 1: load 1.0
		{52, 15, 4},    // line 2: goto line 4 (operandtype 15=integer, data=4)
		{2, 3, 0},      // line 3: store in neuron[0] — SKIPPED
		{2, 3, 1},      // line 4: store in neuron[1] — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f); // skipped
	EXPECT_FLOAT_EQ(neuron[1], 1.0f); // executed
}

TEST(SVRuleOpcode, IfNonZeroGoto) {
	// opcode 49: if acc non-zero goto
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},     // line 1: load 1.0 (non-zero)
		{49, 15, 4},    // line 2: if non-zero goto 4
		{2, 3, 0},      // line 3: store in neuron[0] — SKIPPED
		{2, 3, 1},      // line 4: store in neuron[1] — EXECUTED
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f);
	EXPECT_FLOAT_EQ(neuron[1], 1.0f);
}

// ---------------------------------------------------------------------------
// Opcode tests: special
// ---------------------------------------------------------------------------

TEST(SVRuleOpcode, NoOperation) {
	// opcode 30: no-op
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 10, 0},   // load 1.0
		{30, 0, 0},   // nop
		{2, 3, 0},    // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);
}

TEST(SVRuleOpcode, RegisterAsSpare) {
	// opcode 31: register as spare — runRule should return true
	uint8_t rd[48];
	buildRuleData(rd, {
		{31, 0, 0},  // register as spare
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	bool is_spare = rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_TRUE(is_spare);
}

TEST(SVRuleOpcode, RegisterAsSpareReturnsFalse) {
	// When opcode 31 is NOT in the rule, runRule returns false
	uint8_t rd[48];
	buildRuleData(rd, {
		{30, 0, 0},  // nop
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	bool is_spare = rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FALSE(is_spare);
}

TEST(SVRuleOpcode, BoundInRange01) {
	// opcode 32: bound in [0, 1]
	uint8_t rd[48];
	buildRuleData(rd, {
		{32, 12, 124},  // bound(-0.5, 0, 1) => 0.0
		{2, 3, 0},      // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f);
}

TEST(SVRuleOpcode, BoundInRangeNeg1to1) {
	// opcode 33: bound in [-1, 1]
	uint8_t rd[48];
	buildRuleData(rd, {
		{33, 12, 124},  // bound(-0.5, -1, 1) => -0.5
		{2, 3, 0},      // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], -0.5f);
}

TEST(SVRuleOpcode, AddAndStoreIn) {
	// opcode 34: add acc to operand and store in operand pointer
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 62},    // load 0.25 into acc
		{34, 3, 0},     // add and store in neuron[0]: neuron[0] = bind(0.25 + 0)
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.25f);
}

TEST(SVRuleOpcode, NominalThreshold) {
	// opcode 36: if acc < operand, acc = 0
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 62},    // load 0.25 into acc
		{36, 11, 124},  // nominal threshold 0.5: 0.25 < 0.5 => acc = 0
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.0f);
}

TEST(SVRuleOpcode, NominalThresholdAbove) {
	// When acc >= operand, acc is unchanged
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 11, 124},   // load 0.5 into acc
		{36, 11, 62},   // nominal threshold 0.25: 0.5 >= 0.25 => no change
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

TEST(SVRuleOpcode, StoreAbsIn) {
	// opcode 45: store abs(acc) into operand pointer
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 12, 124},   // load negative value => -0.5
		{45, 3, 0},     // store abs in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

// ---------------------------------------------------------------------------
// Opcode tests: tend to
// ---------------------------------------------------------------------------

TEST(SVRuleOpcode, TendTo) {
	// opcode 24 = set tend rate, opcode 25 = tend to
	// acc += tendrate * (operand - acc)
	// Set rate=1.0, tend to 0.5 from 0.0 => 0.0 + 1.0*(0.5-0.0) = 0.5
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 9, 0},      // load 0.0
		{24, 10, 0},    // set tend rate = 1.0
		{25, 11, 124},  // tend to 0.5 => 0.0 + 1.0*(0.5-0.0) = 0.5
		{2, 3, 0},      // store
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.5f);
}

// ---------------------------------------------------------------------------
// Operand type tests: reading from arrays
// ---------------------------------------------------------------------------

TEST(SVRuleOperand, ReadFromSrcNeuron) {
	// operandtype 1 = input neuron (srcneuron)
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 1, 3},   // load from srcneuron[3]
		{2, 3, 0},   // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	src[3] = 0.42f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.42f);
}

TEST(SVRuleOperand, ReadFromDendrite) {
	// operandtype 2 = dendrite
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 2, 1},   // load from dendrite[1]
		{2, 3, 0},   // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	dendrite[1] = 0.77f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.77f);
}

TEST(SVRuleOperand, ReadFromSpareNeuron) {
	// operandtype 4 = spare neuron
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 4, 6},   // load from spareneuron[6]
		{2, 3, 0},   // store in neuron[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	spare[6] = 0.33f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.33f);
}

TEST(SVRuleOperand, AccumulatorPassthrough) {
	// operandtype 0 = accumulator. The initial acc value passed to runRule.
	uint8_t rd[48];
	buildRuleData(rd, {
		{2, 3, 0},  // store acc into neuron[0] (acc comes from the initial parameter)
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.6f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 0.6f);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(SVRuleEdge, AllZeroInput) {
	// All-zero rule data = opcode 0 (stop) everywhere. Should be safe.
	uint8_t rd[48];
	std::memset(rd, 0, 48);
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	bool is_spare = rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FALSE(is_spare);
}

TEST(SVRuleEdge, AllMaxInput) {
	// All 0xFF bytes. Opcode 255 = unknown, operandtype 255 = unknown.
	// Should not crash — unknown opcodes are silently ignored.
	uint8_t rd[48];
	std::memset(rd, 0xFF, 48);
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	// Should not crash
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
}

TEST(SVRuleEdge, StoreClampsBothDirections) {
	// Store (opcode 2) clamps to [-1, 1].
	// Test positive overflow
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 13, 248},  // load value*10(248) => 10.0
		{2, 3, 0},     // store => clamped to 1.0
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[0], 1.0f);

	// Test negative overflow: load a large negative value
	uint8_t rd2[48];
	buildRuleData(rd2, {
		{26, 13, 248},  // load negation of value*10(248) => -10.0
		{2, 3, 1},      // store in neuron[1] => clamped to -1.0
	});
	c2eSVRule rule2;
	rule2.init(rd2);
	zeroArray(neuron);
	rule2.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(neuron[1], -1.0f);
}

TEST(SVRuleEdge, OperandDataSanitisedTo7) {
	// operandtype 3 (neuron) with operanddata > 7 should be clamped to 7 during init
	uint8_t rd[48];
	buildRuleData(rd, {
		{3, 3, 200},  // load from neuron[200] — init clamps to neuron[7]
		{2, 2, 0},    // store in dendrite[0]
	});
	c2eSVRule rule;
	rule.init(rd);
	float src[8], neuron[8], spare[8], dendrite[8];
	zeroArray(src); zeroArray(neuron); zeroArray(spare); zeroArray(dendrite);
	neuron[7] = 0.88f;
	rule.runRule(0.0f, src, neuron, spare, dendrite, nullptr);
	EXPECT_FLOAT_EQ(dendrite[0], 0.88f);
}

// ---------------------------------------------------------------------------
// Neuron/dendrite struct tests
// ---------------------------------------------------------------------------

TEST(BrainStructs, NeuronDefaultState) {
	c2eNeuron n;
	// Verify struct can be zero-initialised
	std::memset(&n, 0, sizeof(n));
	for (int i = 0; i < 8; i++) {
		EXPECT_FLOAT_EQ(n.variables[i], 0.0f);
	}
	EXPECT_FLOAT_EQ(n.input, 0.0f);
}

TEST(BrainStructs, DendriteDefaultState) {
	c2eDendrite d;
	std::memset(&d, 0, sizeof(d));
	for (int i = 0; i < 8; i++) {
		EXPECT_FLOAT_EQ(d.variables[i], 0.0f);
	}
	EXPECT_EQ(d.source, nullptr);
	EXPECT_EQ(d.dest, nullptr);
}

TEST(BrainStructs, RuleStructPacking) {
	c2erule r;
	r.opcode = 3;
	r.operandtype = 10;
	r.operanddata = 0;
	r.operandvalue = 1.0f;
	EXPECT_EQ(r.opcode, 3);
	EXPECT_EQ(r.operandtype, 10);
	EXPECT_FLOAT_EQ(r.operandvalue, 1.0f);
}
