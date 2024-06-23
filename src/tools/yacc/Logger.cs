namespace YACC
{
    class Logger
    {
        public static Logger LOG = new Logger(string.Empty);

        private string tag;

        public Logger(string tag)
        {
            this.tag = tag;
        }

        public void WriteLine(string text)
        {
            if (string.IsNullOrEmpty(tag))
            {
                Console.WriteLine(text);
            }
            else
            {
                Console.WriteLine($"[{tag}] {text}");
            }
        }

        public void WriteLine(string text, ConsoleColor color)
        {
            var oldColor = Console.ForegroundColor;
            Console.ForegroundColor = color;
            
            if (string.IsNullOrEmpty(tag))
            {
                Console.WriteLine(text);
            }
            else
            {
                Console.WriteLine($"[{tag}] {text}");
            }

            Console.ForegroundColor = oldColor;
        }
    }
}
