using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace McSolverEngine.Wrapper;

public static class McSolverEngineClient
{
    private const string NativeLibraryName = "mcsolverengine_native";
    private static readonly object NativeLibraryPathLock = new();
    private static readonly object NativeLibraryLoadLock = new();
    private static string? _configuredNativeLibraryPath;
    private static string? _configuredNativeLibraryDirectory;
    private static IntPtr _loadedNativeLibraryHandle;

#if NET6_0_OR_GREATER
    private static string NativeLibraryFileName
    {
        get
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                return "mcsolverengine_native.dll";
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                return "libmcsolverengine_native.so";
            if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
                return "libmcsolverengine_native.dylib";
            return NativeLibraryName;
        }
    }
#endif

    public static void ConfigureNativeLibrary(string nativeLibraryPath)
    {
        ValidateRequiredString(nativeLibraryPath, nameof(nativeLibraryPath));
        lock (NativeLibraryPathLock) {
            _configuredNativeLibraryPath = nativeLibraryPath;
            _configuredNativeLibraryDirectory = null;
        }
    }

    public static void ConfigureNativeLibraryDirectory(string nativeLibraryDirectory)
    {
        ValidateRequiredString(nativeLibraryDirectory, nameof(nativeLibraryDirectory));
        lock (NativeLibraryPathLock) {
            _configuredNativeLibraryDirectory = nativeLibraryDirectory;
            _configuredNativeLibraryPath = null;
        }
    }

    public static string GetVersion()
    {
        EnsureNativeLibraryLoaded();
        var versionPointer = McSolverEngine_GetVersion();
        return ReadNativeUtf8String(versionPointer);
    }

    public static StructuredGeometrySolveResponse SolveGeometryFromDocumentXml(
        string documentXml,
        string? sketchName = null
    )
    {
        ValidateRequiredObject(documentXml, nameof(documentXml));
        EnsureNativeLibraryLoaded();
        using var documentXmlHandle = new NativeUtf8String(documentXml);
        using var sketchNameHandle = new NativeUtf8String(sketchName);

        var nativeStatus = McSolverEngine_SolveToGeometry(
            documentXmlHandle.Pointer,
            sketchNameHandle.Pointer,
            out var resultPointer
        );

        return ParseStructuredGeometryResponse(nativeStatus, resultPointer);
    }

    public static StructuredGeometrySolveResponse SolveGeometryFromDocumentXml(
        string documentXml,
        string? sketchName,
        IReadOnlyDictionary<string, string> parameters
    )
    {
        ValidateRequiredObject(documentXml, nameof(documentXml));
        ValidateRequiredObject(parameters, nameof(parameters));
        if (parameters.Count == 0) {
            return SolveGeometryFromDocumentXml(documentXml, sketchName);
        }

        EnsureNativeLibraryLoaded();
        using var documentXmlHandle = new NativeUtf8String(documentXml);
        using var sketchNameHandle = new NativeUtf8String(sketchName);
        using var parameterMapHandle = new NativeParameterMap(parameters);

        var nativeStatus = McSolverEngine_SolveToGeometryWithParameters(
            documentXmlHandle.Pointer,
            sketchNameHandle.Pointer,
            parameterMapHandle.KeysPointer,
            parameterMapHandle.ValuesPointer,
            parameterMapHandle.Count,
            out var resultPointer
        );

        return ParseStructuredGeometryResponse(nativeStatus, resultPointer);
    }

    public static BRepSolveResponse SolveBRepFromDocumentXml(string documentXml, string? sketchName = null)
    {
        ValidateRequiredObject(documentXml, nameof(documentXml));
        EnsureNativeLibraryLoaded();
        using var documentXmlHandle = new NativeUtf8String(documentXml);
        using var sketchNameHandle = new NativeUtf8String(sketchName);
        var nativeStatus = McSolverEngine_SolveToBRep(
            documentXmlHandle.Pointer,
            sketchNameHandle.Pointer,
            out var resultPointer
        );
        return ParseBRepResponse(nativeStatus, resultPointer);
    }

    public static BRepSolveResponse SolveBRepFromDocumentXml(
        string documentXml,
        string? sketchName,
        IReadOnlyDictionary<string, string> parameters
    )
    {
        ValidateRequiredObject(documentXml, nameof(documentXml));
        ValidateRequiredObject(parameters, nameof(parameters));
        if (parameters.Count == 0) {
            return SolveBRepFromDocumentXml(documentXml, sketchName);
        }

        EnsureNativeLibraryLoaded();
        using var documentXmlHandle = new NativeUtf8String(documentXml);
        using var sketchNameHandle = new NativeUtf8String(sketchName);
        using var parameterMapHandle = new NativeParameterMap(parameters);
        var nativeStatus = McSolverEngine_SolveToBRepWithParameters(
            documentXmlHandle.Pointer,
            sketchNameHandle.Pointer,
            parameterMapHandle.KeysPointer,
            parameterMapHandle.ValuesPointer,
            parameterMapHandle.Count,
            out var resultPointer
        );
        return ParseBRepResponse(nativeStatus, resultPointer);
    }

    private static StructuredGeometrySolveResponse ParseStructuredGeometryResponse(
        McSolverEngineNativeStatus nativeStatus,
        IntPtr resultPointer
    )
    {
        var response = new StructuredGeometrySolveResponse {
            NativeStatus = nativeStatus
        };

        if (resultPointer == IntPtr.Zero) {
            return response;
        }

        try {
            var nativeResult = (NativeGeometryResult)Marshal.PtrToStructure(
                resultPointer,
                typeof(NativeGeometryResult)
            )!;
            response.SketchName = ReadNativeUtf8String(nativeResult.SketchName);
            response.ImportStatus = ReadNativeUtf8String(nativeResult.ImportStatus);
            response.SkippedConstraints = nativeResult.SkippedConstraints;
            response.Messages = ReadStringArray(nativeResult.Messages, nativeResult.MessageCount);
            response.SolveStatus = ReadNativeUtf8String(nativeResult.SolveStatus);
            response.DegreesOfFreedom = nativeResult.DegreesOfFreedom;
            response.Conflicting = ReadIntArray(nativeResult.Conflicting, nativeResult.ConflictingCount);
            response.Redundant = ReadIntArray(nativeResult.Redundant, nativeResult.RedundantCount);
            response.PartiallyRedundant = ReadIntArray(
                nativeResult.PartiallyRedundant,
                nativeResult.PartiallyRedundantCount
            );
            response.ExportKind = ReadNativeUtf8String(nativeResult.ExportKind);
            response.ExportStatus = ReadNativeUtf8String(nativeResult.ExportStatus);
            response.Placement = ToPlacementDto(nativeResult.Placement);
            response.Geometries = ReadGeometryRecords(nativeResult.Geometries, nativeResult.GeometryCount);
            return response;
        }
        finally {
            McSolverEngine_FreeGeometryResult(resultPointer);
        }
    }

    private static BRepSolveResponse ParseBRepResponse(
        McSolverEngineNativeStatus nativeStatus,
        IntPtr resultPointer
    )
    {
        var response = new BRepSolveResponse {
            NativeStatus = nativeStatus
        };

        if (resultPointer == IntPtr.Zero) {
            return response;
        }

        try {
            var nativeResult = (NativeBRepResult)Marshal.PtrToStructure(
                resultPointer,
                typeof(NativeBRepResult)
            )!;
            response.SketchName = ReadNativeUtf8String(nativeResult.SketchName);
            response.ImportStatus = ReadNativeUtf8String(nativeResult.ImportStatus);
            response.SkippedConstraints = nativeResult.SkippedConstraints;
            response.Messages = ReadStringArray(nativeResult.Messages, nativeResult.MessageCount);
            response.SolveStatus = ReadNativeUtf8String(nativeResult.SolveStatus);
            response.DegreesOfFreedom = nativeResult.DegreesOfFreedom;
            response.Conflicting = ReadIntArray(nativeResult.Conflicting, nativeResult.ConflictingCount);
            response.Redundant = ReadIntArray(nativeResult.Redundant, nativeResult.RedundantCount);
            response.PartiallyRedundant = ReadIntArray(
                nativeResult.PartiallyRedundant,
                nativeResult.PartiallyRedundantCount
            );
            response.ExportKind = ReadNativeUtf8String(nativeResult.ExportKind);
            response.ExportStatus = ReadNativeUtf8String(nativeResult.ExportStatus);
            response.Placement = ToPlacementDto(nativeResult.Placement);
            response.Brep = ReadNativeUtf8String(nativeResult.BrepUtf8);
            return response;
        }
        finally {
            McSolverEngine_FreeBRepResult(resultPointer);
        }
    }

    private static void EnsureNativeLibraryLoaded()
    {
        if (_loadedNativeLibraryHandle != IntPtr.Zero) {
            return;
        }

        lock (NativeLibraryLoadLock) {
            if (_loadedNativeLibraryHandle != IntPtr.Zero) {
                return;
            }

            var candidate = GetConfiguredLibraryCandidate();
#if NET6_0_OR_GREATER
            if (!string.IsNullOrWhiteSpace(candidate)) {
                var candidatePath = candidate!;
                _loadedNativeLibraryHandle = NativeLibrary.Load(candidatePath);
                if (_loadedNativeLibraryHandle != IntPtr.Zero) {
                    return;
                }
                throw CreateDllNotFoundException(candidatePath);
            }

            _loadedNativeLibraryHandle = NativeLibrary.Load(NativeLibraryFileName);
            if (_loadedNativeLibraryHandle == IntPtr.Zero) {
                throw CreateDllNotFoundException(NativeLibraryFileName);
            }
#else
            if (!string.IsNullOrWhiteSpace(candidate)) {
                var candidatePath = candidate!;
                SetSearchDirectory(Path.GetDirectoryName(candidatePath));
                _loadedNativeLibraryHandle = LoadLibrary(candidatePath!);
                if (_loadedNativeLibraryHandle != IntPtr.Zero) {
                    return;
                }
                throw CreateDllNotFoundException(candidatePath);
            }

            _loadedNativeLibraryHandle = LoadLibrary(NativeLibraryName);
            if (_loadedNativeLibraryHandle == IntPtr.Zero) {
                throw CreateDllNotFoundException(NativeLibraryName);
            }
#endif
        }
    }

    private static string ReadNativeUtf8String(IntPtr pointer)
    {
        if (pointer == IntPtr.Zero) {
            return string.Empty;
        }

        var length = 0;
        while (Marshal.ReadByte(pointer, length) != 0) {
            ++length;
        }

        if (length == 0) {
            return string.Empty;
        }

        var bytes = new byte[length];
        Marshal.Copy(pointer, bytes, 0, length);
        return Encoding.UTF8.GetString(bytes);
    }

    private static PlacementDto ToPlacementDto(NativePlacement placement)
    {
        return new PlacementDto {
            Px = placement.Px,
            Py = placement.Py,
            Pz = placement.Pz,
            Qx = placement.Qx,
            Qy = placement.Qy,
            Qz = placement.Qz,
            Qw = placement.Qw
        };
    }

    private static Point2Dto ToPoint2Dto(NativePoint2 point)
    {
        return new Point2Dto {
            X = point.X,
            Y = point.Y
        };
    }

    private static List<StructuredGeometryRecord> ReadGeometryRecords(IntPtr pointer, int count)
    {
        var geometries = new List<StructuredGeometryRecord>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return geometries;
        }

        var stride = Marshal.SizeOf(typeof(NativeGeometryRecord));
        for (var i = 0; i < count; ++i) {
            var current = IntPtr.Add(pointer, i * stride);
            var nativeRecord = (NativeGeometryRecord)Marshal.PtrToStructure(
                current,
                typeof(NativeGeometryRecord)
            )!;

            geometries.Add(new StructuredGeometryRecord {
                GeometryIndex = nativeRecord.GeometryIndex,
                OriginalId = nativeRecord.OriginalId,
                Kind = nativeRecord.Kind,
                Construction = nativeRecord.Construction != 0,
                External = nativeRecord.External != 0,
                Blocked = nativeRecord.Blocked != 0,
                Point = ToPoint2Dto(nativeRecord.Point),
                Start = ToPoint2Dto(nativeRecord.Start),
                End = ToPoint2Dto(nativeRecord.End),
                Center = ToPoint2Dto(nativeRecord.Center),
                Focus1 = ToPoint2Dto(nativeRecord.Focus1),
                Vertex = ToPoint2Dto(nativeRecord.Vertex),
                Radius = nativeRecord.Radius,
                MinorRadius = nativeRecord.MinorRadius,
                StartAngle = nativeRecord.StartAngle,
                EndAngle = nativeRecord.EndAngle,
                Degree = nativeRecord.Degree,
                Periodic = nativeRecord.Periodic != 0,
                Poles = ReadBSplinePoles(nativeRecord.Poles, nativeRecord.PoleCount),
                Knots = ReadBSplineKnots(nativeRecord.Knots, nativeRecord.KnotCount),
                Constraints = ReadConstraintRefs(nativeRecord.Constraints, nativeRecord.ConstraintCount)
            });
        }

        return geometries;
    }

    private static List<BSplinePoleDto> ReadBSplinePoles(IntPtr pointer, int count)
    {
        var poles = new List<BSplinePoleDto>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return poles;
        }

        var stride = Marshal.SizeOf(typeof(NativeBSplinePole));
        for (var i = 0; i < count; ++i) {
            var current = IntPtr.Add(pointer, i * stride);
            var nativePole = (NativeBSplinePole)Marshal.PtrToStructure(current, typeof(NativeBSplinePole))!;
            poles.Add(new BSplinePoleDto {
                Point = ToPoint2Dto(nativePole.Point),
                Weight = nativePole.Weight
            });
        }

        return poles;
    }

    private static List<BSplineKnotDto> ReadBSplineKnots(IntPtr pointer, int count)
    {
        var knots = new List<BSplineKnotDto>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return knots;
        }

        var stride = Marshal.SizeOf(typeof(NativeBSplineKnot));
        for (var i = 0; i < count; ++i) {
            var current = IntPtr.Add(pointer, i * stride);
            var nativeKnot = (NativeBSplineKnot)Marshal.PtrToStructure(current, typeof(NativeBSplineKnot))!;
            knots.Add(new BSplineKnotDto {
                Value = nativeKnot.Value,
                Multiplicity = nativeKnot.Multiplicity
            });
        }

        return knots;
    }

    private static List<string> ReadStringArray(IntPtr pointer, int count)
    {
        var values = new List<string>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return values;
        }

        for (var i = 0; i < count; ++i) {
            var itemPointer = Marshal.ReadIntPtr(pointer, i * IntPtr.Size);
            values.Add(ReadNativeUtf8String(itemPointer));
        }

        return values;
    }

    private static List<int> ReadIntArray(IntPtr pointer, int count)
    {
        var values = new List<int>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return values;
        }

        for (var i = 0; i < count; ++i) {
            values.Add(Marshal.ReadInt32(pointer, i * sizeof(int)));
        }

        return values;
    }

    private static List<ConstraintRefDto> ReadConstraintRefs(IntPtr pointer, int count)
    {
        var refs = new List<ConstraintRefDto>();
        if (pointer == IntPtr.Zero || count <= 0) {
            return refs;
        }

        var stride = Marshal.SizeOf(typeof(NativeConstraintRef));
        for (var i = 0; i < count; ++i) {
            var current = IntPtr.Add(pointer, i * stride);
            var nativeRef = (NativeConstraintRef)Marshal.PtrToStructure(current, typeof(NativeConstraintRef))!;
            refs.Add(new ConstraintRefDto {
                Kind = (McSolverEngineConstraintKind)nativeRef.Kind,
                Expression = ReadNativeUtf8String(nativeRef.Expression)
            });
        }

        return refs;
    }

    private static string? GetConfiguredLibraryCandidate()
    {
        string? configuredPath;
        string? configuredDirectory;
        lock (NativeLibraryPathLock) {
            configuredPath = _configuredNativeLibraryPath;
            configuredDirectory = _configuredNativeLibraryDirectory;
        }

        if (IsExistingFile(configuredPath)) {
            return configuredPath;
        }

        var environmentPath = GetEnvironmentConfiguredPath();
        if (IsExistingFile(environmentPath)) {
            return environmentPath;
        }

        var configuredDirectoryCandidate = CombineDirectoryCandidate(configuredDirectory);
        if (IsExistingFile(configuredDirectoryCandidate)) {
            return configuredDirectoryCandidate;
        }

        var baseDirectoryCandidate = CombineDirectoryCandidate(AppContext.BaseDirectory);
        return IsExistingFile(baseDirectoryCandidate) ? baseDirectoryCandidate : null;
    }

    private static string? GetEnvironmentConfiguredPath()
    {
        var value = Environment.GetEnvironmentVariable("MCSOLVERENGINE_NATIVE_PATH");
        if (string.IsNullOrWhiteSpace(value)) {
            return null;
        }

        if (Directory.Exists(value)) {
            return CombineDirectoryCandidate(value);
        }

        return value;
    }

    private static string CombineDirectoryCandidate(string? directory)
    {
        return Path.Combine(directory ?? string.Empty, GetPlatformLibraryFileName());
    }

    private static string GetPlatformLibraryFileName()
    {
        return $"{NativeLibraryName}.dll";
    }

    private static bool IsExistingFile(string? candidate)
    {
        return !string.IsNullOrWhiteSpace(candidate) && File.Exists(candidate);
    }

    private static void SetSearchDirectory(string? directory)
    {
#if !NET6_0_OR_GREATER
        var directoryPath = directory;
        if (!string.IsNullOrWhiteSpace(directoryPath)) {
            SetDllDirectory(directoryPath!);
        }
#endif
    }

    private static Exception CreateDllNotFoundException(string libraryPath)
    {
        return new DllNotFoundException(
            string.Format(
                "Unable to load native library '{0}': {1}",
                libraryPath,
                new Win32Exception(Marshal.GetLastWin32Error()).Message
            )
        );
    }

    private static void ValidateRequiredString(string? value, string parameterName)
    {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Value cannot be null or whitespace.", parameterName);
        }
    }

    private static void ValidateRequiredObject(object? value, string parameterName)
    {
        if (value == null) {
            throw new ArgumentNullException(parameterName);
        }
    }

    private sealed class NativeUtf8String : IDisposable
    {
        public NativeUtf8String(string? value)
        {
            if (value == null) {
                Pointer = IntPtr.Zero;
                return;
            }

            var bytes = Encoding.UTF8.GetBytes(value);
            Pointer = Marshal.AllocHGlobal(bytes.Length + 1);
            Marshal.Copy(bytes, 0, Pointer, bytes.Length);
            Marshal.WriteByte(Pointer, bytes.Length, 0);
        }

        public IntPtr Pointer { get; }

        public void Dispose()
        {
            if (Pointer != IntPtr.Zero) {
                Marshal.FreeHGlobal(Pointer);
            }
        }
    }

    private sealed class NativeParameterMap : IDisposable
    {
        private readonly NativeUtf8String[] _keys;
        private readonly NativeUtf8String[] _values;

        public NativeParameterMap(IReadOnlyDictionary<string, string> parameters)
        {
            Count = parameters.Count;
            if (Count == 0) {
                KeysPointer = IntPtr.Zero;
                ValuesPointer = IntPtr.Zero;
                _keys = [];
                _values = [];
                return;
            }

            _keys = new NativeUtf8String[Count];
            _values = new NativeUtf8String[Count];

            KeysPointer = Marshal.AllocHGlobal(IntPtr.Size * Count);
            ValuesPointer = Marshal.AllocHGlobal(IntPtr.Size * Count);

            try {
                var index = 0;
                foreach (var parameter in parameters) {
                    ValidateRequiredString(parameter.Key, nameof(parameters));
                    ValidateRequiredString(parameter.Value, nameof(parameters));

                    _keys[index] = new NativeUtf8String(parameter.Key);
                    _values[index] = new NativeUtf8String(parameter.Value);

                    Marshal.WriteIntPtr(KeysPointer, index * IntPtr.Size, _keys[index].Pointer);
                    Marshal.WriteIntPtr(ValuesPointer, index * IntPtr.Size, _values[index].Pointer);
                    ++index;
                }
            }
            catch {
                Dispose();
                throw;
            }
        }

        public IntPtr KeysPointer { get; }

        public IntPtr ValuesPointer { get; }

        public int Count { get; }

        public void Dispose()
        {
            foreach (var key in _keys) {
                key?.Dispose();
            }
            foreach (var value in _values) {
                value?.Dispose();
            }

            if (KeysPointer != IntPtr.Zero) {
                Marshal.FreeHGlobal(KeysPointer);
            }
            if (ValuesPointer != IntPtr.Zero) {
                Marshal.FreeHGlobal(ValuesPointer);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativePoint2
    {
        public double X;
        public double Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativePlacement
    {
        public double Px;
        public double Py;
        public double Pz;
        public double Qx;
        public double Qy;
        public double Qz;
        public double Qw;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeBSplinePole
    {
        public NativePoint2 Point;
        public double Weight;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeBSplineKnot
    {
        public double Value;
        public int Multiplicity;
    }

    private enum NativeConstraintKind
    {
        Coincident = 0,
        Horizontal = 1,
        Vertical = 2,
        DistanceX = 3,
        DistanceY = 4,
        Distance = 5,
        Parallel = 6,
        Tangent = 7,
        Perpendicular = 8,
        Angle = 9,
        Radius = 10,
        Diameter = 11,
        Equal = 12,
        Symmetric = 13,
        PointOnObject = 14,
        InternalAlignment = 15,
        SnellsLaw = 16,
        Block = 17,
        Weight = 18,
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeConstraintRef
    {
        public NativeConstraintKind Kind;
        public IntPtr Expression;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeGeometryRecord
    {
        public int GeometryIndex;
        public int OriginalId;
        public McSolverEngineGeometryKind Kind;
        public int Construction;
        public int External;
        public int Blocked;
        public NativePoint2 Point;
        public NativePoint2 Start;
        public NativePoint2 End;
        public NativePoint2 Center;
        public NativePoint2 Focus1;
        public NativePoint2 Vertex;
        public double Radius;
        public double MinorRadius;
        public double StartAngle;
        public double EndAngle;
        public int Degree;
        public int Periodic;
        public int PoleCount;
        public IntPtr Poles;
        public int KnotCount;
        public IntPtr Knots;
        public int ConstraintCount;
        public IntPtr Constraints;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeGeometryResult
    {
        public IntPtr SketchName;
        public IntPtr ImportStatus;
        public int SkippedConstraints;
        public int MessageCount;
        public IntPtr Messages;
        public IntPtr SolveStatus;
        public int DegreesOfFreedom;
        public int ConflictingCount;
        public IntPtr Conflicting;
        public int RedundantCount;
        public IntPtr Redundant;
        public int PartiallyRedundantCount;
        public IntPtr PartiallyRedundant;
        public IntPtr ExportKind;
        public IntPtr ExportStatus;
        public NativePlacement Placement;
        public int GeometryCount;
        public IntPtr Geometries;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeBRepResult
    {
        public IntPtr SketchName;
        public IntPtr ImportStatus;
        public int SkippedConstraints;
        public int MessageCount;
        public IntPtr Messages;
        public IntPtr SolveStatus;
        public int DegreesOfFreedom;
        public int ConflictingCount;
        public IntPtr Conflicting;
        public int RedundantCount;
        public IntPtr Redundant;
        public int PartiallyRedundantCount;
        public IntPtr PartiallyRedundant;
        public IntPtr ExportKind;
        public IntPtr ExportStatus;
        public NativePlacement Placement;
        public IntPtr BrepUtf8;
    }

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern IntPtr McSolverEngine_GetVersion();

#if !NET6_0_OR_GREATER
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, EntryPoint = "LoadLibraryW")]
    private static extern IntPtr LoadLibrary(string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, EntryPoint = "SetDllDirectoryW")]
    private static extern bool SetDllDirectory(string pathName);
#endif

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern McSolverEngineNativeStatus McSolverEngine_SolveToGeometry(
        IntPtr documentXmlUtf8,
        IntPtr sketchNameUtf8,
        out IntPtr result
    );

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern McSolverEngineNativeStatus McSolverEngine_SolveToGeometryWithParameters(
        IntPtr documentXmlUtf8,
        IntPtr sketchNameUtf8,
        IntPtr parameterKeysUtf8,
        IntPtr parameterValuesUtf8,
        int parameterCount,
        out IntPtr result
    );

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern McSolverEngineNativeStatus McSolverEngine_SolveToBRep(
        IntPtr documentXmlUtf8,
        IntPtr sketchNameUtf8,
        out IntPtr result
    );

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern McSolverEngineNativeStatus McSolverEngine_SolveToBRepWithParameters(
        IntPtr documentXmlUtf8,
        IntPtr sketchNameUtf8,
        IntPtr parameterKeysUtf8,
        IntPtr parameterValuesUtf8,
        int parameterCount,
        out IntPtr result
    );

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern void McSolverEngine_FreeGeometryResult(IntPtr value);

    [DllImport(NativeLibraryName, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    private static extern void McSolverEngine_FreeBRepResult(IntPtr value);
}
