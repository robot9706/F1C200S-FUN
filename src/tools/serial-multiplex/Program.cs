using System.IO.Ports;
using System.Net;
using System.Net.Sockets;

namespace serial_multiplex
{
    class Args
    {
        public string BindAddress = "127.0.0.1";
        public int BindPort = 5000;

        public string Port;
        public int BaudRate = 115200;
    }

    internal class Program
    {
        static bool _running = true;

        static object _tcpLock = new object();
        static List<Stream> _tcpStreams = new List<Stream>();
        static SerialPort _serialPort;

        static void Log(ConsoleColor color, string message)
        {
            Console.ForegroundColor = color;
            Console.WriteLine(message);
        }

        static Args ParseArgs(string[] args)
        {
            Args obj = new Args();

            Queue<string> argStack = new Queue<string>(args);
            while (argStack.Count > 0)
            {
                string param = argStack.Dequeue();
                if (!param.StartsWith("--"))
                {
                    throw new Exception($"Invalid argument '{param}'");
                }

                param = param.Substring(2);

                if (argStack.Count == 0)
                {
                    throw new Exception($"Argument '{param}' requires a value");
                }

                string value = argStack.Dequeue();

                switch (param)
                {
                    case "ip":
                        obj.BindAddress = value;
                        break;
                    case "port":
                        obj.BindPort = Convert.ToInt32(value);
                        break;
                    case "com":
                        obj.Port = value;
                        break;
                    case "baud":
                        obj.BaudRate = Convert.ToInt32(value);
                        break;

                    default:
                        throw new Exception($"Unknown parameter '{param}'");
                }
            }

            return obj;
        }

        static void OnAcceptClient(IAsyncResult result)
        {
            if (!_running)
            {
                return;
            }

            TcpListener listener = (TcpListener)result.AsyncState;
            TcpClient client = listener.EndAcceptTcpClient(result);

            StartAcceptClient(listener); // Accept next client

            NetworkStream tcpStream = client.GetStream();
            lock (_tcpLock)
            {
                _tcpStreams.Add(tcpStream);
            }

            string remote = client.Client.RemoteEndPoint.ToString();
            Log(ConsoleColor.Gray, $"Client connected from {remote}");

            NetworkStream stream = client.GetStream();
            byte[] buffer = new byte[1024];
            while (client.Client != null && client.Client.Connected)
            {
                try
                {
                    IAsyncResult readAsync = stream.BeginRead(buffer, 0, buffer.Length, null, null);
                    result.AsyncWaitHandle.WaitOne();
                    int readCount = stream.EndRead(readAsync);

                    if (_serialPort != null)
                    {
                        _serialPort.Write(buffer, 0, readCount);
                    }
                }
                catch (IOException ioex)
                {
                    client.Close();
                }
            }

            Log(ConsoleColor.Gray, $"Client disconnected from {remote}");

            lock (_tcpLock)
            {
                _tcpStreams.Remove(tcpStream);
            }
        }

        static void StartAcceptClient(TcpListener listener)
        {
            Log(ConsoleColor.Gray, "Waiting for client");
            listener.BeginAcceptTcpClient(OnAcceptClient, listener);
        }

        static void StartSerial(Args args)
        {
            Task.Factory.StartNew(() =>
            {
                while (_running)
                {
                    _serialPort = null;

                    try
                    {
                        SerialPort openPort = new SerialPort(args.Port, args.BaudRate);
                        openPort.Open();

                        _serialPort = openPort;
                    }
                    catch (Exception ex)
                    {
                        // Port open failed
                        Task.Delay(500).Wait();
                        continue;
                    }

                    Log(ConsoleColor.Gray, $"Opened serial port {args.Port} with baud {args.BaudRate}");

                    try
                    {
                        byte[] buffer = new byte[1024];
                        while (_serialPort.IsOpen)
                        {
                            IAsyncResult readAsync = _serialPort.BaseStream.BeginRead(buffer, 0, buffer.Length, null, null);
                            readAsync.AsyncWaitHandle.WaitOne();
                            int readCount = _serialPort.BaseStream.EndRead(readAsync);

                            lock (_tcpLock)
                            {
                                foreach (var tcpStream in _tcpStreams)
                                {
                                    try
                                    {
                                        tcpStream.Write(buffer, 0, readCount);
                                    }
                                    catch (Exception ex) { }
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Log(ConsoleColor.Gray, $"Serial port failed '{ex.Message}'");
                    }

                    _serialPort = null;
                }
            }, TaskCreationOptions.LongRunning);
        }

        static void Main(string[] strArgs)
        {
            Args args = ParseArgs(strArgs);
            if (string.IsNullOrEmpty(args.Port) || args.BaudRate < 1)
            {
                throw new Exception($"Invalid arguments!");
            }

            TcpListener listener = new TcpListener(IPAddress.Parse(args.BindAddress), args.BindPort);
            listener.Start();
            Log(ConsoleColor.Gray, $"Listening on {args.BindAddress}:{args.BindPort}");
            Log(ConsoleColor.Gray, "Press enter to exit");

            StartAcceptClient(listener);
            StartSerial(args);

            Console.ReadLine();

            _running = false;
            if (_serialPort != null)
            {
                _serialPort.Close();
            }
            listener.Stop();
        }
    }
}
