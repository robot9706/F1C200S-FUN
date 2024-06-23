using GlobExpressions;
using YamlDotNet.Core.Tokens;

namespace YACC
{
    static class Interpolate
    {
        public static string Interpolator(string value, string begin, string end, Func<string, string> replaceFunc)
        {
            string replace = value;

            int paramIndex = -1;
            do
            {
                paramIndex = replace.IndexOf(begin);
                if (paramIndex < 0)
                {
                    break;
                }

                int paramEnd = replace.IndexOf(end, paramIndex);
                if (paramEnd < 0)
                {
                    throw new ArgumentOutOfRangeException($"Invalid parameter template in '{value}' at {paramIndex}");
                }

                string param = replace.Substring(paramIndex + begin.Length, paramEnd - paramIndex - begin.Length);
                string paramValue = replaceFunc(param);

                replace = replace.Substring(0, paramIndex) + paramValue + replace.Substring(paramEnd + end.Length);
            }
            while (paramIndex >= 0);

            return replace;
        }

        public static string InterpolateString(string value, Dictionary<string, List<string>> variables, string beginSeparator = "$(", string endingSeparator = ")")
        {
            return Interpolator(value, beginSeparator, endingSeparator, param =>
            {
                if (!variables.ContainsKey(param))
                {
                    throw new ArgumentOutOfRangeException($"Variable not found '{param}'");
                }

                List<string> paramValues = variables[param];
                return string.Join(" ", paramValues);
            });
        }

        public static string InterpolateGlobs(string value)
        {
            return Interpolator(value, "$[", "]", path =>
            {
                string fixedPath = FixPath(path);
                List<string> globbed = ResolveGlob(fixedPath);

                return string.Join(" ", globbed);
            });
        }

        public static List<string> ResolveGlob(string glob)
        {
            string[] patternParts = glob.Split(Path.DirectorySeparatorChar);
            int firstGlob = Array.FindIndex(patternParts, it => it.Contains('*'));

            string pathPart = String.Join(Path.DirectorySeparatorChar, patternParts, 0, firstGlob);
            string globPart = String.Join(Path.DirectorySeparatorChar, patternParts, firstGlob, patternParts.Length - firstGlob);

            var globbed = Glob.Files(pathPart, globPart).ToList();
            return globbed.Select(it => Path.Combine(pathPart, it)).ToList();
        }

        public static string FixPath(string path)
        {
            return path.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
        }
    }
}
