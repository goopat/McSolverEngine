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

public sealed class VarSetPropertyValueDto
{
    public string Value { get; set; } = string.Empty;
    public string Unit { get; set; } = string.Empty;
}

public sealed class InspectConstraintDto
{
    public int OriginalIndex { get; set; }
    public int Type { get; set; }
    public string Kind { get; set; } = string.Empty;
    public bool Driving { get; set; } = true;
    public double Value { get; set; }
    public List<int> ReferencedGeoIds { get; set; } = [];
}

public sealed class InspectGeometryElementDto
{
    public int Index { get; set; }
    public int OriginalId { get; set; }
    public string Type { get; set; } = string.Empty;
    public bool Construction { get; set; }
    public bool External { get; set; }
    public List<int> ConstraintIndices { get; set; } = [];
}

public sealed class SketchInfoDto
{
    public string Label { get; set; } = string.Empty;
    public string ObjectName { get; set; } = string.Empty;
    public List<ScalarPropertyInfoDto> Properties { get; set; } = [];
    public List<InspectGeometryElementDto> Geometries { get; set; } = [];
    public List<InspectConstraintDto> Constraints { get; set; } = [];
}

public sealed class ScalarPropertyInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Type { get; set; } = string.Empty;
    public string ScalarValue { get; set; } = string.Empty;
    public string PropertyXml { get; set; } = string.Empty;
}

public sealed class VarSetParameterInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Type { get; set; } = string.Empty;
    public string RawValue { get; set; } = string.Empty;
    public string Expression { get; set; } = string.Empty;
    public string PropertyXml { get; set; } = string.Empty;
}

public sealed class VarSetInfoDto
{
    public string Label { get; set; } = string.Empty;
    public string ObjectName { get; set; } = string.Empty;
    public List<VarSetParameterInfoDto> Parameters { get; set; } = [];
}

public sealed class DocumentInfoDto
{
    public List<SketchInfoDto> Sketches { get; set; } = [];
    public List<VarSetInfoDto> VarSets { get; set; } = [];
    public List<string> Messages { get; set; } = [];
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
    public Dictionary<string, VarSetPropertyValueDto> VarSetProperties { get; set; } = [];
    public PlacementDto Placement { get; set; } = new();
    public string? Brep { get; set; }
}
