using System.Text.RegularExpressions;
using YACC.Data;

namespace YACC.Build
{
    internal class FileScannerCInclude : FileScanner
    {
        static Regex CIncludeRegex = new Regex("^\\s*\\#include\\s+[\"<]([^\">]+)[\">]");

        public override List<string> FilterFiles(Build build, StepDef step, ScanDef scan, List<string> files, string outputFolder)
        {
            Logger logger = new Logger("FileScan");

            var dependency = BuildDependencyList(logger, build, scan, files);

            return files.Where(inputFile =>
            {
                string baseFileName = Path.GetFileNameWithoutExtension(inputFile);
                string outFileName = Path.Combine(outputFolder, baseFileName + ".obj");

                if (!File.Exists(outFileName))
                {
                    return true;
                }

                FileInfo inputFileInfo = new FileInfo(inputFile);
                FileInfo outputFileInfo = new FileInfo(outFileName);

                if (inputFileInfo.LastWriteTimeUtc > outputFileInfo.LastWriteTimeUtc)
                {
                    return true;
                }

                var dependencies = dependency[inputFile];

                if (dependencies.Any(dep =>
                {
                    FileInfo depFileInfo = new FileInfo(dep);

                    return depFileInfo.LastWriteTimeUtc > outputFileInfo.LastWriteTimeUtc;
                }))
                {
                    return true;
                }

                return false;
            }).ToList();
        }

        private Dictionary<string, List<string>> BuildDependencyList(Logger log, Build build, ScanDef scan, List<string> files)
        {
            var resolveVars = scan.Resolve.Select(it => build.Context.InterpolateString(it)).ToList();
            var resolveFolders = ProcessIncludeArgs(resolveVars);

            return files.ToDictionary(it => it, inputFile =>
            {
                var includedFiles = ReadIncludeFiles(inputFile);

                return includedFiles.Select(includeFile =>
                {
                    // Resolve input file
                    List<string> possibleFiles = resolveFolders
                        .Select(it => Interpolate.FixPath(Path.Combine(it, includeFile)))
                        .Where(it => File.Exists(it))
                        .ToList();

                    if (possibleFiles.Count == 0)
                    {
                        log.WriteLine($"Included file '{includeFile}' in '{inputFile}' not found in paths defined by scan resolve!");
                        // throw new Exception($"Included file '{includeFile}' in '{inputItem}' not found in paths defined by scan resolve!");
                        return null;
                    }
                    else if (possibleFiles.Count > 1)
                    {
                        throw new Exception($"Included file '{includeFile}' in '{inputFile}' resolved to multiple files! {String.Join(",", possibleFiles)}");
                    }

                    var resolvedFilePath = possibleFiles[0];
                    if (!Path.IsPathRooted(resolvedFilePath))
                    {
                        resolvedFilePath = Path.Combine(Environment.CurrentDirectory, resolvedFilePath);
                        resolvedFilePath = Path.GetFullPath(resolvedFilePath); // Removes ./
                    }

                    return resolvedFilePath;
                })
                    .Where(it => !string.IsNullOrEmpty(it))
                    .Distinct()
                    .ToList() as List<string>;
            });
        }

        static HashSet<string> ReadIncludeFiles(string file)
        {
            HashSet<string> includedFiles = new HashSet<string>();

            using (StreamReader sr = new StreamReader(file))
            {
                string line;
                while ((line = sr.ReadLine()) != null)
                {
                    Match match = CIncludeRegex.Match(line);
                    if (match.Captures.Count == 0)
                    {
                        continue;
                    }

                    foreach (Match capture in match.Captures)
                    {
                        var includeFile = capture.Groups[1].Value;

                        includedFiles.Add(includeFile);
                    }
                }
            }

            return includedFiles;
        }

        static List<string> ProcessIncludeArgs(List<string> includeArgs)
        {
            var splitArgs = includeArgs.SelectMany(it => it.Split(' ', options: StringSplitOptions.RemoveEmptyEntries));

            return splitArgs.Select(it =>
            {
                if (it.StartsWith("-I"))
                {
                    return it.Substring(2);
                }

                return it;
            }).ToList();
        }
    }
}
