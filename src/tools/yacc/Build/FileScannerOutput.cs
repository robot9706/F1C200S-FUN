using YACC.Data;

namespace YACC.Build
{
    /// <summary>
    /// Scans files based on their outputs and their modified times.
    /// </summary>
    internal class FileScannerOutput : FileScanner
    {
        public override List<string> FilterFiles(Build build, StepDef step, ScanDef scan, List<string> files, string outputFolder)
        {
            return files.Where(file =>
            {
                string baseFileName = Path.GetFileNameWithoutExtension(file);
                string outFileName = Path.Combine(outputFolder, baseFileName + ".obj");

                if (!File.Exists(outFileName))
                {
                    return true;
                }

                FileInfo inputFileInfo = new FileInfo(file);
                FileInfo outputFileInfo = new FileInfo(outFileName);

                return inputFileInfo.LastWriteTimeUtc > outputFileInfo.LastWriteTimeUtc;
            }).ToList();
        }
    }
}
