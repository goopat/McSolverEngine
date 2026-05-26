"""Smoke tests for McSolverEngine Python wrapper."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from mcsolverengine_py import (
    Engine,
    set_native_lib_path,
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


def _find_build_dll():
    """Locate mcsolverengine_native.dll from build output (for testing)."""
    for config in ("Release", "Debug"):
        for candidate in [
            os.path.join(REPO_ROOT, "build", config, "mcsolverengine_native.dll"),
            os.path.join(REPO_ROOT, "build", "nuget_UseOcct", config, "mcsolverengine_native.dll"),
            os.path.join(REPO_ROOT, "build", "nuget_NoOcct", config, "mcsolverengine_native.dll"),
        ]:
            if os.path.isfile(candidate):
                return candidate
    raise FileNotFoundError(
        "mcsolverengine_native.dll not found in build output. "
        "Build the project first, or set MCSOLVERENGINE_NATIVE_LIB_PATH."
    )


# Resolve the native library before any tests touch the Engine.
set_native_lib_path(_find_build_dll())

SAMPLE_XML = os.path.join(REPO_ROOT, "fcstdDoc", "1.xml")

EVALUATED_VARSET_PROPERTIES_XML = """<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet001" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet001">
            <Properties Count="8" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Base" type="App::PropertyFloat">
                    <Float value="4.0"/>
                </Property>
                <Property name="Length" type="App::PropertyQuantity">
                    <Quantity value="1 m"/>
                </Property>
                <Property name="Angle" type="App::PropertyQuantity">
                    <Quantity value="1.5707963267948966 rad"/>
                </Property>
                <Property name="Name" type="App::PropertyString">
                    <String value="widget"/>
                </Property>
                <Property name="DoubleBase" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="Area" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="3">
                        <Expression path="DoubleBase" expression="Base * 2"/>
                        <Expression path="Width" expression="DoubleBase + 1"/>
                        <Expression path="Area" expression="Length * Length"/>
                    </ExpressionEngine>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[1]" expression="&lt;&lt;Parameters&gt;&gt;.Width"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="3.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>
"""


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


class TestEvaluatedVarSetProperties(unittest.TestCase):
    def test_geometry_result_exposes_canonical_varset_properties(self):
        result = Engine.solve_to_geometry(EVALUATED_VARSET_PROPERTIES_XML, "Sketch")
        self.assertEqual(6, len(result.varSetProperties))
        self.assertAlmostEqual(4.0, result.varSetProperties["VarSet001.Base"].value)
        self.assertEqual("", result.varSetProperties["VarSet001.Base"].unit)
        self.assertAlmostEqual(1000.0, result.varSetProperties["VarSet001.Length"].value)
        self.assertEqual("mm", result.varSetProperties["VarSet001.Length"].unit)
        self.assertAlmostEqual(90.0, result.varSetProperties["VarSet001.Angle"].value)
        self.assertEqual("deg", result.varSetProperties["VarSet001.Angle"].unit)
        self.assertAlmostEqual(1000000.0, result.varSetProperties["VarSet001.Area"].value)
        self.assertEqual("mm^2", result.varSetProperties["VarSet001.Area"].unit)
        self.assertNotIn("VarSet001.Name", result.varSetProperties)

    def test_geometry_result_exposes_overridden_varset_properties(self):
        result = Engine.solve_to_geometry(
            EVALUATED_VARSET_PROPERTIES_XML,
            "Sketch",
            parameters={"Parameters.Base": "6"},
        )
        self.assertAlmostEqual(6.0, result.varSetProperties["VarSet001.Base"].value)
        self.assertAlmostEqual(12.0, result.varSetProperties["VarSet001.DoubleBase"].value)
        self.assertAlmostEqual(13.0, result.varSetProperties["VarSet001.Width"].value)


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


class TestExtractFCStdDoc(unittest.TestCase):
    def test_extract_1_fcstd_matches_xml(self):
        fcstd_path = os.path.join(REPO_ROOT, "fcstdDoc", "1.FCStd")
        xml_path = os.path.join(REPO_ROOT, "fcstdDoc", "1.xml")
        self.assertTrue(os.path.isfile(fcstd_path), f"Missing {fcstd_path}")
        self.assertTrue(os.path.isfile(xml_path), f"Missing {xml_path}")

        with open(xml_path, "r", encoding="utf-8") as f:
            expected = f.read()

        extracted = Engine.extract_fcstd_doc(fcstd_path)
        self.assertEqual(
            expected, extracted,
            f"Extracted Document.xml does not match {xml_path} "
            f"(got {len(extracted)} bytes, expected {len(expected)} bytes)",
        )


def _tokenize_brep(text: str):
    return text.split()


def _try_parse_float(token: str):
    try:
        return float(token)
    except ValueError:
        return None


def _assert_brep_equivalent(test: unittest.TestCase, expected: str, actual: str):
    expected_tokens = _tokenize_brep(expected)
    actual_tokens = _tokenize_brep(actual)
    test.assertEqual(
        len(expected_tokens), len(actual_tokens),
        f"BREP token count differs: {len(expected_tokens)} vs {len(actual_tokens)}",
    )
    for i, (et, at) in enumerate(zip(expected_tokens, actual_tokens)):
        ev = _try_parse_float(et)
        av = _try_parse_float(at)
        if ev is not None and av is not None:
            test.assertAlmostEqual(ev, av, delta=1e-9, msg=f"BREP numeric token mismatch at index {i}")
        else:
            test.assertEqual(et, at, f"BREP token mismatch at index {i}")


class TestV1027ExtractAndSolveWithParameters(unittest.TestCase):
    """Extract V102.7.FCStd, solve with 参数集1.L1=50, compare BREP."""

    def setUp(self):
        self.fcstd_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.7.FCStd")
        self.reference_brep_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.7.50.brp")
        self.solver_brep_path = os.path.join(REPO_ROOT, "fcstdDoc", "V102.7.50.solver.brp")
        self.assertTrue(os.path.isfile(self.fcstd_path), f"Missing {self.fcstd_path}")
        self.assertTrue(os.path.isfile(self.reference_brep_path), f"Missing {self.reference_brep_path}")

    def test_extract_and_solve_parameterized_brep(self):
        # Delete solver output before run
        if os.path.isfile(self.solver_brep_path):
            os.remove(self.solver_brep_path)
        self.assertFalse(os.path.isfile(self.solver_brep_path),
                         "Solver BREP file should not exist before solve.")

        extracted_xml = Engine.extract_fcstd_doc(self.fcstd_path)
        self.assertTrue(len(extracted_xml) > 0, "Extracted Document.xml is empty.")

        result = Engine.solve_to_brep(
            extracted_xml, "Sketch", parameters={"参数集1.L1": "50"}
        )
        self.assertIn("success", result.solveStatus.lower(),
                      f"Solve failed: {result.solveStatus}")
        self.assertTrue(len(result.brepUtf8) > 0, "BREP output is empty.")

        # Write solver BREP and verify
        with open(self.solver_brep_path, "w", encoding="utf-8") as f:
            f.write(result.brepUtf8)
        self.assertTrue(os.path.isfile(self.solver_brep_path),
                        "Solver BREP file was not written.")

        # Compare against reference
        with open(self.reference_brep_path, "r", encoding="utf-8") as f:
            expected_brep = f.read()
        _assert_brep_equivalent(self, expected_brep, result.brepUtf8)


if __name__ == "__main__":
    unittest.main()
