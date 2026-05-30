"""ctypes bindings for mcsolverengine_native."""

import ctypes
import os
import platform
import sys

if platform.system() == "Windows":
    _NATIVE_LIB_NAME = "mcsolverengine_native.dll"
    _OCCT_TKBREP_NAME = "TKBRep.dll"
elif platform.system() == "Darwin":
    _NATIVE_LIB_NAME = "libmcsolverengine_native.dylib"
    _OCCT_TKBREP_NAME = "libTKBRep.dylib"
else:
    _NATIVE_LIB_NAME = "libmcsolverengine_native.so"
    _OCCT_TKBREP_NAME = "libTKBRep.so"

# ── DLL path configuration ─────────────────────────────────────

_explicit_dll_path: "str | None" = None


def set_native_lib_path(path: str) -> None:
    """Specify the absolute path to the native library.

    This is the highest-priority discovery method. Must be called before
    any Engine method that triggers library loading.
    """
    global _explicit_dll_path
    if not os.path.isabs(path):
        raise ValueError(f"Path must be absolute: {path}")
    if not os.path.isfile(path):
        raise FileNotFoundError(f"Native library not found at: {path}")
    _explicit_dll_path = path


def _find_dll() -> str:
    """Locate the native library using 3-tier priority."""
    # 1. Explicitly set path (highest priority)
    if _explicit_dll_path:
        return _explicit_dll_path

    # 2. Same directory as this .py file
    py_dir = os.path.dirname(os.path.abspath(__file__))
    candidate = os.path.join(py_dir, _NATIVE_LIB_NAME)
    if os.path.isfile(candidate):
        return candidate

    # 3. Walk through PATH
    for path_dir in os.environ.get("PATH", "").split(os.pathsep):
        if not path_dir:
            continue
        candidate = os.path.join(path_dir.strip(), _NATIVE_LIB_NAME)
        if os.path.isfile(candidate):
            return candidate

    raise FileNotFoundError(
        f"{_NATIVE_LIB_NAME} not found. "
        "Use set_native_lib_path() to specify the path explicitly, "
        "place the library next to the .py files, or add it to PATH."
    )


def _find_occt_runtime() -> "str | None":
    """Locate the OCCT runtime binary directory."""
    home = os.environ.get("USERPROFILE", "") or os.environ.get("HOME", "")
    conda_prefix = os.environ.get("CONDA_PREFIX", "")
    package_root = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(package_root, "..", "..", ".."))
    candidates = [
        os.path.join(conda_prefix, "Library", "bin"),
        os.path.join(conda_prefix, "bin"),
        os.path.join(conda_prefix, "lib"),
        os.path.join(repo_root, ".pixi", "envs", "default", "Library", "bin"),
        os.path.join(repo_root, ".pixi", "envs", "default", "lib"),
        os.path.join(home, ".nuget", "packages", "opencascade.7.9-native", "1.0.0", "lib", "build", "bin"),
    ]
    seen = set()
    for p in candidates:
        if not p or p in seen:
            continue
        seen.add(p)
        tkbrep = os.path.join(p, _OCCT_TKBREP_NAME)
        if os.path.isfile(tkbrep):
            return os.path.abspath(p)
    return None


# ── Lazy DLL loading ───────────────────────────────────────────

_native_lib = None
_loaded = False


def _ensure_loaded():
    global _native_lib, _loaded
    if _loaded:
        return
    dll_path = _find_dll()
    occt_dir = _find_occt_runtime()
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(os.path.dirname(dll_path))
        if occt_dir:
            os.add_dll_directory(occt_dir)
    _native_lib = ctypes.cdll.LoadLibrary(dll_path)
    _setup_signatures(_native_lib)
    _loaded = True


class _LazyLib:
    def __getattr__(self, name):
        _ensure_loaded()
        globals()["_native"] = _native_lib
        return getattr(_native_lib, name)


_native = _LazyLib()

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

class VarSetProperty(ctypes.Structure):
    _fields_ = [
        ("keyUtf8", ctypes.c_char_p),
        ("valueUtf8", ctypes.c_char_p),
        ("unitUtf8", ctypes.c_char_p),
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
        ("varSetPropertyCount", ctypes.c_int),
        ("varSetProperties", ctypes.POINTER(VarSetProperty)),
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
        ("varSetPropertyCount", ctypes.c_int),
        ("varSetProperties", ctypes.POINTER(VarSetProperty)),
        ("placement", Placement),
        ("brepUtf8", ctypes.c_char_p),
    ]

# ── Function signatures ──────────────────────────────────────

def _setup_signatures(lib):
    lib.McSolverEngine_GetVersion.restype = ctypes.c_char_p
    lib.McSolverEngine_GetVersion.argtypes = []

    lib.McSolverEngine_SolveToGeometry.restype = ctypes.c_int
    lib.McSolverEngine_SolveToGeometry.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.POINTER(GeometryResult)),
    ]

    lib.McSolverEngine_SolveToGeometryWithParameters.restype = ctypes.c_int
    lib.McSolverEngine_SolveToGeometryWithParameters.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_char_p),
        ctypes.POINTER(ctypes.c_char_p),
        ctypes.c_int,
        ctypes.POINTER(ctypes.POINTER(GeometryResult)),
    ]

    lib.McSolverEngine_SolveToBRep.restype = ctypes.c_int
    lib.McSolverEngine_SolveToBRep.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.POINTER(BRepResult)),
    ]

    lib.McSolverEngine_SolveToBRepWithParameters.restype = ctypes.c_int
    lib.McSolverEngine_SolveToBRepWithParameters.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_char_p),
        ctypes.POINTER(ctypes.c_char_p),
        ctypes.c_int,
        ctypes.POINTER(ctypes.POINTER(BRepResult)),
    ]

    lib.McSolverEngine_FreeGeometryResult.restype = None
    lib.McSolverEngine_FreeGeometryResult.argtypes = [ctypes.POINTER(GeometryResult)]

    lib.McSolverEngine_FreeBRepResult.restype = None
    lib.McSolverEngine_FreeBRepResult.argtypes = [ctypes.POINTER(BRepResult)]

    lib.McSolverEngine_ExtractFCStdDoc.restype = ctypes.c_int
    lib.McSolverEngine_ExtractFCStdDoc.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_char_p),
    ]

    lib.McSolverEngine_FreeFCStdDoc.restype = None
    lib.McSolverEngine_FreeFCStdDoc.argtypes = [ctypes.c_char_p]

    lib.McSolverEngine_GetLastError.restype = ctypes.c_char_p
    lib.McSolverEngine_GetLastError.argtypes = []


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
