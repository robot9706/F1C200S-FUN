using System.Diagnostics;
using YACC.Data;

namespace YACC.Build
{
    internal class Build
    {
        public BuildContext Context;

        public Build(BuildContext context)
        {
            Context = context;
        }

        public void ExecuteTarget(string targetName)
        {
            var target = Context.Project.Targets[targetName];

            foreach (var step in target.Steps)
            {
                var logger = new Logger($"Step #{step.Name ?? "N/A"}");
                logger.WriteLine("Executing step", ConsoleColor.Magenta);

                if (!Context.Project.Tools.ContainsKey(step.ToolName))
                {
                    throw new Exception($"Tool '{step.ToolName}' not found in step '{step.Name}'");
                }
            
                if (step.InPlain == null)
                {
                    ExecuteStepCommand(logger, step);
                }
                else
                {
                    ExecuteStepOnFiles(logger, step);
                }
            }
        }

        private void ExecuteStepOnFiles(Logger logger, StepDef step)
        {
            string output = string.IsNullOrEmpty(step.OutPlain) ? null : Context.InterpolateString(step.OutPlain);
            if (!string.IsNullOrEmpty(output) && !Directory.Exists(output))
            {
                Directory.CreateDirectory(output);
            }

            // Check input for glob patterns
            var allInputs = new List<string>();
            foreach (var inputPlain in step.InPlain)
            {
                var inputResolved = Context.InterpolateString(inputPlain);

                string cleanedPath = Interpolate.FixPath(inputResolved);

                if (!inputResolved.Contains("*"))
                {
                    allInputs.Add(cleanedPath);
                    continue;
                }

                var globbedFiles = Interpolate.ResolveGlob(cleanedPath);

                allInputs.AddRange(globbedFiles);
            }

            // Make sure all paths are absolute
            allInputs = allInputs!.Select(it =>
            {
                if (!Path.IsPathRooted(it))
                {
                    var resolved = Path.Combine(Environment.CurrentDirectory, it);
                    return Path.GetFullPath(resolved); // Removes ./
                }

                return it;
            }).ToList();

            // Scan files
            FileScanner scanner = null;
            if (!string.IsNullOrEmpty(output))
            {
                if (step.ScanDef == null)
                {
                    scanner = new FileScannerOutput();
                }
                else
                {
                    switch (step.ScanDef.Mode)
                    {
                        case FileScanStyle.CInclude:
                            scanner = new FileScannerCInclude();
                            break;
                        default:
                            throw new Exception($"Unknown scan mode '{step.ScanDef.Mode}'");
                    }
                }
            }

            var filteredInputFiles = scanner == null ? allInputs : scanner.FilterFiles(this, step, step.ScanDef, allInputs, output);

            // Build each input
            var tool = Context.Project.Tools[step.ToolName];

            foreach (string inputItem in filteredInputFiles)
            {
                if (!File.Exists(inputItem))
                {
                    throw new Exception($"File does not exist: '{inputItem}'");
                }

                logger.WriteLine($"Building '{Path.GetFileName(inputItem)}'", ConsoleColor.DarkGray);

                string outFileName = null;
                if (!string.IsNullOrEmpty(output))
                {
                    string baseFileName = Path.GetFileNameWithoutExtension(inputItem);
                    outFileName = Path.Combine(output, baseFileName + ".obj");
                }

                // Execute tool
                var toolBinary = Context.InterpolateString(tool.BinaryPlain);
                ProcessStartInfo info = new ProcessStartInfo(toolBinary);

                var paramsDict = new Dictionary<string, List<string>>
                {
                    { "IN", new List<string>() { inputItem } }
                };
                if (!string.IsNullOrEmpty(outFileName))
                {
                    paramsDict.Add("OUT", new List<string>() { outFileName });
                }

                var args = Context.InterpolateString(step.ToolArgsPlain); // Interpolate build variables
                args = Interpolate.InterpolateString(args, paramsDict, "${", "}"); // Interpolate tool variables
                info.Arguments = Interpolate.InterpolateGlobs(args);

                ExecuteProcessInfo(info);
            }
        }

        private void ExecuteStepCommand(Logger logger, StepDef step)
        {
            string output = string.IsNullOrEmpty(step.OutPlain) ? null : Context.InterpolateString(step.OutPlain);
            if (!string.IsNullOrEmpty(output))
            {
                string dirName = Path.GetDirectoryName(output);
                if (!Directory.Exists(dirName))
                {
                    Directory.CreateDirectory(dirName);
                }
            }

            var tool = Context.Project.Tools[step.ToolName];

            var toolBinary = Context.InterpolateString(tool.BinaryPlain);
            ProcessStartInfo info = new ProcessStartInfo(toolBinary);

            var paramsDict = new Dictionary<string, List<string>>();
            if (!string.IsNullOrEmpty(output))
            {
                paramsDict.Add("OUT", new List<string>() { output });
            }

            var args = Context.InterpolateString(step.ToolArgsPlain); // Interpolate build variables
            args = Interpolate.InterpolateString(args, paramsDict, "${", "}"); // Interpolate tool variables
            info.Arguments = Interpolate.InterpolateGlobs(args);

            ExecuteProcessInfo(info);
        }

        private void ExecuteProcessInfo(ProcessStartInfo processInfo)
        {
            Console.WriteLine($"Executing '{processInfo.FileName} {processInfo.Arguments}'");

            var process = Process.Start(processInfo);

            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                throw new ArgumentOutOfRangeException($"Command failed with exit code {process.ExitCode}");
            }
        }
    }
}
