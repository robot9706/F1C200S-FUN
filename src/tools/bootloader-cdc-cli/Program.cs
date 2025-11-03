using System.Collections;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Formats.Tar;
using System.IO.Ports;
using System.Net;
using System.Net.Sockets;
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
        public abstract void SendReset();
    }

    class ComPort : PortLike
    {
        private SerialPort _com;

        public ComPort(string com, int baud)
        { 
            _com = new SerialPort(com, baud);
            _com.Open();
            _com.DtrEnable = false;
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

        public override void SendReset()
        {
            _com.DtrEnable = true;
            Thread.Sleep(250);
            _com.DtrEnable = false;
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

        public override void SendReset()
        {
            throw new NotSupportedException();
        }
    }

    class PortTCPMultiplexer : PortLike
    {
        private bool _running;

        private PortLike _source;

        private TcpListener _listener;

        private ByteQueue _readBuffer;

        private object _tcpLock;
        private List<Stream> _tcpStreams;

        private ManualResetEventSlim _waitClient;

        public PortTCPMultiplexer(PortLike source, int port)
        {
            _running = true;

            _source = source;

            _tcpLock = new object();
            _tcpStreams = new List<Stream>();

            _waitClient = new ManualResetEventSlim(false);

            _readBuffer = new ByteQueue(1024);

            _listener = new TcpListener(IPAddress.Parse("127.0.0.1"), port);
            _listener.Start();

            Console.WriteLine($"[TCP] Listening on 127.0.0.1:{port}");

            StartReader();
            StartAcceptClient();
        }

        public override void Close()
        {
            _running = false;

            _listener.Stop();

            if (_source != null)
            {
                _source.Close();
                _source = null;
            }
        }

        public override void Flush()
        {
            _readBuffer.Clear();

            if (_source == null)
            {
                throw new Exception();
            }

            _source.Flush();
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            if (_source == null)
            {
                throw new Exception();
            }

            int ret;
            lock (_readBuffer)
            {
                ret = _readBuffer.Dequeue(buffer, offset, count);
            }

            return ret;
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            if (_source == null)
            {
                throw new Exception();
            }

            _source.Write(buffer, offset, count);
        }

        public override void SendReset()
        {
            if (_source == null)
            {
                throw new Exception();
            }

            _source.SendReset();
        }

        private void StartAcceptClient()
        {
            _listener.BeginAcceptTcpClient(OnAcceptClient, null);
        }

        private void OnAcceptClient(IAsyncResult result)
        {
            if (!_running)
            {
                return;
            }

            TcpListener listener = (TcpListener)result.AsyncState;
            TcpClient client = _listener.EndAcceptTcpClient(result);

            StartAcceptClient(); // Accept next client

            NetworkStream tcpStream = client.GetStream();
            lock (_tcpLock)
            {
                _tcpStreams.Add(tcpStream);

                _waitClient.Set();
            }

            string remote = client.Client.RemoteEndPoint.ToString();
            Console.WriteLine($"[TCP] Client connected from {remote}");

            NetworkStream stream = client.GetStream();
            byte[] buffer = new byte[1024];
            while (client.Client != null && client.Client.Connected)
            {
                try
                {
                    IAsyncResult readAsync = stream.BeginRead(buffer, 0, buffer.Length, null, null);
                    result.AsyncWaitHandle.WaitOne();
                    int readCount = stream.EndRead(readAsync);

                    if (_source == null)
                    {
                        break;
                    }
                    else
                    {
                        _source.Write(buffer, 0, readCount);
                    }
                }
                catch (IOException ioex)
                {
                    break;
                }
            }

            try
            {
                client.Close();
            }
            catch
            { }

            Console.WriteLine($"[TCP] Client disconnected from {remote}");

            lock (_tcpLock)
            {
                _tcpStreams.Remove(tcpStream);

                if (_tcpStreams.Count <= 0)
                {
                    _waitClient.Reset();
                }
            }
        }

        public void WaitForClient()
        {
            _waitClient.Wait();
        }

        private void StartReader()
        {
            Task.Factory.StartNew(() =>
            {
                try
                {
                    byte[] buffer = new byte[1024];
                    while (_running)
                    {
                        int ret = _source.Read(buffer, 0, buffer.Length);
                        if (ret <= 0)
                        {
                            continue;
                        }

                        lock (_readBuffer)
                        {
                            _readBuffer.Enqueue(buffer, 0, ret);
                        }

                        lock (_tcpLock)
                        {
                            foreach (var tcpStream in _tcpStreams)
                            {
                                try
                                {
                                    tcpStream.Write(buffer, 0, ret);
                                }
                                catch (Exception ex) { }
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Port failed '{ex.Message}'");
                }
                finally
                {
                    _source = null;
                }
            }, TaskCreationOptions.LongRunning);
        }
    }

    internal class Program
    {
        static int PORT_TIMEOUT_MS = 5000;
        static int UBOOT_PROMPT_TIMEOUT = 10000;

        public enum ResetMode
        {
            None, // Program exits afte completes the boot process
            Manual, // Program waits for user input then resets the port and loops
            UBoot, // Program waits for UBoot string and loops
        }

        class Args
        {
            [Required]
            public ResetMode ResetMode = ResetMode.None;

            [Required]
            public string UBootPort; // Port where UBoot CLI is accessible

            [AllowNull]
            public string[] UBootFatloadCommand; // UBoot fatload command parameters

            [AllowNull]
            public int? UBootPortServePort = null; // UBoot port TCP server port

            [AllowNull]
            public bool UBootPortServeWait = false; // Wait for a connection before continuing

            [AllowNull]
            public string BL2Port; // Bootloader second stage port

            [AllowNull]
            public string BL2LoadFile; // File to load using BL2
        }

        static void Main(string[] strArgs)
        {
            try
            {
                var args = ParseArguments(strArgs);

                Console.Write("Waiting for UBoot port...");
                PortLike ubootPort = WaitForPort(args.UBootPort, PORT_TIMEOUT_MS);
                Console.WriteLine("Connected");

                if (args.UBootPortServePort.HasValue)
                {
                    ubootPort = new PortTCPMultiplexer(ubootPort, args.UBootPortServePort.Value);

                    if (args.UBootPortServeWait)
                    {
                        Console.WriteLine("Waiting for a TCP client to connect...");
                        ((PortTCPMultiplexer)ubootPort).WaitForClient();
                    }
                }

                while (args.ResetMode != ResetMode.None)
                {
                    if (ExecuteUBootFatLoad(ubootPort, args.UBootFatloadCommand, args.ResetMode))
                    {
                        if (!string.IsNullOrEmpty(args.BL2Port) && !string.IsNullOrEmpty(args.BL2LoadFile))
                        {
                            ExecuteCDCLoad(args.BL2Port, args.BL2LoadFile);
                        }

                        Console.WriteLine("Done");
                    }
                    else
                    {
                        Console.WriteLine("UBoot failed");
                    }

                    if (args.ResetMode == ResetMode.Manual)
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

        private static string[] ConsumeArguments(Stack<string> args)
        {
            // Consumes arguments until a new command (starting with -- is found)
            List<string> parts = new List<string>();

            while (args.Count > 0)
            {
                string arg = args.Peek();
                if (arg.StartsWith("--"))
                {
                    break;
                }

                args.Pop();

                parts.Add(arg);
            }

            return parts.ToArray();
        }

        private static ResetMode MapResetMode(string mode)
        {
            switch (mode)
            {
                case "manual":
                    return ResetMode.Manual;
                case "on-uboot":
                    return ResetMode.UBoot;
                case "none":
                    return ResetMode.None;
            }

            throw new Exception($"Unknown reset mode '{mode}'");
        }

        private static Args ParseArguments(string[] args)
        {
            var ret = new Args();

            Array.Reverse(args);
            Stack<string> argsStack = new Stack<string>(args);

            while (argsStack.Count > 0)
            {
                string currentArg = argsStack.Pop();
                if (!currentArg.StartsWith("--"))
                {
                    throw new Exception($"Invalid argument '{currentArg}'");
                }

                string[] argParams = ConsumeArguments(argsStack);

                switch (currentArg.Substring(2))
                {
                    case "reset":
                        if (argParams.Length != 1)
                        {
                            throw new Exception("--reset expects one parameter");
                        }

                        string mode = argParams[0];

                        ret.ResetMode = MapResetMode(mode);
                        break;

                    case "uboot-port":
                        if (argParams.Length != 1)
                        {
                            throw new Exception("--uboot-port expects one parameter");
                        }

                        ret.UBootPort = argParams[0];
                        break;

                    case "uboot-port-serve":
                        if (argParams.Length < 1 || argParams.Length > 2)
                        {
                            throw new Exception("--uboot-port-serve expects a TCP port");
                        }

                        int port;
                        if (!Int32.TryParse(argParams[0], out port))
                        {
                            throw new Exception($"Invalid TCP port '{argParams[0]}'");
                        }

                        ret.UBootPortServePort = port;

                        if (argParams.Length == 2)
                        {
                            if (argParams[1] == "wait")
                            {
                                ret.UBootPortServeWait = true;
                            }
                            else
                            {
                                throw new Exception("Unknown extra arg for --uboot-port-serve [port] {wait}");
                            }
                        }
                        break;

                    case "uboot-fatload":
                        if (argParams.Length != 3)
                        {
                            throw new Exception("--uboot-fatload expects 3 parameters");
                        }

                        ret.UBootFatloadCommand = argParams;
                        break;

                    case "bl2":
                        if (argParams.Length != 2)
                        {
                            throw new Exception("--bl2 expects the BL2 CDC port and the file to load");
                        }

                        ret.BL2Port = argParams[0];
                        ret.BL2LoadFile = argParams[1];
                        break;

                    default:
                        throw new Exception($"Unknown argument: '{currentArg.Substring(2)}'");
                }
            }

            List<ValidationResult> validation = new List<ValidationResult>();
            if (!Validator.TryValidateObject(ret, new ValidationContext(ret), validation))
            {
                Console.WriteLine("Error in following options:");
                foreach (var err in validation)
                {
                    Console.WriteLine($"{String.Join(",", err.MemberNames)}: {err.ErrorMessage}");
                }

                throw new Exception("Invalid options");
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

        private static bool ExecuteUBootFatLoad(PortLike ubootPort, string[] fatload, ResetMode resetMode)
        {
            byte[] ubootPrompt = new byte[] { (byte)'=', (byte)'>' };
            byte[] ubootHeader = new byte[] { (byte)'U', (byte)'-', (byte)'B', (byte)'o', (byte)'o', (byte)'t' };

            byte[] buffer = new byte[128];
            int bufferPointer = 0;

            string ubootDisk = fatload[0];
            string ubootDiskPart = fatload[1];
            string ubootDiskFile = fatload[2];

            string command = $"fatload {ubootDisk} {ubootDiskPart} 80000000 {ubootDiskFile}; go 80000000;\n";
            
            if (resetMode == ResetMode.Manual)
            {
                Console.WriteLine("[UBOOT] Reset");

                ubootPort.SendReset();
            }

            if (resetMode == ResetMode.UBoot)
            {
                Console.WriteLine("[UBOOT] Waiting for 'U-Boot'");

                ubootPort.Flush();

                while (true)
                {
                    Thread.Sleep(50);

                    bufferPointer += ubootPort.Read(buffer, bufferPointer, buffer.Length - bufferPointer);
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

            ubootPort.Flush();

            Console.WriteLine("[UBOOT] Waiting for prompt");

            DateTimeOffset timeout = DateTimeOffset.UtcNow.AddMilliseconds(UBOOT_PROMPT_TIMEOUT);
            bool hasPrompt = false;
            while (DateTimeOffset.UtcNow < timeout && !hasPrompt)
            {
                ubootPort.Write(new byte[] { 0x0D }, 0, 1); // CR
                Thread.Sleep(50);

                bufferPointer += ubootPort.Read(buffer, bufferPointer, buffer.Length - bufferPointer);
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
                Console.WriteLine("Failed to obtain UBoot prompt");
                return false;
            }

            Console.WriteLine("[UBOOT] Got prompt");

            Console.Write("[UBOOT] " + command);
            Console.WriteLine("[UBOOT] Done");

            ubootPort.Write(command);
            ubootPort.Write(new byte[] { 0x0D }, 0, 1); // Execute
            ubootPort.Flush();

            return true;
        }

        private static void ExecuteCDCLoad(string cdcPort, string cdcFile)
        {
            if (!File.Exists(cdcFile))
            {
                Console.WriteLine($"[BL2] File not found: {cdcFile}");
                return;
            }

            Console.WriteLine($"[BL2] Waiting for serial '{cdcPort}'");

            PortLike port = WaitForPort(cdcPort, PORT_TIMEOUT_MS);
            if (port == null) 
            {
                Console.WriteLine($"[BL2] Port not found '{cdcPort}'");
                return;
            }

            Console.Write("[BL2] Transfering file");

            var sw = Stopwatch.StartNew();

            long tx = 0;
            using (FileStream inputFile = new FileStream(cdcFile, FileMode.Open))
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
            Console.WriteLine($"[BL2] Sent {tx} bytes in {sw.ElapsedMilliseconds}ms ({(tx / sw.ElapsedMilliseconds) * 1000} bit/s)");

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
