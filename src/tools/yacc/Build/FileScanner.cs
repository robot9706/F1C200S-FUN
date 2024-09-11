using YACC.Data;

namespace YACC.Build
{
    internal abstract class FileScanner
    {
        public abstract List<string> FilterFiles(Build build, StepDef step, ScanDef scan, List<string> files, string outputFolder);
    }
}
