/*
 *  CaosValueTest.cpp
 *  openc2e
 *
 *  Unit tests for the caosValue type system.
 */

#include "openc2e/caosValue.h"

#include <gtest/gtest.h>
#include <string>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(CaosValue, DefaultConstructionIsInt0) {
	caosValue v;
	// Default-constructed caosValue is int(0), not null
	EXPECT_TRUE(v.hasInt());
	EXPECT_EQ(v.getInt(), 0);
}

TEST(CaosValue, ConstructFromInt) {
	caosValue v(42);
	EXPECT_FALSE(v.isNull());
	EXPECT_EQ(v.getType(), CAOSINT);
	EXPECT_EQ(v.getInt(), 42);
}

TEST(CaosValue, ConstructFromNegativeInt) {
	caosValue v(-7);
	EXPECT_EQ(v.getInt(), -7);
	EXPECT_TRUE(v.hasInt());
}

TEST(CaosValue, ConstructFromZeroInt) {
	caosValue v(0);
	EXPECT_EQ(v.getInt(), 0);
	EXPECT_FALSE(v.isNull()); // int 0 is NOT null
}

TEST(CaosValue, ConstructFromFloat) {
	caosValue v(3.14f);
	EXPECT_EQ(v.getType(), CAOSFLOAT);
	EXPECT_FLOAT_EQ(v.getFloat(), 3.14f);
}

TEST(CaosValue, ConstructFromString) {
	caosValue v(std::string("hello"));
	EXPECT_EQ(v.getType(), CAOSSTR);
	EXPECT_EQ(v.getString(), "hello");
}

TEST(CaosValue, ConstructFromEmptyString) {
	caosValue v(std::string(""));
	EXPECT_EQ(v.getType(), CAOSSTR);
	EXPECT_EQ(v.getString(), "");
	EXPECT_FALSE(v.isNull()); // empty string is NOT null
}

TEST(CaosValue, ConstructFromNullAgent) {
	caosValue v((Agent*)nullptr);
	EXPECT_EQ(v.getType(), CAOSAGENT);
	EXPECT_TRUE(v.hasAgent());
	EXPECT_EQ(v.getAgent(), nullptr);
}

TEST(CaosValue, ConstructFromVector) {
	Vector<float> vec(3.0f, 4.0f);
	caosValue v(vec);
	EXPECT_EQ(v.getType(), CAOSVEC);
	EXPECT_TRUE(v.hasVector());
	EXPECT_FLOAT_EQ(v.getVector().x, 3.0f);
	EXPECT_FLOAT_EQ(v.getVector().y, 4.0f);
}

TEST(CaosValue, ConstructFromByteString) {
	bytestring_t bs = {0x01, 0x02, 0xFF};
	caosValue v(bs);
	EXPECT_EQ(v.getType(), CAOSBYTESTRING);
	EXPECT_TRUE(v.hasByteStr());
	EXPECT_EQ(v.getByteStr().size(), 3u);
	EXPECT_EQ(v.getByteStr()[2], 0xFF);
}

TEST(CaosValue, CopyConstruction) {
	caosValue a(99);
	caosValue b(a);
	EXPECT_EQ(b.getInt(), 99);
	// Mutating original must not affect copy
	a.setInt(1);
	EXPECT_EQ(b.getInt(), 99);
}

TEST(CaosValue, AssignmentOperator) {
	caosValue a(10);
	caosValue b(std::string("x"));
	b = a;
	EXPECT_EQ(b.getType(), CAOSINT);
	EXPECT_EQ(b.getInt(), 10);
}

// ---------------------------------------------------------------------------
// Type predicates
// ---------------------------------------------------------------------------

TEST(CaosValue, HasIntTrueForInt) {
	caosValue v(5);
	EXPECT_TRUE(v.hasInt());
	EXPECT_FALSE(v.hasFloat());
	EXPECT_FALSE(v.hasString());
	EXPECT_FALSE(v.hasAgent());
	EXPECT_FALSE(v.hasVector());
	EXPECT_FALSE(v.hasByteStr());
}

TEST(CaosValue, HasFloatTrueForFloat) {
	caosValue v(1.0f);
	EXPECT_TRUE(v.hasFloat());
	EXPECT_FALSE(v.hasInt());
}

TEST(CaosValue, HasDecimalCoversIntAndFloat) {
	caosValue i(1);
	caosValue f(1.0f);
	EXPECT_TRUE(i.hasDecimal());
	EXPECT_TRUE(f.hasDecimal());
	EXPECT_TRUE(i.hasNumber());
	EXPECT_TRUE(f.hasNumber());
}

TEST(CaosValue, HasDecimalCoversVector) {
	Vector<float> vec(1.0f, 2.0f);
	caosValue v(vec);
	EXPECT_TRUE(v.hasDecimal());
	EXPECT_TRUE(v.hasNumber());
}

// ---------------------------------------------------------------------------
// Getters — successful cross-type coercions
// ---------------------------------------------------------------------------

TEST(CaosValue, GetFloatFromInt) {
	// getFloat() on an int should promote to float
	caosValue v(7);
	EXPECT_FLOAT_EQ(v.getFloat(), 7.0f);
}

TEST(CaosValue, GetIntFromFloatTruncatesBelow05) {
	// The custom rounding: positive float with fractional < 0.5 truncates toward zero
	caosValue v(2.3f);
	EXPECT_EQ(v.getInt(), 2);
}

TEST(CaosValue, GetIntFromFloatRoundsUpAtHalf) {
	// Fractional == 0.5 rounds UP for positive values
	caosValue v(2.5f);
	EXPECT_EQ(v.getInt(), 3);
}

TEST(CaosValue, GetIntFromFloatRoundsUpAboveHalf) {
	caosValue v(2.7f);
	EXPECT_EQ(v.getInt(), 3);
}

TEST(CaosValue, GetIntFromNegativeFloatTruncatesAboveMinus05) {
	// Negative float with |fractional| < 0.5 truncates toward zero
	caosValue v(-2.3f);
	EXPECT_EQ(v.getInt(), -2);
}

TEST(CaosValue, GetIntFromNegativeFloatRoundsDownAtMinusHalf) {
	// Fractional == -0.5 rounds DOWN (away from zero) for negative values
	caosValue v(-2.5f);
	EXPECT_EQ(v.getInt(), -3);
}

TEST(CaosValue, GetIntFromNegativeFloatRoundsDownBelowMinusHalf) {
	caosValue v(-2.7f);
	EXPECT_EQ(v.getInt(), -3);
}

TEST(CaosValue, GetIntFromExactFloat) {
	caosValue v(5.0f);
	EXPECT_EQ(v.getInt(), 5);
}

TEST(CaosValue, GetIntFromVector) {
	// Vector getInt returns magnitude: sqrt(3^2 + 4^2) = 5
	Vector<float> vec(3.0f, 4.0f);
	caosValue v(vec);
	EXPECT_EQ(v.getInt(), 5);
}

TEST(CaosValue, GetFloatFromVector) {
	Vector<float> vec(3.0f, 4.0f);
	caosValue v(vec);
	EXPECT_FLOAT_EQ(v.getFloat(), 5.0f);
}

// ---------------------------------------------------------------------------
// Getters — type errors
// ---------------------------------------------------------------------------

TEST(CaosValue, GetIntFromStringThrows) {
	caosValue v(std::string("nope"));
	EXPECT_THROW(v.getInt(), wrongCaosValueTypeException);
}

TEST(CaosValue, GetFloatFromStringThrows) {
	caosValue v(std::string("nope"));
	EXPECT_THROW(v.getFloat(), wrongCaosValueTypeException);
}

TEST(CaosValue, GetStringFromIntThrows) {
	caosValue v(42);
	EXPECT_THROW(v.getString(), wrongCaosValueTypeException);
}

TEST(CaosValue, GetAgentFromIntThrows) {
	caosValue v(1);
	// In engine version 3 (C3/DS), int-to-agent coercion is not supported
	// This may or may not throw depending on engine.version global, but the
	// code path for non-agent non-int types always throws.
	EXPECT_THROW(v.getAgentRef(), wrongCaosValueTypeException);
}

TEST(CaosValue, GetByteStrFromIntThrows) {
	caosValue v(10);
	EXPECT_THROW(v.getByteStr(), wrongCaosValueTypeException);
}

TEST(CaosValue, GetVectorFromIntThrows) {
	caosValue v(10);
	EXPECT_THROW(v.getVector(), wrongCaosValueTypeException);
}

TEST(CaosValue, DefaultValueReturnsZero) {
	// Default caosValue is int(0), so getInt succeeds
	caosValue v;
	EXPECT_EQ(v.getInt(), 0);
}

// ---------------------------------------------------------------------------
// Setters — changing type in-place
// ---------------------------------------------------------------------------

TEST(CaosValue, SetIntOverwritesPreviousType) {
	caosValue v(std::string("was a string"));
	v.setInt(123);
	EXPECT_EQ(v.getType(), CAOSINT);
	EXPECT_EQ(v.getInt(), 123);
}

TEST(CaosValue, SetFloatOverwritesPreviousType) {
	caosValue v(42);
	v.setFloat(1.5f);
	EXPECT_EQ(v.getType(), CAOSFLOAT);
	EXPECT_FLOAT_EQ(v.getFloat(), 1.5f);
}

TEST(CaosValue, SetStringOverwritesPreviousType) {
	caosValue v(42);
	v.setString("now a string");
	EXPECT_EQ(v.getType(), CAOSSTR);
	EXPECT_EQ(v.getString(), "now a string");
}

TEST(CaosValue, ResetMakesNull) {
	caosValue v(42);
	v.reset();
	EXPECT_TRUE(v.isNull());
	EXPECT_EQ(v.getType(), CAOSNULL);
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST(CaosValue, IntEquality) {
	caosValue a(10);
	caosValue b(10);
	caosValue c(20);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
	EXPECT_TRUE(a != c);
	EXPECT_FALSE(a != b);
}

TEST(CaosValue, FloatEquality) {
	caosValue a(1.5f);
	caosValue b(1.5f);
	EXPECT_TRUE(a == b);
}

TEST(CaosValue, IntVsFloatEquality) {
	// int 3 hasInt, float 3.0 hasFloat; both hasDecimal
	// operator== promotes to float comparison via getFloat()
	caosValue i(3);
	caosValue f(3.0f);
	EXPECT_TRUE(i == f);
	EXPECT_TRUE(f == i);
}

TEST(CaosValue, IntVsFloatInequality) {
	caosValue i(3);
	caosValue f(3.5f);
	EXPECT_FALSE(i == f);
}

TEST(CaosValue, StringEquality) {
	caosValue a(std::string("abc"));
	caosValue b(std::string("abc"));
	caosValue c(std::string("xyz"));
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(CaosValue, IntLessThan) {
	caosValue a(1);
	caosValue b(2);
	EXPECT_TRUE(a < b);
	EXPECT_FALSE(b < a);
}

TEST(CaosValue, IntGreaterThan) {
	caosValue a(5);
	caosValue b(3);
	EXPECT_TRUE(a > b);
	EXPECT_FALSE(b > a);
}

TEST(CaosValue, FloatLessThan) {
	caosValue a(1.1f);
	caosValue b(2.2f);
	EXPECT_TRUE(a < b);
}

TEST(CaosValue, IntVsFloatLessThan) {
	// Cross-type: int and float both hasDecimal, so comparison promotes to float
	caosValue i(2);
	caosValue f(2.5f);
	EXPECT_TRUE(i < f);
	EXPECT_FALSE(f < i);
}

TEST(CaosValue, StringLessThan) {
	caosValue a(std::string("aaa"));
	caosValue b(std::string("bbb"));
	EXPECT_TRUE(a < b);
	EXPECT_TRUE(b > a);
}

TEST(CaosValue, ComparisonIntVsStringThrows) {
	caosValue i(1);
	caosValue s(std::string("x"));
	EXPECT_THROW((void)(i == s), caosException);
	EXPECT_THROW((void)(i < s), caosException);
	EXPECT_THROW((void)(i > s), caosException);
}

TEST(CaosValue, VectorEquality) {
	Vector<float> v1(1.0f, 2.0f);
	Vector<float> v2(1.0f, 2.0f);
	Vector<float> v3(3.0f, 4.0f);
	caosValue a(v1);
	caosValue b(v2);
	caosValue c(v3);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(CaosValue, VectorLessThanGreaterThan) {
	Vector<float> v1(1.0f, 1.0f);
	Vector<float> v2(2.0f, 1.0f);
	caosValue a(v1);
	caosValue b(v2);
	EXPECT_TRUE(a < b);
	EXPECT_TRUE(b > a);
	EXPECT_FALSE(a > b);
}

// ---------------------------------------------------------------------------
// dump()
// ---------------------------------------------------------------------------

TEST(CaosValue, DumpDefault) {
	// Default caosValue is int(0)
	caosValue v;
	EXPECT_EQ(v.dump(), "Int 0 ");
}

TEST(CaosValue, DumpInt) {
	caosValue v(42);
	std::string d = v.dump();
	// Should contain "Int" and "42"
	EXPECT_NE(d.find("Int"), std::string::npos);
	EXPECT_NE(d.find("42"), std::string::npos);
}

TEST(CaosValue, DumpFloat) {
	caosValue v(1.5f);
	std::string d = v.dump();
	EXPECT_NE(d.find("Float"), std::string::npos);
}

TEST(CaosValue, DumpString) {
	caosValue v(std::string("test"));
	std::string d = v.dump();
	EXPECT_NE(d.find("String"), std::string::npos);
	EXPECT_NE(d.find("test"), std::string::npos);
}

TEST(CaosValue, DumpVector) {
	Vector<float> vec(1.0f, 2.0f);
	caosValue v(vec);
	std::string d = v.dump();
	EXPECT_NE(d.find("Vector"), std::string::npos);
}

TEST(CaosValue, DumpByteString) {
	bytestring_t bs = {0x0A};
	caosValue v(bs);
	std::string d = v.dump();
	EXPECT_NE(d.find("Bytestring"), std::string::npos);
	EXPECT_NE(d.find("10"), std::string::npos); // 0x0A = 10 decimal
}

// ---------------------------------------------------------------------------
// FaceValue dual-type behavior
// ---------------------------------------------------------------------------

TEST(CaosValue, FaceValueHasIntAndString) {
	FaceValue fv;
	fv.pose = 7;
	fv.sprite_filename = "norn.spr";
	caosValue v(fv);
	EXPECT_EQ(v.getType(), CAOSFACEVALUE);
	// FaceValue reports as both hasInt and hasString
	EXPECT_TRUE(v.hasInt());
	EXPECT_TRUE(v.hasString());
	EXPECT_TRUE(v.hasDecimal());
	EXPECT_EQ(v.getInt(), 7);
	EXPECT_EQ(v.getString(), "norn.spr");
}

TEST(CaosValue, FaceValueGetFloat) {
	FaceValue fv;
	fv.pose = 3;
	fv.sprite_filename = "test.spr";
	caosValue v(fv);
	EXPECT_FLOAT_EQ(v.getFloat(), 3.0f);
}

/* vim: set noet: */
