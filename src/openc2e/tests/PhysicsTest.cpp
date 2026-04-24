#include "openc2e/physics.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Point tests
// ---------------------------------------------------------------------------

TEST(Point, DefaultConstruction) {
	Point p;
	EXPECT_FLOAT_EQ(p.x, 0.0f);
	EXPECT_FLOAT_EQ(p.y, 0.0f);
}

TEST(Point, ParameterisedConstruction) {
	Point p(3.5f, -7.2f);
	EXPECT_FLOAT_EQ(p.x, 3.5f);
	EXPECT_FLOAT_EQ(p.y, -7.2f);
}

TEST(Point, CopyConstruction) {
	Point a(1.0f, 2.0f);
	Point b(a);
	EXPECT_FLOAT_EQ(b.x, 1.0f);
	EXPECT_FLOAT_EQ(b.y, 2.0f);
}

TEST(Point, EqualityAndInequality) {
	Point a(1.0f, 2.0f);
	Point b(1.0f, 2.0f);
	Point c(1.0f, 3.0f);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a != c);
	EXPECT_FALSE(a == c);
}

TEST(Point, Assignment) {
	Point a(5.0f, 6.0f);
	Point b;
	b = a;
	EXPECT_FLOAT_EQ(b.x, 5.0f);
	EXPECT_FLOAT_EQ(b.y, 6.0f);
}

// ---------------------------------------------------------------------------
// Vector tests
// ---------------------------------------------------------------------------

TEST(Vector, DefaultConstruction) {
	Vector<> v;
	EXPECT_DOUBLE_EQ(v.x, 0.0);
	EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(Vector, ParameterisedConstruction) {
	Vector<> v(3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.x, 3.0);
	EXPECT_DOUBLE_EQ(v.y, 4.0);
}

TEST(Vector, ConstructFromTwoPoints) {
	Point a(1.0f, 2.0f);
	Point b(4.0f, 6.0f);
	Vector<> v(a, b);
	EXPECT_DOUBLE_EQ(v.x, 3.0);
	EXPECT_DOUBLE_EQ(v.y, 4.0);
}

TEST(Vector, Magnitude) {
	Vector<> v(3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.getMagnitude(), 5.0);
}

TEST(Vector, MagnitudeZero) {
	Vector<> v(0.0, 0.0);
	EXPECT_DOUBLE_EQ(v.getMagnitude(), 0.0);
}

TEST(Vector, Addition) {
	Vector<> a(1.0, 2.0);
	Vector<> b(3.0, 5.0);
	Vector<> c = a + b;
	EXPECT_DOUBLE_EQ(c.x, 4.0);
	EXPECT_DOUBLE_EQ(c.y, 7.0);
}

TEST(Vector, Subtraction) {
	Vector<> a(5.0, 7.0);
	Vector<> b(3.0, 2.0);
	Vector<> c = a - b;
	EXPECT_DOUBLE_EQ(c.x, 2.0);
	EXPECT_DOUBLE_EQ(c.y, 5.0);
}

TEST(Vector, ScalarMultiply) {
	Vector<> v(2.0, 3.0);
	Vector<> r = v * 4.0;
	EXPECT_DOUBLE_EQ(r.x, 8.0);
	EXPECT_DOUBLE_EQ(r.y, 12.0);
}

TEST(Vector, Scale) {
	Vector<> v(2.0, 3.0);
	Vector<> r = v.scale(0.5);
	EXPECT_DOUBLE_EQ(r.x, 1.0);
	EXPECT_DOUBLE_EQ(r.y, 1.5);
}

TEST(Vector, ScaleToMagnitude) {
	Vector<> v(3.0, 4.0); // magnitude 5
	Vector<> r = v.scaleToMagnitude(10.0);
	EXPECT_NEAR(r.getMagnitude(), 10.0, 1e-9);
	// Direction preserved: ratio x/y should be the same
	EXPECT_NEAR(r.x / r.y, 3.0 / 4.0, 1e-9);
}

TEST(Vector, Equality) {
	Vector<> a(1.0, 2.0);
	Vector<> b(1.0, 2.0);
	Vector<> c(1.0, 3.0);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(Vector, UnitVector) {
	Vector<> v = Vector<>::unitVector(0.0); // angle 0 -> (1, 0)
	EXPECT_NEAR(v.x, 1.0, 1e-9);
	EXPECT_NEAR(v.y, 0.0, 1e-9);

	Vector<> v2 = Vector<>::unitVector(M_PI / 2.0); // angle pi/2 -> (0, 1)
	EXPECT_NEAR(v2.x, 0.0, 1e-9);
	EXPECT_NEAR(v2.y, 1.0, 1e-9);
}

TEST(Vector, PointPlusVector) {
	Point p(1.0f, 2.0f);
	Vector<> v(3.0, 4.0);
	Point r = p + v;
	EXPECT_FLOAT_EQ(r.x, 4.0f);
	EXPECT_FLOAT_EQ(r.y, 6.0f);

	// Commutative
	Point r2 = v + p;
	EXPECT_FLOAT_EQ(r2.x, 4.0f);
	EXPECT_FLOAT_EQ(r2.y, 6.0f);
}

// ---------------------------------------------------------------------------
// Line construction tests
// ---------------------------------------------------------------------------

TEST(Line, DefaultConstruction) {
	Line l;
	EXPECT_FLOAT_EQ(l.getStart().x, 0.0f);
	EXPECT_FLOAT_EQ(l.getStart().y, 0.0f);
	EXPECT_FLOAT_EQ(l.getEnd().x, 1.0f);
	EXPECT_FLOAT_EQ(l.getEnd().y, 1.0f);
	EXPECT_EQ(l.getType(), NORMAL);
	EXPECT_DOUBLE_EQ(l.getSlope(), 1.0);
}

TEST(Line, HorizontalLine) {
	Line l(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	EXPECT_EQ(l.getType(), HORIZONTAL);
	EXPECT_DOUBLE_EQ(l.getSlope(), 0.0);
	EXPECT_DOUBLE_EQ(l.yIntercept(), 5.0);
}

TEST(Line, VerticalLine) {
	Line l(Point(3.0f, 0.0f), Point(3.0f, 10.0f));
	EXPECT_EQ(l.getType(), VERTICAL);
	EXPECT_DOUBLE_EQ(l.xIntercept(), 3.0);
}

TEST(Line, NormalLine) {
	// y = 2x + 1: passes through (0,1) and (5,11)
	Line l(Point(0.0f, 1.0f), Point(5.0f, 11.0f));
	EXPECT_EQ(l.getType(), NORMAL);
	EXPECT_DOUBLE_EQ(l.getSlope(), 2.0);
	EXPECT_DOUBLE_EQ(l.yIntercept(), 1.0);
	EXPECT_DOUBLE_EQ(l.xIntercept(), -0.5);
}

TEST(Line, ConstructionSwapsEndpoints) {
	// When start.x > end.x, the constructor swaps them
	Line l(Point(10.0f, 5.0f), Point(2.0f, 3.0f));
	EXPECT_FLOAT_EQ(l.getStart().x, 2.0f);
	EXPECT_FLOAT_EQ(l.getEnd().x, 10.0f);
}

// ---------------------------------------------------------------------------
// Line::containsX / containsY
// ---------------------------------------------------------------------------

TEST(Line, ContainsX) {
	Line l(Point(2.0f, 0.0f), Point(8.0f, 0.0f));
	EXPECT_TRUE(l.containsX(2.0));
	EXPECT_TRUE(l.containsX(5.0));
	EXPECT_TRUE(l.containsX(8.0));
	EXPECT_FALSE(l.containsX(1.99));
	EXPECT_FALSE(l.containsX(8.01));
}

TEST(Line, ContainsY_AscendingLine) {
	Line l(Point(0.0f, 2.0f), Point(10.0f, 8.0f));
	EXPECT_TRUE(l.containsY(2.0));
	EXPECT_TRUE(l.containsY(5.0));
	EXPECT_TRUE(l.containsY(8.0));
	EXPECT_FALSE(l.containsY(1.5));
	EXPECT_FALSE(l.containsY(9.0));
}

TEST(Line, ContainsY_DescendingLine) {
	// start.y > end.y after swap (start is left-most)
	Line l(Point(0.0f, 10.0f), Point(10.0f, 2.0f));
	EXPECT_TRUE(l.containsY(2.0));
	EXPECT_TRUE(l.containsY(6.0));
	EXPECT_TRUE(l.containsY(10.0));
	EXPECT_FALSE(l.containsY(1.0));
	EXPECT_FALSE(l.containsY(11.0));
}

// ---------------------------------------------------------------------------
// Line::pointAtX / pointAtY
// ---------------------------------------------------------------------------

TEST(Line, PointAtX_NormalLine) {
	// y = 2x + 1
	Line l(Point(0.0f, 1.0f), Point(5.0f, 11.0f));
	Point p = l.pointAtX(3.0);
	EXPECT_NEAR(p.x, 3.0f, 1e-5);
	EXPECT_NEAR(p.y, 7.0f, 1e-5);
}

TEST(Line, PointAtX_HorizontalLine) {
	Line l(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	Point p = l.pointAtX(7.0);
	EXPECT_NEAR(p.x, 7.0f, 1e-5);
	EXPECT_NEAR(p.y, 5.0f, 1e-5);
}

TEST(Line, PointAtY_NormalLine) {
	// y = 2x + 1 => x = (y - 1) / 2
	Line l(Point(0.0f, 1.0f), Point(5.0f, 11.0f));
	Point p = l.pointAtY(7.0);
	EXPECT_NEAR(p.x, 3.0f, 1e-5);
	EXPECT_NEAR(p.y, 7.0f, 1e-5);
}

TEST(Line, PointAtY_VerticalLine) {
	Line l(Point(3.0f, 0.0f), Point(3.0f, 10.0f));
	Point p = l.pointAtY(5.0);
	EXPECT_NEAR(p.x, 3.0f, 1e-5);
	EXPECT_NEAR(p.y, 5.0f, 1e-5);
}

// ---------------------------------------------------------------------------
// Line::containsPoint
// ---------------------------------------------------------------------------

TEST(Line, ContainsPoint_OnNormalLine) {
	// y = 2x + 1, point (2, 5) is on the line
	Line l(Point(0.0f, 1.0f), Point(5.0f, 11.0f));
	EXPECT_TRUE(l.containsPoint(Point(2.0f, 5.0f)));
	EXPECT_TRUE(l.containsPoint(Point(0.0f, 1.0f)));  // start
	EXPECT_TRUE(l.containsPoint(Point(5.0f, 11.0f))); // end
}

TEST(Line, ContainsPoint_OffNormalLine) {
	Line l(Point(0.0f, 1.0f), Point(5.0f, 11.0f));
	// Point clearly not on the line
	EXPECT_FALSE(l.containsPoint(Point(2.0f, 10.0f)));
	// Point beyond segment endpoint
	EXPECT_FALSE(l.containsPoint(Point(6.0f, 13.0f)));
	// Point before segment start
	EXPECT_FALSE(l.containsPoint(Point(-1.0f, -1.0f)));
}

TEST(Line, ContainsPoint_OnHorizontalLine) {
	Line l(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	EXPECT_TRUE(l.containsPoint(Point(5.0f, 5.0f)));
	EXPECT_TRUE(l.containsPoint(Point(0.0f, 5.0f)));
	EXPECT_TRUE(l.containsPoint(Point(10.0f, 5.0f)));
}

TEST(Line, ContainsPoint_OffHorizontalLine) {
	Line l(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	// Wrong y
	EXPECT_FALSE(l.containsPoint(Point(5.0f, 6.5f)));
	// Outside x range
	EXPECT_FALSE(l.containsPoint(Point(11.0f, 5.0f)));
}

TEST(Line, ContainsPoint_OnVerticalLine) {
	Line l(Point(3.0f, 0.0f), Point(3.0f, 10.0f));
	EXPECT_TRUE(l.containsPoint(Point(3.0f, 5.0f)));
	EXPECT_TRUE(l.containsPoint(Point(3.0f, 0.0f)));
	EXPECT_TRUE(l.containsPoint(Point(3.0f, 10.0f)));
}

TEST(Line, ContainsPoint_OffVerticalLine) {
	Line l(Point(3.0f, 0.0f), Point(3.0f, 10.0f));
	// Wrong x
	EXPECT_FALSE(l.containsPoint(Point(5.0f, 5.0f)));
	// Outside y range
	EXPECT_FALSE(l.containsPoint(Point(3.0f, 11.0f)));
}

// ---------------------------------------------------------------------------
// Line::intersect — crossing lines
// ---------------------------------------------------------------------------

TEST(LineIntersect, TwoNormalLinesCrossing) {
	// y = x (through origin, 0..10) and y = -x + 10 (through (0,10)..(10,0))
	Line l1(Point(0.0f, 0.0f), Point(10.0f, 10.0f));
	Line l2(Point(0.0f, 10.0f), Point(10.0f, 0.0f));
	Point where;
	EXPECT_TRUE(l1.intersect(l2, where));
	EXPECT_NEAR(where.x, 5.0f, 1e-4);
	EXPECT_NEAR(where.y, 5.0f, 1e-4);
}

TEST(LineIntersect, HorizontalCrossesNormal) {
	// Horizontal y=3, x in [0..10]
	Line horiz(Point(0.0f, 3.0f), Point(10.0f, 3.0f));
	// y = x, x in [0..10]
	Line diag(Point(0.0f, 0.0f), Point(10.0f, 10.0f));
	Point where;
	EXPECT_TRUE(horiz.intersect(diag, where));
	EXPECT_NEAR(where.x, 3.0f, 1e-4);
	EXPECT_NEAR(where.y, 3.0f, 1e-4);
}

TEST(LineIntersect, VerticalCrossesNormal) {
	// Vertical x=4, y in [0..10]
	Line vert(Point(4.0f, 0.0f), Point(4.0f, 10.0f));
	// y = x, x in [0..10]
	Line diag(Point(0.0f, 0.0f), Point(10.0f, 10.0f));
	Point where;
	EXPECT_TRUE(vert.intersect(diag, where));
	EXPECT_NEAR(where.x, 4.0f, 1e-4);
	EXPECT_NEAR(where.y, 4.0f, 1e-4);
}

TEST(LineIntersect, HorizontalCrossesVertical) {
	Line horiz(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	Line vert(Point(3.0f, 0.0f), Point(3.0f, 10.0f));
	Point where;
	EXPECT_TRUE(horiz.intersect(vert, where));
	EXPECT_NEAR(where.x, 3.0f, 1e-4);
	EXPECT_NEAR(where.y, 5.0f, 1e-4);

	// Commutative check
	Point where2;
	EXPECT_TRUE(vert.intersect(horiz, where2));
	EXPECT_NEAR(where2.x, 3.0f, 1e-4);
	EXPECT_NEAR(where2.y, 5.0f, 1e-4);
}

// ---------------------------------------------------------------------------
// Line::intersect — non-intersecting cases
// ---------------------------------------------------------------------------

TEST(LineIntersect, ParallelNormalLines) {
	// y = x and y = x + 2 (same slope, different intercept)
	Line l1(Point(0.0f, 0.0f), Point(10.0f, 10.0f));
	Line l2(Point(0.0f, 2.0f), Point(10.0f, 12.0f));
	Point where;
	EXPECT_FALSE(l1.intersect(l2, where));
}

TEST(LineIntersect, ParallelHorizontalLines) {
	Line l1(Point(0.0f, 3.0f), Point(10.0f, 3.0f));
	Line l2(Point(0.0f, 7.0f), Point(10.0f, 7.0f));
	Point where;
	EXPECT_FALSE(l1.intersect(l2, where));
}

TEST(LineIntersect, ParallelVerticalLines) {
	Line l1(Point(2.0f, 0.0f), Point(2.0f, 10.0f));
	Line l2(Point(5.0f, 0.0f), Point(5.0f, 10.0f));
	Point where;
	EXPECT_FALSE(l1.intersect(l2, where));
}

TEST(LineIntersect, CollinearOverlappingReturnsNoIntersection) {
	// The implementation returns false for collinear/coincident lines
	Line l1(Point(0.0f, 0.0f), Point(10.0f, 10.0f));
	Line l2(Point(5.0f, 5.0f), Point(15.0f, 15.0f));
	Point where;
	EXPECT_FALSE(l1.intersect(l2, where));
}

TEST(LineIntersect, NonOverlappingSegments) {
	// Lines would cross if extended, but segments don't overlap
	// y = x on [0,3] and y = -x + 10 on [8,10]
	Line l1(Point(0.0f, 0.0f), Point(3.0f, 3.0f));
	Line l2(Point(8.0f, 2.0f), Point(10.0f, 0.0f));
	Point where;
	EXPECT_FALSE(l1.intersect(l2, where));
}

// ---------------------------------------------------------------------------
// Line::intersect — T-intersections (endpoint touching)
// ---------------------------------------------------------------------------

TEST(LineIntersect, TIntersectionVerticalMeetsHorizontal) {
	// Vertical line ending exactly on horizontal line
	Line horiz(Point(0.0f, 5.0f), Point(10.0f, 5.0f));
	Line vert(Point(5.0f, 0.0f), Point(5.0f, 5.0f));
	Point where;
	EXPECT_TRUE(horiz.intersect(vert, where));
	EXPECT_NEAR(where.x, 5.0f, 1e-4);
	EXPECT_NEAR(where.y, 5.0f, 1e-4);
}

TEST(LineIntersect, NormalMeetsAtEndpoint) {
	// y = x on [0..5] meets y = -x + 10 on [5..10], intersecting at (5,5)
	Line l1(Point(0.0f, 0.0f), Point(5.0f, 5.0f));
	Line l2(Point(5.0f, 5.0f), Point(10.0f, 0.0f));
	Point where;
	EXPECT_TRUE(l1.intersect(l2, where));
	EXPECT_NEAR(where.x, 5.0f, 1e-4);
	EXPECT_NEAR(where.y, 5.0f, 1e-4);
}

// ---------------------------------------------------------------------------
// Vector::extendFrom
// ---------------------------------------------------------------------------

TEST(Vector, ExtendFrom) {
	Vector<> v(5.0, 3.0);
	Point origin(1.0f, 2.0f);
	Line l = v.extendFrom(origin);
	EXPECT_FLOAT_EQ(l.getStart().x, 1.0f);
	EXPECT_FLOAT_EQ(l.getStart().y, 2.0f);
	EXPECT_FLOAT_EQ(l.getEnd().x, 6.0f);
	EXPECT_FLOAT_EQ(l.getEnd().y, 5.0f);
}

TEST(Vector, ExtendFromNegativeDirection) {
	// Negative x direction: constructor will swap start/end
	Vector<> v(-5.0, 0.0);
	Point origin(10.0f, 3.0f);
	Line l = v.extendFrom(origin);
	// After swap, start.x <= end.x
	EXPECT_FLOAT_EQ(l.getStart().x, 5.0f);
	EXPECT_FLOAT_EQ(l.getEnd().x, 10.0f);
}

// ---------------------------------------------------------------------------
// Vector::extendIntersect
// ---------------------------------------------------------------------------

TEST(Vector, ExtendIntersectHitsBarrier) {
	Vector<> velocity(10.0, 0.0); // moving right
	Point start(0.0f, 5.0f);
	Line barrier(Point(5.0f, 0.0f), Point(5.0f, 10.0f)); // vertical wall at x=5
	Vector<> residual;
	EXPECT_TRUE(velocity.extendIntersect(start, barrier, residual));
	// Residual should be the remaining vector after the intersection
	EXPECT_NEAR(residual.x, 5.0, 1e-4);
	EXPECT_NEAR(residual.y, 0.0, 1e-4);
}

TEST(Vector, ExtendIntersectMissesBarrier) {
	Vector<> velocity(10.0, 0.0); // moving right
	Point start(0.0f, 5.0f);
	// Barrier above the path
	Line barrier(Point(3.0f, 10.0f), Point(7.0f, 10.0f));
	Vector<> residual;
	EXPECT_FALSE(velocity.extendIntersect(start, barrier, residual));
}
