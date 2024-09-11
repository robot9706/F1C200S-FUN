namespace YACC.Data
{
    internal enum FileScanStyle
    {
        CInclude, // #include
    }

    internal class ScanDef
    {
        public FileScanStyle Mode { get; set; }

        public List<string> Resolve { get; set; } // Paths where includes can be resolved
    }
}
