namespace YACC.Data
{
    internal class ProjectDef
    {
        public List<string> Args;

        public Dictionary<string, List<string>> Variables;

        public Dictionary<string, ToolDef> Tools;

        public Dictionary<string, TargetDef> Targets;

        public ProjectDef(Dictionary<object, object> buildProject)
        {
            Variables = new Dictionary<string, List<string>>();
            Tools = new Dictionary<string, ToolDef>();
            Targets = new Dictionary<string, TargetDef>();

            Args = buildProject.ContainsKey("args")
                     ? ((List<object>)buildProject["args"]).Select(it => it.ToString()).ToList()
                     : new List<string>();

            if (buildProject.ContainsKey("variables"))
            {
                var variables = (List<object>)buildProject["variables"];
                foreach (object obj in variables)
                {
                    if (obj is string)
                    {
                        var (key, values) = ParseStringVariable((string)obj);

                        if (Variables.ContainsKey(key))
                        {
                            throw new ArgumentOutOfRangeException($"Variable '{key}' is defined multiple times");
                        }

                        Variables.Add(key, values.ToList());
                    }
                    else if (obj is Dictionary<object, object>)
                    {
                        Dictionary<object, object> dict = (Dictionary<object, object>)obj;

                        if (dict.Count != 1)
                        {
                            throw new ArgumentOutOfRangeException("Well this is unexpected, dictionary has more than one entry!");
                        }

                        string key = (string)dict.Keys.ToList()[0];
                        List<object> values = (List<object>)dict[key];

                        if (Variables.ContainsKey(key))
                        {
                            throw new ArgumentOutOfRangeException($"Variable '{key}' is defined multiple times");
                        }

                        Variables.Add(key, values.Select(it => it.ToString()).ToList());
                    }
                    else
                    {
                        throw new ArgumentOutOfRangeException($"Variable has unknown type: {obj.ToString()}");
                    }
                }
            }

            if (buildProject.ContainsKey("tools"))
            {
                List<object> toolsList = (List<object>)buildProject["tools"];
                foreach (object tool in toolsList)
                {
                    Dictionary<object, object> dict = (Dictionary<object, object>)tool;

                    if (dict.Count != 1)
                    {
                        throw new ArgumentOutOfRangeException("Well this is unexpected, dictionary has more than one entry!");
                    }

                    string toolName = (string)dict.Keys.ToList()[0];
                    object toolValue = dict[toolName];

                    if (Tools.ContainsKey(toolName))
                    {
                        throw new ArgumentOutOfRangeException($"Tool '{toolName}' defined multiple times!");
                    }

                    if (toolValue is string)
                    {
                        Tools.Add(toolName, new ToolDef()
                        {
                            BinaryPlain = (string)toolValue,
                        });
                    }
                    else if (toolValue is Dictionary<object, object>)
                    {
                        Dictionary<object, object> toolDict = (Dictionary<object, object>)toolValue;
                        if (!toolDict.ContainsKey("bin"))
                        {
                            throw new ArgumentOutOfRangeException($"Tool '{toolName}' has no binary defined!");
                        }

                        string bin = (string)toolDict["bin"];

                        List<string> args = new List<string>();

                        if (toolDict.ContainsKey("args"))
                        {
                            object toolArgs = toolDict["args"];
                            if (toolArgs is string)
                            {
                                string[] strArgs = ((string)toolArgs).Split(' ');

                                args.AddRange(strArgs);
                            }
                            else if (toolArgs is List<object>)
                            {
                                List<object> objArgs = (List<object>)toolArgs;
                                List<string> strArgs = objArgs.Select(it => it.ToString()).ToList();

                                args.AddRange(strArgs);
                            }
                            else
                            {
                                throw new ArgumentOutOfRangeException($"Tool '{toolName}' has unknown args type: {toolArgs.ToString()}");
                            }
                        }

                        Tools.Add(toolName, new ToolDef()
                        {
                            BinaryPlain = bin,
                            Arguments = args
                        });
                    }
                    else
                    {
                        throw new ArgumentOutOfRangeException($"Tool '{toolName}' has unknown type: {toolValue.ToString()}");
                    }
                }
            }

            if (buildProject.ContainsKey("targets"))
            {
                var targets = (Dictionary<object, object>)buildProject["targets"];

                foreach (KeyValuePair<object, object> pair in targets)
                {
                    var targetData = (Dictionary<object, object>)pair.Value;

                    var targetDef = new TargetDef(targetData);

                    var key = pair.Key.ToString();
                    if (Targets.ContainsKey(key))
                    {
                        throw new Exception($"Multiple targets defined with the same key: '{key}'");
                    }

                    Targets.Add(key, targetDef);
                }
            }
        }

        static (string, string[]) ParseStringVariable(string arg)
        {
            int firstEquals = arg.IndexOf('=');
            if (firstEquals < 0)
            {
                throw new ArgumentOutOfRangeException($"Unexpected value '{arg}'");
            }

            string key = arg.Substring(0, firstEquals);
            string val = arg.Substring(firstEquals + 1);

            if (val.StartsWith("\""))
            {
                val = val.Substring(1);
            }
            if (val.EndsWith("\""))
            {
                val = val.Substring(0, val.Length - 1);
            }

            return (key, new string[] { val });
        }
    }
}
