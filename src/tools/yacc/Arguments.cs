namespace YACC
{
    class ArgumentParseException : Exception
    {
        public ArgumentParseException(string message) : base(message) { }
    }

    class Arguments
    {
        public static Dictionary<string, object> Parse(string[] args)
        {
            Dictionary<string, object> parsed = new Dictionary<string, object>();
            
            Queue<string> queue = new Queue<string>(args);

            while (queue.Count > 0) 
            {
                string elem = queue.Dequeue();
                if (elem.StartsWith("--"))
                {
                    if (queue.Count <= 0)
                    {
                        throw new ArgumentParseException($"Missing value for argument '{elem}'");
                    }

                    string value = queue.Dequeue();
                    string clean = elem.Substring(2);

                    if (value.Contains("="))
                    {
                        Dictionary<string, string> dict = null;
                        if (parsed.ContainsKey(clean))
                        {
                            dict = (Dictionary<string, string>)parsed[clean];
                        }
                        else
                        {
                            dict = new Dictionary<string, string>();
                            parsed.Add(clean, dict);
                        }

                        string[] parts = value.Split('=');
                        if (parts.Length != 2)
                        {
                            throw new ArgumentParseException($"Argument '{value}' is not a valid key-value.");
                        }
                        else
                        {
                            dict.Add(parts[0], parts[1]);
                        }
                    }
                    else
                    {
                        parsed.Add(clean, value);
                    }
                }
                else if (elem.StartsWith("-"))
                {
                    string clean = elem.Substring(1);

                    parsed.Add(clean, true);
                }
                else
                {
                    throw new ArgumentParseException($"Invalid argument type '{elem}', each argument must start with '-' or '--'");
                }
            }

            return parsed;
        }        
    }
}
