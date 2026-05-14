namespace McSolverEngine.Wrapper;

public sealed class PlacementDto
{
    public double Px { get; set; }
    public double Py { get; set; }
    public double Pz { get; set; }
    public double Qx { get; set; }
    public double Qy { get; set; }
    public double Qz { get; set; }
    public double Qw { get; set; } = 1.0;
}

public sealed class BRepSolveResponse
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
    public string? Brep { get; set; }
}
