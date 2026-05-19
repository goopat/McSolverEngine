"""McSolverEngine Python wrapper."""

from ._engine import (
    Engine,
    GeometryResult,
    BRepResult,
    GeometryRecord,
    Point2,
    Placement,
    BSplinePole,
    BSplineKnot,
    ConstraintRef,
)

from ._svg import write_visible_geometry_svg

from ._bindings import (
    RESULT_SUCCESS,
    RESULT_INVALID_ARGUMENT,
    RESULT_IMPORT_FAILED,
    RESULT_SOLVE_FAILED,
    RESULT_UNSUPPORTED,
    RESULT_GEOMETRY_EXPORT_FAILED,
    RESULT_BREP_EXPORT_FAILED,
    RESULT_OPEN_CASCADE_UNAVAILABLE,
    RESULT_OUT_OF_MEMORY,
    RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET,
    GEOMETRY_POINT,
    GEOMETRY_LINE_SEGMENT,
    GEOMETRY_CIRCLE,
    GEOMETRY_ARC,
    GEOMETRY_ELLIPSE,
    GEOMETRY_ARC_OF_ELLIPSE,
    GEOMETRY_ARC_OF_HYPERBOLA,
    GEOMETRY_ARC_OF_PARABOLA,
    GEOMETRY_BSPLINE,
    result_code_name,
)

__all__ = [
    "Engine",
    "GeometryResult",
    "BRepResult",
    "GeometryRecord",
    "Point2",
    "Placement",
    "BSplinePole",
    "BSplineKnot",
    "ConstraintRef",
    "write_visible_geometry_svg",
    "RESULT_SUCCESS",
    "RESULT_OPEN_CASCADE_UNAVAILABLE",
    "GEOMETRY_POINT",
    "GEOMETRY_LINE_SEGMENT",
    "GEOMETRY_CIRCLE",
    "GEOMETRY_BSPLINE",
    "result_code_name",
]
