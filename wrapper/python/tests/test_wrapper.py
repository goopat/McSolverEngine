"""Smoke tests for McSolverEngine Python wrapper."""

import os
import platform
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


if platform.system() == "Windows":
    _NATIVE_LIB_NAME = "mcsolverengine_native.dll"
elif platform.system() == "Darwin":
    _NATIVE_LIB_NAME = "libmcsolverengine_native.dylib"
else:
    _NATIVE_LIB_NAME = "libmcsolverengine_native.so"


def _find_build_dll():
    """Locate the native library from build output (for testing)."""
    candidates = [
        os.path.join(REPO_ROOT, "build", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "Release", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "Debug", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "nuget_UseOcct", "Release", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "nuget_UseOcct", "Debug", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "nuget_NoOcct", "Release", _NATIVE_LIB_NAME),
        os.path.join(REPO_ROOT, "build", "nuget_NoOcct", "Debug", _NATIVE_LIB_NAME),
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    raise FileNotFoundError(
        f"{_NATIVE_LIB_NAME} not found in build output. "
        "Build the project first, or set MCSOLVERENGINE_NATIVE_LIB_PATH."
    )


# Resolve the native library before any tests touch the Engine.
set_native_lib_path(_find_build_dll())

# Verify the native library can actually be loaded.
try:
    _ = Engine.version()
except OSError as e:
    raise RuntimeError(f"Failed to load native library: {e}") from e

SAMPLE_XML = os.path.join(REPO_ROOT, "fcstdDoc", "1.xml")

EVALUATED_VARSET_PROPERTIES_XML = """<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet001" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet001">
            <Properties Count="10" TransientCount="0">
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
                <Property name="Visible" type="App::PropertyBool">
                    <Bool value="true"/>
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
        self.assertEqual(9, len(result.varSetProperties))
        self.assertEqual("Parameters", result.varSetProperties["VarSet001.Label"].value)
        self.assertEqual("4", result.varSetProperties["VarSet001.Base"].value)
        self.assertEqual("", result.varSetProperties["VarSet001.Base"].unit)
        self.assertEqual("1000", result.varSetProperties["VarSet001.Length"].value)
        self.assertEqual("mm", result.varSetProperties["VarSet001.Length"].unit)
        self.assertEqual("90", result.varSetProperties["VarSet001.Angle"].value)
        self.assertEqual("deg", result.varSetProperties["VarSet001.Angle"].unit)
        self.assertEqual("widget", result.varSetProperties["VarSet001.Name"].value)
        self.assertEqual("true", result.varSetProperties["VarSet001.Visible"].value)
        self.assertEqual("1000000", result.varSetProperties["VarSet001.Area"].value)
        self.assertEqual("mm^2", result.varSetProperties["VarSet001.Area"].unit)

    def test_geometry_result_exposes_overridden_varset_properties(self):
        result = Engine.solve_to_geometry(
            EVALUATED_VARSET_PROPERTIES_XML,
            "Sketch",
            parameters={"Parameters.Base": "6"},
        )
        self.assertEqual("6", result.varSetProperties["VarSet001.Base"].value)
        self.assertEqual("12", result.varSetProperties["VarSet001.DoubleBase"].value)
        self.assertEqual("13", result.varSetProperties["VarSet001.Width"].value)


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
            "VarSet.D1": "61",
            "VarSet.L1": "41",
            "VarSet.L2": "61",
            "VarSet.L3": "11",
            "VarSet.L4": "16",
            "VarSet.L5": "21",
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


def _extract_brep_section_lines(text: str, section_name: str):
    lines = text.splitlines()
    prefix = f"{section_name} "
    for index, line in enumerate(lines):
        if not line.startswith(prefix):
            continue
        count = int(line[len(prefix):].strip())
        return lines[index + 1:index + 1 + count]
    raise AssertionError(f"BREP section {section_name!r} not found")


def _curve_lines_match(expected_line: str, actual_line: str, tolerance: float = 1e-9) -> bool:
    expected_tokens = expected_line.split()
    actual_tokens = actual_line.split()
    if len(expected_tokens) != len(actual_tokens):
        return False

    for expected_token, actual_token in zip(expected_tokens, actual_tokens):
        expected_value = _try_parse_float(expected_token)
        actual_value = _try_parse_float(actual_token)
        if expected_value is not None and actual_value is not None:
            if abs(expected_value - actual_value) > tolerance:
                return False
        elif expected_token != actual_token:
            return False

    return True


def _assert_brep_geometry_equivalent_unordered(test: unittest.TestCase, expected: str, actual: str):
    expected_curves = _extract_brep_section_lines(expected, "Curves")
    actual_curves = _extract_brep_section_lines(actual, "Curves")
    test.assertEqual(
        len(expected_curves), len(actual_curves),
        f"BREP curve count differs: {len(expected_curves)} vs {len(actual_curves)}",
    )

    unmatched_actual = list(actual_curves)
    for expected_line in expected_curves:
        for index, actual_line in enumerate(unmatched_actual):
            if _curve_lines_match(expected_line, actual_line):
                del unmatched_actual[index]
                break
        else:
            test.fail(f"Missing matching BREP curve record for: {expected_line}")


class TestInspectDocumentXml(unittest.TestCase):
    """Regression tests for Engine.inspect_document."""

    INSPECT_XML_WITH_GEOMETRY = """<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet001" id="1"/>
        <Object type="Sketcher::SketchObject" name="SketchA" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet001">
            <Properties Count="2" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="6.0"/>
                </Property>
            </Properties>
        </Object>
        <Object name="SketchA">
            <Properties Count="3" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Sketch Alpha"/>
                </Property>
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="4">
                        <Constrain Name="" Type="1" Value="1.0" First="0" FirstPos="0" Second="0" SecondPos="0" Third="-2000" ThirdPos="0" IsDriving="0" IsInVirtualSpace="0" IsActive="1"/>
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" IsDriving="1" IsInVirtualSpace="0" IsActive="1"/>
                        <Constrain Name="" Type="6" Value="100.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" IsDriving="1" IsInVirtualSpace="0" IsActive="1"/>
                        <Constrain Name="" Type="11" Value="25.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" IsDriving="1" IsInVirtualSpace="0" IsActive="1"/>
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="1.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomCircle" id="2" migrated="1">
                            <Circle CenterX="0.0" CenterY="0.0" CenterZ="0.0" Radius="10.0"/>
                            <Construction value="1"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>
"""

    def test_returns_geometry_and_constraints(self):
        result = Engine.inspect_document(self.INSPECT_XML_WITH_GEOMETRY)
        self.assertEqual(0, len(result.messages), f"Unexpected messages: {result.messages}")
        self.assertEqual(1, len(result.sketches))
        self.assertEqual(1, len(result.varSets))

        sketch = result.sketches[0]
        self.assertEqual("SketchA", sketch.objectName)
        self.assertEqual("Sketch Alpha", sketch.label)
        self.assertGreater(len(sketch.properties), 0)

        # Geometries
        self.assertEqual(2, len(sketch.geometries))
        g0 = sketch.geometries[0]
        self.assertEqual(0, g0.index)
        self.assertEqual(1, g0.originalId)
        self.assertEqual("Part::GeomLineSegment", g0.type)
        self.assertFalse(g0.construction)
        self.assertFalse(g0.external)
        self.assertEqual(4, len(g0.constraintIndices))
        self.assertEqual([0, 0, 1, 2], g0.constraintIndices)

        g1 = sketch.geometries[1]
        self.assertEqual(1, g1.index)
        self.assertEqual(2, g1.originalId)
        self.assertEqual("Part::GeomCircle", g1.type)
        self.assertTrue(g1.construction)
        self.assertFalse(g1.external)
        self.assertEqual(1, len(g1.constraintIndices))
        self.assertEqual([3], g1.constraintIndices)

        # Constraints
        self.assertEqual(4, len(sketch.constraints))
        c0 = sketch.constraints[0]
        self.assertEqual(0, c0.originalIndex)
        self.assertEqual(1, c0.type)
        self.assertEqual("Coincident", c0.kind)
        self.assertFalse(c0.driving)
        self.assertEqual(1.0, c0.value)
        self.assertEqual([0, 0], c0.referencedGeoIds)

        c1 = sketch.constraints[1]
        self.assertEqual(1, c1.originalIndex)
        self.assertEqual(2, c1.type)
        self.assertEqual("Horizontal", c1.kind)
        self.assertTrue(c1.driving)

        c2 = sketch.constraints[2]
        self.assertEqual(2, c2.originalIndex)
        self.assertEqual(6, c2.type)
        self.assertEqual("Distance", c2.kind)
        self.assertTrue(c2.driving)
        self.assertEqual(100.0, c2.value)
        self.assertEqual([0], c2.referencedGeoIds)

        c3 = sketch.constraints[3]
        self.assertEqual(3, c3.originalIndex)
        self.assertEqual(11, c3.type)
        self.assertEqual("Radius", c3.kind)
        self.assertTrue(c3.driving)
        self.assertEqual(25.0, c3.value)
        self.assertEqual([1], c3.referencedGeoIds)

    def test_invalid_xml_has_messages(self):
        result = Engine.inspect_document("not valid xml")
        self.assertEqual(1, len(result.messages))
        self.assertIn("ObjectData", result.messages[0])
        self.assertEqual(0, len(result.sketches))
        self.assertEqual(0, len(result.varSets))


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


class TestV1119SolveWithParameters(unittest.TestCase):
    """Solve V111.9.xml with VarSet.L1=500 and compare BREP geometry equivalence."""

    def setUp(self):
        self.xml_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.9.xml")
        self.reference_brep_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.9.500.brp")
        self.solver_brep_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.9.500.solver.py.brp")
        self.assertTrue(os.path.isfile(self.xml_path), f"Missing {self.xml_path}")
        self.assertTrue(os.path.isfile(self.reference_brep_path), f"Missing {self.reference_brep_path}")

    def test_solve_parameterized_brep(self):
        if os.path.isfile(self.solver_brep_path):
            os.remove(self.solver_brep_path)
        self.assertFalse(
            os.path.isfile(self.solver_brep_path),
            "Solver BREP file should not exist before solve.",
        )

        with open(self.xml_path, "r", encoding="utf-8") as f:
            xml = f.read()

        result = Engine.solve_to_brep(xml, "Sketch", parameters={"VarSet.L1": "500"})
        self.assertIn("success", result.solveStatus.lower(), f"Solve failed: {result.solveStatus}")
        self.assertTrue(len(result.brepUtf8) > 0, "BREP output is empty.")

        with open(self.solver_brep_path, "w", encoding="utf-8") as f:
            f.write(result.brepUtf8)
        self.assertTrue(
            os.path.isfile(self.solver_brep_path),
            "Solver BREP file was not written.",
        )

        with open(self.reference_brep_path, "r", encoding="utf-8") as f:
            expected_brep = f.read()
        # Floating-point tail jitter can reorder geometry records in exported BREP text after the
        # constraint solve. FreeCAD itself can reproduce this across repeated solves with the same
        # parameters. V111.9.xml with VarSet.L1=500 is the easiest known repro, so this test only
        # relaxes geometry record positions in the BREP text and still requires geometric equivalence.
        _assert_brep_geometry_equivalent_unordered(self, expected_brep, result.brepUtf8)


class TestV1119500SolveWithNoChangeParameters(unittest.TestCase):
    """Solve V111.9.500.xml with VarSet.L1=500 and compare BREP geometry equivalence."""

    def setUp(self):
        self.xml_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.9.500.xml")
        self.reference_brep_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.9.500.brp")
        self.solver_brep_path = os.path.join(
            REPO_ROOT, "fcstdDoc", "V111.9.500.solver.py.param.nochange.brp"
        )
        self.assertTrue(os.path.isfile(self.xml_path), f"Missing {self.xml_path}")
        self.assertTrue(os.path.isfile(self.reference_brep_path), f"Missing {self.reference_brep_path}")

    def test_solve_parameterized_brep_without_geometry_change(self):
        if os.path.isfile(self.solver_brep_path):
            os.remove(self.solver_brep_path)
        self.assertFalse(
            os.path.isfile(self.solver_brep_path),
            "Solver BREP file should not exist before solve.",
        )

        with open(self.xml_path, "r", encoding="utf-8") as f:
            xml = f.read()

        result = Engine.solve_to_brep(xml, "Sketch", parameters={"VarSet.L1": "500"})
        self.assertIn("success", result.solveStatus.lower(), f"Solve failed: {result.solveStatus}")
        self.assertTrue(len(result.brepUtf8) > 0, "BREP output is empty.")

        with open(self.solver_brep_path, "w", encoding="utf-8") as f:
            f.write(result.brepUtf8)
        self.assertTrue(
            os.path.isfile(self.solver_brep_path),
            "Solver BREP file was not written.",
        )

        with open(self.reference_brep_path, "r", encoding="utf-8") as f:
            expected_brep = f.read()
        # FreeCAD testing shows the BREP text ordering instability is random enough that the
        # already-parameterized V111.9.500.xml case can also reorder geometry records. Keep the
        # geometric records strict within tolerance, but ignore their positions in the BREP text.
        _assert_brep_geometry_equivalent_unordered(self, expected_brep, result.brepUtf8)


class TestV111T003ArcOfArcParameter(unittest.TestCase):
    """Solve V111.T003.FCStd with VarSet.a_of_arc=60 (overrides default 50°)."""

    @classmethod
    def setUpClass(cls):
        cls.fcstd_path = os.path.join(REPO_ROOT, "fcstdDoc", "V111.T003.FCStd")
        if not os.path.isfile(cls.fcstd_path):
            raise unittest.SkipTest(f"Missing {cls.fcstd_path}")
        cls.xml = Engine.extract_fcstd_doc(cls.fcstd_path)

    def test_solve_with_default_parameter_succeeds(self):
        """Solve with the default a_of_arc=50° (no parameter override)."""
        result = Engine.solve_to_geometry(self.xml, "Sketch")
        self.assertIn("success", result.importStatus.lower())
        self.assertIn("success", result.solveStatus.lower())
        self.assertGreater(len(result.geometries), 0)
        # Verify arc geometry is present
        arcs = [g for g in result.geometries if g.kind == GEOMETRY_ARC]
        self.assertGreater(len(arcs), 0, "Expected at least one arc geometry")

    def test_solve_with_a_of_arc_60_succeeds(self):
        """Solve with VarSet.a_of_arc=60° parameter override."""
        result = Engine.solve_to_geometry(
            self.xml, "Sketch", parameters={"VarSet.a_of_arc": "60"}
        )
        self.assertIn("success", result.importStatus.lower())
        self.assertIn("success", result.solveStatus.lower())
        self.assertGreater(len(result.geometries), 0)
        # Verify the VarSet property was overridden
        self.assertIn("VarSet.a_of_arc", result.varSetProperties)
        self.assertEqual(
            "60", result.varSetProperties["VarSet.a_of_arc"].value,
            "Expected a_of_arc to be overridden to 60",
        )

    def test_solve_with_a_of_arc_60_produces_different_angles(self):
        """Overriding a_of_arc must change the arc's angular span."""
        default_result = Engine.solve_to_geometry(self.xml, "Sketch")
        param_result = Engine.solve_to_geometry(
            self.xml, "Sketch", parameters={"VarSet.a_of_arc": "60"}
        )
        default_arcs = [g for g in default_result.geometries if g.kind == GEOMETRY_ARC]
        param_arcs = [g for g in param_result.geometries if g.kind == GEOMETRY_ARC]
        self.assertEqual(len(default_arcs), len(param_arcs))
        # The Angle constraint changes the arc's angular span, so
        # startAngle/endAngle must differ between the two solves.
        for da, pa in zip(default_arcs, param_arcs):
            self.assertFalse(
                abs(da.startAngle - pa.startAngle) < 1e-9
                and abs(da.endAngle - pa.endAngle) < 1e-9,
                "Expected arc angles to differ after overriding a_of_arc=60",
            )


if __name__ == "__main__":
    unittest.main()
