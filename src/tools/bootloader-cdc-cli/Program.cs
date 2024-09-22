using System.IO.Ports;

namespace CLI
{
    internal class Program
    {
        static void Main(string[] args)
        {
            if (args.Length != 2)
            {
                Console.WriteLine("[COM] [FILE.bin]");
                return;
            }

            string portName = args[0];
            string file = args[1];

            if (!File.Exists(file))
            {
                Console.WriteLine($"File not found: {file}");
                return;
            }

            SerialPort port = new SerialPort(portName, 115200);
            port.Open();

            using (FileStream inputFile = new FileStream(file, FileMode.Open))
            {
                byte[] buffer = new byte[32];

                while (inputFile.Position < inputFile.Length)
                {
                    int readCount = inputFile.Read(buffer, 0, buffer.Length);

                    // Write data packet
                    port.Write(new byte[] { (byte)readCount }, 0, 1);
                    port.Write(buffer, 0, readCount);

                    port.BaseStream.Flush();
                }
            }

            // Done
            Console.WriteLine("Press enter to boot");
            Console.ReadLine();
            port.Write(new byte[] { 0x00 }, 0, 1);

            try
            {
                port.BaseStream.Flush();
                port.Close();
            }
            catch 
            {
                // Once the new code starts USB drops
            }
        }
    }
}
