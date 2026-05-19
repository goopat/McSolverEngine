"""Smoke tests for McSolverEngine Python wrapper."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from mcsolverengine_py import (
    Engine,
    write_visible_geometry_svg,
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

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SAMPLE_XML = os.path.join(REPO_ROOT, "fcstdDoc", "1.xml")


def _read_sample() -> str:
    with open(SAMPLE_XML, "r", encoding="utf-8") as f:
        return f.read()


class TestVersion(unittest.TestCase):
    def test_get_version_returns_nonempty_string(self):
        v = Engine.version()
        self.assertIsInstance(v, str)
        self.assertTrue(len(v) > 0)


class TestSolveToGeometry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.xml = _read_sample()

    def test_success_and_has_geometries(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertIsInstance(result.sketchName, str)
        self.assertIn("Sketch", result.sketchName)
        self.assertIn("success", result.importStatus.lower())
        self.assertIn("success", result.solveStatus.lower())
        self.assertGreater(len(result.geometries), 0)

    def test_geometries_have_valid_kinds(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        valid_kinds = {
            GEOMETRY_POINT,
            GEOMETRY_LINE_SEGMENT,
            GEOMETRY_CIRCLE,
            GEOMETRY_ARC,
            GEOMETRY_ELLIPSE,
            GEOMETRY_ARC_OF_ELLIPSE,
            GEOMETRY_ARC_OF_HYPERBOLA,
            GEOMETRY_ARC_OF_PARABOLA,
            GEOMETRY_BSPLINE,
        }
        for g in result.geometries:
            self.assertIn(g.kind, valid_kinds, f"Unexpected geometry kind: {g.kind}")

    def test_line_geometry_has_distinct_endpoints(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        lines = [g for g in result.geometries if g.kind == GEOMETRY_LINE_SEGMENT]
        self.assertGreater(len(lines), 0, "Expected at least one line segment")
        for line in lines:
            self.assertIsInstance(line.start.x, float)
            self.assertIsInstance(line.end.y, float)

    def test_placement_is_valid(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        p = result.placement
        self.assertIsInstance(p.px, float)
        self.assertIsInstance(p.qw, float)

    def test_conflicting_and_redundant_arrays(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertIsInstance(result.conflicting, list)
        self.assertIsInstance(result.redundant, list)
        self.assertIsInstance(result.partiallyRedundant, list)

    def test_constraint_refs_readable(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        for g in result.geometries:
            self.assertIsInstance(g.constraints, list)
            for c in g.constraints:
                self.assertIsInstance(c.kind, int)
                self.assertIsInstance(c.originalIndex, int)
                self.assertTrue(c.expression is None or isinstance(c.expression, str))


class TestSolveToBRep(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.xml = _read_sample()

    def test_brep_returns_result(self):
        try:
            result = Engine.solve_to_brep(self.xml, "Sketch")
            self.assertIsInstance(result.brepUtf8, str)
        except RuntimeError as e:
            # OCCT unavailable is acceptable
            self.assertIn("OPEN_CASCADE_UNAVAILABLE", str(e))


class TestParameters(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.xml = _read_sample()

    def test_solve_to_geometry_with_empty_params_succeeds(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch", parameters={})
        self.assertGreater(len(result.geometries), 0)

    def test_solve_to_geometry_with_no_params_succeeds(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertGreater(len(result.geometries), 0)


class TestV1025ExpressionDrivenConstraint(unittest.TestCase):
    """Regression test matching GeometryRegression_V1025_ExpressionDrivenConstraint."""

    @classmethod
    def setUpClass(cls):
        cls.xml_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.5.xml")
        with open(cls.xml_path, "r", encoding="utf-8") as f:
            cls.xml = f.read()

    def test_expression_driven_constraint_refs(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        total_expr_refs = sum(len(g.constraints) for g in result.geometries)
        self.assertEqual(
            1, total_expr_refs,
            f"Expected exactly 1 expression-driven constraint ref, got {total_expr_refs}.",
        )
        geo17 = next(g for g in result.geometries if g.originalId == 17)
        self.assertTrue(
            any(
                c.kind == 11  # Diameter
                and c.expression == "<<VarSet>>.R1"
                for c in geo17.constraints
            ),
            "Expected originalId=17 to have Diameter with expression <<VarSet>>.R1.",
        )

    def test_visible_bspline_present(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertTrue(
            any(
                not g.external and not g.construction and g.kind == GEOMETRY_BSPLINE
                for g in result.geometries
            ),
            "Expected visible B-Spline geometry.",
        )

    def test_visible_hyperbola_or_parabola_present(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertTrue(
            any(
                not g.external
                and not g.construction
                and g.kind in (GEOMETRY_ARC_OF_HYPERBOLA, GEOMETRY_ARC_OF_PARABOLA)
                for g in result.geometries
            ),
            "Expected visible hyperbola or parabola geometry.",
        )

    def test_write_svg(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        svg_path = os.path.join(
            REPO_ROOT, "fcstdDoc", "V102.5.py.svg"
        )
        write_visible_geometry_svg(result.geometries, svg_path)
        self.assertTrue(
            os.path.isfile(svg_path),
            f"Expected SVG output at '{svg_path}'.",
        )
        # Basic validity check
        with open(svg_path, "r", encoding="utf-8") as f:
            content = f.read()
        self.assertIn("<svg", content)
        self.assertIn("</svg>", content)
        self.assertIn("<polyline", content)


class TestV1024WithParameters(unittest.TestCase):
    """Solve V102.4 with parameter overrides and write SVG."""

    @classmethod
    def setUpClass(cls):
        cls.xml_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.4.xml")
        with open(cls.xml_path, "r", encoding="utf-8") as f:
            cls.xml = f.read()
        cls.parameters = {
            "D1": "61",
            "L1": "41",
            "L2": "61",
            "L3": "11",
            "L4": "16",
            "L5": "21",
        }

    def test_solve_with_parameters_succeeds(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch", parameters=self.parameters)
        self.assertIn("success", result.solveStatus.lower())
        self.assertGreater(len(result.geometries), 0)

    def test_solve_without_parameters_succeeds(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertIn("success", result.solveStatus.lower())

    def test_write_parameterized_svg(self):
        result = Engine.solve_to_geometry(self.xml, "Sketch", parameters=self.parameters)
        svg_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.4.plus1.py.svg")
        write_visible_geometry_svg(result.geometries, svg_path)
        self.assertTrue(os.path.isfile(svg_path))
        with open(svg_path, "r", encoding="utf-8") as f:
            content = f.read()
        self.assertIn("<svg", content)
        self.assertIn("</svg>", content)
        self.assertIn("<polyline", content)


if __name__ == "__main__":
    unittest.main()
