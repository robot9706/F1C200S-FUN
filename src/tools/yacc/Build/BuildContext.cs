using YACC.Data;

namespace YACC.Build
{
    class BuildContext
    {
        public ProjectDef Project;

        public Dictionary<string, List<string>> Variables;

        public BuildContext(ProjectDef project, BuildOptions buildOptions)
        {
            Project = project;
            Variables = project.Variables;

            // Validate required arguments
            foreach (var argName in project.Args)
            {
                string argNameString = argName.ToString();

                if (!buildOptions.BuildArgs.ContainsKey(argNameString))
                {
                    throw new ArgumentOutOfRangeException($"Build argument '{argNameString}' no defined, please add '--{argNameString}' to the command line arguments");
                }
            }

            // Copy build options vars to context
            foreach (KeyValuePair<string, string> pair in buildOptions.BuildArgs)
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

        /// <summary>
        /// Interpolates a string containing $(VAR) tokens
        /// </summary>
        /// <param name="value"></param>
        /// <returns></returns>
        public string InterpolateString(string value)
        {
            return Interpolate.InterpolateString(value, Variables, "$(", ")");
        }
    }
}
