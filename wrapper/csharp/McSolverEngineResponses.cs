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

    public string? Brep { get; set; }
}
