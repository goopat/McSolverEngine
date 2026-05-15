namespace McSolverEngine.Wrapper;

public enum McSolverEngineGeometryKind
{
    Point = 0,
    LineSegment = 1,
    Circle = 2,
    Arc = 3,
    Ellipse = 4,
    ArcOfEllipse = 5,
    ArcOfHyperbola = 6,
    ArcOfParabola = 7,
    BSpline = 8
}

public sealed class Point2Dto
{
    public double X { get; set; }
    public double Y { get; set; }
}

public sealed class BSplinePoleDto
{
    public Point2Dto Point { get; set; } = new();
    public double Weight { get; set; } = 1.0;
}

public sealed class BSplineKnotDto
{
    public double Value { get; set; }
    public int Multiplicity { get; set; } = 1;
}

public sealed class StructuredGeometryRecord
{
    public int GeometryIndex { get; set; } = -1;
    public int OriginalId { get; set; } = -99999999;
    public McSolverEngineGeometryKind Kind { get; set; }
    public bool Construction { get; set; }
    public bool External { get; set; }
    public bool Blocked { get; set; }
    public Point2Dto Point { get; set; } = new();
    public Point2Dto Start { get; set; } = new();
    public Point2Dto End { get; set; } = new();
    public Point2Dto Center { get; set; } = new();
    public Point2Dto Focus1 { get; set; } = new();
    public Point2Dto Vertex { get; set; } = new();
    public double Radius { get; set; }
    public double MinorRadius { get; set; }
    public double StartAngle { get; set; }
    public double EndAngle { get; set; }
    public int Degree { get; set; }
    public bool Periodic { get; set; }
    public List<BSplinePoleDto> Poles { get; set; } = [];
    public List<BSplineKnotDto> Knots { get; set; } = [];
}

public sealed class StructuredGeometrySolveResponse
{
    public McSolverEngineNativeStatus NativeStatus { get; set; }
    public string SketchName { get; set; } = string.Empty;
    public string ImportStatus { get; set; } = string.Empty;
    public int SkippedConstraints { get; set; }
    public List<string> Messages { get; set; } = [];
    public string SolveStatus { get; set; } = string.Empty;
    public int DegreesOfFreedom { get; set; }
    public List<int> Conflicting { get; set; } = [];
    public List<int> Redundant { get; set; } = [];
    public List<int> PartiallyRedundant { get; set; } = [];
    public string ExportKind { get; set; } = string.Empty;
    public string ExportStatus { get; set; } = string.Empty;
    public PlacementDto Placement { get; set; } = new();
    public List<StructuredGeometryRecord> Geometries { get; set; } = [];
}
