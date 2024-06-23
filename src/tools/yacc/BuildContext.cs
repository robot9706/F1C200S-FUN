using System;
using YamlDotNet.Core;

namespace YACC
{
    class BuildContext
    {
        public Dictionary<string, List<string>> Variables;

        public Dictionary<string, Tool> Tools;

        public BuildContext()
        {
            Variables = new Dictionary<string, List<string>>();
            Tools = new Dictionary<string, Tool>();
        }

        public void ParseVariables(object variables)
        {
            List<object> list = (List<object>)variables;
            foreach (object obj in list)
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

        public void ParseBuildArgs(Dictionary<string, string> args)
        {
            foreach (KeyValuePair<string, string> pair in args)
            {
                if (Variables.ContainsKey(pair.Key))
                {
                    throw new ArgumentOutOfRangeException($"Unable to set build argument variable, already exists: '{pair.Key}'");
                }

                Variables.Add(pair.Key, new List<string>()
                {
                    pair.Value,
                });
            }
        }

        public void ParseTools(object tools)
        {
            List<object> toolsList = (List<object>)tools;

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
                    string interpolated = InterpolateString((string)toolValue);

                    Tools.Add(toolName, new Tool()
                    {
                        Binary = interpolated,
                    });
                }
                else if (toolValue is Dictionary<object, object>)
                {
                    Dictionary<object, object> toolDict = (Dictionary<object, object>)toolValue;
                    if (!toolDict.ContainsKey("bin"))
                    {
                        throw new ArgumentOutOfRangeException($"Tool '{toolName}' has no binary defined!");
                    }

                    string bin = InterpolateString((string)toolDict["bin"]);

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

                    Tools.Add(toolName, new Tool()
                    { 
                        Binary = bin,
                        Arguments = args
                    });
                }
                else
                {
                    throw new ArgumentOutOfRangeException($"Tool '{toolName}' has unknown type: {toolValue.ToString()}");
                }
            }
        }

        public string InterpolateString(string value)
        {
            return Interpolate.InterpolateString(value, Variables, "$(", ")");
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

    class Tool
    {
        public string Binary;

        public List<string> Arguments;

        public Tool()
        {
            Arguments = new List<string>();
        }
    }
}
