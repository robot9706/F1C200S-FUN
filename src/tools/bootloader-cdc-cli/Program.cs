using System.Diagnostics;
using System.IO.Ports;
using System.Net.Sockets;
using System.Runtime.InteropServices.Marshalling;
using System.Text;

namespace CLI
{
    abstract class PortLike
    {
        public void Write(byte val)
        {
            Write(new byte[] { val }, 0, 1);
        }

        public void Write(string str)
        {
            byte[] buf = Encoding.ASCII.GetBytes(str);
            Write(buf, 0, buf.Length);
        }

        public abstract void Write(byte[] buffer, int offset, int count);
        public abstract int Read(byte[] buffer, int offset, int count);
        public abstract void Flush();
        public abstract void Close();
    }

    class ComPort : PortLike
    {
        private SerialPort _com;

        public ComPort(string com, int baud)
        { 
            _com = new SerialPort(com, baud);
            _com.Open();
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            return _com.Read(buffer, offset, count);
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            _com.Write(buffer, offset, count);
        }

        public override void Close()
        {
            _com.Close();
        }

        public override void Flush()
        {
            _com.BaseStream.Flush();
        }
    }

    class TcpPort : PortLike
    {
        private TcpClient _client;

        public TcpPort(string address, int port)
        {
            _client = new TcpClient();
            _client.Connect(address, port);
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            return _client.GetStream().Read(buffer, offset, count);
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            _client.GetStream().Write(buffer, offset, count);
        }

        public override void Close()
        {
            _client.Close();
        }

        public override void Flush()
        {
            _client.GetStream().Flush();
        }
    }

    internal class Program
    {
        static int PORT_TIMEOUT_MS = 5000;
        static int UBOOT_PROMPT_TIMEOUT = 10000;

        enum ActionEnum
        {
            UBootFatLoad,
            LoadCDC
        }

        class LoaderAction
        {
            public ActionEnum Action;

            public List<string> Params = new List<string>();
        }

        public enum LoopMode
        {
            None,
            Manual,
            UBoot,
        }

        class Args
        {
            public LoopMode Loop = LoopMode.None;

            public List<LoaderAction> Actions;
        }

        static void Main(string[] strArgs)
        {
            try
            {
                var args = ParseArguments(strArgs);

                if (args.Actions.Count == 0)
                {
                    throw new Exception("No actions provided");
                }

                while (args.Loop != LoopMode.None)
                {
                    foreach (var action in args.Actions)
                    {
                        switch (action.Action)
                        {
                            case ActionEnum.UBootFatLoad:
                                ExecuteUBootFatLoad(action.Params, args);
                                break;
                            case ActionEnum.LoadCDC:
                                ExecuteCDCLoad(action.Params);
                                break;
                        }
                    }

                    Console.WriteLine("Done");

                    if (args.Loop == LoopMode.Manual)
                    {
                        Console.WriteLine("Press ENTER to restart");

                        var key = Console.ReadKey();
                        if (key.Key != ConsoleKey.Enter)
                        {
                            break;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine("An error occurred: " + ex.Message);

                Console.ForegroundColor = ConsoleColor.Gray;
                Console.WriteLine(ex.StackTrace);
            }
        }

        private static Args ParseArguments(string[] args)
        {
            var ret = new Args();

            Array.Reverse(args);
            Stack<string> argsStack = new Stack<string>(args);
            ret.Actions = new List<LoaderAction>();

            LoaderAction currentAction = null;
            while (argsStack.Count > 0)
            {
                string currentArg = argsStack.Pop();

                if (currentArg.StartsWith("--"))
                {
                    if (currentAction != null)
                    {
                        ret.Actions.Add(currentAction);
                        currentAction = null;
                    }

                    switch (currentArg.Substring(2))
                    {
                        case "loop":
                            if (argsStack.Peek().StartsWith("--"))
                            {
                                ret.Loop = LoopMode.Manual;
                            }
                            else
                            {
                                string modeString = argsStack.Pop();
                                if (modeString == "on-uboot")
                                {
                                    ret.Loop = LoopMode.UBoot;
                                }
                                else
                                {
                                    throw new Exception($"Unknown loop mode '{modeString}'");
                                }
                            }
                            break;

                        case "uboot-fatload":
                            currentAction = new LoaderAction();
                            currentAction.Action = ActionEnum.UBootFatLoad;
                            break;
                        case "cdc":
                            currentAction = new LoaderAction();
                            currentAction.Action = ActionEnum.LoadCDC;
                            break;
                        default:
                            throw new Exception($"Unknown action: '{currentArg.Substring(2)}'");
                    }
                }
                else if (currentAction == null)
                {
                    throw new Exception($"Invalid argument '{currentArg}', expected action");
                }
                else
                {
                    currentAction.Params.Add(currentArg);
                }
            }

            if (currentAction != null)
            {
                ret.Actions.Add(currentAction);
            }

            return ret;
        }

        private static PortLike WaitForComPort(string port, int baud, int timeoutMs)
        {
            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTimeOffset.UtcNow < timeout)
            {
                try
                {
                    ComPort com = new ComPort(port, baud);

                    return com;
                }
                catch (FileNotFoundException)
                {
                    Thread.Sleep(250);

                    continue;
                }
            }

            return null;
        }

        private static PortLike WaitForTcp(string address, int port, int timeoutMs)
        {
            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTimeOffset.UtcNow < timeout)
            {
                try
                {
                    TcpPort tcp = new TcpPort(address, port);

                    return tcp;
                }
                catch (SocketException)
                {
                    Thread.Sleep(250);

                    continue;
                }
            }

            return null;
        }

        private static PortLike WaitForPort(string portParam, int timeoutMs)
        {
            string[] param = portParam.Split(':');

            if (param.Length == 1)
            {
                return WaitForComPort(param[0], 115200, timeoutMs);
            }
            else if (param.Length == 2)
            {
                return WaitForComPort(param[0], Convert.ToInt32(param[1]), timeoutMs);
            }
            else if (param.Length == 3)
            {
                switch (param[0])
                {
                    case "serial":
                        return WaitForComPort(param[1], Convert.ToInt32(param[2]), timeoutMs);
                    case "tcp":
                        return WaitForTcp(param[1], Convert.ToInt32(param[2]), timeoutMs);
                }
            }
            else
            {
                throw new Exception($"Unknown port parameters '{portParam}'");
            }

            return null;
        }

        private static bool FindPattern(ref byte[] buffer, int bufferLength, byte[] pattern)
        {
            if (bufferLength < pattern.Length)
            {
                return false;
            }

            int end = buffer.Length - pattern.Length;
            for (int x = 0; x < end; x++)
            {
                // Compare
                bool compare = true;
                for (int y = 0; y < pattern.Length; y++)
                {
                    if (buffer[x + y] != pattern[y])
                    {
                        compare = false;
                        break;
                    }
                }

                if (compare)
                {
                    return true;
                }
            }

            return false;
        }

        private static void ExecuteUBootFatLoad(List<string> args, Args cliArgs)
        {
            if (args.Count != 4)
            {
                throw new Exception("Invalid args");
            }

            byte[] ubootPrompt = new byte[] { (byte)'=', (byte)'>' };
            byte[] ubootHeader = new byte[] { (byte)'U', (byte)'-', (byte)'B', (byte)'o', (byte)'o', (byte)'t' };

            byte[] buffer = new byte[128];
            int bufferPointer = 0;

            string port = args[0];

            string ubootDisk = args[1];
            string ubootDiskPart = args[2];
            string ubootDiskFile = args[3];

            string command = $"fatload {ubootDisk} {ubootDiskPart} 80000000 {ubootDiskFile}; go 80000000;\n";
            Console.WriteLine($"[UBOOT] Waiting for UBoot on port '{port}'");

            PortLike serial = WaitForPort(port, PORT_TIMEOUT_MS);
            if (serial == null)
            {
                throw new Exception($"[UBOOT] Port not found '{port}'");
            }

            Console.WriteLine("[UBOOT] Got serial");

            if (cliArgs.Loop == LoopMode.UBoot)
            {
                Console.WriteLine("[UBOOT] Waiting for 'U-Boot'");

                while (true)
                {
                    Thread.Sleep(50);

                    bufferPointer += serial.Read(buffer, bufferPointer, buffer.Length - bufferPointer);
                    if (FindPattern(ref buffer, bufferPointer, ubootHeader))
                    {
                        break;
                    }
                    else if (bufferPointer > ubootHeader.Length)
                    {
                        int from = Math.Max(0, bufferPointer - ubootHeader.Length);
                        int len = buffer.Length - from;
                        Array.Copy(buffer, from, buffer, 0, len);
                        bufferPointer = len;
                    }
                }

                Console.WriteLine("[UBOOT] Got 'U-Boot'");
            }

            Array.Clear(buffer);
            bufferPointer = 0;

            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(UBOOT_PROMPT_TIMEOUT);
            bool hasPrompt = false;
            while (DateTimeOffset.UtcNow < timeout && !hasPrompt)
            {
                serial.Write(new byte[] { 0x0D }, 0, 1); // CR
                Thread.Sleep(50);

                bufferPointer += serial.Read(buffer, bufferPointer, buffer.Length - bufferPointer);
                if (FindPattern(ref buffer, bufferPointer, ubootPrompt))
                {
                    hasPrompt = true;
                    break;
                }
                else if (bufferPointer > ubootPrompt.Length)
                {
                    int from = Math.Max(0, bufferPointer - ubootPrompt.Length);
                    int len = buffer.Length - from;
                    Array.Copy(buffer, from, buffer, 0, buffer.Length - from);
                    bufferPointer = len;
                }
            }

            if (!hasPrompt)
            {
                throw new Exception("Failed to obtain UBoot prompt");
            }

            Console.WriteLine("[UBOOT] Got prompt");

            Console.Write("[UBOOT] " + command);
            Console.WriteLine("[UBOOT] Done");

            serial.Write(command);
            serial.Write(new byte[] { 0x0D }, 0, 1); // Execute
            serial.Flush();

            serial.Close();
        }

        private static void ExecuteCDCLoad(List<string> args)
        {
            string portName = args[0];
            string file = args[1];

            if (!File.Exists(file))
            {
                throw new Exception($"File not found: {file}");
            }

            Console.WriteLine($"[CDC] Waiting for serial '{portName}'");

            PortLike port = WaitForPort(portName, PORT_TIMEOUT_MS);
            if (port == null) 
            {
                throw new Exception($"[CDC] Port not found '{portName}'");
            }

            Console.Write("[CDC] Transfering file");

            var sw = Stopwatch.StartNew();

            long tx = 0;
            using (FileStream inputFile = new FileStream(file, FileMode.Open))
            {
                long fileLength = inputFile.Length;
                if (fileLength > UInt32.MaxValue)
                {
                    throw new Exception($"Input file is larger than max! {fileLength}");
                }

                // Send the EP3COMMAND_UPLOAD_TO_RAM command
                byte[] commandBuffer = new byte[] { 0xA1, (byte)(fileLength & 0xFF), (byte)((fileLength >> 8) & 0xFF), (byte)((fileLength >> 16) & 0xFF), (byte)((fileLength >> 24) & 0xFF) };
                port.Write(commandBuffer, 0, commandBuffer.Length);
                port.Flush();

                // Transfer data in 64byte chunks
                byte[] buffer = new byte[256];
                while (inputFile.Position < inputFile.Length)
                {
                    int readCount = inputFile.Read(buffer, 0, buffer.Length);

                    // Write data packet
                    port.Write(buffer, 0, readCount);
                    port.Flush();

                    Console.Write(".");
                }

                tx = inputFile.Length;
            }

            sw.Stop();

            Console.WriteLine();
            Console.WriteLine($"[CDC] Sent {tx} bytes in {sw.ElapsedMilliseconds}ms ({(tx / sw.ElapsedMilliseconds) * 1000} bit/s)");

            // Done
            //Console.WriteLine("[CDC] Press enter to boot");
            //Console.ReadLine();

            // Send the EP3COMMAND_EXECUTE command
            port.Write((byte)0xB1);

            try
            {
                port.Flush();
                port.Close();
            }
            catch
            {
                // Once the new code starts USB drops
            }
        }
    }
}
