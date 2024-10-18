using System.Diagnostics;
using System.IO.Ports;
using System.Threading;

namespace CLI
{
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

        class Args
        {
            public bool Loop;

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

                while (args.Loop)
                {
                    foreach (var action in args.Actions)
                    {
                        switch (action.Action)
                        {
                            case ActionEnum.UBootFatLoad:
                                ExecuteUBootFatLoad(action.Params);
                                break;
                            case ActionEnum.LoadCDC:
                                ExecuteCDCLoad(action.Params);
                                break;
                        }
                    }

                    Console.WriteLine("Done");

                    if (args.Loop)
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
                            ret.Loop = true;
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

        private static SerialPort WaitForPort(string portName, int baud, int timeoutMs)
        {
            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTimeOffset.UtcNow < timeout)
            {
                try
                {
                    SerialPort port = new SerialPort(portName, baud);
                    port.Open();

                    return port;
                }
                catch (FileNotFoundException)
                {
                    Thread.Sleep(250);

                    continue;
                }
            }

            return null;
        }

        private static void ExecuteUBootFatLoad(List<string> args)
        {
            if (args.Count != 4)
            {
                throw new Exception("Invalid args");
            }

            string port = args[0];

            string ubootDisk = args[1];
            string ubootDiskPart = args[2];
            string ubootDiskFile = args[3];

            string command = $"fatload {ubootDisk} {ubootDiskPart} 80000000 {ubootDiskFile}; go 80000000;";
            Console.WriteLine($"[UBOOT] Waiting for UBoot on port '{port}'");

            SerialPort serial = WaitForPort(port, 115200, PORT_TIMEOUT_MS);
            if (serial == null)
            {
                throw new Exception($"[UBOOT] Port not found '{port}'");
            }

            Console.WriteLine("[UBOOT] Got serial, entering prompt");

            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(UBOOT_PROMPT_TIMEOUT);
            byte[] promptBuffer = new byte[128];
            int promptBufferPointer = 0;
            bool hasPrompt = false;
            while (DateTimeOffset.UtcNow < timeout && !hasPrompt)
            {
                serial.Write(new byte[] { 0x0D }, 0, 1); // CR
                Thread.Sleep(50);

                promptBufferPointer += serial.Read(promptBuffer, promptBufferPointer, promptBuffer.Length - promptBufferPointer);

                if (promptBufferPointer > 1) // At least 2 bytes in the buffer
                {
                    for (int x = 0; x < promptBufferPointer - 1; x++)
                    {
                        if (promptBuffer[x] == '=' && promptBuffer[x + 1] == '>')
                        {
                            // Found prompt
                            hasPrompt = true;
                            break;
                        }
                    }

                    if (hasPrompt)
                    {
                        break;
                    }

                    promptBuffer[0] = promptBuffer[promptBufferPointer - 1];
                    promptBufferPointer = 1;
                }
            }

            if (!hasPrompt)
            {
                throw new Exception("Failed to obtain UBoot prompt");
            }

            Console.WriteLine("[UBOOT] " + command);
            Console.WriteLine("[UBOOT] Done");

            serial.Write(command);
            serial.Write(new byte[] { 0x0D }, 0, 1); // Execute
            serial.BaseStream.Flush();

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

            SerialPort port = WaitForPort(portName, 115200, PORT_TIMEOUT_MS);
            if (port == null) 
            {
                throw new Exception($"[CDC] Port not found '{portName}'");
            }

            Console.Write("[CDC] Transfering file");

            var sw = Stopwatch.StartNew();

            long tx = 0;
            using (FileStream inputFile = new FileStream(file, FileMode.Open))
            {
                byte[] buffer = new byte[17];

                while (inputFile.Position < inputFile.Length)
                {
                    int readCount = inputFile.Read(buffer, 1, buffer.Length - 1);

                    // Write data packet
                    buffer[0] = (byte)readCount;
                    port.Write(buffer, 0, readCount + 1);

                    port.BaseStream.Flush();

                    Console.Write(".");
                    // Thread.Sleep(10);
                }

                tx = inputFile.Length;
            }

            sw.Stop();

            Console.WriteLine();
            Console.WriteLine($"[CDC] Sent {tx} bytes in {sw.ElapsedMilliseconds}ms");

            // Done
            //Console.WriteLine("[CDC] Press enter to boot");
            //Console.ReadLine();
            
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
