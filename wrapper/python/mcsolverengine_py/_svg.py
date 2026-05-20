"""SVG geometry sampling and output utilities.

Matches the C# WrapperRegressionTests.WriteVisibleGeometrySvg
and SampleGeometry dispatch, using rhino3dm only for B-Spline curves.
"""

from __future__ import annotations

import math
from typing import List, Tuple

import warnings

try:
    import rhino3dm as rg
except ModuleNotFoundError:
    rg = None
    warnings.warn("rhino3dm not installed; B-Spline curves will export as control polygon")

from ._engine import GeometryRecord
from ._bindings import (
    GEOMETRY_POINT,
    GEOMETRY_LINE_SEGMENT,
    GEOMETRY_CIRCLE,
    GEOMETRY_ARC,
    GEOMETRY_ELLIPSE,
    GEOMETRY_ARC_OF_ELLIPSE,
    GEOMETRY_ARC_OF_HYPERBOLA,
    GEOMETRY_ARC_OF_PARABOLA,
    GEOMETRY_BSPLINE,
)

SvgPoint = Tuple[float, float]


def _positive_sweep(start: float, end: float) -> float:
    sweep = end - start
    while sweep <= 0.0:
        sweep += math.pi * 2.0
    return sweep


# ── Manual parametric geometry sampling (matching C# exactly) ─

def _sample_circle(cx: float, cy: float, r: float) -> List[SvgPoint]:
    if r <= 0.0:
        return []
    n = 96
    return [
        (cx + r * math.cos(2.0 * math.pi * i / n), cy + r * math.sin(2.0 * math.pi * i / n))
        for i in range(n + 1)
    ]


def _sample_circular_arc(
    cx: float, cy: float, r: float, start_angle: float, end_angle: float,
) -> List[SvgPoint]:
    if r <= 0.0:
        return []
    sweep = _positive_sweep(start_angle, end_angle)
    n = max(8, int(math.ceil(64.0 * sweep / (2.0 * math.pi))))
    return [
        (cx + r * math.cos(start_angle + sweep * i / n),
         cy + r * math.sin(start_angle + sweep * i / n))
        for i in range(n + 1)
    ]


def _sample_ellipse(
    cx: float, cy: float,
    fx: float, fy: float,
    minor_r: float,
    start_angle: float, end_angle: float,
) -> List[SvgPoint]:
    if minor_r <= 0.0:
        return []
    fdx = fx - cx
    fdy = fy - cy
    fd = math.sqrt(fdx * fdx + fdy * fdy)
    if fd < 1e-12:
        ax_x, ax_y = 1.0, 0.0
    else:
        ax_x, ax_y = fdx / fd, fdy / fd
    ay_x, ay_y = -ax_y, ax_x
    major_r = math.sqrt(minor_r * minor_r + fd * fd)
    sweep = _positive_sweep(start_angle, end_angle)
    n = max(24, int(math.ceil(96.0 * sweep / (2.0 * math.pi))))
    result: List[SvgPoint] = []
    for i in range(n + 1):
        a = start_angle + sweep * i / n
        x = cx + major_r * math.cos(a) * ax_x + minor_r * math.sin(a) * ay_x
        y = cy + major_r * math.cos(a) * ax_y + minor_r * math.sin(a) * ay_y
        result.append((x, y))
    return result


def _sample_hyperbola(record: GeometryRecord) -> List[SvgPoint]:
    fx, fy = record.focus1.x, record.focus1.y
    cx, cy = record.center.x, record.center.y
    mr = record.minorRadius
    fdx = fx - cx
    fdy = fy - cy
    fd = math.sqrt(fdx * fdx + fdy * fdy)
    if fd < 1e-12 or mr <= 0.0:
        return []
    ax_x, ax_y = fdx / fd, fdy / fd
    ay_x, ay_y = -ax_y, ax_x
    major_r = math.sqrt(max(fd * fd - mr * mr, 0.0))
    sweep = record.endAngle - record.startAngle
    n = max(24, int(math.ceil(48.0 * abs(sweep) / math.pi)))
    result: List[SvgPoint] = []
    for i in range(n + 1):
        t = record.startAngle + sweep * i / n
        x = cx + major_r * math.cosh(t) * ax_x + mr * math.sinh(t) * ay_x
        y = cy + major_r * math.cosh(t) * ax_y + mr * math.sinh(t) * ay_y
        result.append((x, y))
    return result


def _sample_parabola(record: GeometryRecord) -> List[SvgPoint]:
    fx, fy = record.focus1.x, record.focus1.y
    vx, vy = record.vertex.x, record.vertex.y
    fdx = fx - vx
    fdy = fy - vy
    focal = math.sqrt(fdx * fdx + fdy * fdy)
    if focal < 1e-12:
        return []
    ax_x, ax_y = fdx / focal, fdy / focal
    ay_x, ay_y = -ax_y, ax_x
    sweep = record.endAngle - record.startAngle
    n = max(24, int(math.ceil(32.0 * abs(sweep))))
    result: List[SvgPoint] = []
    for i in range(n + 1):
        t = record.startAngle + sweep * i / n
        local_x = t * t / (4.0 * focal)
        local_y = t
        x = vx + local_x * ax_x + local_y * ay_x
        y = vy + local_x * ax_y + local_y * ay_y
        result.append((x, y))
    return result


# ── B-Spline via rhino3dm (matching C# SampleRhinoNurbs) ─────

def _sample_bspline(record: GeometryRecord) -> List[SvgPoint]:
    if record.periodic:
        return _sample_periodic_bspline(record)
    return _sample_nonperiodic_bspline(record)


def _sample_nonperiodic_bspline(record: GeometryRecord) -> List[SvgPoint]:
    poles = record.poles
    degree = record.degree
    if not poles or degree < 1:
        return [(p.point.x, p.point.y) for p in poles]

    expanded: List[float] = []
    for k in record.knots:
        for _ in range(k.multiplicity):
            expanded.append(k.value)

    if len(expanded) < len(poles) + degree + 1:
        return [(p.point.x, p.point.y) for p in poles]

    t_start = expanded[degree]
    t_end = expanded[len(expanded) - degree - 1]
    if t_end <= t_start:
        return [(p.point.x, p.point.y) for p in poles]

    return _sample_rhino_nurbs_curve(
        controls=[(p.point.x, p.point.y, p.weight) for p in poles],
        degree=degree,
        knots=expanded,
        start=t_start,
        end=t_end,
        num_samples=128,
        close=False,
    )


def _sample_periodic_bspline(record: GeometryRecord) -> List[SvgPoint]:
    poles = record.poles
    degree = record.degree
    knots = record.knots
    if degree < 1:
        return _sample_closed_control_polygon(poles)

    expanded: List[float] = []
    for k in knots:
        for _ in range(k.multiplicity):
            expanded.append(k.value)

    if len(expanded) < 2:
        return _sample_closed_control_polygon(poles)

    period = expanded[-1] - expanded[0]
    if period <= 0.0:
        return _sample_closed_control_polygon(poles)

    first_mult = knots[0].multiplicity
    last_mult = knots[-1].multiplicity
    continuity = degree + 1 - first_mult
    first_final_knot_index = len(expanded) - last_mult
    if (continuity <= 0
            or first_final_knot_index - continuity < 0
            or first_mult + continuity > len(expanded)
            or len(poles) < continuity):
        return _sample_closed_control_polygon(poles)

    # Build periodic knot sequence (mirrors OCCT periodic KnotSequence)
    periodic_knots: List[float] = []
    for i in range(first_final_knot_index - continuity, first_final_knot_index):
        periodic_knots.append(expanded[i] - period)
    periodic_knots.extend(expanded)
    for i in range(first_mult, first_mult + continuity):
        periodic_knots.append(expanded[i] + period)

    # Append continuity leading poles (Rhino convention)
    controls = [(p.point.x, p.point.y, p.weight) for p in poles]
    for i in range(continuity):
        controls.append((poles[i].point.x, poles[i].point.y, poles[i].weight))

    t_start = periodic_knots[degree]
    return _sample_rhino_nurbs_curve(
        controls=controls,
        degree=degree,
        knots=periodic_knots,
        start=t_start,
        end=t_start + period,
        num_samples=256,
        close=True,
    )


def _sample_closed_control_polygon(poles) -> List[SvgPoint]:
    pts = [(p.point.x, p.point.y) for p in poles]
    if pts:
        pts.append(pts[0])
    return pts


def _sample_rhino_nurbs_curve(
    controls: List[Tuple[float, float, float]],
    degree: int,
    knots: List[float],
    start: float,
    end: float,
    num_samples: int,
    close: bool,
) -> List[SvgPoint]:
    if rg is None:
        return [(x, y) for (x, y, _) in controls]
    is_rational = any(abs(w - 1.0) > 1e-12 for (_, _, w) in controls)

    nc = rg.NurbsCurve(3, is_rational, degree + 1, len(controls))
    for i, (x, y, w) in enumerate(controls):
        nc.Points[i] = rg.Point4d(x, y, 0.0, w)

    rhino_knots = knots[1:-1]
    if len(nc.Knots) == len(rhino_knots):
        for i, kv in enumerate(rhino_knots):
            nc.Knots[i] = kv
    else:
        return [(x, y) for (x, y, _) in controls]

    if not nc.IsValid:
        return [(x, y) for (x, y, _) in controls]

    result: List[SvgPoint] = []
    last_index = num_samples - 1 if close else num_samples
    for i in range(last_index + 1):
        u = start + (end - start) * i / num_samples if close else (
            end if i == num_samples else start + (end - start) * i / num_samples
        )
        pt = nc.PointAt(u)
        result.append((pt.X, pt.Y))

    if close and result:
        result.append(result[0])

    return result


# ── Dispatch ─────────────────────────────────────────────────

def sample_geometry(record: GeometryRecord) -> List[SvgPoint]:
    kind = record.kind

    if kind == GEOMETRY_POINT:
        return [(record.point.x, record.point.y)]
    if kind == GEOMETRY_LINE_SEGMENT:
        return [(record.start.x, record.start.y), (record.end.x, record.end.y)]
    if kind == GEOMETRY_CIRCLE:
        return _sample_circle(record.center.x, record.center.y, record.radius)
    if kind == GEOMETRY_ARC:
        return _sample_circular_arc(
            record.center.x, record.center.y, record.radius,
            record.startAngle, record.endAngle,
        )
    if kind == GEOMETRY_ELLIPSE:
        return _sample_ellipse(
            record.center.x, record.center.y,
            record.focus1.x, record.focus1.y,
            record.minorRadius, 0.0, 2.0 * math.pi,
        )
    if kind == GEOMETRY_ARC_OF_ELLIPSE:
        return _sample_ellipse(
            record.center.x, record.center.y,
            record.focus1.x, record.focus1.y,
            record.minorRadius, record.startAngle, record.endAngle,
        )
    if kind == GEOMETRY_ARC_OF_HYPERBOLA:
        return _sample_hyperbola(record)
    if kind == GEOMETRY_ARC_OF_PARABOLA:
        return _sample_parabola(record)
    if kind == GEOMETRY_BSPLINE:
        return _sample_bspline(record)
    return []


# ── SVG output ───────────────────────────────────────────────

def _format_value(v: float) -> str:
    return f"{v:.6f}".rstrip("0").rstrip(".")


def write_visible_geometry_svg(geometries: List[GeometryRecord], svg_path: str):
    curves: List[List[SvgPoint]] = []
    points: List[SvgPoint] = []

    for g in geometries:
        if g.external or g.construction:
            continue
        sampled = sample_geometry(g)
        if not sampled:
            continue
        if g.kind == GEOMETRY_POINT:
            points.append(sampled[0])
        else:
            curves.append(sampled)

    all_pts = [p for c in curves for p in c] + points
    if not all_pts:
        raise ValueError("No visible geometry found for SVG export.")

    min_x = min(p[0] for p in all_pts)
    max_x = max(p[0] for p in all_pts)
    min_y = min(p[1] for p in all_pts)
    max_y = max(p[1] for p in all_pts)
    model_w = max(max_x - min_x, 1.0)
    model_h = max(max_y - min_y, 1.0)
    pad = max(max(model_w, model_h) * 0.04, 10.0)
    view_w = model_w + pad * 2.0
    view_h = model_h + pad * 2.0
    stroke_w = max(max(model_w, model_h) / 600.0, 1.0)
    point_r = stroke_w * 2.0

    def to_svg(p: SvgPoint) -> str:
        x = p[0] - min_x + pad
        y = max_y - p[1] + pad
        return f"{_format_value(x)},{_format_value(y)}"

    svg_lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{_format_value(1200.0 * view_h / view_w)}" viewBox="0 0 {_format_value(view_w)} {_format_value(view_h)}">',
        '  <!-- Generated from structured geometry; external and construction geometry are omitted. -->',
        '  <rect width="100%" height="100%" fill="white"/>',
        f'  <g fill="none" stroke="#111827" stroke-width="{_format_value(stroke_w)}" stroke-linecap="round" stroke-linejoin="round">',
    ]
    for curve in curves:
        pts_attr = " ".join(to_svg(p) for p in curve)
        svg_lines.append(f'    <polyline points="{pts_attr}"/>')
    svg_lines.append("  </g>")
    svg_lines.append('  <g fill="#dc2626" stroke="none">')
    for pt in points:
        cx, cy = to_svg(pt).split(",")
        svg_lines.append(f'    <circle cx="{cx}" cy="{cy}" r="{_format_value(point_r)}"/>')
    svg_lines.append("  </g>")
    svg_lines.append("</svg>")

    with open(svg_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(svg_lines) + "\n")
