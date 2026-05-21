namespace McSolverEngine.Wrapper;

public enum McSolverEngineNativeStatus
{
    Success = 0,
    InvalidArgument = 1,
    ImportFailed = 2,
    SolveFailed = 3,
    Unsupported = 4,
    GeometryExportFailed = 5,
    BRepExportFailed = 6,
    OpenCascadeUnavailable = 7,
    OutOfMemory = 8,
    VarSetExpressionUnsupportedSubset = 9
}

public enum McSolverEngineFCStdStatus
{
    Success = 0,
    OpenFailed = 1,
    NotZip = 2,
    XmlNotFound = 3,
    DecompressFailed = 4,
    OutOfMemory = 5
}
