namespace YACC.Data
{
    internal class StepDef
    {
        public string Name;

        public string ToolName;

        public string ToolArgsPlain;

        public string OutPlain;

        public List<string> InPlain;

        public ScanDef ScanDef;

        public StepDef(Dictionary<object, object> data)
        {
            Name = (string)data["name"];

            // Verify tool
            if (!data.ContainsKey("tool"))
            {
                throw new ArgumentOutOfRangeException("Step has no tool defined");
            }

            ToolName = (string)data["tool"];

            // Verify tool args
            if (!data.ContainsKey("args"))
            {
                throw new ArgumentOutOfRangeException("Step has no tool args defined!");
            }

            ToolArgsPlain = (string)data["args"];

            // Verify output
            if (data.ContainsKey("out"))
            {
                OutPlain = (string)data["out"];
                
                if (!ToolArgsPlain.Contains("${OUT}"))
                {
                    throw new ArgumentOutOfRangeException("Tool args does not define the ${OUT} parameter but step has output defined!");
                }
            }

            // Check for scan params
            if (data.ContainsKey("scan"))
            {
                var scanOptions = (Dictionary<object, object>)data["scan"];

                ScanDef = new ScanDef()
                {
                    Mode = ParseScanStyle((string)scanOptions["mode"]),
                    Resolve = new List<string>(),
                };

                var resolve = scanOptions["resolve"];
                if (resolve is string)
                {
                    ScanDef.Resolve.Add((string)resolve);
                }
                else if (resolve is List<object>)
                {
                    var resolveList = (List<object>)resolve;
                    var resolveStringList = resolveList.Select(it => it.ToString());

                    ScanDef.Resolve.AddRange(resolveStringList!);
                }
                else
                {
                    throw new Exception("Unknown scan resolve value!");
                }
            }

            // Verify input
            if (data.ContainsKey("in"))
            {
                if (!ToolArgsPlain.Contains("${IN}"))
                {
                    throw new ArgumentOutOfRangeException("Tool args does not define the ${IN} parameter but step has input defined!");
                }

                InPlain = new List<string>();

                object stepIn = data["in"];
                if (stepIn is string)
                {
                    string stepInStr = (string)stepIn;

                    InPlain = new List<string>()
                        {
                            stepInStr,
                        };
                }
                else if (stepIn is List<object>)
                {
                    List<object> stepInObj = (List<object>)stepIn;
                    InPlain = stepInObj.Select(it => it.ToString()).ToList();
                }
                else
                {
                    throw new ArgumentOutOfRangeException($"Step input has undefined type '{stepIn.GetType()}'");
                }
            }
        }

        static FileScanStyle ParseScanStyle(string inputScanStyle)
        {
            switch (inputScanStyle)
            {
                case "c-include":
                    return FileScanStyle.CInclude;
            }

            throw new ArgumentOutOfRangeException($"Unknown scan style: '{inputScanStyle}'");
        }
    }
}
