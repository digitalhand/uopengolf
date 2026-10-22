// SPDX-License-Identifier: MIT
// Minimal stand-ins for the Godot types the included physics files reference.
namespace Godot.Collections
{
    public class Dictionary : System.Collections.Generic.Dictionary<string, object> { }
}

public static class PhysicsLogger
{
    public enum Level { Off = 0, Error = 1, Info = 2, Verbose = 3 }
    public static Level LogLevel { get; set; } = Level.Error;
    internal static void Info(string message) { }
    internal static void Verbose(string message) { }
    internal static void Error(string message) => System.Console.Error.WriteLine(message);
    internal static void PushError(string message) => System.Console.Error.WriteLine(message);
}
