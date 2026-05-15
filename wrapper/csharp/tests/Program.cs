using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace McSolverEngine.Wrapper.Tests;

public static class Program
{
    public static int Main(string[] args)
    {
        Console.WriteLine("=== McSolverEngine Wrapper Tests ===");

        var testClassType = typeof(WrapperRegressionTests);

        var assemblyInitMethod = testClassType.GetMethod(
            "AssemblyInitialize",
            BindingFlags.Public | BindingFlags.Static
        );

        if (assemblyInitMethod != null)
        {
            Console.Write("AssemblyInitialize... ");
            try
            {
                assemblyInitMethod.Invoke(null, new object[] { null! });
                Console.WriteLine("OK");
            }
            catch (TargetInvocationException ex)
            {
                Console.WriteLine("FAILED");
                Console.Error.WriteLine($"Error: {ex.InnerException?.Message}");
                Console.Error.WriteLine(ex.InnerException?.StackTrace);
                return 1;
            }
        }

        var testMethods = testClassType
            .GetMethods(BindingFlags.Public | BindingFlags.Instance)
            .Where(m => m.GetCustomAttribute<TestMethodAttribute>() != null)
            .ToList();

        Console.WriteLine($"\nRunning {testMethods.Count} tests...\n");

        int passed = 0;
        int failed = 0;
        var failures = new List<(string Method, string Error)>();

        foreach (var method in testMethods)
        {
            Console.Write($"  {method.Name}... ");

            var instance = Activator.CreateInstance(testClassType);

            try
            {
                method.Invoke(instance, null);
                Console.WriteLine("PASS");
                passed++;
            }
            catch (TargetInvocationException ex)
            {
                var inner = ex.InnerException;
                var message = inner?.Message ?? "Unknown error";

                if (inner is AssertInconclusiveException)
                {
                    Console.WriteLine("SKIP (inconclusive)");
                }
                else
                {
                    Console.WriteLine("FAIL");
                    Console.Error.WriteLine($"    {message}");
                    failed++;
                    failures.Add((method.Name, message));
                }
            }
        }

        Console.WriteLine($"\n=== {passed} passed, {failed} failed, {testMethods.Count} total ===");

        if (failures.Count > 0)
        {
            Console.Error.WriteLine("\nFailures:");
            foreach (var (method, error) in failures)
            {
                Console.Error.WriteLine($"  - {method}: {error}");
            }
            return 1;
        }

        return 0;
    }
}
