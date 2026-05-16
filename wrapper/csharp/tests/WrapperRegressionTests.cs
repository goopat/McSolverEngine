using System.IO;
using System.Linq;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Xml;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using McSolverEngine.Wrapper;

namespace McSolverEngine.Wrapper.Tests;

[TestClass]
public class WrapperRegressionTests
{
    private const string ParameterizedDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <Objects Count="2">
                <Object type="App::VarSet" name="VarSet" id="1"/>
                <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
            </Objects>
            <ObjectData Count="2">
                <Object name="VarSet">
                    <Properties Count="2" TransientCount="0">
                        <Property name="Label" type="App::PropertyString">
                            <String value="Parameters"/>
                        </Property>
                        <Property name="Width" type="App::PropertyFloat">
                            <Float value="6.0"/>
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
        """;

    private const string ParameterizedAngleDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <Objects Count="2">
                <Object type="App::VarSet" name="VarSet" id="1"/>
                <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
            </Objects>
            <ObjectData Count="2">
                <Object name="VarSet">
                    <Properties Count="2" TransientCount="0">
                        <Property name="Label" type="App::PropertyString">
                            <String value="Parameters"/>
                        </Property>
                        <Property name="Angle" type="App::PropertyQuantity">
                            <Quantity value="90 deg"/>
                        </Property>
                    </Properties>
                </Object>
                <Object name="Sketch">
                    <Properties Count="3" TransientCount="0">
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="4">
                                <Constrain Name="" Type="7" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="8" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="10.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="9" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                        <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                            <ExpressionEngine count="1">
                                <Expression path="Constraints[3]" expression="&lt;&lt;Parameters&gt;&gt;.Angle"/>
                            </ExpressionEngine>
                        </Property>
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="1">
                                <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                                    <LineSegment StartX="1.0" StartY="2.0" StartZ="0.0" EndX="4.0" EndY="6.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private const string PointwiseAngleDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <ObjectData Count="1">
                <Object name="Sketch">
                    <Properties Count="2" TransientCount="0">
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="8">
                                <Constrain Name="" Type="7" Value="0.0" First="2" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="8" Value="0.0" First="2" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="1" Value="0.0" First="2" FirstPos="1" Second="0" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="1" Value="0.0" First="2" FirstPos="1" Second="1" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="4.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="3.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="9" Value="0.7853981633974483" First="0" FirstPos="1" Second="1" SecondPos="1" Third="2" ThirdPos="1" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="3">
                                <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                                    <LineSegment StartX="1.0" StartY="1.0" StartZ="0.0" EndX="5.0" EndY="1.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                                <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                                    <LineSegment StartX="1.5" StartY="0.5" StartZ="0.0" EndX="3.0" EndY="2.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                                <Geometry type="Part::GeomPoint" id="3" migrated="1">
                                    <Point X="0.5" Y="-0.5" Z="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private const string PointwisePerpendicularDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <ObjectData Count="1">
                <Object name="Sketch">
                    <Properties Count="2" TransientCount="0">
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="6">
                                <Constrain Name="" Type="11" Value="2.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="7" Value="0.0" First="0" FirstPos="3" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="8" Value="0.0" First="0" FirstPos="3" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="7" Value="0.0" First="1" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="4.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="10" Value="0.0" First="1" FirstPos="1" Second="0" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="2">
                                <Geometry type="Part::GeomCircle" id="1" migrated="1">
                                    <Circle CenterX="1.0" CenterY="-1.0" CenterZ="0.0" NormalX="0.0" NormalY="0.0" NormalZ="1.0" Radius="3.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                                <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                                    <LineSegment StartX="-1.0" StartY="2.5" StartZ="0.0" EndX="2.0" EndY="3.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private const string BsplineKnotTangentDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <ObjectData Count="1">
                <Object name="Sketch">
                    <Properties Count="3" TransientCount="0">
                        <Property name="ExternalGeo" type="Part::PropertyGeometryList">
                            <GeometryList count="1">
                                <Geometry type="Part::GeomBSplineCurve" id="-1" migrated="1">
                                    <BSplineCurve Degree="2" IsPeriodic="0">
                                        <Pole X="0.0" Y="0.0" Weight="1.0"/>
                                        <Pole X="1.0" Y="2.0" Weight="1.0"/>
                                        <Pole X="2.0" Y="2.0" Weight="1.0"/>
                                        <Pole X="3.0" Y="0.0" Weight="1.0"/>
                                        <Pole X="4.0" Y="-1.0" Weight="1.0"/>
                                        <Knot Value="0.0" Mult="3"/>
                                        <Knot Value="1.0" Mult="1"/>
                                        <Knot Value="2.0" Mult="1"/>
                                        <Knot Value="3.0" Mult="3"/>
                                    </BSplineCurve>
                                    <Construction value="1"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="2">
                                <Geometry type="Part::GeomPoint" id="1" migrated="1">
                                    <Point X="1.2" Y="1.0" Z="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                                <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                                    <LineSegment StartX="1.0" StartY="1.5" StartZ="0.0" EndX="2.5" EndY="2.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="3">
                                <Constrain Name="" Type="15" InternalAlignmentType="10" InternalAlignmentIndex="1" Value="0.0" First="0" FirstPos="1" Second="-1" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="5" Value="0.0" First="0" FirstPos="1" Second="1" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="2.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private const string RejectedBsplineKnotTangentDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <ObjectData Count="1">
                <Object name="Sketch">
                    <Properties Count="3" TransientCount="0">
                        <Property name="ExternalGeo" type="Part::PropertyGeometryList">
                            <GeometryList count="1">
                                <Geometry type="Part::GeomBSplineCurve" id="-1" migrated="1">
                                    <BSplineCurve Degree="2" IsPeriodic="0">
                                        <Pole X="0.0" Y="0.0" Weight="1.0"/>
                                        <Pole X="1.0" Y="2.0" Weight="1.0"/>
                                        <Pole X="2.0" Y="2.0" Weight="1.0"/>
                                        <Pole X="3.0" Y="0.0" Weight="1.0"/>
                                        <Pole X="4.0" Y="-1.0" Weight="1.0"/>
                                        <Knot Value="0.0" Mult="3"/>
                                        <Knot Value="1.0" Mult="2"/>
                                        <Knot Value="2.0" Mult="3"/>
                                    </BSplineCurve>
                                    <Construction value="1"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="2">
                                <Geometry type="Part::GeomPoint" id="1" migrated="1">
                                    <Point X="1.2" Y="1.0" Z="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                                <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                                    <LineSegment StartX="1.0" StartY="1.5" StartZ="0.0" EndX="2.5" EndY="2.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="3">
                                <Constrain Name="" Type="15" InternalAlignmentType="10" InternalAlignmentIndex="1" Value="0.0" First="0" FirstPos="1" Second="-1" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="5" Value="0.0" First="0" FirstPos="1" Second="1" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="6" Value="2.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private const string BlockPreanalysisDocumentXml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <Document>
            <ObjectData Count="1">
                <Object name="Sketch">
                    <Properties Count="2" TransientCount="0">
                        <Property name="Geometry" type="Part::PropertyGeometryList">
                            <GeometryList count="1">
                                <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                                    <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="5.0" EndY="0.0" EndZ="0.0"/>
                                    <Construction value="0"/>
                                </Geometry>
                            </GeometryList>
                        </Property>
                        <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                            <ConstraintList count="2">
                                <Constrain Name="" Type="6" Value="10.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                                <Constrain Name="" Type="17" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                            </ConstraintList>
                        </Property>
                    </Properties>
                </Object>
            </ObjectData>
        </Document>
        """;

    private static string _projectRoot = string.Empty;
    private static string _workspaceRoot = string.Empty;
    private static string _sampleXmlPath = string.Empty;
    private static string _expectedBrepPath = string.Empty;
    private static string _v1021XmlPath = string.Empty;
    private static string _v1021ExpectedBrepPath = string.Empty;
    private static string _v1022XmlPath = string.Empty;
    private static string _v1022SketchExpectedBrepPath = string.Empty;
    private static string _v1022Sketch001ExpectedBrepPath = string.Empty;
    private static string _v1022Sketch002ExpectedBrepPath = string.Empty;
    private static string _v1024XmlPath = string.Empty;
    private static string _v1024ExpectedBrepPath = string.Empty;
    private static string _v1024Plus1ExpectedBrepPath = string.Empty;
    private static string _v1025XmlPath = string.Empty;
    private static string _v1025ExpectedBrepPath = string.Empty;
    private static string _v1026XmlPath = string.Empty;

    [AssemblyInitialize]
    public static void AssemblyInitialize(TestContext _)
    {
        _projectRoot = FindProjectRoot();
        _workspaceRoot = Directory.GetParent(_projectRoot)?.FullName ?? _projectRoot;
        _sampleXmlPath = Path.Combine(_projectRoot, "fcstdDoc", "1.xml");
        _expectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "1.brp");
        _v1021XmlPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.1.xml");
        _v1021ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.1.brp");
        _v1022XmlPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.2.xml");
        _v1022SketchExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.2.Sketch.Shape.brp");
        _v1022Sketch001ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.2.Sketch001.Shape.brp");
        _v1022Sketch002ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.2.Sketch002.Shape.brp");
        _v1024XmlPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.4.xml");
        _v1024ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.4.brp");
        _v1024Plus1ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.4.plus1.brp");
        _v1025XmlPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.5.xml");
        _v1025ExpectedBrepPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.5.brp");
        _v1026XmlPath = Path.Combine(_projectRoot, "fcstdDoc", "V102.6.xml");

        var nativeDirectory = FindNativeLibraryDirectory();
        var occtRuntimeDirectory = FindOcctRuntimeDirectory();

        Assert.IsTrue(
            File.Exists(Path.Combine(nativeDirectory, "mcsolverengine_native.dll")),
            $"Failed to locate mcsolverengine_native.dll in '{nativeDirectory}'."
        );
        Assert.IsTrue(File.Exists(_sampleXmlPath));
        Assert.IsTrue(File.Exists(_expectedBrepPath));
        Assert.IsTrue(File.Exists(_v1021XmlPath));
        Assert.IsTrue(File.Exists(_v1021ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1022XmlPath));
        Assert.IsTrue(File.Exists(_v1022SketchExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1022Sketch001ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1022Sketch002ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1024XmlPath));
        Assert.IsTrue(File.Exists(_v1024ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1024Plus1ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1025XmlPath));
        Assert.IsTrue(File.Exists(_v1025ExpectedBrepPath));
        Assert.IsTrue(File.Exists(_v1026XmlPath));
        Assert.IsFalse(string.IsNullOrWhiteSpace(occtRuntimeDirectory), "Failed to locate an OCCT runtime directory.");

        var currentPath = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        Environment.SetEnvironmentVariable(
            "PATH",
            string.Join(
                ";",
                new[] { nativeDirectory, occtRuntimeDirectory, currentPath }
                    .Where(value => !string.IsNullOrWhiteSpace(value))
                    .Distinct(StringComparer.OrdinalIgnoreCase)
            )
        );

        McSolverEngineClient.ConfigureNativeLibraryDirectory(nativeDirectory);
    }

    [TestMethod]
    public void GeometryRegression_ForSample1_ReturnsExpectedStructuredResult()
    {
        var documentXml = File.ReadAllText(_sampleXmlPath);

        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Sketch", result.SketchName);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);
        Assert.AreEqual("Geometry", result.ExportKind);
        Assert.AreEqual("Success", result.ExportStatus);
        Assert.AreEqual(11, result.Geometries.Count);
        Assert.AreEqual(10, result.Geometries.Count(record => record.Kind == McSolverEngineGeometryKind.LineSegment));
        Assert.AreEqual(1, result.Geometries.Count(record => record.Kind == McSolverEngineGeometryKind.Arc));
        Assert.AreEqual(0, result.Conflicting.Count);
        Assert.AreEqual(0, result.Redundant.Count);
        Assert.AreEqual(0, result.PartiallyRedundant.Count);
        Assert.AreEqual(0.0, result.Placement.Px, 1e-12);
        Assert.AreEqual(0.0, result.Placement.Py, 1e-12);
        Assert.AreEqual(0.0, result.Placement.Pz, 1e-12);
        Assert.AreEqual(1.0, result.Placement.Qw, 1e-12);
    }

    [TestMethod]
    public void BRepRegression_ForSample1_MatchesExpectedBrep()
    {
        var documentXml = File.ReadAllText(_sampleXmlPath);
        var expectedBrep = File.ReadAllText(_expectedBrepPath);

        var result = McSolverEngineClient.SolveBRepFromDocumentXml(documentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(result.Brep));
        StringAssert.Contains(result.Brep!, "CASCADE Topology V1");
        StringAssert.Contains(result.Brep!, "Locations 1");
        Assert.IsFalse(result.Brep!.Contains("Locations 0"));
        AssertBrepEquivalent(expectedBrep, result.Brep!);

        var geometryResult = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");
        Assert.AreEqual(McSolverEngineNativeStatus.Success, geometryResult.NativeStatus);
        Assert.AreEqual(ParseGeometryCount(documentXml, "Sketch"), geometryResult.Geometries.Count,
            "Geometry count mismatch for Sketch in 1.xml");
        Assert.IsTrue(
            geometryResult.Geometries.All(r => r.OriginalId > -99999999),
            "Expected all geometry OriginalId > -99999999 for 1.xml");
    }

    [TestMethod]
    public void BRepRegression_ForV1024_MatchesExpectedBrep()
    {
        var documentXml = File.ReadAllText(_v1024XmlPath);
        var expectedBrep = File.ReadAllText(_v1024ExpectedBrepPath);

        var result = McSolverEngineClient.SolveBRepFromDocumentXml(documentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(result.Brep));
        StringAssert.Contains(result.Brep!, "CASCADE Topology V1");
        StringAssert.Contains(result.Brep!, "Locations 1");
        Assert.IsFalse(result.Brep!.Contains("Locations 0"));
        AssertBrepEquivalent(expectedBrep, result.Brep!);

        var geometryResult = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");
        Assert.AreEqual(McSolverEngineNativeStatus.Success, geometryResult.NativeStatus);
        Assert.AreEqual(ParseGeometryCount(documentXml, "Sketch"), geometryResult.Geometries.Count,
            "Geometry count mismatch for Sketch in V102.4.xml");
        Assert.IsTrue(
            geometryResult.Geometries.All(r => r.OriginalId > -99999999),
            "Expected all geometry OriginalId > -99999999 for V102.4.xml");
    }

    [TestMethod]
    public void BRepRegression_ForV1024WithParameters_MatchesExpectedBrep()
    {
        var documentXml = File.ReadAllText(_v1024XmlPath);
        var expectedBrep = File.ReadAllText(_v1024Plus1ExpectedBrepPath);

        var result = McSolverEngineClient.SolveBRepFromDocumentXml(
            documentXml,
            "Sketch",
            new Dictionary<string, string> {
                ["D1"] = "61",
                ["L1"] = "41",
                ["L2"] = "61",
                ["L3"] = "11",
                ["L4"] = "16",
                ["L5"] = "21",
            }
        );

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(result.Brep));
        StringAssert.Contains(result.Brep!, "CASCADE Topology V1");
        StringAssert.Contains(result.Brep!, "Locations 1");
        Assert.IsFalse(result.Brep!.Contains("Locations 0"));
        AssertBrepEquivalent(expectedBrep, result.Brep!);

        var geometryResult = McSolverEngineClient.SolveGeometryFromDocumentXml(
            documentXml, "Sketch",
            new Dictionary<string, string> {
                ["D1"] = "61", ["L1"] = "41", ["L2"] = "61",
                ["L3"] = "11", ["L4"] = "16", ["L5"] = "21",
            });
        Assert.AreEqual(McSolverEngineNativeStatus.Success, geometryResult.NativeStatus);
        Assert.AreEqual(ParseGeometryCount(documentXml, "Sketch"), geometryResult.Geometries.Count,
            "Geometry count mismatch for Sketch (with parameters) in V102.4.xml");
        Assert.IsTrue(
            geometryResult.Geometries.All(r => r.OriginalId > -99999999),
            "Expected all geometry OriginalId > -99999999 for V102.4.xml with parameters");
    }

    [TestMethod]
    public void BRepRegression_ForV1021_MatchesExpectedBrep()
    {
        var documentXml = File.ReadAllText(_v1021XmlPath);
        var expectedBrep = File.ReadAllText(_v1021ExpectedBrepPath);

        var result = McSolverEngineClient.SolveBRepFromDocumentXml(documentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(result.Brep));
        StringAssert.Contains(result.Brep!, "CASCADE Topology V1");
        StringAssert.Contains(result.Brep!, "Locations 1");
        Assert.IsFalse(result.Brep!.Contains("Locations 0"));
        AssertBrepEquivalent(expectedBrep, result.Brep!);

        var geometryResult = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");
        Assert.AreEqual(McSolverEngineNativeStatus.Success, geometryResult.NativeStatus);
        Assert.AreEqual(ParseGeometryCount(documentXml, "Sketch"), geometryResult.Geometries.Count,
            "Geometry count mismatch for Sketch in V102.1.xml");
        Assert.IsTrue(
            geometryResult.Geometries.All(r => r.OriginalId > -99999999),
            "Expected all geometry OriginalId > -99999999 for V102.1.xml");
    }

    [TestMethod]
    public void BRepRegression_ForV1022Sketch_MatchesExpectedBrep()
    {
        AssertBrepRegression(_v1022XmlPath, "Sketch", _v1022SketchExpectedBrepPath);
    }

    [TestMethod]
    public void BRepRegression_ForV1022Sketch001_MatchesExpectedBrep()
    {
        AssertBrepRegression(_v1022XmlPath, "Sketch001", _v1022Sketch001ExpectedBrepPath);
    }

    [TestMethod]
    public void BRepRegression_ForV1022Sketch002_MatchesExpectedBrep()
    {
        AssertBrepRegression(_v1022XmlPath, "Sketch002", _v1022Sketch002ExpectedBrepPath);
    }

    [TestMethod]
    public void BRepRegression_ForV1025_MatchesExpectedBrep()
    {
        AssertBrepRegression(_v1025XmlPath, "Sketch", _v1025ExpectedBrepPath);
    }

    [TestMethod]
    public void GeometryRegression_V1025_ExpressionDrivenConstraint()
    {
        var documentXml = File.ReadAllText(_v1025XmlPath);

        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");
        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);

        // Only expression-driven constraints are exported.
        // V102.5 has one: <<VarSet>>.R1 on the Diameter constraint at originalId=17.
        var totalExprRefs = result.Geometries.Sum(r => r.Constraints.Count);
        Assert.AreEqual(1, totalExprRefs,
            $"Expected exactly 1 expression-driven constraint ref, got {totalExprRefs}.");

        var geo17 = result.Geometries.First(r => r.OriginalId == 17);
        Assert.IsTrue(
            geo17.Constraints.Any(c => c.Kind == McSolverEngineConstraintKind.Diameter
                                       && c.Expression == "<<VarSet>>.R1"),
            "Expected originalId=17 to have Diameter with expression <<VarSet>>.R1.");
    }

    [TestMethod]
    public void GeometryRegression_V1026_ExpressionDrivenConstraints()
    {
        var documentXml = File.ReadAllText(_v1026XmlPath);

        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, "Sketch");
        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);

        // V102.6 has 4 expression-driven constraint refs:
        //   originalId=2:  Angle with "VarSet.R1"
        //   originalId=2:  Distance with "VarSet001.V2L2"
        //   originalId=-1: Angle with "VarSet.R1" (external geometry)
        //   originalId=6:  Radius with "VarSet.L1 * 3"
        var totalExprRefs = result.Geometries.Sum(r => r.Constraints.Count);
        Assert.AreEqual(4, totalExprRefs,
            $"V102.6: expected 4 expression refs, got {totalExprRefs}.");

        var geo2 = result.Geometries.First(r => r.OriginalId == 2);
        Assert.IsTrue(
            geo2.Constraints.Any(c => c.Kind == McSolverEngineConstraintKind.Angle
                                      && c.Expression == "VarSet.R1"),
            "V102.6: expected originalId=2 Angle with VarSet.R1.");
        Assert.IsTrue(
            geo2.Constraints.Any(c => c.Kind == McSolverEngineConstraintKind.Distance
                                      && c.Expression == "VarSet001.V2L2"),
            "V102.6: expected originalId=2 Distance with VarSet001.V2L2.");

        var geoExt = result.Geometries.First(r => r.OriginalId == -1);
        Assert.IsTrue(
            geoExt.Constraints.Any(c => c.Kind == McSolverEngineConstraintKind.Angle
                                        && c.Expression == "VarSet.R1"),
            "V102.6: expected external geometry originalId=-1 Angle with VarSet.R1.");

        var geo6 = result.Geometries.First(r => r.OriginalId == 6);
        Assert.IsTrue(
            geo6.Constraints.Any(c => c.Kind == McSolverEngineConstraintKind.Radius
                                      && c.Expression == "VarSet.L1 * 3"),
            "V102.6: expected originalId=6 Radius with VarSet.L1 * 3.");
    }

    [TestMethod]
    public void GeometryRegression_WithParameters_UsesWrappedParameterOverrides()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(
            ParameterizedDocumentXml,
            "Sketch",
            new Dictionary<string, string> { ["Width"] = "8.5" }
        );

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);
        Assert.AreEqual(1, result.Geometries.Count);

        var line = result.Geometries.Single();
        Assert.AreEqual(McSolverEngineGeometryKind.LineSegment, line.Kind);
        Assert.AreEqual(8.5, LineLength(line), 1e-9);
        Assert.AreEqual(0.0, line.End.Y - line.Start.Y, 1e-9);
    }

    [TestMethod]
    public void BRepRegression_WithParameters_UsesWrappedParameterOverrides()
    {
        var defaultResult = McSolverEngineClient.SolveBRepFromDocumentXml(ParameterizedDocumentXml, "Sketch");
        var overriddenResult = McSolverEngineClient.SolveBRepFromDocumentXml(
            ParameterizedDocumentXml,
            "Sketch",
            new Dictionary<string, string> { ["VarSet.Width"] = "9.5" }
        );

        Assert.AreEqual(McSolverEngineNativeStatus.Success, defaultResult.NativeStatus);
        Assert.AreEqual(McSolverEngineNativeStatus.Success, overriddenResult.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(defaultResult.Brep));
        Assert.IsFalse(string.IsNullOrEmpty(overriddenResult.Brep));
        StringAssert.Contains(overriddenResult.Brep!, "CASCADE Topology V1");
        Assert.AreNotEqual(defaultResult.Brep, overriddenResult.Brep);
    }

    [TestMethod]
    public void GeometryRegression_WithAngleParameters_ConvertsDegreesBeforeSolving()
    {
        var defaultResult = McSolverEngineClient.SolveGeometryFromDocumentXml(ParameterizedAngleDocumentXml, "Sketch");
        var overriddenResult = McSolverEngineClient.SolveGeometryFromDocumentXml(
            ParameterizedAngleDocumentXml,
            "Sketch",
            new Dictionary<string, string> { ["Angle"] = "45" }
        );

        Assert.AreEqual(McSolverEngineNativeStatus.Success, defaultResult.NativeStatus);
        Assert.AreEqual(McSolverEngineNativeStatus.Success, overriddenResult.NativeStatus);
        Assert.AreEqual(90.0, LineAngleDegrees(defaultResult.Geometries.Single()), 1e-9);
        Assert.AreEqual(45.0, LineAngleDegrees(overriddenResult.Geometries.Single()), 1e-9);
        Assert.AreEqual(10.0, LineLength(defaultResult.Geometries.Single()), 1e-9);
        Assert.AreEqual(10.0, LineLength(overriddenResult.Geometries.Single()), 1e-9);
    }

    [TestMethod]
    public void GeometryRegression_WithInvalidParameterValue_RejectsNonNumericInput()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(
            ParameterizedAngleDocumentXml,
            "Sketch",
            new Dictionary<string, string> { ["Angle"] = "45 deg" }
        );

        Assert.AreEqual(McSolverEngineNativeStatus.ImportFailed, result.NativeStatus);
        Assert.AreEqual("Failed", result.ImportStatus);
        Assert.IsTrue(result.Messages.Any(message => message.Contains("must be a numeric value")));
    }

    [TestMethod]
    public void GeometryRegression_WithPointwiseAngleConstraint_UsesAngleAtPointSemantics()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(PointwiseAngleDocumentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);
        Assert.AreEqual(0, result.SkippedConstraints);
        Assert.AreEqual(3, result.Geometries.Count);

        var baseLine = result.Geometries.Single(record => record.GeometryIndex == 0);
        var angleLine = result.Geometries.Single(record => record.GeometryIndex == 1);
        var point = result.Geometries.Single(record => record.GeometryIndex == 2);

        Assert.AreEqual(McSolverEngineGeometryKind.LineSegment, baseLine.Kind);
        Assert.AreEqual(McSolverEngineGeometryKind.LineSegment, angleLine.Kind);
        Assert.AreEqual(McSolverEngineGeometryKind.Point, point.Kind);
        Assert.AreEqual(0.0, point.Point.X, 1e-9);
        Assert.AreEqual(0.0, point.Point.Y, 1e-9);
        Assert.AreEqual(0.0, baseLine.Start.X, 1e-9);
        Assert.AreEqual(0.0, baseLine.Start.Y, 1e-9);
        Assert.AreEqual(4.0, LineLength(baseLine), 1e-9);
        Assert.AreEqual(0.0, LineAngleDegrees(baseLine), 1e-9);
        Assert.AreEqual(0.0, angleLine.Start.X, 1e-9);
        Assert.AreEqual(0.0, angleLine.Start.Y, 1e-9);
        Assert.AreEqual(3.0, LineLength(angleLine), 1e-9);
        Assert.AreEqual(45.0, LineAngleDegrees(angleLine), 1e-9);
    }

    [TestMethod]
    public void GeometryRegression_WithPointwisePerpendicularConstraint_UsesAngleAtPointSemantics()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(PointwisePerpendicularDocumentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);
        Assert.AreEqual(0, result.SkippedConstraints);
        Assert.AreEqual(2, result.Geometries.Count);

        var line = result.Geometries.Single(record => record.GeometryIndex == 1);

        Assert.AreEqual(McSolverEngineGeometryKind.LineSegment, line.Kind);
        Assert.AreEqual(0.0, line.Start.X, 1e-9);
        Assert.AreEqual(2.0, Math.Abs(line.Start.Y), 1e-9);
        Assert.AreEqual(4.0, LineLength(line), 1e-9);
        Assert.AreEqual(0.0, line.End.X - line.Start.X, 1e-9);
    }

    [TestMethod]
    public void GeometryRegression_WithBsplineKnotTangentConstraint_UsesSpecialCase()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(BsplineKnotTangentDocumentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);
        Assert.AreEqual(0, result.SkippedConstraints);

        var point = result.Geometries.Single(record => record.Kind == McSolverEngineGeometryKind.Point);
        var line = result.Geometries.Single(record => record.Kind == McSolverEngineGeometryKind.LineSegment);

        Assert.AreEqual(2.0, LineLength(line), 1e-9);
        Assert.AreEqual(point.Point.X, line.Start.X, 1e-6);
        Assert.AreEqual(point.Point.Y, line.Start.Y, 1e-6);
    }

    [TestMethod]
    public void GeometryRegression_WithDiscontinuousBsplineKnotTangent_ReturnsUnsupported()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(RejectedBsplineKnotTangentDocumentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Unsupported, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Unsupported", result.SolveStatus);
    }

    [TestMethod]
    public void GeometryRegression_WithBlockAfterDrivingConstraint_PreservesBlockedGeometry()
    {
        var result = McSolverEngineClient.SolveGeometryFromDocumentXml(BlockPreanalysisDocumentXml, "Sketch");

        Assert.AreEqual(McSolverEngineNativeStatus.Success, result.NativeStatus);
        Assert.AreEqual("Success", result.ImportStatus);
        Assert.AreEqual("Success", result.SolveStatus);

        var line = result.Geometries.Single(record => record.Kind == McSolverEngineGeometryKind.LineSegment);
        Assert.AreEqual(5.0, LineLength(line), 1e-9);
        Assert.AreEqual(0.0, line.Start.X, 1e-9);
        Assert.AreEqual(0.0, line.Start.Y, 1e-9);
        Assert.AreEqual(5.0, line.End.X, 1e-9);
        Assert.AreEqual(0.0, line.End.Y, 1e-9);
    }

    private static void AssertBrepEquivalent(string expected, string actual)
    {
        var expectedTokens = TokenizeBrep(expected);
        var actualTokens = TokenizeBrep(actual);

        Assert.AreEqual(expectedTokens.Length, actualTokens.Length, "BREP token count changed.");

        for (var index = 0; index < expectedTokens.Length; index++) {
            var expectedToken = expectedTokens[index];
            var actualToken = actualTokens[index];

            if (TryParseFloatingToken(expectedToken, out var expectedNumber)
                && TryParseFloatingToken(actualToken, out var actualNumber)) {
                Assert.AreEqual(expectedNumber, actualNumber, 1e-9, $"BREP numeric token mismatch at index {index}.");
                continue;
            }

            Assert.AreEqual(expectedToken, actualToken, $"BREP token mismatch at index {index}.");
        }
    }

    private static void AssertBrepRegression(string xmlPath, string sketchName, string expectedBrepPath)
    {
        var documentXml = File.ReadAllText(xmlPath);
        var expectedBrep = File.ReadAllText(expectedBrepPath);

        var brepResult = McSolverEngineClient.SolveBRepFromDocumentXml(documentXml, sketchName);

        Assert.AreEqual(McSolverEngineNativeStatus.Success, brepResult.NativeStatus);
        Assert.IsFalse(string.IsNullOrEmpty(brepResult.Brep));
        StringAssert.Contains(brepResult.Brep!, "CASCADE Topology V1");
        StringAssert.Contains(brepResult.Brep!, "Locations 1");
        Assert.IsFalse(brepResult.Brep!.Contains("Locations 0"));
        AssertBrepEquivalent(expectedBrep, brepResult.Brep!);

        var geometryResult = McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, sketchName);
        Assert.AreEqual(McSolverEngineNativeStatus.Success, geometryResult.NativeStatus);
        var expectedCount = ParseGeometryCount(documentXml, sketchName);
        Assert.AreEqual(expectedCount, geometryResult.Geometries.Count,
            $"Geometry count mismatch for {sketchName} in {xmlPath}");
        Assert.IsTrue(
            geometryResult.Geometries.All(r => r.OriginalId > -99999999),
            $"Expected all geometry OriginalId > -99999999 for {sketchName} in {xmlPath}");
    }

    private static int ParseGeometryCount(string documentXml, string sketchName)
    {
        var doc = new XmlDocument();
        doc.LoadXml(documentXml);
        var objectNode = doc.SelectSingleNode(
            $"/Document/ObjectData/Object[@name=\"{sketchName}\"]");
        if (objectNode == null) {
            throw new InvalidOperationException($"Sketch object '{sketchName}' not found in document XML.");
        }
        var totalCount = 0;
        foreach (var propertyName in new[] { "ExternalGeometry", "ExternalGeo", "Geometry" }) {
            var geomListNode = objectNode.SelectSingleNode(
                $"Properties/Property[@name=\"{propertyName}\"]/GeometryList");
            if (geomListNode == null) {
                continue;
            }
            var countAttr = geomListNode.Attributes?["count"];
            if (countAttr != null && int.TryParse(countAttr.Value, out var partCount)) {
                totalCount += partCount;
            }
        }
        if (totalCount == 0) {
            throw new InvalidOperationException($"No geometry count found for '{sketchName}'.");
        }
        return totalCount;
    }

    private static string[] TokenizeBrep(string value)
    {
        return value
            .Replace("\r\n", "\n")
            .Split(new[] { ' ', '\t', '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
    }

    private static bool TryParseFloatingToken(string value, out double parsed)
    {
        return double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out parsed);
    }

    private static double LineLength(StructuredGeometryRecord record)
    {
        var dx = record.End.X - record.Start.X;
        var dy = record.End.Y - record.Start.Y;
        return Math.Sqrt(dx * dx + dy * dy);
    }

    private static double LineAngleDegrees(StructuredGeometryRecord record)
    {
        var dx = record.End.X - record.Start.X;
        var dy = record.End.Y - record.Start.Y;
        return Math.Atan2(dy, dx) * 180.0 / Math.PI;
    }

    private static string FindProjectRoot()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);
        while (current != null) {
            if (File.Exists(Path.Combine(current.FullName, "fcstdDoc", "1.xml"))
                && File.Exists(Path.Combine(current.FullName, "CMakeLists.txt"))) {
                return current.FullName;
            }

            var nestedProject = Path.Combine(current.FullName, "McSolverEngine");
            if (File.Exists(Path.Combine(nestedProject, "fcstdDoc", "1.xml"))
                && File.Exists(Path.Combine(nestedProject, "CMakeLists.txt"))) {
                return nestedProject;
            }
            current = current.Parent;
        }

        throw new DirectoryNotFoundException("Failed to locate the McSolverEngine project root from the test base directory.");
    }

    private static string FindNativeLibraryDirectory()
    {
        var configuredPath = Environment.GetEnvironmentVariable("MCSOLVERENGINE_NATIVE_PATH");
        var configuredDirectory = ResolveNativeLibraryDirectory(configuredPath);
        if (!string.IsNullOrWhiteSpace(configuredDirectory)) {
            return configuredDirectory!;
        }

        foreach (var configuration in GetPreferredBuildConfigurations()) {
            foreach (var candidate in new[] {
                         AppContext.BaseDirectory,
                         Path.Combine(_projectRoot, "build", configuration),
                         Path.Combine(_workspaceRoot, "build", "mcsolverengine", configuration),
                     }) {
                if (HasNativeLibrary(candidate)) {
                    return candidate;
                }
            }
        }

        throw new DirectoryNotFoundException("Failed to locate a build output directory containing mcsolverengine_native.dll.");
    }

    private static string? FindOcctRuntimeDirectory()
    {
        var configuredDirectory = ResolveOcctRuntimeDirectory(Environment.GetEnvironmentVariable("MCSOLVERENGINE_OCCT_RUNTIME_DIR"));
        if (!string.IsNullOrWhiteSpace(configuredDirectory)) {
            return configuredDirectory;
        }

        var currentPath = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var pathEntry in currentPath.Split(new[] { Path.PathSeparator }, StringSplitOptions.RemoveEmptyEntries)) {
            var candidate = ResolveOcctRuntimeDirectory(pathEntry);
            if (!string.IsNullOrWhiteSpace(candidate)) {
                return candidate;
            }
        }

        foreach (var candidate in new[] {
                     Path.Combine(_projectRoot, ".pixi", "envs", "default", "Library", "bin"),
                     Path.Combine(_workspaceRoot, ".pixi", "envs", "default", "Library", "bin"),
                     @"D:\work\bim2025-client\packages\opencascade.7.9-native.1.0.0\lib\build\bin",
                     @"D:\work\bim2025-client\packages\opencascade.7.9-native.1.0.0\lib\build\bind",
                 }) {
            var runtimeDirectory = ResolveOcctRuntimeDirectory(candidate);
            if (!string.IsNullOrWhiteSpace(runtimeDirectory)) {
                return runtimeDirectory;
            }
        }

        return null;
    }

    private static IEnumerable<string> GetPreferredBuildConfigurations()
    {
#if DEBUG
        yield return "Debug";
        yield return "Release";
#else
        yield return "Release";
        yield return "Debug";
#endif
    }

    private static string? ResolveNativeLibraryDirectory(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) {
            return null;
        }

        if (File.Exists(value)) {
            return Path.GetDirectoryName(value);
        }

        return HasNativeLibrary(value) ? value : null;
    }

    private static string? ResolveOcctRuntimeDirectory(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) {
            return null;
        }

        if (File.Exists(value) && string.Equals(Path.GetFileName(value), OcctRuntimeFileName, StringComparison.OrdinalIgnoreCase)) {
            return Path.GetDirectoryName(value);
        }

        return HasOcctRuntime(value) ? value : null;
    }

#if NET6_0_OR_GREATER
    private static string NativeLibraryFileName =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "mcsolverengine_native.dll"
        : RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? "libmcsolverengine_native.so"
        : RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "libmcsolverengine_native.dylib"
        : "mcsolverengine_native";

    private static string OcctRuntimeFileName =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "TKBRep.dll"
        : RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? "libTKBRep.so"
        : RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "libTKBRep.dylib"
        : "TKBRep";
#else
    private const string NativeLibraryFileName = "mcsolverengine_native.dll";
    private const string OcctRuntimeFileName = "TKBRep.dll";
#endif

    private static bool HasNativeLibrary(string? directory)
    {
        return !string.IsNullOrWhiteSpace(directory)
            && File.Exists(Path.Combine(directory, NativeLibraryFileName));
    }

    private static bool HasOcctRuntime(string? directory)
    {
        return !string.IsNullOrWhiteSpace(directory)
            && File.Exists(Path.Combine(directory, OcctRuntimeFileName));
    }
}
