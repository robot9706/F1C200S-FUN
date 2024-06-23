using YamlDotNet.Serialization.NamingConventions;
using YamlDotNet.Serialization;
using System.Diagnostics;
using GlobExpressions;

namespace YACC
{
    class BuildOptions
    {
        public string BuildFile = null;
        public Dictionary<string, string> BuildArgs = new Dictionary<string, string>();

        public void ParseFromArgs(Dictionary<string, object> args)
        {
            if (args.ContainsKey("build"))
            {
                BuildFile = (string)args["build"];
            }

            if (args.ContainsKey("build-arg"))
            {
                var argsDict = (Dictionary<string, string>)args["build-arg"];
                foreach (var pair in argsDict)
                {
                    BuildArgs.Add(pair.Key, pair.Value);
                }
            }

            Logger.LOG.WriteLine("Parsed command line arguments", ConsoleColor.Blue);
        }

        public void ParseFromDotBuild(string dotBuild)
        {
            if (!File.Exists(dotBuild))
            {
                return;
            }

            string yamlFile = File.ReadAllText(dotBuild);

            var data = Program.YamlDeserializer.Deserialize<Dictionary<string, object>>(yamlFile);

            var yaccOptions = (Dictionary<object, object>)data["yacc"];

            if (yaccOptions.ContainsKey("build"))
            {
                if (string.IsNullOrEmpty(BuildFile))
                {
                    BuildFile = (string)yaccOptions["build"];
                }
                else
                {
                    Logger.LOG.WriteLine("Both command line and dotbuild defines build project, using command line", ConsoleColor.Yellow);
                }
            }

            if (yaccOptions.ContainsKey("buildArgs"))
            {
                var args = (List<object>)yaccOptions["buildArgs"];

                foreach (var arg in args)
                {
                    string[] parts = ((string)arg).Split('=');
                    if (parts.Length != 2)
                    {
                        throw new Exception($"Invalid dotbuild argument: '{arg}'");
                    }

                    if (BuildArgs.ContainsKey(parts[0]))
                    {
                        Logger.LOG.WriteLine($"Both command line and dotbuild defines '{parts[0]}', using command line", ConsoleColor.Yellow);
                    }
                    else
                    {
                        BuildArgs.Add(parts[0], parts[1]);
                    }
                }
            }

            Logger.LOG.WriteLine("Parsed dotbuild", ConsoleColor.Blue);
        }
    }

    internal class Program
    {
        static string BUILD_PROJECT_VERSION = "1";

        public static IDeserializer YamlDeserializer
        {
            get => new DeserializerBuilder()
                .WithNamingConvention(CamelCaseNamingConvention.Instance)
                .Build();
        }

        static void Main(string[] args)
        {
            try
            {
                // Parse and validate command line arguments and dotbuild
                var parsedArgs = Arguments.Parse(args);

                string dotBuild = Path.Combine(Directory.GetCurrentDirectory(), ".build");

                var buildOpts = new BuildOptions();
                buildOpts.ParseFromArgs(parsedArgs);
                buildOpts.ParseFromDotBuild(dotBuild);

                if (string.IsNullOrEmpty(buildOpts.BuildFile))
                {
                    throw new ArgumentParseException("Build project missing, use '--build' to define a YAML project.");
                }

                // Parse project file
                string yamlFilePath = buildOpts.BuildFile;
                if (!File.Exists(yamlFilePath))
                {
                    throw new FileNotFoundException(message: "Build project missing", fileName: yamlFilePath);
                }

                Logger.LOG.WriteLine("Parsing project file", ConsoleColor.Magenta);
                string yamlFile = File.ReadAllText(yamlFilePath);

                var data = YamlDeserializer.Deserialize<Dictionary<string, object>>(yamlFile);

                if (!data.ContainsKey("version"))
                {
                    throw new ArgumentOutOfRangeException("'version' field missing");
                }
                if (data["version"].ToString() != BUILD_PROJECT_VERSION)
                {
                    throw new ArgumentOutOfRangeException($"Invalid 'version', '{BUILD_PROJECT_VERSION}' expected");
                }

                Dictionary<object, object> buildProject = (Dictionary<object, object>)data["build"];

                // Check args
                Dictionary<string, string> inputBuildArgs = buildOpts.BuildArgs;

                List<string> buildArgs = buildProject.ContainsKey("args")
                     ? ((List<object>)buildProject["args"]).Select(it => it.ToString()).ToList()
                     : new List<string>();

                foreach (var argName in buildArgs)
                {
                    string argNameString = argName.ToString();

                    if (!inputBuildArgs.ContainsKey(argNameString))
                    {
                        throw new ArgumentOutOfRangeException($"Build argument '{argNameString}' no defined, please add '--{argNameString}' to the command line arguments");
                    }
                }

                // Prepare context
                BuildContext context = new BuildContext();

                if (buildProject.ContainsKey("variables"))
                {
                    context.ParseVariables(buildProject["variables"]);
                }

                context.ParseBuildArgs(inputBuildArgs);

                if (buildProject.ContainsKey("tools"))
                {
                    context.ParseTools(buildProject["tools"]);
                }

                // Execute target
                Dictionary<object, object> targets = (Dictionary<object, object>)buildProject["targets"];

                Dictionary<object, object> target = (Dictionary<object, object>)targets["build"];

                Logger.LOG.WriteLine("Executing build", ConsoleColor.Magenta);

                ExecuteTarget(context, target);

                Logger.LOG.WriteLine("Completed", ConsoleColor.Green);
            }
            catch (ArgumentParseException aex)
            {
                Logger.LOG.WriteLine(aex.Message, ConsoleColor.Red);
            }
            catch (FileNotFoundException fex)
            {
                Logger.LOG.WriteLine($"{fex.Message}. File: '{fex.FileName}'", ConsoleColor.Red);
            }
            catch (ArgumentOutOfRangeException aoex)
            {
                Logger.LOG.WriteLine(aoex.Message, ConsoleColor.Red);
            }
        }

        static void ExecuteTarget(BuildContext context, Dictionary<object, object> target)
        {
            List<object> steps = (List<object>)target["steps"];

            for (int step = 0;step < steps.Count; step++)
            {
                Dictionary<object, object> currentStep = (Dictionary<object, object>)steps[step];

                var logger = new Logger(currentStep.ContainsKey("name") ? (string)currentStep["name"] : $"Step #{step}");
                logger.WriteLine("Executing step", ConsoleColor.Magenta);

                // Verify tool
                if (!currentStep.ContainsKey("tool"))
                {
                    throw new ArgumentOutOfRangeException("Step has no tool defined");
                }

                string toolName = (string)currentStep["tool"];
                if (!context.Tools.ContainsKey(toolName))
                {
                    throw new ArgumentOutOfRangeException($"Undefined tool '{toolName}'");
                }

                // Verify tool args
                if (!currentStep.ContainsKey("args"))
                {
                    throw new ArgumentOutOfRangeException("Step has no tool args defined!");
                }

                string toolArgs = (string)currentStep["args"];
                toolArgs = context.InterpolateString(toolArgs);

                // Verify output
                string output = null;
                if (currentStep.ContainsKey("out"))
                {
                    output = (string)currentStep["out"];
                    output = context.InterpolateString(output);

                    if (!toolArgs.Contains("${OUT}"))
                    {
                        throw new ArgumentOutOfRangeException("Tool args does not define the ${OUT} parameter!");
                    }
                }

                // Verify input
                if (currentStep.ContainsKey("in"))
                {
                    if (!toolArgs.Contains("${IN}"))
                    {
                        throw new ArgumentOutOfRangeException("Tool args does not define the ${IN} parameter!");
                    }

                    List<string> input = null;

                    object stepIn = currentStep["in"];
                    if (stepIn is string)
                    {
                        string stepInStr = context.InterpolateString((string)stepIn);

                        input = new List<string>()
                        {
                            stepInStr,
                        };
                    }
                    else if (stepIn is List<object>)
                    {
                        List<object> stepInObj = (List<object>)stepIn;
                        input = stepInObj.Select(it =>
                        {
                            string strVal = it.ToString();

                            return context.InterpolateString(strVal);
                        }).ToList();
                    }
                    else
                    {
                        throw new ArgumentOutOfRangeException($"Step input has undefined type '{stepIn.GetType()}'");
                    }

                    ExecuteTargetOnFiles(logger, context, toolName, toolArgs, input, output);
                }
                else
                {
                    ExecuteTargetOnCommand(context, toolName, toolArgs, output);
                }

                // Insert NL
                Console.WriteLine();
            }
        }

        static void ExecuteTargetOnFiles(Logger log, BuildContext context, string toolName, string toolArgs, List<string> input, string output)
        {
            // Process inputs
            if (input.Count == 0)
            {
                log.WriteLine("Step has no input, skipping", ConsoleColor.DarkGray);
                return;
            }

            if (!string.IsNullOrEmpty(output) && !Directory.Exists(output))
            {
                Directory.CreateDirectory(output);
            }

            // Check for glob patterns
            var allInputs = new List<string>();
            foreach (var inputPattern in input)
            {
                string cleanedPath = Interpolate.FixPath(inputPattern);

                if (!inputPattern.Contains("*"))
                {
                    allInputs.Add(cleanedPath);
                    continue;
                }

                allInputs.AddRange(Interpolate.ResolveGlob(cleanedPath));
            }

            // Build each input
            foreach (string inputItem in allInputs)
            {
                string outFileName = null;
                if (!string.IsNullOrEmpty(output))
                {
                    string baseFileName = Path.GetFileNameWithoutExtension(inputItem);
                    outFileName = Path.Combine(output, baseFileName + ".obj");

                    if (!File.Exists(inputItem))
                    {
                        throw new Exception($"File does not exist: '{inputItem}'");
                    }

                    if (File.Exists(outFileName))
                    {
                        FileInfo inputFileInfo = new FileInfo(inputItem);
                        FileInfo outFileInfo = new FileInfo(outFileName);

                        if (outFileInfo.LastAccessTimeUtc >= inputFileInfo.LastAccessTimeUtc)
                        {
                            log.WriteLine($"Input '{inputItem}' unmodified, skipping", ConsoleColor.DarkGray);
                            continue;
                        }
                    }
                }

                // Execute tool
                var tool = context.Tools[toolName];

                ProcessStartInfo info = new ProcessStartInfo(tool.Binary);

                var paramsDict = new Dictionary<string, List<string>>();
                paramsDict.Add("IN", new List<string>() { inputItem });
                if (!string.IsNullOrEmpty(outFileName))
                {
                    paramsDict.Add("OUT", new List<string>() { outFileName });
                }

                var argsInterpolated = Interpolate.InterpolateString(toolArgs, paramsDict, "${", "}");
                argsInterpolated = Interpolate.InterpolateGlobs(argsInterpolated);

                info.Arguments = argsInterpolated;

                ExecuteProcessInfo(info);
            }
        }

        static void ExecuteTargetOnCommand(BuildContext context, string toolName, string toolArgs, string output)
        {
            if (!string.IsNullOrEmpty(output))
            {
                string dirName = Path.GetDirectoryName(output);
                if (!Directory.Exists(dirName))
                {
                    Directory.CreateDirectory(dirName);
                }
            }

            var tool = context.Tools[toolName];

            ProcessStartInfo info = new ProcessStartInfo(tool.Binary);

            var paramsDict = new Dictionary<string, List<string>>();
            if (!string.IsNullOrEmpty(output))
            {
                paramsDict.Add("OUT", new List<string>() { output });
            }

            var argsInterpolated = Interpolate.InterpolateString(toolArgs, paramsDict, "${", "}");
            argsInterpolated = Interpolate.InterpolateGlobs(argsInterpolated);

            info.Arguments = argsInterpolated;

            ExecuteProcessInfo(info);
        }

        static void ExecuteProcessInfo(ProcessStartInfo processInfo)
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
