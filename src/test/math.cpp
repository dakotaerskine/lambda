#include <cmath>

#include <gtest/gtest.h>

#include "math/complex.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/transform.h"
#include "math/vector.h"
#include "test/test.h"
#include "test/tolerances.h"

namespace lambda {
    namespace {
        TEST(RandomTest, SameSeedAndStreamRepeat) {
            Random a(7, 3);
            Random b(7, 3);

            for (int i = 0; i < 100; i++) EXPECT_EQ(a.next(), b.next());
        }

        TEST(RandomTest, DifferentStreamsDiffer) {
            Random a(7, 3);
            Random b(7, 4);

            int equal = 0;

            for (int i = 0; i < 100; i++)
                if (a.next() == b.next()) equal++;

            EXPECT_LT(equal, 5);
        }

        TEST(RandomTest, DifferentSeedsDiffer) {
            Random a(7, 3);
            Random b(8, 3);

            int equal = 0;

            for (int i = 0; i < 100; i++)
                if (a.next() == b.next()) equal++;

            EXPECT_LT(equal, 5);
        }

        TEST(RandomTest, SamplesStayInUnitInterval) {
            Random state(1, 0);

            for (int i = 0; i < 10000; i++) {
                Float x = state.next();

                ASSERT_GE(x, 0);
                ASSERT_LT(x, 1);
            }
        }

        TEST(RandomTest, SamplesAreUniform) {
            Random state(1, 0);

            Float sum = 0;
            Float sumSquared = 0;

            for (int i = 0; i < 20000; i++) {
                Float x = state.next();

                sum += x;
                sumSquared += x * x;
            }

            Float mean = sum / 20000;

            EXPECT_NEAR(mean, Float(0.5), 5 * std::sqrt(Float(1) / 12 / 20000));
            EXPECT_NEAR(sumSquared / 20000 - mean * mean, Float(1) / 12, 5 * std::sqrt((Float(1) / 80 - Float(1) / 144) / 20000));
        }

        TEST(ComplexTest, DefaultsToZero) {
            Complex c;

            EXPECT_EQ(c.real(), 0);
            EXPECT_EQ(c.imag(), 0);
        }

        TEST(ComplexTest, RealConstructionHasNoImaginaryPart) {
            Complex c(Float(2.5));

            EXPECT_EQ(c.real(), Float(2.5));
            EXPECT_EQ(c.imag(), 0);
        }

        TEST(ComplexTest, Equality) {
            EXPECT_TRUE(Complex(1, 2) == Complex(1, 2));
            EXPECT_FALSE(Complex(1, 2) == Complex(1, 3));
            EXPECT_FALSE(Complex(1, 2) == Complex(2, 2));

            EXPECT_TRUE(Complex(1, 2) != Complex(1, 3));
            EXPECT_TRUE(Complex(1, 2) != Complex(2, 2));
            EXPECT_FALSE(Complex(1, 2) != Complex(1, 2));
        }

        TEST(ComplexTest, AdditionSubtractionAndNegation) {
            EXPECT_EQ(Complex(1, 2) + Complex(3, 4), Complex(4, 6));
            EXPECT_EQ(Complex(3, 4) - Complex(1, 2), Complex(2, 2));
            EXPECT_EQ(-Complex(1, -2), Complex(-1, 2));
        }

        TEST(ComplexTest, Multiplication) {
            Complex c = Complex(1, 2) * Complex(3, 4);

            EXPECT_NEAR(c.real(), -5, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 10, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, Division) {
            Complex c = Complex(-5, 10) / Complex(3, 4);

            EXPECT_NEAR(c.real(), 1, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 2, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, DivisionInvertsMultiplication) {
            Complex a(Float(0.3), Float(-1.7));
            Complex b(Float(2.1), Float(0.4));

            Complex c = a * b / b;

            EXPECT_NEAR(c.real(), a.real(), constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), a.imag(), constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, CompoundAssignment) {
            Complex c(1, 2);

            c += Complex(3, 4);

            EXPECT_EQ(c, Complex(4, 6));

            c -= Complex(1, 1);

            EXPECT_EQ(c, Complex(3, 5));

            c *= Complex(0, 1);

            EXPECT_EQ(c, Complex(-5, 3));

            c /= Complex(0, 1);

            EXPECT_NEAR(c.real(), 3, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 5, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, Magnitude) {
            EXPECT_NEAR(abs(Complex(3, 4)), 5, constants::TEST_TOLERANCE);
            EXPECT_NEAR(abs(Complex(0, -2)), 2, constants::TEST_TOLERANCE);
            EXPECT_EQ(abs(Complex(0, 0)), 0);
        }

        TEST(ComplexTest, ExponentialOfImaginaryIsOnUnitCircle) {
            Complex c = exp(Complex(0, constants::PI));

            EXPECT_NEAR(c.real(), -1, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 0, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, ExponentialOfRealMatchesScalar) {
            Complex c = exp(Complex(Float(1.5), 0));

            EXPECT_NEAR(c.real(), std::exp(Float(1.5)), constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 0, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, SquareRootOfNegativeReal) {
            Complex c = sqrt(Complex(-1, 0));

            EXPECT_NEAR(c.real(), 0, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 1, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, SquareRootOfPositiveReal) {
            Complex c = sqrt(Complex(4, 0));

            EXPECT_NEAR(c.real(), 2, constants::TEST_TOLERANCE);
            EXPECT_NEAR(c.imag(), 0, constants::TEST_TOLERANCE);
        }

        TEST(ComplexTest, SquareRootOfZero) {
            EXPECT_EQ(sqrt(Complex(0, 0)), Complex(0, 0));
        }

        TEST(ComplexTest, SquareRootUsesPrincipalBranch) {
            Complex values[4] = { Complex(3, 4), Complex(-3, 4), Complex(-3, -4), Complex(3, -4) };
            Complex expected[4] = { Complex(2, 1), Complex(1, 2), Complex(1, -2), Complex(2, -1) };

            for (int i = 0; i < 4; i++) {
                Complex c = sqrt(values[i]);

                EXPECT_NEAR(c.real(), expected[i].real(), constants::TEST_TOLERANCE);
                EXPECT_NEAR(c.imag(), expected[i].imag(), constants::TEST_TOLERANCE);
            }
        }

        TEST(ComplexTest, SquareRootSquaresBack) {
            for (Float re = -2; re <= 2; re += Float(0.7))
                for (Float im = -2; im <= 2; im += Float(0.9)) {
                    Complex c = sqrt(Complex(re, im));
                    Complex squared = c * c;

                    EXPECT_NEAR(squared.real(), re, tolerances::ACCUMULATED);
                    EXPECT_NEAR(squared.imag(), im, tolerances::ACCUMULATED);
                }
        }

        TEST(ComplexTest, ComponentwiseMinimumAndMaximum) {
            EXPECT_EQ(fmin(Complex(1, 5), Complex(3, 2)), Complex(1, 2));
            EXPECT_EQ(fmax(Complex(1, 5), Complex(3, 2)), Complex(3, 5));
        }

        TEST(RandomTest, MatchesPCG32Reference) {
            Random state(42, 54);

            EXPECT_EQ(state.next(), Float(0xa15c02) / Float(1 << 24));
            EXPECT_EQ(state.next(), Float(0x7b47f4) / Float(1 << 24));
            EXPECT_EQ(state.next(), Float(0xba1d33) / Float(1 << 24));
        }

        class VectorTest : public GeometryTest {};

        TEST_F(VectorTest, DefaultsToZero) {
            Vector<Float, 3> v;

            for (int i = 0; i < 3; i++) EXPECT_EQ(v[i], 0);
        }

        TEST_F(VectorTest, ScalarBroadcasts) {
            Vector<Float, 4> v(Float(2.5));

            for (int i = 0; i < 4; i++) EXPECT_EQ(v[i], Float(2.5));
        }

        TEST_F(VectorTest, ElementwiseConstruction) {
            Vector<Float, 3> v(1, 2, 3);

            EXPECT_EQ(v[0], 1);
            EXPECT_EQ(v[1], 2);
            EXPECT_EQ(v[2], 3);
        }

        TEST_F(VectorTest, TruncatesLargerVector) {
            Vector<Float, 3> v(Vector<Float, 4>(1, 2, 3, 4));

            expectEqual(v, Vector<Float, 3>(1, 2, 3));
        }

        TEST_F(VectorTest, ExtendsSmallerVector) {
            expectEqual(Vector<Float, 4>(Vector<Float, 3>(1, 2, 3), 4), Vector<Float, 4>(1, 2, 3, 4));
            expectEqual(Vector<Float, 4>(Vector<Float, 2>(1, 2), 3, 4), Vector<Float, 4>(1, 2, 3, 4));
        }

        TEST_F(VectorTest, ConvertsElementType) {
            Vector<Float, 3> v(Vector<int, 3>(1, 2, 3));

            expectEqual(v, Vector<Float, 3>(1, 2, 3));
        }

        TEST_F(VectorTest, ConstructsFromPointer) {
            Float values[3] = { 4, 5, 6 };

            Vector<Float, 3> v(values);

            expectEqual(v, Vector<Float, 3>(4, 5, 6));
        }

        TEST_F(VectorTest, IndexingIsMutable) {
            Vector<Float, 3> v(1, 2, 3);

            v[1] = 7;

            expectEqual(v, Vector<Float, 3>(1, 7, 3));
        }

        TEST_F(VectorTest, DataExposesContiguousElements) {
            Vector<Float, 3> v(1, 2, 3);

            EXPECT_EQ(v.data(), &v[0]);
            EXPECT_EQ(v.data()[2], 3);
        }

        TEST_F(VectorTest, EqualityAndInequality) {
            Vector<Float, 3> v(1, 2, 3);
            Vector<Float, 3> same(1, 2, 3);
            Vector<Float, 3> different(0, 2, 3);

            EXPECT_TRUE(v == same);
            EXPECT_FALSE(v == different);
            EXPECT_TRUE(v != different);
            EXPECT_FALSE(v != same);
        }

        TEST_F(VectorTest, Arithmetic) {
            Vector<Float, 3> a(1, 2, 3);
            Vector<Float, 3> b(4, 5, 6);

            expectEqual(a + b, Vector<Float, 3>(5, 7, 9));
            expectEqual(b - a, Vector<Float, 3>(3, 3, 3));
            expectEqual(-a, Vector<Float, 3>(-1, -2, -3));
            expectEqual(a * Float(2), Vector<Float, 3>(2, 4, 6));
            expectEqual(Float(2) * a, Vector<Float, 3>(2, 4, 6));
            expectEqual(a * b, Vector<Float, 3>(4, 10, 18));
            expectEqual(b / Float(2), Vector<Float, 3>(2, Float(2.5), 3));
        }

        TEST_F(VectorTest, CompoundAssignment) {
            Vector<Float, 3> v(1, 2, 3);

            v += Vector<Float, 3>(1, 1, 1);

            expectEqual(v, Vector<Float, 3>(2, 3, 4));

            v -= Vector<Float, 3>(0, 1, 2);

            expectEqual(v, Vector<Float, 3>(2, 2, 2));

            v *= Float(3);

            expectEqual(v, Vector<Float, 3>(6, 6, 6));

            v *= Vector<Float, 3>(1, 2, 3);

            expectEqual(v, Vector<Float, 3>(6, 12, 18));

            v /= Float(6);

            expectEqual(v, Vector<Float, 3>(1, 2, 3));
        }

        TEST_F(VectorTest, LengthAndNormalize) {
            Vector<Float, 3> v(2, 3, 6);

            EXPECT_EQ(v.lengthSquared(), 49);
            EXPECT_EQ(v.length(), 7);

            Vector<Float, 3> n = normalize(v);

            EXPECT_NEAR(n.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(n[0], Float(2) / 7, tolerances::STANDARD);
        }

        TEST_F(VectorTest, DotProduct) {
            Vector<Float, 3> a(1, 2, 3);
            Vector<Float, 3> b(4, -5, 6);
            Vector<Float, 3> x(1, 0, 0);
            Vector<Float, 3> y(0, 1, 0);

            EXPECT_EQ(dot(a, b), 12);
            EXPECT_EQ(dot(x, y), 0);
        }

        TEST_F(VectorTest, CrossProductOfAxes) {
            expectEqual(cross(Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0)), Vector<Float, 3>(0, 0, 1));
            expectEqual(cross(Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(0, 0, 1)), Vector<Float, 3>(1, 0, 0));
            expectEqual(cross(Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(1, 0, 0)), Vector<Float, 3>(0, 1, 0));
        }

        TEST_F(VectorTest, CrossProductIsAntiCommutativeAndOrthogonal) {
            Vector<Float, 3> a(1, 2, 3);
            Vector<Float, 3> b(-2, 0, 5);

            Vector<Float, 3> c = cross(a, b);

            EXPECT_EQ(c, -cross(b, a));
            EXPECT_EQ(dot(c, a), 0);
            EXPECT_EQ(dot(c, b), 0);
        }

        TEST_F(VectorTest, SevenDimensionalCrossProductIsAntiCommutativeAndOrthogonal) {
            Vector<Float, 7> a(1, 2, 3, 4, 5, 6, 7);
            Vector<Float, 7> b(7, -1, 2, 0, 3, -4, 1);

            Vector<Float, 7> c = cross(a, b);

            for (int i = 0; i < 7; i++) EXPECT_EQ(c[i], -cross(b, a)[i]);

            EXPECT_EQ(dot(c, a), 0);
            EXPECT_EQ(dot(c, b), 0);
        }

        TEST_F(VectorTest, SevenDimensionalCrossProductSatisfiesLagrangeIdentity) {
            Vector<Float, 7> a(1, 2, 3, 4, 5, 6, 7);
            Vector<Float, 7> b(7, -1, 2, 0, 3, -4, 1);

            Float d = dot(a, b);

            EXPECT_EQ(cross(a, b).lengthSquared(), a.lengthSquared() * b.lengthSquared() - d * d);
        }

        TEST_F(VectorTest, ComponentwiseMinimumAndMaximum) {
            Vector<Float, 3> a(1, 5, 3);
            Vector<Float, 3> b(2, 4, 3);

            expectEqual(min(a, b), Vector<Float, 3>(1, 4, 3));
            expectEqual(max(a, b), Vector<Float, 3>(2, 5, 3));
        }

        TEST_F(VectorTest, ComplexLengthSquared) {
            Vector<Complex, 2> v(Complex(1, 2), Complex(3, 4));

            EXPECT_EQ(v.lengthSquared(), Complex(-10, 28));
        }

        class MatrixTest : public GeometryTest {
            protected:
                template <int N>
                static void expectIdentity(const Matrix<Float, N> & m, Float tolerance = tolerances::STANDARD) {
                    for (int i = 0; i < N; i++)
                        for (int j = 0; j < N; j++) EXPECT_NEAR(m(i, j), i == j ? 1 : 0, tolerance);
                }

                template <int N>
                static Matrix<Float, N> makeIdentityPlusOnes() {
                    Matrix<Float, N> m;

                    for (int i = 0; i < N; i++)
                        for (int j = 0; j < N; j++) m(i, j) += 1;

                    return m;
                }
        };

        TEST_F(MatrixTest, DefaultsToIdentity) {
            expectIdentity(Matrix<Float, 4>());
        }

        TEST_F(MatrixTest, ScalarConstructsDiagonal) {
            Matrix<Float, 3> m(Float(2.5));

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) EXPECT_EQ(m(i, j), i == j ? Float(2.5) : 0);
        }

        TEST_F(MatrixTest, ElementwiseConstructionIsRowMajor) {
            Matrix<Float, 2> m(1, 2, 3, 4);

            EXPECT_EQ(m(0, 0), 1);
            EXPECT_EQ(m(0, 1), 2);
            EXPECT_EQ(m(1, 0), 3);
            EXPECT_EQ(m(1, 1), 4);
        }

        TEST_F(MatrixTest, TruncatesLargerMatrix) {
            Matrix<Float, 3> m(Matrix<Float, 4>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));

            expectEqual(m, Matrix<Float, 3>(1, 2, 3, 5, 6, 7, 9, 10, 11));
        }

        TEST_F(MatrixTest, ConstructsFromArray) {
            Float values[2][2] = { { 1, 2 }, { 3, 4 } };

            expectEqual(Matrix<Float, 2>(values), Matrix<Float, 2>(1, 2, 3, 4));
        }

        TEST_F(MatrixTest, ConvertsElementType) {
            expectEqual(Matrix<Float, 2>(Matrix<int, 2>(1, 2, 3, 4)), Matrix<Float, 2>(1, 2, 3, 4));
        }

        TEST_F(MatrixTest, DataIsRowMajor) {
            Matrix<Float, 2> m(1, 2, 3, 4);

            EXPECT_EQ(m.data(), &m(0, 0));
            EXPECT_EQ(m.data()[1], 2);
            EXPECT_EQ(m.data()[2], 3);
        }

        TEST_F(MatrixTest, EqualityAndInequality) {
            Matrix<Float, 2> m(1, 2, 3, 4);
            Matrix<Float, 2> same(1, 2, 3, 4);
            Matrix<Float, 2> different(0, 2, 3, 4);

            EXPECT_TRUE(m == same);
            EXPECT_FALSE(m == different);
            EXPECT_TRUE(m != different);
            EXPECT_FALSE(m != same);
        }

        TEST_F(MatrixTest, AdditionAndSubtraction) {
            Matrix<Float, 2> a(1, 2, 3, 4);
            Matrix<Float, 2> b(4, 3, 2, 1);

            expectEqual(a + b, Matrix<Float, 2>(5, 5, 5, 5));
            expectEqual(a - b, Matrix<Float, 2>(-3, -1, 1, 3));
        }

        TEST_F(MatrixTest, ScalarMultiplicationAndDivision) {
            Matrix<Float, 2> m(1, 2, 3, 4);

            expectEqual(m * Float(2), Matrix<Float, 2>(2, 4, 6, 8));
            expectEqual(Float(2) * m, Matrix<Float, 2>(2, 4, 6, 8));
            expectEqual(m / Float(2), Matrix<Float, 2>(Float(0.5), 1, Float(1.5), 2));
        }

        TEST_F(MatrixTest, CompoundAssignment) {
            Matrix<Float, 2> m(1, 2, 3, 4);

            m += Matrix<Float, 2>(1, 1, 1, 1);

            expectEqual(m, Matrix<Float, 2>(2, 3, 4, 5));

            m -= Matrix<Float, 2>(1, 1, 1, 1);

            expectEqual(m, Matrix<Float, 2>(1, 2, 3, 4));

            m *= Float(2);

            expectEqual(m, Matrix<Float, 2>(2, 4, 6, 8));

            m /= Float(2);

            expectEqual(m, Matrix<Float, 2>(1, 2, 3, 4));

            m *= Matrix<Float, 2>(0, 1, 1, 0);

            expectEqual(m, Matrix<Float, 2>(2, 1, 4, 3));
        }

        TEST_F(MatrixTest, MultiplicationByIdentity) {
            Matrix<Float, 4> m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

            expectEqual(m * Matrix<Float, 4>(), m);
            expectEqual(Matrix<Float, 4>() * m, m);
        }

        TEST_F(MatrixTest, MultiplicationOneByOne) {
            expectEqual(Matrix<Float, 1>(3) * Matrix<Float, 1>(4), Matrix<Float, 1>(12));
        }

        TEST_F(MatrixTest, MultiplicationTwoByTwo) {
            expectEqual(Matrix<Float, 2>(1, 2, 3, 4) * Matrix<Float, 2>(5, 6, 7, 8), Matrix<Float, 2>(19, 22, 43, 50));
        }

        TEST_F(MatrixTest, MultiplicationThreeByThree) {
            expectEqual(Matrix<Float, 3>(1, 2, 3, 4, 5, 6, 7, 8, 9) * Matrix<Float, 3>(9, 8, 7, 6, 5, 4, 3, 2, 1), Matrix<Float, 3>(30, 24, 18, 84, 69, 54, 138, 114, 90));
        }

        TEST_F(MatrixTest, MultiplicationFourByFour) {
            Matrix<Float, 4> a(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
            Matrix<Float, 4> b(1, 0, 0, 2, 0, 1, 3, 0, 0, 4, 1, 0, 5, 0, 0, 1);

            expectEqual(a * b, Matrix<Float, 4>(21, 14, 9, 6, 45, 34, 25, 18, 69, 54, 41, 30, 93, 74, 57, 42));
        }

        TEST_F(MatrixTest, MultiplicationFiveByFiveUsesGeneralProduct) {
            Matrix<Float, 5> a;
            Matrix<Float, 5> b;

            for (int i = 0; i < 5; i++)
                for (int j = 0; j < 5; j++) {
                    a(i, j) = Float(i + j);
                    b(i, j) = Float(i == j ? 2 : 1);
                }

            Matrix<Float, 5> c = a * b;

            for (int i = 0; i < 5; i++)
                for (int j = 0; j < 5; j++) {
                    Float expected = 0;

                    for (int k = 0; k < 5; k++) expected += a(i, k) * b(k, j);

                    EXPECT_EQ(c(i, j), expected);
                }
        }

        TEST_F(MatrixTest, MultiplicationIsNotCommutative) {
            Matrix<Float, 2> a(1, 2, 3, 4);
            Matrix<Float, 2> b(0, 1, 1, 0);

            EXPECT_NE(a * b, b * a);
        }

        TEST_F(MatrixTest, MultipliesColumnVector) {
            expectEqual(Matrix<Float, 3>(1, 2, 3, 4, 5, 6, 7, 8, 9) * Vector<Float, 3>(1, 0, -1), Vector<Float, 3>(-2, -2, -2));
        }

        TEST_F(MatrixTest, MultipliesRowVector) {
            expectEqual(Vector<Float, 3>(1, 0, -1) * Matrix<Float, 3>(1, 2, 3, 4, 5, 6, 7, 8, 9), Vector<Float, 3>(-6, -6, -6));
        }

        TEST_F(MatrixTest, Transpose) {
            expectEqual(transpose(Matrix<Float, 3>(1, 2, 3, 4, 5, 6, 7, 8, 9)), Matrix<Float, 3>(1, 4, 7, 2, 5, 8, 3, 6, 9));
        }

        TEST_F(MatrixTest, TransposeIsInvolution) {
            Matrix<Float, 4> m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

            EXPECT_EQ(transpose(transpose(m)), m);
        }

        TEST_F(MatrixTest, DeterminantOneByOne) {
            Matrix<Float, 1> m(5);

            EXPECT_EQ(m.determinant(), 5);
        }

        TEST_F(MatrixTest, DeterminantTwoByTwo) {
            Matrix<Float, 2> m(1, 2, 3, 4);

            EXPECT_EQ(m.determinant(), -2);
        }

        TEST_F(MatrixTest, DeterminantThreeByThree) {
            Matrix<Float, 3> m(2, 0, 1, 1, 3, 2, 1, 1, 4);

            EXPECT_EQ(m.determinant(), 18);
        }

        TEST_F(MatrixTest, DeterminantFourByFour) {
            Matrix<Float, 4> triangular(1, 2, 3, 4, 0, 5, 6, 7, 0, 0, 8, 9, 0, 0, 0, 10);
            Matrix<Float, 4> swapped(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

            EXPECT_EQ(triangular.determinant(), 400);
            EXPECT_EQ(swapped.determinant(), -1);
        }

        TEST_F(MatrixTest, DeterminantOfIdentityPlusOnes) {
            EXPECT_NEAR(makeIdentityPlusOnes<5>().determinant(), 6, tolerances::ACCUMULATED);
            EXPECT_NEAR(makeIdentityPlusOnes<6>().determinant(), 7, tolerances::ACCUMULATED);
        }

        TEST_F(MatrixTest, DeterminantTracksRowSwapSign) {
            Matrix<Float, 5> m;

            m(0, 0) = 0;
            m(0, 1) = 1;
            m(1, 0) = 1;
            m(1, 1) = 0;

            EXPECT_NEAR(m.determinant(), -1, tolerances::STANDARD);
        }

        TEST_F(MatrixTest, DeterminantOfSingularLargeMatrixIsZero) {
            Matrix<Float, 5> m;

            for (int i = 0; i < 5; i++)
                for (int j = 0; j < 5; j++) m(i, j) = 1;

            EXPECT_EQ(m.determinant(), 0);
        }

        TEST_F(MatrixTest, DeterminantOfProductIsProductOfDeterminants) {
            Matrix<Float, 3> a(2, 0, 1, 1, 3, 2, 1, 1, 4);
            Matrix<Float, 3> b(1, 2, 0, 0, 1, 3, 2, 0, 1);

            EXPECT_NEAR((a * b).determinant(), a.determinant() * b.determinant(), tolerances::COARSE);
        }

        TEST_F(MatrixTest, InverseOneByOne) {
            expectEqual(inverse(Matrix<Float, 1>(4)), Matrix<Float, 1>(Float(0.25)));
        }

        TEST_F(MatrixTest, InverseTwoByTwo) {
            Matrix<Float, 2> m(4, 7, 2, 6);

            Matrix<Float, 2> inv = inverse(m);

            EXPECT_NEAR(inv(0, 0), Float(0.6), tolerances::STANDARD);
            EXPECT_NEAR(inv(0, 1), Float(-0.7), tolerances::STANDARD);
            EXPECT_NEAR(inv(1, 0), Float(-0.2), tolerances::STANDARD);
            EXPECT_NEAR(inv(1, 1), Float(0.4), tolerances::STANDARD);

            expectIdentity(m * inv);
        }

        TEST_F(MatrixTest, InverseThreeByThree) {
            Matrix<Float, 3> m(2, 0, 1, 1, 3, 2, 1, 1, 4);

            expectIdentity(m * inverse(m));
            expectIdentity(inverse(m) * m);
        }

        TEST_F(MatrixTest, InverseFourByFour) {
            Matrix<Float, 4> m(1, 2, 3, 4, 5, 1, 6, 7, 8, 9, 1, 2, 3, 4, 5, 1);

            expectIdentity(m * inverse(m));
            expectIdentity(inverse(m) * m);
        }

        TEST_F(MatrixTest, InverseLargeMatrix) {
            expectIdentity(makeIdentityPlusOnes<5>() * inverse(makeIdentityPlusOnes<5>()), Float(1e-4));
            expectIdentity(makeIdentityPlusOnes<6>() * inverse(makeIdentityPlusOnes<6>()), Float(1e-4));
        }

        TEST_F(MatrixTest, InverseLargeMatrixPivots) {
            Matrix<Float, 5> m;

            m(0, 0) = 0;
            m(0, 1) = 2;
            m(1, 0) = 3;
            m(1, 1) = 0;

            Matrix<Float, 5> inv = inverse(m);

            EXPECT_NEAR(inv(0, 1), Float(1) / 3, tolerances::STANDARD);
            EXPECT_NEAR(inv(1, 0), Float(0.5), tolerances::STANDARD);

            expectIdentity(m * inv);
        }

        TEST_F(MatrixTest, InverseIsInvolution) {
            Matrix<Float, 3> m(2, 0, 1, 1, 3, 2, 1, 1, 4);

            Matrix<Float, 3> back = inverse(inverse(m));

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) EXPECT_NEAR(back(i, j), m(i, j), tolerances::ACCUMULATED);
        }

        TEST_F(MatrixTest, InverseOfSingularMatrixIsIdentity) {
            expectEqual(inverse(Matrix<Float, 1>(0)), Matrix<Float, 1>());
            expectEqual(inverse(Matrix<Float, 2>(1, 2, 2, 4)), Matrix<Float, 2>());
            expectEqual(inverse(Matrix<Float, 3>(1, 2, 3, 2, 4, 6, 0, 1, 1)), Matrix<Float, 3>());
            expectEqual(inverse(Matrix<Float, 4>(1, 2, 3, 4, 1, 2, 3, 4, 0, 1, 0, 1, 2, 0, 2, 0)), Matrix<Float, 4>());
            expectEqual(inverse(Matrix<Float, 5>(0)), Matrix<Float, 5>());
        }

        TEST_F(MatrixTest, ComplexMatrixMultiplication) {
            Matrix<Complex, 2> a(Complex(1, 1), Complex(0, 0), Complex(0, 0), Complex(1, -1));
            Matrix<Complex, 2> b(Complex(1, -1), Complex(2, 0), Complex(0, 0), Complex(1, 1));

            Matrix<Complex, 2> c = a * b;

            EXPECT_EQ(c(0, 0), Complex(2, 0));
            EXPECT_EQ(c(0, 1), Complex(2, 2));
            EXPECT_EQ(c(1, 0), Complex(0, 0));
            EXPECT_EQ(c(1, 1), Complex(2, 0));
        }

        TEST_F(MatrixTest, ComplexMatrixScalarOperations) {
            Matrix<Complex, 2> m(Complex(1, 1), Complex(0, 0), Complex(0, 0), Complex(1, 1));

            m *= Complex(0, 1);

            EXPECT_EQ(m(0, 0), Complex(-1, 1));

            m /= Complex(0, 1);

            EXPECT_NEAR(m(1, 1).real(), 1, tolerances::STANDARD);
            EXPECT_NEAR(m(1, 1).imag(), 1, tolerances::STANDARD);
        }

        TEST_F(MatrixTest, ComplexDeterminant) {
            Matrix<Complex, 2> m(Complex(1, 1), Complex(0, 0), Complex(0, 0), Complex(1, -1));

            EXPECT_EQ(m.determinant(), Complex(2, 0));
        }

        class RayTest : public GeometryTest {};

        TEST_F(RayTest, DefaultPointsUpWithoutMedium) {
            Ray r;

            expectEqual(r.getOrigin(), Vector<Float, 3>(0, 0, 0));
            expectEqual(r.getDirection(), Vector<Float, 3>(0, 1, 0));
            EXPECT_EQ(r.getMedium(), nullptr);
        }

        TEST_F(RayTest, StoresConstructorArguments) {
            Medium medium;

            Ray r(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(0, 0, 1), SampledSpectrum<Float>(550), &medium);

            expectEqual(r.getOrigin(), Vector<Float, 3>(1, 2, 3));
            expectEqual(r.getDirection(), Vector<Float, 3>(0, 0, 1));
            EXPECT_EQ(r.getLambdas(), SampledSpectrum<Float>(550));
            EXPECT_EQ(r.getMedium(), &medium);
        }

        TEST_F(RayTest, AtAdvancesAlongDirection) {
            Ray r(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(0, 0, 2), SampledSpectrum<Float>(550), nullptr);

            expectEqual(r.at(0), Vector<Float, 3>(1, 2, 3));
            expectEqual(r.at(2), Vector<Float, 3>(1, 2, 7));
            expectEqual(r.at(-1), Vector<Float, 3>(1, 2, 1));
        }

        class TransformTest : public GeometryTest {};

        TEST_F(TransformTest, DefaultIsIdentity) {
            Transform transform;

            Vector<Float, 3> p(1, 2, 3);

            EXPECT_EQ(transform.pointToWorld(p), p);
            EXPECT_EQ(transform.pointToLocal(p), p);
            EXPECT_EQ(transform.vectorToWorld(p), p);
            EXPECT_EQ(transform.vectorToLocal(p), p);
        }

        TEST_F(TransformTest, TranslationMovesPointsButNotVectors) {
            Transform transform(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1));

            expectEqual(transform.pointToWorld(Vector<Float, 3>(1, 1, 1)), Vector<Float, 3>(2, 3, 4));
            expectEqual(transform.vectorToWorld(Vector<Float, 3>(1, 1, 1)), Vector<Float, 3>(1, 1, 1));
            expectEqual(transform.pointToLocal(Vector<Float, 3>(2, 3, 4)), Vector<Float, 3>(1, 1, 1));
        }

        TEST_F(TransformTest, ScaleStretchesAxes) {
            Transform transform(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(2, 3, 4));

            expectEqual(transform.pointToWorld(Vector<Float, 3>(1, 1, 1)), Vector<Float, 3>(2, 3, 4));

            Vector<Float, 3> local = transform.pointToLocal(Vector<Float, 3>(2, 3, 4));

            for (int i = 0; i < 3; i++) EXPECT_NEAR(local[i], 1, tolerances::STANDARD);
        }

        TEST_F(TransformTest, ScaleAccessors) {
            Transform transform(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(Float(0.3), Float(0.4), Float(0.5)), Vector<Float, 3>(2, 3, 4));

            for (int i = 0; i < 3; i++) {
                EXPECT_NEAR(transform.scale(i), Float(i + 2), tolerances::STANDARD);
                EXPECT_NEAR(transform.scaleSquared(i), Float((i + 2) * (i + 2)), tolerances::ACCUMULATED);
                EXPECT_NEAR(transform.scale()[i], transform.scale(i), tolerances::STANDARD);
                EXPECT_NEAR(transform.scaleSquared()[i], transform.scaleSquared(i), tolerances::ACCUMULATED);
            }
        }

        TEST_F(TransformTest, RotationAboutEachAxis) {
            Transform x(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(constants::PI / 2, 0, 0), Vector<Float, 3>(1, 1, 1));
            Transform y(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, constants::PI / 2, 0), Vector<Float, 3>(1, 1, 1));
            Transform z(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 0, constants::PI / 2), Vector<Float, 3>(1, 1, 1));

            Vector<Float, 3> rx = x.vectorToWorld(Vector<Float, 3>(0, 1, 0));
            Vector<Float, 3> ry = y.vectorToWorld(Vector<Float, 3>(1, 0, 0));
            Vector<Float, 3> rz = z.vectorToWorld(Vector<Float, 3>(1, 0, 0));

            EXPECT_NEAR(rx[2], 1, tolerances::STANDARD);
            EXPECT_NEAR(ry[2], -1, tolerances::STANDARD);
            EXPECT_NEAR(rz[1], 1, tolerances::STANDARD);

            EXPECT_NEAR(rx.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(ry.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(rz.length(), 1, tolerances::STANDARD);
        }

        TEST_F(TransformTest, RotationAppliesXThenYThenZ) {
            Transform transform(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(constants::PI / 2, constants::PI / 2, 0), Vector<Float, 3>(1, 1, 1));

            Vector<Float, 3> v = transform.vectorToWorld(Vector<Float, 3>(0, 1, 0));

            EXPECT_NEAR(v[0], 1, tolerances::STANDARD);
            EXPECT_NEAR(v[1], 0, tolerances::STANDARD);
            EXPECT_NEAR(v[2], 0, tolerances::STANDARD);
        }

        TEST_F(TransformTest, ComposesScaleThenRotationThenTranslation) {
            Transform transform(Vector<Float, 3>(1, 1, 1), Vector<Float, 3>(0, 0, constants::PI / 2), Vector<Float, 3>(2, 1, 1));

            Vector<Float, 3> p = transform.pointToWorld(Vector<Float, 3>(1, 0, 0));

            EXPECT_NEAR(p[0], 1, tolerances::STANDARD);
            EXPECT_NEAR(p[1], 3, tolerances::STANDARD);
            EXPECT_NEAR(p[2], 1, tolerances::STANDARD);
        }

        TEST_F(TransformTest, LocalIsInverseOfWorld) {
            Transform transform(Vector<Float, 3>(1, -2, 3), Vector<Float, 3>(Float(0.3), Float(-0.8), Float(1.1)), Vector<Float, 3>(2, Float(0.5), 3));

            Vector<Float, 3> p(Float(0.7), Float(-1.3), Float(2.9));

            Vector<Float, 3> point = transform.pointToLocal(transform.pointToWorld(p));
            Vector<Float, 3> vector = transform.vectorToLocal(transform.vectorToWorld(p));

            for (int i = 0; i < 3; i++) {
                EXPECT_NEAR(point[i], p[i], tolerances::ACCUMULATED);
                EXPECT_NEAR(vector[i], p[i], tolerances::ACCUMULATED);
            }
        }

        TEST_F(TransformTest, NormalsStayPerpendicularUnderNonUniformScale) {
            Transform transform(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(Float(0.3), Float(0.4), Float(0.5)), Vector<Float, 3>(2, Float(0.5), 3));

            Vector<Float, 3> normal = normalize(Vector<Float, 3>(1, 1, 0));
            Vector<Float, 3> tangent = Vector<Float, 3>(1, -1, 5);

            ASSERT_EQ(dot(normal, tangent), 0);

            Vector<Float, 3> worldNormal = transform.normalToWorld(normal);

            EXPECT_NEAR(worldNormal.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(dot(worldNormal, transform.vectorToWorld(tangent)), 0, tolerances::ACCUMULATED);
        }

        TEST_F(TransformTest, RayToLocalTransformsGeometryAndKeepsPayload) {
            Medium medium;

            Transform transform(Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(0, 0, constants::PI / 2), Vector<Float, 3>(2, 2, 2));

            Ray world(Vector<Float, 3>(3, 4, 5), Vector<Float, 3>(0, 1, 0), SampledSpectrum<Float>(550), &medium);

            Ray local = transform.rayToLocal(world);

            EXPECT_EQ(local.getLambdas(), world.getLambdas());
            EXPECT_EQ(local.getMedium(), &medium);

            for (Float t = 0; t < 3; t += 1) {
                Vector<Float, 3> expected = transform.pointToLocal(world.at(t));

                for (int i = 0; i < 3; i++) EXPECT_NEAR(local.at(t)[i], expected[i], tolerances::ACCUMULATED);
            }
        }

        TEST(SpectrumTest, LinearInterpolatesBetweenSamples) {
            Allocator allocator;

            Float lambdas[3] = { 400, 500, 700 };
            Float values[3] = { 1, 3, 2 };

            LinearSpectrum<Float> spectrum(3, lambdas, values);

            EXPECT_NEAR(spectrum[400], 1, tolerances::STANDARD);
            EXPECT_NEAR(spectrum[450], 2, tolerances::STANDARD);
            EXPECT_NEAR(spectrum[500], 3, tolerances::STANDARD);
            EXPECT_NEAR(spectrum[600], Float(2.5), tolerances::STANDARD);
            EXPECT_NEAR(spectrum[700], 2, tolerances::STANDARD);
        }

        TEST(SpectrumTest, LinearIsZeroOutsideRange) {
            Allocator allocator;

            Float lambdas[2] = { 400, 500 };
            Float values[2] = { 1, 3 };

            LinearSpectrum<Float> spectrum(2, lambdas, values);

            EXPECT_EQ(spectrum[399], 0);
            EXPECT_EQ(spectrum[501], 0);
            EXPECT_EQ(spectrum[-100], 0);
            EXPECT_EQ(spectrum[constants::MAX], 0);
        }

        TEST(SpectrumTest, LinearContainsIsInclusive) {
            Allocator allocator;

            Float lambdas[2] = { 400, 500 };
            Float values[2] = { 1, 3 };

            LinearSpectrum<Float> spectrum(2, lambdas, values);

            EXPECT_TRUE(spectrum.contains(400));
            EXPECT_TRUE(spectrum.contains(450));
            EXPECT_TRUE(spectrum.contains(500));
            EXPECT_FALSE(spectrum.contains(399));
            EXPECT_FALSE(spectrum.contains(501));
        }

        TEST(SpectrumTest, LinearSingleSampleIsOnlyDefinedAtItsWavelength) {
            Allocator allocator;

            LinearSpectrum<Float> spectrum(1, allocator.make(Float(550)), allocator.make(Float(2)));

            EXPECT_EQ(spectrum[550], 2);
            EXPECT_EQ(spectrum[549], 0);
            EXPECT_EQ(spectrum[551], 0);

            EXPECT_TRUE(spectrum.contains(550));
            EXPECT_FALSE(spectrum.contains(551));
        }

        TEST(SpectrumTest, LinearDefaultIsEmpty) {
            LinearSpectrum<Float> spectrum;

            EXPECT_EQ(spectrum[550], 0);

            EXPECT_FALSE(spectrum.contains(550));

            EXPECT_EQ(spectrum.min(), 0);
            EXPECT_EQ(spectrum.max(), 0);
            EXPECT_EQ(spectrum.average(), 0);
        }

        TEST(SpectrumTest, LinearMinimumMaximumAndAverage) {
            Allocator allocator;

            Float lambdas[3] = { 400, 500, 700 };
            Float values[3] = { 1, 3, 2 };

            LinearSpectrum<Float> spectrum(3, lambdas, values);

            EXPECT_EQ(spectrum.min(), 1);
            EXPECT_EQ(spectrum.max(), 3);
            EXPECT_NEAR(spectrum.average(), Float(7) / 3, tolerances::STANDARD);
        }

        TEST(SpectrumTest, LinearAverageOfConstantIsConstant) {
            Allocator allocator;

            Float lambdas[4] = { 400, 450, 600, 700 };
            Float values[4] = { 2, 2, 2, 2 };

            LinearSpectrum<Float> spectrum(4, lambdas, values);

            EXPECT_NEAR(spectrum.average(), 2, tolerances::STANDARD);
        }

        TEST(SpectrumTest, LinearAverageRequiresTwoSamples) {
            Allocator allocator;

            LinearSpectrum<Float> spectrum(1, allocator.make(Float(550)), allocator.make(Float(2)));

            EXPECT_EQ(spectrum.average(), 0);
        }

        TEST(SpectrumTest, LinearComplexInterpolatesComponents) {
            Allocator allocator;

            Float lambdas[2] = { 400, 600 };
            Complex values[2] = { Complex(1, 0), Complex(2, 4) };

            LinearSpectrum<Complex> spectrum(2, lambdas, values);

            Complex c = spectrum[500];

            EXPECT_NEAR(c.real(), Float(1.5), tolerances::STANDARD);
            EXPECT_NEAR(c.imag(), 2, tolerances::STANDARD);

            EXPECT_EQ(spectrum[399], Complex(0, 0));
        }

        TEST(SpectrumTest, LinearComplexMinimumAndMaximumAreComponentwise) {
            Allocator allocator;

            Float lambdas[3] = { 400, 500, 600 };
            Complex values[3] = { Complex(1, 5), Complex(3, 2), Complex(2, 4) };

            LinearSpectrum<Complex> spectrum(3, lambdas, values);

            EXPECT_EQ(spectrum.min(), Complex(1, 2));
            EXPECT_EQ(spectrum.max(), Complex(3, 5));
        }

        TEST(SpectrumTest, SampledBroadcastsScalar) {
            SampledSpectrum<Float> s(Float(2.5));

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_EQ(s[i], Float(2.5));
        }

        TEST(SpectrumTest, SampledDefaultsToZero) {
            SampledSpectrum<Float> s;

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_EQ(s[i], 0);
        }

        TEST(SpectrumTest, SampledEqualityAndInequality) {
            SampledSpectrum<Float> a(1);
            SampledSpectrum<Float> b(1);

            EXPECT_TRUE(a == b);
            EXPECT_FALSE(a != b);

            b[constants::HERO_COUNT - 1] = 2;

            EXPECT_FALSE(a == b);
            EXPECT_TRUE(a != b);
        }

        TEST(SpectrumTest, SampledArithmetic) {
            SampledSpectrum<Float> a;
            SampledSpectrum<Float> b(2);

            for (int i = 0; i < constants::HERO_COUNT; i++) a[i] = Float(i + 1);

            for (int i = 0; i < constants::HERO_COUNT; i++) {
                EXPECT_EQ((a + b)[i], Float(i + 3));
                EXPECT_EQ((a - b)[i], Float(i - 1));
                EXPECT_EQ((-a)[i], -Float(i + 1));
                EXPECT_EQ((a * Float(3))[i], Float(3 * (i + 1)));
                EXPECT_EQ((Float(3) * a)[i], Float(3 * (i + 1)));
                EXPECT_EQ((a * b)[i], Float(2 * (i + 1)));
                EXPECT_EQ((a / Float(2))[i], Float(i + 1) / 2);
                EXPECT_EQ((a / b)[i], Float(i + 1) / 2);
            }
        }

        TEST(SpectrumTest, SampledCompoundAssignment) {
            SampledSpectrum<Float> s(4);

            s += SampledSpectrum<Float>(2);

            EXPECT_EQ(s, SampledSpectrum<Float>(6));

            s -= SampledSpectrum<Float>(1);

            EXPECT_EQ(s, SampledSpectrum<Float>(5));

            s *= Float(2);

            EXPECT_EQ(s, SampledSpectrum<Float>(10));

            s *= SampledSpectrum<Float>(3);

            EXPECT_EQ(s, SampledSpectrum<Float>(30));

            s /= Float(5);

            EXPECT_EQ(s, SampledSpectrum<Float>(6));

            s /= SampledSpectrum<Float>(2);

            EXPECT_EQ(s, SampledSpectrum<Float>(3));
        }

        TEST(SpectrumTest, SampledMinimumMaximumAndAverage) {
            SampledSpectrum<Float> s;

            for (int i = 0; i < constants::HERO_COUNT; i++) s[i] = Float(i * i);

            EXPECT_EQ(s.min(), 0);
            EXPECT_EQ(s.max(), Float((constants::HERO_COUNT - 1) * (constants::HERO_COUNT - 1)));

            Float sum = 0;

            for (int i = 0; i < constants::HERO_COUNT; i++) sum += s[i];

            EXPECT_NEAR(s.average(), sum / constants::HERO_COUNT, tolerances::STANDARD);
        }

        TEST(SpectrumTest, SampledComplexArithmetic) {
            SampledSpectrum<Complex> a(Complex(1, 1));
            SampledSpectrum<Complex> b(Complex(0, 1));

            SampledSpectrum<Complex> product = a * b;

            for (int i = 0; i < constants::HERO_COUNT; i++) {
                EXPECT_EQ(product[i], Complex(-1, 1));
                EXPECT_EQ((a + b)[i], Complex(1, 2));
            }

            EXPECT_EQ(a.min(), Complex(1, 1));
            EXPECT_EQ(a.max(), Complex(1, 1));
        }
    }
}