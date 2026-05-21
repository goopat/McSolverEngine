"""ctypes bindings for mcsolverengine_native.dll."""

import ctypes
import os
import sys

# ── DLL loading ──────────────────────────────────────────────

def _find_dll() -> str:
    """Locate mcsolverengine_native.dll from build output."""
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
    for config in ("Release", "Debug"):
        for candidate in [
            os.path.join(repo_root, "build", config, "mcsolverengine_native.dll"),
            os.path.join(repo_root, "build", "nuget_UseOcct", config, "mcsolverengine_native.dll"),
            os.path.join(repo_root, "build", "nuget_NoOcct", config, "mcsolverengine_native.dll"),
        ]:
            if os.path.isfile(candidate):
                return candidate
    raise FileNotFoundError(
        f"mcsolverengine_native.dll not found."
    )

def _find_occt_runtime() -> str | None:
    """Locate the OCCT runtime binary directory."""
    home = os.environ.get("USERPROFILE", "")
    candidates = [
        os.path.join(home, ".nuget", "packages", "opencascade.7.9-native", "1.0.0", "lib", "build", "bin"),
    ]
    for p in candidates:
        tkbrep = os.path.join(p, "TKBRep.dll")
        if os.path.isfile(tkbrep):
            return os.path.abspath(p)
    return None

_dll_path = _find_dll()
_occt_dir = _find_occt_runtime()

if hasattr(os, "add_dll_directory"):
    os.add_dll_directory(os.path.dirname(_dll_path))
    if _occt_dir:
        os.add_dll_directory(_occt_dir)

_native = ctypes.cdll.LoadLibrary(_dll_path)

# ── Enums ────────────────────────────────────────────────────

ResultCode = ctypes.c_int
RESULT_SUCCESS = 0
RESULT_INVALID_ARGUMENT = 1
RESULT_IMPORT_FAILED = 2
RESULT_SOLVE_FAILED = 3
RESULT_UNSUPPORTED = 4
RESULT_GEOMETRY_EXPORT_FAILED = 5
RESULT_BREP_EXPORT_FAILED = 6
RESULT_OPEN_CASCADE_UNAVAILABLE = 7
RESULT_OUT_OF_MEMORY = 8
RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET = 9

_RESULT_CODE_NAMES = {
    0: "SUCCESS",
    1: "INVALID_ARGUMENT",
    2: "IMPORT_FAILED",
    3: "SOLVE_FAILED",
    4: "UNSUPPORTED",
    5: "GEOMETRY_EXPORT_FAILED",
    6: "BREP_EXPORT_FAILED",
    7: "OPEN_CASCADE_UNAVAILABLE",
    8: "OUT_OF_MEMORY",
    9: "VARSET_EXPRESSION_UNSUPPORTED_SUBSET",
}

def result_code_name(code: int) -> str:
    return _RESULT_CODE_NAMES.get(code, f"UNKNOWN({code})")

GeometryKind = ctypes.c_int
GEOMETRY_POINT = 0
GEOMETRY_LINE_SEGMENT = 1
GEOMETRY_CIRCLE = 2
GEOMETRY_ARC = 3
GEOMETRY_ELLIPSE = 4
GEOMETRY_ARC_OF_ELLIPSE = 5
GEOMETRY_ARC_OF_HYPERBOLA = 6
GEOMETRY_ARC_OF_PARABOLA = 7
GEOMETRY_BSPLINE = 8

ConstraintKind = ctypes.c_int
CONSTRAINT_COINCIDENT = 0
CONSTRAINT_HORIZONTAL = 1
CONSTRAINT_VERTICAL = 2
CONSTRAINT_DISTANCE_X = 3
CONSTRAINT_DISTANCE_Y = 4
CONSTRAINT_DISTANCE = 5
CONSTRAINT_PARALLEL = 6
CONSTRAINT_TANGENT = 7
CONSTRAINT_PERPENDICULAR = 8
CONSTRAINT_ANGLE = 9
CONSTRAINT_RADIUS = 10
CONSTRAINT_DIAMETER = 11
CONSTRAINT_EQUAL = 12
CONSTRAINT_SYMMETRIC = 13
CONSTRAINT_POINT_ON_OBJECT = 14
CONSTRAINT_INTERNAL_ALIGNMENT = 15
CONSTRAINT_SNELLS_LAW = 16
CONSTRAINT_BLOCK = 17
CONSTRAINT_WEIGHT = 18

# ── Structs ──────────────────────────────────────────────────

class Point2(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_double),
        ("y", ctypes.c_double),
    ]

class Placement(ctypes.Structure):
    _fields_ = [
        ("px", ctypes.c_double),
        ("py", ctypes.c_double),
        ("pz", ctypes.c_double),
        ("qx", ctypes.c_double),
        ("qy", ctypes.c_double),
        ("qz", ctypes.c_double),
        ("qw", ctypes.c_double),
    ]

class BSplinePole(ctypes.Structure):
    _fields_ = [
        ("point", Point2),
        ("weight", ctypes.c_double),
    ]

class BSplineKnot(ctypes.Structure):
    _fields_ = [
        ("value", ctypes.c_double),
        ("multiplicity", ctypes.c_int),
    ]

class ConstraintRef(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_int),
        ("originalIndex", ctypes.c_int),
        ("expression", ctypes.c_char_p),
    ]

class GeometryRecord(ctypes.Structure):
    _fields_ = [
        ("geometryIndex", ctypes.c_int),
        ("originalId", ctypes.c_int),
        ("kind", ctypes.c_int),
        ("construction", ctypes.c_int),
        ("external", ctypes.c_int),
        ("blocked", ctypes.c_int),
        ("point", Point2),
        ("start", Point2),
        ("end", Point2),
        ("center", Point2),
        ("focus1", Point2),
        ("vertex", Point2),
        ("radius", ctypes.c_double),
        ("minorRadius", ctypes.c_double),
        ("startAngle", ctypes.c_double),
        ("endAngle", ctypes.c_double),
        ("degree", ctypes.c_int),
        ("periodic", ctypes.c_int),
        ("poleCount", ctypes.c_int),
        ("poles", ctypes.POINTER(BSplinePole)),
        ("knotCount", ctypes.c_int),
        ("knots", ctypes.POINTER(BSplineKnot)),
        ("constraintCount", ctypes.c_int),
        ("constraints", ctypes.POINTER(ConstraintRef)),
    ]

class GeometryResult(ctypes.Structure):
    _fields_ = [
        ("sketchName", ctypes.c_char_p),
        ("importStatus", ctypes.c_char_p),
        ("skippedConstraints", ctypes.c_int),
        ("messageCount", ctypes.c_int),
        ("messages", ctypes.POINTER(ctypes.c_char_p)),
        ("solveStatus", ctypes.c_char_p),
        ("degreesOfFreedom", ctypes.c_int),
        ("conflictingCount", ctypes.c_int),
        ("conflicting", ctypes.POINTER(ctypes.c_int)),
        ("redundantCount", ctypes.c_int),
        ("redundant", ctypes.POINTER(ctypes.c_int)),
        ("partiallyRedundantCount", ctypes.c_int),
        ("partiallyRedundant", ctypes.POINTER(ctypes.c_int)),
        ("exportKind", ctypes.c_char_p),
        ("exportStatus", ctypes.c_char_p),
        ("placement", Placement),
        ("geometryCount", ctypes.c_int),
        ("geometries", ctypes.POINTER(GeometryRecord)),
    ]

class BRepResult(ctypes.Structure):
    _fields_ = [
        ("sketchName", ctypes.c_char_p),
        ("importStatus", ctypes.c_char_p),
        ("skippedConstraints", ctypes.c_int),
        ("messageCount", ctypes.c_int),
        ("messages", ctypes.POINTER(ctypes.c_char_p)),
        ("solveStatus", ctypes.c_char_p),
        ("degreesOfFreedom", ctypes.c_int),
        ("conflictingCount", ctypes.c_int),
        ("conflicting", ctypes.POINTER(ctypes.c_int)),
        ("redundantCount", ctypes.c_int),
        ("redundant", ctypes.POINTER(ctypes.c_int)),
        ("partiallyRedundantCount", ctypes.c_int),
        ("partiallyRedundant", ctypes.POINTER(ctypes.c_int)),
        ("exportKind", ctypes.c_char_p),
        ("exportStatus", ctypes.c_char_p),
        ("placement", Placement),
        ("brepUtf8", ctypes.c_char_p),
    ]

# ── Function signatures ──────────────────────────────────────

_native.McSolverEngine_GetVersion.restype = ctypes.c_char_p
_native.McSolverEngine_GetVersion.argtypes = []

_native.McSolverEngine_SolveToGeometry.restype = ctypes.c_int
_native.McSolverEngine_SolveToGeometry.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.POINTER(GeometryResult)),
]

_native.McSolverEngine_SolveToGeometryWithParameters.restype = ctypes.c_int
_native.McSolverEngine_SolveToGeometryWithParameters.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.c_int,
    ctypes.POINTER(ctypes.POINTER(GeometryResult)),
]

_native.McSolverEngine_SolveToBRep.restype = ctypes.c_int
_native.McSolverEngine_SolveToBRep.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.POINTER(BRepResult)),
]

_native.McSolverEngine_SolveToBRepWithParameters.restype = ctypes.c_int
_native.McSolverEngine_SolveToBRepWithParameters.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.c_int,
    ctypes.POINTER(ctypes.POINTER(BRepResult)),
]

_native.McSolverEngine_FreeGeometryResult.restype = None
_native.McSolverEngine_FreeGeometryResult.argtypes = [ctypes.POINTER(GeometryResult)]

_native.McSolverEngine_FreeBRepResult.restype = None
_native.McSolverEngine_FreeBRepResult.argtypes = [ctypes.POINTER(BRepResult)]

# -- FCStd status codes
FCSTD_SUCCESS = 0
FCSTD_OPEN_FAILED = 1
FCSTD_NOT_ZIP = 2
FCSTD_XML_NOT_FOUND = 3
FCSTD_DECOMPRESS_FAILED = 4
FCSTD_OUT_OF_MEMORY = 5

_FCSTD_CODE_NAMES = {
    0: "FCSTD_SUCCESS",
    1: "FCSTD_OPEN_FAILED",
    2: "FCSTD_NOT_ZIP",
    3: "FCSTD_XML_NOT_FOUND",
    4: "FCSTD_DECOMPRESS_FAILED",
    5: "FCSTD_OUT_OF_MEMORY",
}


def fcstd_result_code_name(code: int) -> str:
    return _FCSTD_CODE_NAMES.get(code, f"FCSTD_UNKNOWN({code})")


_native.McSolverEngine_ExtractFCStdDoc.restype = ctypes.c_int
_native.McSolverEngine_ExtractFCStdDoc.argtypes = [
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]

_native.McSolverEngine_FreeFCStdDoc.restype = None
_native.McSolverEngine_FreeFCStdDoc.argtypes = [ctypes.c_char_p]

_native.McSolverEngine_GetLastError.restype = ctypes.c_char_p
_native.McSolverEngine_GetLastError.argtypes = []
