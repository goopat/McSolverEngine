#include "PlaneTransform.h"

#include <cmath>

namespace McSolverEngine::Compat
{

void quaternionToRotationMatrix(
    const double qx,
    const double qy,
    const double qz,
    const double qw,
    double R[9]
) noexcept
{
    // Standard quaternion-to-rotation-matrix formula.
    // q = qw + qx*i + qy*j + qz*k
    const double xx = qx * qx;
    const double yy = qy * qy;
    const double zz = qz * qz;
    const double xy = qx * qy;
    const double xz = qx * qz;
    const double yz = qy * qz;
    const double wx = qw * qx;
    const double wy = qw * qy;
    const double wz = qw * qz;

    // Row 0: X axis in world
    R[0] = 1.0 - 2.0 * (yy + zz);
    R[1] = 2.0 * (xy - wz);
    R[2] = 2.0 * (xz + wy);

    // Row 1: Y axis in world
    R[3] = 2.0 * (xy + wz);
    R[4] = 1.0 - 2.0 * (xx + zz);
    R[5] = 2.0 * (yz - wx);

    // Row 2: Z axis (normal) in world
    R[6] = 2.0 * (xz - wy);
    R[7] = 2.0 * (yz + wx);
    R[8] = 1.0 - 2.0 * (xx + yy);
}

namespace
{

// Extract the 3D basis vectors from a Placement.
// u, v, n are the X, Y, Z axes in world coordinates.
// T is the position.
void decomposePlacement(
    const Placement& p,
    double u[3],
    double v[3],
    double n[3],
    double T[3]
)
{
    double R[9];
    quaternionToRotationMatrix(p.qx, p.qy, p.qz, p.qw, R);

    u[0] = R[0]; u[1] = R[1]; u[2] = R[2];  // X = row 0
    v[0] = R[3]; v[1] = R[4]; v[2] = R[5];  // Y = row 1
    n[0] = R[6]; n[1] = R[7]; n[2] = R[8];  // Z = row 2

    T[0] = p.px;
    T[1] = p.py;
    T[2] = p.pz;
}

// Dot product of two 3D vectors.
double dot(const double a[3], const double b[3]) noexcept
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

PlaneTransform PlaneTransform::between(
    const Placement& source,
    const Placement& target
) noexcept
{
    // Decompose both placements
    double us[3], vs[3], ns[3], Ts[3];
    double ut[3], vt[3], nt[3], Tt[3];
    decomposePlacement(source, us, vs, ns, Ts);
    decomposePlacement(target, ut, vt, nt, Tt);

    // Offset vector: world-space difference, projected onto target axes
    double dT[3] = {
        Ts[0] - Tt[0],
        Ts[1] - Tt[1],
        Ts[2] - Tt[2]
    };

    // M = [ut·us,  ut·vs]
    //     [vt·us,  vt·vs]
    // b = [ut·dT]
    //     [vt·dT]
    PlaneTransform tf;
    tf.m00 = dot(ut, us);
    tf.m01 = dot(ut, vs);
    tf.m10 = dot(vt, us);
    tf.m11 = dot(vt, vs);
    tf.tx = dot(ut, dT);
    tf.ty = dot(vt, dT);

    return tf;
}

bool PlaneTransform::isConformal() const noexcept
{
    // A 2×2 matrix M is a scaled rotation iff M * M^T = s * I
    // i.e., the columns are orthogonal and have equal length.
    // Check: m00*m00 + m10*m10 ≈ m01*m01 + m11*m11  (equal column lengths)
    //   and  m00*m01 + m10*m11 ≈ 0                    (orthogonal columns)

    const double col0LenSq = m00 * m00 + m10 * m10;
    const double col1LenSq = m01 * m01 + m11 * m11;
    const double dotCols = m00 * m01 + m10 * m11;

    // Tolerance: 1e-12 is tight enough to distinguish parallel planes
    // from general orientations.
    constexpr double tol = 1e-12;
    return std::abs(col0LenSq - col1LenSq) <= tol * std::max(1.0, std::max(col0LenSq, col1LenSq))
        && std::abs(dotCols) <= tol * std::max(1.0, std::max(col0LenSq, col1LenSq));
}

double PlaneTransform::scaleFactor() const noexcept
{
    // sqrt of average column length squared
    const double col0LenSq = m00 * m00 + m10 * m10;
    const double col1LenSq = m01 * m01 + m11 * m11;
    return std::sqrt(0.5 * (col0LenSq + col1LenSq));
}

}  // namespace McSolverEngine::Compat
