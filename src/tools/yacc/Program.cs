using YACC.Build;
using YACC.Data;

namespace YACC
{
    internal class Program
    {
        static string BUILD_PROJECT_VERSION = "1";

        static void Main(string[] args)
        {
#if !DEBUG
            try
            {
#endif
                // Parse command line arguments & .build file
                var parsedArgs = Arguments.Parse(args);
                var dotBuild = Path.Combine(Directory.GetCurrentDirectory(), ".build");

                var buildOptions = new BuildOptions();
                buildOptions.ParseFromArgs(parsedArgs);
                buildOptions.ParseFromDotBuild(dotBuild);

                if (string.IsNullOrEmpty(buildOptions.BuildFile))
                {
                    throw new ArgumentParseException("Build project missing, use '--build' to define a YAML project.");
                }

                // Parse project file
                string yamlFilePath = buildOptions.BuildFile;
                if (!File.Exists(yamlFilePath))
                {
                    throw new FileNotFoundException(message: "Build project missing", fileName: yamlFilePath);
                }

                Logger.LOG.WriteLine("Parsing project file", ConsoleColor.Magenta);
                string yamlFile = File.ReadAllText(yamlFilePath);

                var data = YAML.YamlDeserializer.Deserialize<Dictionary<string, object>>(yamlFile);

                if (!data.ContainsKey("version"))
                {
                    throw new ArgumentOutOfRangeException("'version' field missing");
                }
                if (data["version"].ToString() != BUILD_PROJECT_VERSION)
                {
                    throw new ArgumentOutOfRangeException($"Invalid 'version', '{BUILD_PROJECT_VERSION}' expected");
                }

                var buildProject = (Dictionary<object, object>)data["build"];

                // Parse ProjectDef
                var project = new ProjectDef(buildProject);

                string target = parsedArgs.ContainsKey("target") ? (string)parsedArgs["target"] : "build";

                if (!project.Targets.ContainsKey(target))
                {
                    throw new KeyNotFoundException($"Project has no target: '{target}'");
                }

                // Create BuildContext
                var buildContext = new BuildContext(project, buildOptions);

                // Do the thing
                var build = new Build.Build(buildContext);

                Logger.LOG.WriteLine("Executing build", ConsoleColor.Magenta);

                build.ExecuteTarget(target);

                // Done :)
                Logger.LOG.WriteLine("Completed", ConsoleColor.Green);
#if !DEBUG
        }
            catch (ArgumentParseException aex)
            {
                Logger.LOG.WriteLine(aex.Message, ConsoleColor.Red);
                if (aex.StackTrace != null)
                {
                    Logger.LOG.WriteLine(aex.StackTrace, ConsoleColor.Red);
                }
            }
            catch (FileNotFoundException fex)
            {
                Logger.LOG.WriteLine($"{fex.Message}. File: '{fex.FileName}'", ConsoleColor.Red);
                if (fex.StackTrace != null)
                {
                    Logger.LOG.WriteLine(fex.StackTrace, ConsoleColor.Red);
                }
            }
            catch (ArgumentOutOfRangeException aoex)
            {
                Logger.LOG.WriteLine(aoex.Message, ConsoleColor.Red);
                if (aoex.StackTrace != null)
                {
                    Logger.LOG.WriteLine(aoex.StackTrace, ConsoleColor.Red);
                }
            }
            catch (Exception ex)
            {
                Logger.LOG.WriteLine(ex.Message, ConsoleColor.Red);
                if (ex.StackTrace != null)
                {
                    Logger.LOG.WriteLine(ex.StackTrace, ConsoleColor.Red);
                }
            }
#endif
        }
    }
}
