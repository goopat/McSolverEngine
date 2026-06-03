"""High-level Engine class wrapping native DLL calls."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass, field
from typing import Optional, List

from . import _bindings as _n

# ── Result types ─────────────────────────────────────────────

@dataclass
class Point2:
    x: float
    y: float

@dataclass
class Placement:
    px: float = 0
    py: float = 0
    pz: float = 0
    qx: float = 0
    qy: float = 0
    qz: float = 0
    qw: float = 0

@dataclass
class BSplinePole:
    point: Point2
    weight: float

@dataclass
class BSplineKnot:
    value: float
    multiplicity: int

@dataclass
class ConstraintRef:
    kind: int
    originalIndex: int
    expression: Optional[str]

@dataclass
class VarSetPropertyValue:
    value: str
    unit: str = ""

@dataclass
class GeometryRecord:
    geometryIndex: int
    originalId: int
    kind: int
    construction: bool
    external: bool
    blocked: bool
    point: Point2
    start: Point2
    end: Point2
    center: Point2
    focus1: Point2
    vertex: Point2
    radius: float
    minorRadius: float
    startAngle: float
    endAngle: float
    degree: int
    periodic: bool
    poles: List[BSplinePole] = field(default_factory=list)
    knots: List[BSplineKnot] = field(default_factory=list)
    constraints: List[ConstraintRef] = field(default_factory=list)

@dataclass
class GeometryResult:
    sketchName: str
    importStatus: str
    skippedConstraints: int
    messages: List[str]
    solveStatus: str
    degreesOfFreedom: int
    conflicting: List[int]
    redundant: List[int]
    partiallyRedundant: List[int]
    exportKind: str
    exportStatus: str
    varSetProperties: dict[str, VarSetPropertyValue] = field(default_factory=dict)
    placement: Placement = field(default_factory=Placement)
    geometries: List[GeometryRecord] = field(default_factory=list)

@dataclass
class BRepResult:
    sketchName: str
    importStatus: str
    skippedConstraints: int
    messages: List[str]
    solveStatus: str
    degreesOfFreedom: int
    conflicting: List[int]
    redundant: List[int]
    partiallyRedundant: List[int]
    exportKind: str
    exportStatus: str
    varSetProperties: dict[str, VarSetPropertyValue] = field(default_factory=dict)
    placement: Placement = field(default_factory=Placement)
    brepUtf8: str = ""


@dataclass
class InspectConstraintDesc:
    originalIndex: int
    type: int
    kind: str
    driving: bool
    value: float
    referencedGeoIds: List[int] = field(default_factory=list)


@dataclass
class InspectGeometryElementDesc:
    index: int
    originalId: int
    type: str
    construction: bool
    external: bool
    constraintIndices: List[int] = field(default_factory=list)


@dataclass
class ScalarPropertyInfoDesc:
    name: str
    type: str
    scalarValue: str
    propertyXml: str


@dataclass
class VarSetParameterInfoDesc:
    name: str
    type: str
    rawValue: str
    expression: str
    propertyXml: str


@dataclass
class SketchDesc:
    label: str
    objectName: str
    properties: List[ScalarPropertyInfoDesc] = field(default_factory=list)
    geometries: List[InspectGeometryElementDesc] = field(default_factory=list)
    constraints: List[InspectConstraintDesc] = field(default_factory=list)


@dataclass
class VarSetDesc:
    label: str
    objectName: str
    parameters: List[VarSetParameterInfoDesc] = field(default_factory=list)


@dataclass
class DocumentDesc:
    sketches: List[SketchDesc] = field(default_factory=list)
    varSets: List[VarSetDesc] = field(default_factory=list)
    messages: List[str] = field(default_factory=list)


# ── Helper ───────────────────────────────────────────────────

def _decode(s: Optional[bytes]) -> str:
    if s is None:
        return ""
    return s.decode("utf-8", errors="replace")

def _str_array(ptr, count: int) -> List[str]:
    result: List[str] = []
    for i in range(count):
        s = ptr[i]
        if s is not None:
            result.append(_decode(s))
        else:
            result.append("")
    return result

def _int_array(ptr, count: int) -> List[int]:
    if not ptr or count <= 0:
        return []
    return [int(ptr[i]) for i in range(count)]

def _varset_properties(ptr, count: int) -> dict[str, VarSetPropertyValue]:
    values: dict[str, VarSetPropertyValue] = {}
    if not ptr or count <= 0:
        return values
    for i in range(count):
        item = ptr[i]
        values[_decode(item.keyUtf8)] = VarSetPropertyValue(
            value=_decode(item.valueUtf8),
            unit=_decode(item.unitUtf8),
        )
    return values

def _convert_point2(p: _n.Point2) -> Point2:
    return Point2(x=p.x, y=p.y)

def _convert_placement(p: _n.Placement) -> Placement:
    return Placement(px=p.px, py=p.py, pz=p.pz, qx=p.qx, qy=p.qy, qz=p.qz, qw=p.qw)

def _convert_poles(ptr, count: int) -> List[BSplinePole]:
    if not ptr or count <= 0:
        return []
    return [
        BSplinePole(point=_convert_point2(ptr[i].point), weight=ptr[i].weight)
        for i in range(count)
    ]

def _convert_knots(ptr, count: int) -> List[BSplineKnot]:
    if not ptr or count <= 0:
        return []
    return [
        BSplineKnot(value=ptr[i].value, multiplicity=ptr[i].multiplicity)
        for i in range(count)
    ]

def _convert_constraints(ptr, count: int) -> List[ConstraintRef]:
    if not ptr or count <= 0:
        return []
    return [
        ConstraintRef(
            kind=ptr[i].kind,
            originalIndex=ptr[i].originalIndex,
            expression=_decode(ptr[i].expression) if ptr[i].expression else None,
        )
        for i in range(count)
    ]

def _convert_geometry(g: _n.GeometryRecord) -> GeometryRecord:
    return GeometryRecord(
        geometryIndex=g.geometryIndex,
        originalId=g.originalId,
        kind=g.kind,
        construction=bool(g.construction),
        external=bool(g.external),
        blocked=bool(g.blocked),
        point=_convert_point2(g.point),
        start=_convert_point2(g.start),
        end=_convert_point2(g.end),
        center=_convert_point2(g.center),
        focus1=_convert_point2(g.focus1),
        vertex=_convert_point2(g.vertex),
        radius=g.radius,
        minorRadius=g.minorRadius,
        startAngle=g.startAngle,
        endAngle=g.endAngle,
        degree=g.degree,
        periodic=bool(g.periodic),
        poles=_convert_poles(g.poles, g.poleCount),
        knots=_convert_knots(g.knots, g.knotCount),
        constraints=_convert_constraints(g.constraints, g.constraintCount),
    )

def _convert_geometry_result(raw: _n.GeometryResult) -> GeometryResult:
    return GeometryResult(
        sketchName=_decode(raw.sketchName),
        importStatus=_decode(raw.importStatus),
        skippedConstraints=raw.skippedConstraints,
        messages=_str_array(raw.messages, raw.messageCount),
        solveStatus=_decode(raw.solveStatus),
        degreesOfFreedom=raw.degreesOfFreedom,
        conflicting=_int_array(raw.conflicting, raw.conflictingCount),
        redundant=_int_array(raw.redundant, raw.redundantCount),
        partiallyRedundant=_int_array(raw.partiallyRedundant, raw.partiallyRedundantCount),
        exportKind=_decode(raw.exportKind),
        exportStatus=_decode(raw.exportStatus),
        varSetProperties=_varset_properties(raw.varSetProperties, raw.varSetPropertyCount),
        placement=_convert_placement(raw.placement),
        geometries=[
            _convert_geometry(raw.geometries[i]) for i in range(raw.geometryCount)
        ],
    )

def _convert_brep_result(raw: _n.BRepResult) -> BRepResult:
    return BRepResult(
        sketchName=_decode(raw.sketchName),
        importStatus=_decode(raw.importStatus),
        skippedConstraints=raw.skippedConstraints,
        messages=_str_array(raw.messages, raw.messageCount),
        solveStatus=_decode(raw.solveStatus),
        degreesOfFreedom=raw.degreesOfFreedom,
        conflicting=_int_array(raw.conflicting, raw.conflictingCount),
        redundant=_int_array(raw.redundant, raw.redundantCount),
        partiallyRedundant=_int_array(raw.partiallyRedundant, raw.partiallyRedundantCount),
        exportKind=_decode(raw.exportKind),
        exportStatus=_decode(raw.exportStatus),
        varSetProperties=_varset_properties(raw.varSetProperties, raw.varSetPropertyCount),
        placement=_convert_placement(raw.placement),
        brepUtf8=_decode(raw.brepUtf8) if raw.brepUtf8 else "",
    )


def _convert_inspect_constraint(raw: _n.InspectConstraint) -> InspectConstraintDesc:
    return InspectConstraintDesc(
        originalIndex=raw.originalIndex,
        type=raw.type,
        kind=_decode(raw.kindUtf8),
        driving=bool(raw.driving),
        value=raw.value,
        referencedGeoIds=_int_array(raw.referencedGeoIds, raw.referencedGeoIdCount),
    )


def _convert_inspect_geometry(raw: _n.InspectGeometryElement) -> InspectGeometryElementDesc:
    return InspectGeometryElementDesc(
        index=raw.index,
        originalId=raw.originalId,
        type=_decode(raw.typeUtf8),
        construction=bool(raw.construction),
        external=bool(raw.external),
        constraintIndices=_int_array(raw.constraintIndices, raw.constraintCount),
    )


def _convert_scalar_property(raw: _n.ScalarPropertyInfo) -> ScalarPropertyInfoDesc:
    return ScalarPropertyInfoDesc(
        name=_decode(raw.nameUtf8),
        type=_decode(raw.typeUtf8),
        scalarValue=_decode(raw.scalarValueUtf8),
        propertyXml=_decode(raw.propertyXmlUtf8),
    )


def _convert_varset_parameter(raw: _n.VarSetParameterInfo) -> VarSetParameterInfoDesc:
    return VarSetParameterInfoDesc(
        name=_decode(raw.nameUtf8),
        type=_decode(raw.typeUtf8),
        rawValue=_decode(raw.rawValueUtf8),
        expression=_decode(raw.expressionUtf8),
        propertyXml=_decode(raw.propertyXmlUtf8),
    )


def _convert_document_info(raw: _n.DocumentInfo) -> DocumentDesc:
    return DocumentDesc(
        sketches=[
            SketchDesc(
                label=_decode(raw.sketches[i].labelUtf8),
                objectName=_decode(raw.sketches[i].objectNameUtf8),
                properties=[
                    _convert_scalar_property(raw.sketches[i].properties[j])
                    for j in range(raw.sketches[i].propertyCount)
                ],
                geometries=[
                    _convert_inspect_geometry(raw.sketches[i].geometries[j])
                    for j in range(raw.sketches[i].geometryCount)
                ],
                constraints=[
                    _convert_inspect_constraint(raw.sketches[i].constraints[j])
                    for j in range(raw.sketches[i].constraintCount)
                ],
            )
            for i in range(raw.sketchCount)
        ],
        varSets=[
            VarSetDesc(
                label=_decode(raw.varSets[i].labelUtf8),
                objectName=_decode(raw.varSets[i].objectNameUtf8),
                parameters=[
                    _convert_varset_parameter(raw.varSets[i].parameters[j])
                    for j in range(raw.varSets[i].parameterCount)
                ],
            )
            for i in range(raw.varSetCount)
        ],
        messages=_str_array(raw.messages, raw.messageCount),
    )


# ── Engine ───────────────────────────────────────────────────

class Engine:
    """Python wrapper for McSolverEngine native library."""

    @staticmethod
    def version() -> str:
        return _decode(_n._native.McSolverEngine_GetVersion())

    @staticmethod
    def solve_to_geometry(
        document_xml: str,
        sketch_name: str,
        parameters: Optional[dict[str, str]] = None,
    ) -> GeometryResult:
        if parameters:
            keys = list(parameters.keys())
            vals = list(parameters.values())
            keys_array = (ctypes.c_char_p * len(keys))()
            vals_array = (ctypes.c_char_p * len(vals))()
            for i, k in enumerate(keys):
                keys_array[i] = k.encode("utf-8")
            for i, v in enumerate(vals):
                vals_array[i] = v.encode("utf-8")

            result_ptr = ctypes.POINTER(_n.GeometryResult)()
            code = _n._native.McSolverEngine_SolveToGeometryWithParameters(
                document_xml.encode("utf-8"),
                sketch_name.encode("utf-8"),
                keys_array,
                vals_array,
                len(keys),
                ctypes.byref(result_ptr),
            )
        else:
            result_ptr = ctypes.POINTER(_n.GeometryResult)()
            code = _n._native.McSolverEngine_SolveToGeometry(
                document_xml.encode("utf-8"),
                sketch_name.encode("utf-8"),
                ctypes.byref(result_ptr),
            )

        if code != 0:
            raise RuntimeError(
                f"SolveToGeometry failed: code={code} ({_n.result_code_name(code)})"
            )

        try:
            return _convert_geometry_result(result_ptr.contents)
        finally:
            _n._native.McSolverEngine_FreeGeometryResult(result_ptr)

    @staticmethod
    def solve_to_brep(
        document_xml: str,
        sketch_name: str,
        parameters: Optional[dict[str, str]] = None,
    ) -> BRepResult:
        if parameters:
            keys = list(parameters.keys())
            vals = list(parameters.values())
            keys_array = (ctypes.c_char_p * len(keys))()
            vals_array = (ctypes.c_char_p * len(vals))()
            for i, k in enumerate(keys):
                keys_array[i] = k.encode("utf-8")
            for i, v in enumerate(vals):
                vals_array[i] = v.encode("utf-8")

            result_ptr = ctypes.POINTER(_n.BRepResult)()
            code = _n._native.McSolverEngine_SolveToBRepWithParameters(
                document_xml.encode("utf-8"),
                sketch_name.encode("utf-8"),
                keys_array,
                vals_array,
                len(keys),
                ctypes.byref(result_ptr),
            )
        else:
            result_ptr = ctypes.POINTER(_n.BRepResult)()
            code = _n._native.McSolverEngine_SolveToBRep(
                document_xml.encode("utf-8"),
                sketch_name.encode("utf-8"),
                ctypes.byref(result_ptr),
            )

        if code != 0:
            raise RuntimeError(
                f"SolveToBRep failed: code={code} ({_n.result_code_name(code)})"
            )

        try:
            return _convert_brep_result(result_ptr.contents)
        finally:
            _n._native.McSolverEngine_FreeBRepResult(result_ptr)

    @staticmethod
    def extract_fcstd_doc(fcstd_path: str) -> str:
        """Extract ``Document.xml`` from a .FCStd archive. *fcstd_path* must be UTF-8."""
        out_ptr = ctypes.c_char_p()
        code = _n._native.McSolverEngine_ExtractFCStdDoc(
            fcstd_path.encode("utf-8"),
            ctypes.byref(out_ptr),
        )
        if code != 0:
            last_error = _n._native.McSolverEngine_GetLastError()
            err_msg = last_error.decode("utf-8") if last_error else ""
            raise RuntimeError(
                f"ExtractFCStdDoc failed: code={code} ({_n.fcstd_result_code_name(code)}); {err_msg}"
            )
        try:
            return out_ptr.value.decode("utf-8")
        finally:
            _n._native.McSolverEngine_FreeFCStdDoc(out_ptr)

    @staticmethod
    def inspect_document(document_xml: str) -> DocumentDesc:
        """Inspect Document.xml content and return sketch/varSet metadata,
        including geometry elements and constraints for each sketch.

        On parse failure the returned DocumentDesc will have an empty
        sketches/varSets list and messages describing the problem.
        """
        result_ptr = ctypes.POINTER(_n.DocumentInfo)()
        _n._native.McSolverEngine_InspectDocumentXml(
            document_xml.encode("utf-8"),
            ctypes.byref(result_ptr),
        )
        try:
            return _convert_document_info(result_ptr.contents)
        finally:
            _n._native.McSolverEngine_FreeDocumentInfo(result_ptr)
