#pragma once

#include "McSolverEngine/CompatModel.h"

namespace McSolverEngine::Compat
{

/// A 2D affine transform between two sketch planes.
///
/// Transforms a point from source-sketch-local coordinates to
/// target-sketch-local coordinates:
///   p_target = M * p_source + b
///
/// Derived from the two sketch Placements. The projection is along the
/// target sketch normal (Z axis), matching FreeCAD's inverse-placement
/// projection in SketchObject::rebuildExternalGeometry().
struct PlaneTransform
{
    double m00, m01;   // first row of 2×2 matrix
    double m10, m11;   // second row of 2×2 matrix
    double tx, ty;     // 2D offset vector

    [[nodiscard]] Point2 apply(Point2 p) const noexcept
    {
        return {
            m00 * p.x + m01 * p.y + tx,
            m10 * p.x + m11 * p.y + ty
        };
    }

    /// Compute the transform from sourcePlacement to targetPlacement.
    [[nodiscard]] static PlaneTransform between(
        const Placement& source,
        const Placement& target
    ) noexcept;

    /// Returns true if this transform preserves circles (i.e., M is a scaled
    /// rotation: M*M^T = s*I for some scalar s).
    [[nodiscard]] bool isConformal() const noexcept;

    /// When isConformal(), returns the uniform scale factor sqrt(s).
    [[nodiscard]] double scaleFactor() const noexcept;

    /// Returns true when M flips orientation (det(M) < 0).  Between two
    /// sketch placements this happens when the plane normals point in
    /// opposite directions (target sketch on the back face).
    [[nodiscard]] bool isReflection() const noexcept
    {
        return m00 * m11 - m01 * m10 < 0.0;
    }

    /// When isConformal(), returns the reference angle of M:
    ///   - rotation (det > 0):  the in-plane rotation angle θ, so a
    ///     polar angle α maps to α + θ.
    ///   - reflection (det < 0): the mirror axis angle φ, so a polar
    ///     angle α maps to φ − α.
    [[nodiscard]] double referenceAngle() const noexcept;
};

/// Convert a quaternion (qx, qy, qz, qw) to a 3×3 rotation matrix.
/// Output R[9] is row-major: R[row*3 + col].
void quaternionToRotationMatrix(
    double qx, double qy, double qz, double qw,
    double R[9]
) noexcept;

}  // namespace McSolverEngine::Compat
