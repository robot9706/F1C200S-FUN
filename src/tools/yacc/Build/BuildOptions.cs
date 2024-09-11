namespace YACC.Build
{
    internal class BuildOptions
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

            var data = YAML.YamlDeserializer.Deserialize<Dictionary<string, object>>(yamlFile);

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

            Logger.LOG.WriteLine("Parsed .build", ConsoleColor.Blue);
        }
    }
}
