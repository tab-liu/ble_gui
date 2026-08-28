using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Foundation;
using Windows.Storage.Streams;
using Org.BouncyCastle.Asn1.Sec;
using Org.BouncyCastle.Asn1.X9;
using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Agreement;
using Org.BouncyCastle.Crypto.Generators;
using Org.BouncyCastle.Crypto.Modes;
using Org.BouncyCastle.Crypto.Engines;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Crypto.Signers;
using Org.BouncyCastle.Math;
using Org.BouncyCastle.Security;

internal sealed class BluettiBleBridge
{
    private const int MaxDevices = 256;
    private static readonly Guid ServiceUuid = new Guid("0000ff00-0000-1000-8000-00805f9b34fb");
    private static readonly Guid WriteUuid = new Guid("0000ff02-0000-1000-8000-00805f9b34fb");
    private static readonly Guid NotifyUuid = new Guid("0000ff01-0000-1000-8000-00805f9b34fb");
    private static readonly Guid NotifyUuidNew = new Guid("0000ff03-0000-1000-8000-00805f9b34fb");

    /* 来自用户旧版上位机 core.comm_protocols.ble.gatt.py 的同一套认证参数。 */
    private static readonly byte[] AesKey = HexToBytesStatic("459FC535808941F17091E0993EE3E93D");
    private static readonly byte[] PrivateKeyL1 = HexToBytesStatic("4F19A16E3E87BDD9BD24D3E5495B88041511943CBC8B969ADE9641D0F56AF337");
    private static readonly byte[] PublicKeyK2 = HexToBytesStatic("A73ABF5D2232C8C1C72E68304343C272495E3A8FD6F30EA96DE2F4B3CE60B251EE21AC667CF8A71E18B46B664EAEFFE3C489F24F695B6411DB7E22CCC85A8594");

    private sealed class ScanRecord
    {
        public ulong Address;
        public int AddressType;
        public int Rssi;
        public string Name;
        public DateTime LastSeenUtc;
        public bool NameResolveStarted;
    }

    private sealed class ResponsePacket
    {
        public byte[] Raw;
        public byte[] Plain;
    }
    private sealed class OtaManifestItem
    {
        public int Index;
        public int Chip;
        public byte FirmwareType;
        public uint Version;
        public uint ImageSize;
        public long FileSize;
        public string Path;
        public string Name;
        public int State; /* 0 pending, 1 running, 2 success, 3 failed */
        public int Progress;      /* 总进度：PC->IOT占前50%，IOT分发占后50% */
        public int PcProgress;
        public int DeviceProgress;
        public int DistributionSlot = -1;
        public int DistributionDepth;
        public int DistributionError;
        public ushort OtaGroup;
        public string Message;
    }

    private sealed class OtaManifest
    {
        public int GapSeconds = 18;
        public int FailureTimeoutSeconds = 30;
        public int ChannelMode = 0; /* 0 auto, 1 encrypted, 2 plain */
        public int SlaveId = 1;
        public ulong Address;
        public int AddressType = 2;
        public readonly List<OtaManifestItem> Items = new List<OtaManifestItem>();
    }

    private sealed class NotificationItem
    {
        public byte[] Data;
        public Guid SourceUuid;
    }

    private readonly string _commandPath;
    private readonly string _devicePath;
    private readonly string _statusPath;
    private readonly string _dataPath;
    private readonly string _manualModbusPath;
    private readonly string _trafficPath;
    private readonly string _otaStatusPath;
    private readonly string _baseDirectory;
    private readonly string _logPath;

    private System.Windows.Forms.Timer _timer;
    private BluetoothLEAdvertisementWatcher _watcher;
    private readonly object _scanLock = new object();
    private readonly Dictionary<ulong, ScanRecord> _scanRecords = new Dictionary<ulong, ScanRecord>();
    private bool _scanActive;
    private bool _snapshotDirty;
    private DateTime _lastSnapshotUtc = DateTime.MinValue;

    private BluetoothLEDevice _device;
    private string _connectStage = "IDLE";
    private GattSession _gattSession;
    private readonly AutoResetEvent _gattSessionActiveEvent = new AutoResetEvent(false);
    private GattDeviceService _service;
    private GattCharacteristic _writeCharacteristic;
    private GattCharacteristic _notifyCharacteristic;
    private GattCharacteristic _notifyCharacteristicFf03;
    private object _connection;
    private ulong _selectedAddress;
    private string _selectedName = string.Empty;
    private bool _connectPending;
    private int _connectedServiceCount;
    private bool _gattReadyReported;
    private bool _gattWaitLogged;
    private DateTime _connectStartedUtc = DateTime.MinValue;
    private bool _exiting;

    private readonly object _notificationLock = new object();
    private readonly object _notificationQueueLock = new object();
    private readonly object _gattWriteLock = new object();
    private readonly object _sendLock = new object();
    private readonly Queue<NotificationItem> _notificationQueue = new Queue<NotificationItem>();
    private readonly AutoResetEvent _notificationEvent = new AutoResetEvent(false);
    private readonly Queue<ResponsePacket> _responses = new Queue<ResponsePacket>();
    private readonly AutoResetEvent _responseEvent = new AutoResetEvent(false);
    private Thread _notificationThread;
    private int _connectBusy;
    private readonly List<byte> _businessAccumulator = new List<byte>();
    private readonly List<byte> _handshakeAccumulator = new List<byte>();
    private string _handshakePrefix = string.Empty;

    private bool _authStarted;
    private bool _encryptionRequired;
    private bool _authSucceeded;
    private bool _keyExchangeStatus;
    private bool _encryptionReady;
    private byte[] _md5Hash;
    private byte[] _newAesKey;
    private byte[] _iotPublicKey;
    private byte[] _sharedKey;
    private ECPrivateKeyParameters _ephemeralPrivateKey;
    private ECDomainParameters _ecDomain;
    private byte[] _lastAirTx = new byte[0];
    private byte[] _lastAirRx = new byte[0];
    private byte[] _lastDecodedRx = new byte[0];

    private long _lastSequence = -1;
    private int _pollIntervalSeconds = 2;
    private DateTime _nextPollUtc = DateTime.MaxValue;
    private int _modbusBusy;
    private long _dataSequence;
    private int _modbusSlaveId = 1;
    private DateTime _nextDeviceInfoReadUtc = DateTime.MinValue;
    private string _deviceTypeText = "--";
    private string _deviceSnText = "--";
    private string _deviceVersionsText = "--";
    private int _pendingWriteAddress = -1;
    private int _pendingWriteValue;
    private string _pendingWriteName = string.Empty;
    private int _lastAcState = -1;
    private int _lastDcState = -1;
    private int _pendingManualSlave = -1;
    private int _pendingManualFunction = -1;
    private int _pendingManualRegister;
    private int _pendingManualParameter;
    private int _pendingManualTimeout;
    private long _manualSequence;
    private long _otaStatusSequence;
    private volatile bool _otaRunning;
    private volatile bool _otaCancelRequested;
    private bool _otaRawReceiveActive;
    private bool _otaRawEncrypted;
    private bool _otaStartControlPassthrough;
    private Thread _otaThread;
    private readonly Queue<byte> _otaControlBytes = new Queue<byte>();

    public BluettiBleBridge(string commandPath, string devicePath, string statusPath, string dataPath, string manualModbusPath, string trafficPath, string otaStatusPath, string baseDirectory)
    {
        _commandPath = commandPath;
        _devicePath = devicePath;
        _statusPath = statusPath;
        _dataPath = dataPath;
        _manualModbusPath = manualModbusPath;
        _trafficPath = trafficPath;
        _otaStatusPath = otaStatusPath;
        _baseDirectory = baseDirectory;
        string cacheDirectory = Path.Combine(Path.GetTempPath(), "BLUETTI_Firmware_Studio", "BLE");
        Directory.CreateDirectory(cacheDirectory);
        _logPath = Path.Combine(cacheDirectory, "BLE_Direct_V1.3.5.log");
        InitializeEcDomain();
    }

    public void Start()
    {
        Application.ThreadException += OnThreadException;
        AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
        WriteLog("----------------------------------------");
        WriteLog("启动 V1.3.5 Direct WinRT BLE + BLE OTA bridge");
        WriteAtomicLines(_trafficPath, new string[] { "FLOWSEQ\t0", "Direct BLE ready" });
        WriteStatus("READY\t蓝牙后台已就绪（Windows WinRT Direct BLE + 旧版加密协议）");
        _notificationThread = new Thread(NotificationWorker);
        _notificationThread.IsBackground = true;
        _notificationThread.Name = "BLUETTI-BLE-Notification";
        _notificationThread.SetApartmentState(ApartmentState.MTA);
        _notificationThread.Start();
        _timer = new System.Windows.Forms.Timer();
        _timer.Interval = 80;
        _timer.Tick += TimerTick;
        _timer.Start();
    }

    private void TimerTick(object sender, EventArgs eventArgs)
    {
        if (_exiting) { return; }
        try
        {
            ProcessCommand();
            PollConnection();
            ProcessModbusSchedule();
            DateTime now = DateTime.UtcNow;
            if (_scanActive && (_snapshotDirty || (now - _lastSnapshotUtc).TotalMilliseconds >= 350.0))
            {
                UpdateDeviceSnapshot();
                _snapshotDirty = false;
                _lastSnapshotUtc = now;
            }
        }
        catch (Exception exception)
        {
            WriteLog("后台运行异常：" + FlattenException(exception));
            WriteStatus((_connectPending ? "INFO\t" : "ERROR\t") + "蓝牙后台运行异常：" + FlattenException(exception));
        }
    }

    private void ProcessCommand()
    {
        if (!File.Exists(_commandPath)) { return; }
        string text;
        try { text = File.ReadAllText(_commandPath, Encoding.Unicode).Trim('\uFEFF', '\r', '\n', ' ', '\t'); }
        catch { return; }
        if (text.Length == 0) { return; }
        string[] parts = text.Split('\t');
        long sequence;
        if (parts.Length < 2 || !long.TryParse(parts[0], out sequence) || sequence == _lastSequence) { return; }
        _lastSequence = sequence;
        string command = parts[1].Trim().ToUpperInvariant();
        WriteLog("收到命令：" + command);

        // 只有真正开始OTA传输后才独占BLE业务通道。进入OTA页面本身不会停止正常SOC/版本轮询。
        if (_otaRunning && (command == "INTERVAL" || command == "READNOW" || command == "SLAVE" || command == "MODBUS" || command == "WRITE_AC" || command == "WRITE_DC"))
        {
            WriteLog("OTA传输中：忽略普通查询/控制命令 " + command);
            WriteStatus("INFO\tOTA升级进行中，普通Modbus查询/控制已暂停");
            return;
        }

        if (command == "START") { StartScan(); }
        else if (command == "STOP") { StopScan(); WriteStatus("IDLE\t蓝牙状态：扫描已停止"); }
        else if (command == "CONNECT" && parts.Length >= 3)
        {
            if (Interlocked.CompareExchange(ref _connectBusy, 1, 0) == 0)
            {
                string addressHex = parts[2];
                int addressType = 2;
                if (parts.Length >= 4) { int.TryParse(parts[3], out addressType); }
                if (addressType < 0 || addressType > 2) { addressType = 2; }
                WriteStatus("INFO\t正在建立 Windows Direct BLE 连接……");
                ThreadPool.QueueUserWorkItem(delegate(object state)
                {
                    try { BeginConnect(addressHex, addressType); }
                    catch (Exception exception)
                    {
                        string detail = "Stage=" + _connectStage + "，HRESULT=0x" + exception.HResult.ToString("X8") + "，" + FlattenException(exception);
                        WriteLog("连接失败：" + detail);
                        CloseConnection();
                        WriteStatus("ERROR\t蓝牙连接失败：" + detail);
                    }
                    finally { Interlocked.Exchange(ref _connectBusy, 0); }
                });
            }
            else { WriteStatus("INFO\t蓝牙连接正在处理中，请稍候"); }
        }
        else if (command == "DISCONNECT") { CloseConnection(); StopScan(); WriteDataLine("IDLE\t蓝牙数据：已断开"); WriteStatus("IDLE\t蓝牙状态：已断开"); }
        else if (command == "INTERVAL" && parts.Length >= 3)
        {
            int seconds;
            if (int.TryParse(parts[2], out seconds)) { _pollIntervalSeconds = Math.Max(1, Math.Min(10, seconds)); _nextPollUtc = DateTime.UtcNow; }
        }
        else if (command == "READNOW")
        {
            int slaveId;
            if (parts.Length >= 3 && int.TryParse(parts[2], out slaveId) && slaveId >= 0 && slaveId <= 247) { _modbusSlaveId = slaveId; }
            _nextPollUtc = DateTime.UtcNow;
        }
        else if (command == "SLAVE" && parts.Length >= 3)
        {
            int slaveId;
            if (int.TryParse(parts[2], out slaveId) && slaveId >= 0 && slaveId <= 247)
            {
                _modbusSlaveId = slaveId; _nextDeviceInfoReadUtc = DateTime.MinValue; _nextPollUtc = DateTime.UtcNow;
                WriteStatus("INFO\tModbus从机地址已设置为 " + slaveId.ToString());
            }
            else { WriteStatus("INFO\tModbus从机地址无效，范围应为0～247"); }
        }
        else if (command == "MODBUS" && parts.Length >= 7)
        {
            int slaveId, functionCode, registerAddress, parameter, timeoutMs;
            if (int.TryParse(parts[2], out slaveId) && int.TryParse(parts[3], out functionCode) && int.TryParse(parts[4], out registerAddress) && int.TryParse(parts[5], out parameter) && int.TryParse(parts[6], out timeoutMs))
            { QueueManualModbus(slaveId, functionCode, registerAddress, parameter, timeoutMs); }
            else { WriteManualResult(false, "指令参数格式错误", new byte[0], new byte[0], new byte[0], new byte[0], "", BuildBleDiagnosticSnapshot()); }
        }
        else if (command == "WRITE_AC" && parts.Length >= 3)
        {
            int slaveId; if (parts.Length >= 4 && int.TryParse(parts[3], out slaveId) && slaveId >= 0 && slaveId <= 247) { _modbusSlaveId = slaveId; }
            QueueControlWrite(2011, parts[2] == "0" ? 0 : 1, "AC输出");
        }
        else if (command == "WRITE_DC" && parts.Length >= 3)
        {
            int slaveId; if (parts.Length >= 4 && int.TryParse(parts[3], out slaveId) && slaveId >= 0 && slaveId <= 247) { _modbusSlaveId = slaveId; }
            QueueControlWrite(2012, parts[2] == "0" ? 0 : 1, "DC输出");
        }
        else if (command == "OTA_START" && parts.Length >= 3)
        {
            StartBleOta(parts[2]);
        }
        else if (command == "OTA_STOP")
        {
            _otaCancelRequested = true;
            WriteOtaStatus(null, "STOPPING", 0, 0, 0, 0, 0, 0, "正在终止 OTA……", 0);
        }
        else if (command == "EXIT") { _otaCancelRequested = true; ExitBridge(); }
    }

    private void StartScan()
    {
        CloseConnection();
        StopScan();
        lock (_scanLock) { _scanRecords.Clear(); }
        WriteAtomicLines(_devicePath, new string[0]);
        _watcher = new BluetoothLEAdvertisementWatcher();
        _watcher.ScanningMode = BluetoothLEScanningMode.Active;
        _watcher.Received += OnAdvertisementReceived;
        _watcher.Stopped += OnWatcherStopped;
        _watcher.Start();
        _scanActive = true;
        _snapshotDirty = true;
        WriteDataLine("IDLE\t蓝牙数据：等待连接设备");
        WriteStatus("SCANNING\t蓝牙状态：正在使用 Windows WinRT 直接扫描附近 BLE 设备");
        WriteLog("WinRT BluetoothLEAdvertisementWatcher.Start() 已调用，不再依赖 ComMod.dll 的 DeviceList 扫描。");
    }

    private void StopScan()
    {
        BluetoothLEAdvertisementWatcher watcher = _watcher;
        _watcher = null;
        _scanActive = false;
        if (watcher != null)
        {
            try { watcher.Stop(); } catch { }
            try { watcher.Received -= OnAdvertisementReceived; } catch { }
            try { watcher.Stopped -= OnWatcherStopped; } catch { }
        }
    }

    private void OnWatcherStopped(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementWatcherStoppedEventArgs args)
    {
        WriteLog("BLE watcher stopped，错误=" + args.Error.ToString());
    }

    private void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args)
    {
        try
        {
            string name = args.Advertisement == null ? string.Empty : args.Advertisement.LocalName;
            if (name == null) { name = string.Empty; }
            bool resolveName = false;
            lock (_scanLock)
            {
                ScanRecord record;
                if (!_scanRecords.TryGetValue(args.BluetoothAddress, out record))
                {
                    record = new ScanRecord();
                    record.Address = args.BluetoothAddress;
                    _scanRecords.Add(args.BluetoothAddress, record);
                }
                record.Rssi = (int)args.RawSignalStrengthInDBm;
                try { record.AddressType = (int)args.BluetoothAddressType; } catch { record.AddressType = 2; }
                if (!string.IsNullOrWhiteSpace(name)) { record.Name = Sanitize(name); }
                if (record.Name == null) { record.Name = string.Empty; }
                record.LastSeenUtc = DateTime.UtcNow;

                /*
                 * BleakScanner.discover() 在旧上位机中不仅依赖广播包 LocalName，
                 * 还可以从 Windows 设备对象获得名称。很多产品把 LocalName 放在
                 * Scan Response 中，因此单次 AdvertisementReceived 可能为空。
                 * 对无名称设备只启动一次异步名称解析，避免 AP200 被前缀筛选隐藏。
                 */
                if (record.Name.Length == 0 && !record.NameResolveStarted)
                {
                    record.NameResolveStarted = true;
                    resolveName = true;
                }
            }
            _snapshotDirty = true;
            if (resolveName)
            {
                ulong address = args.BluetoothAddress;
                ThreadPool.QueueUserWorkItem(delegate(object state) { ResolveAdvertisedDeviceName(address); });
            }
        }
        catch (Exception exception) { WriteLog("BLE广播回调异常：" + FlattenException(exception)); }
    }

    private void ResolveAdvertisedDeviceName(ulong address)
    {
        BluetoothLEDevice tempDevice = null;
        try
        {
            tempDevice = WaitOperation(BluetoothLEDevice.FromBluetoothAddressAsync(address), 4500, "解析BLE设备名称");
            string name = tempDevice == null ? string.Empty : tempDevice.Name;
            if (!string.IsNullOrWhiteSpace(name))
            {
                lock (_scanLock)
                {
                    ScanRecord record;
                    if (_scanRecords.TryGetValue(address, out record) && record != null)
                    {
                        record.Name = Sanitize(name);
                        record.LastSeenUtc = DateTime.UtcNow;
                    }
                }
                _snapshotDirty = true;
                WriteLog("解析BLE名称：" + address.ToString("X12") + " -> " + Sanitize(name));
            }
        }
        catch (Exception exception)
        {
            WriteLog("BLE名称解析未完成：" + address.ToString("X12") + "；" + FlattenException(exception));
        }
        finally
        {
            try { if (tempDevice != null) { tempDevice.Dispose(); } } catch { }
        }
    }

    private void UpdateDeviceSnapshot()
    {
        List<ScanRecord> records = new List<ScanRecord>();
        lock (_scanLock)
        {
            foreach (ScanRecord record in _scanRecords.Values)
            {
                if ((DateTime.UtcNow - record.LastSeenUtc).TotalSeconds <= 30.0) { records.Add(record); }
            }
        }
        records.Sort(delegate(ScanRecord left, ScanRecord right)
        {
            int leftKey = GetNameLastFourSortKey(left == null ? null : left.Name);
            int rightKey = GetNameLastFourSortKey(right == null ? null : right.Name);
            int compare = leftKey.CompareTo(rightKey);
            if (compare != 0) { return compare; }
            string leftName = left == null || left.Name == null ? string.Empty : left.Name;
            string rightName = right == null || right.Name == null ? string.Empty : right.Name;
            compare = string.Compare(leftName, rightName, StringComparison.OrdinalIgnoreCase);
            if (compare != 0) { return compare; }
            return right.Rssi.CompareTo(left.Rssi);
        });
        if (records.Count > MaxDevices) { records.RemoveRange(MaxDevices, records.Count - MaxDevices); }
        List<string> lines = new List<string>();
        int namedCount = 0;
        int index;
        for (index = 0; index < records.Count; index++)
        {
            ScanRecord record = records[index];
            string name = string.IsNullOrWhiteSpace(record.Name) ? "未命名 BLE 设备" : Sanitize(record.Name);
            if (!string.IsNullOrWhiteSpace(record.Name)) { namedCount++; }
            /* 第2列发布扫描时的真实 BluetoothAddressType：Public=0、Random=1、Unspecified=2。 */
            lines.Add(record.Address.ToString("X12") + "	" + record.AddressType.ToString() + "	" + record.Rssi.ToString() + "	" + name);
        }
        WriteAtomicLines(_devicePath, lines.ToArray());
        if (_scanActive)
        {
            WriteStatus("SCANNING	蓝牙状态：已发现 " + lines.Count.ToString() + " 个 BLE 设备，其中 " + namedCount.ToString() + " 个已解析名称");
        }
    }

    private static int GetNameLastFourSortKey(string name)
    {
        if (string.IsNullOrWhiteSpace(name) || name.Length < 4) { return int.MaxValue; }
        string suffix = name.Substring(name.Length - 4, 4);
        int value;
        return int.TryParse(suffix, out value) ? value : int.MaxValue;
    }

    private int GetScanAddressType(ulong address, int fallbackType)
    {
        lock (_scanLock)
        {
            ScanRecord record;
            if (_scanRecords.TryGetValue(address, out record) && record != null && record.AddressType >= 0 && record.AddressType <= 2) { return record.AddressType; }
        }
        return fallbackType >= 0 && fallbackType <= 2 ? fallbackType : 2;
    }

    private bool WaitForFreshAdvertisement(ulong address, int timeoutMilliseconds)
    {
        Stopwatch stopwatch = Stopwatch.StartNew();
        while (stopwatch.ElapsedMilliseconds < timeoutMilliseconds)
        {
            lock (_scanLock)
            {
                ScanRecord record;
                if (_scanRecords.TryGetValue(address, out record) && record != null && (DateTime.UtcNow - record.LastSeenUtc).TotalMilliseconds <= 1200.0) { return true; }
            }
            Thread.Sleep(40);
        }
        return false;
    }

    private BluetoothLEDevice CreateBluetoothDevice(ulong address, int observedAddressType, string action)
    {
        /*
         * 用户旧版程序调用 BleakClient(address) 时没有传 winrt.address_type。
         * Bleak WinRT 因而使用 FromBluetoothAddressAsync(address) 自动解析地址类型，
         * 而不是强制 Public/Random。V1.2.4 强制使用广播 AddressType 后，在部分
         * 隐私地址设备上可能触发 HRESULT 0x80070016。这里恢复旧版行为。
         */
        WriteLog("创建设备对象：使用Auto地址解析；广播AddressType仅诊断=" + AddressTypeToText(observedAddressType));
        return WaitOperation(BluetoothLEDevice.FromBluetoothAddressAsync(address), 10000, action + "(Auto)");
    }

    private void OnGattSessionStatusChanged(GattSession sender, GattSessionStatusChangedEventArgs args)
    {
        try
        {
            WriteLog("GattSession.Status=" + args.Status.ToString() + "，Error=" + args.Error.ToString());
            if (args.Status == GattSessionStatus.Active) { _gattSessionActiveEvent.Set(); }
        }
        catch { }
    }

    private GattDeviceServicesResult DiscoverServicesLikeOldBleak(int observedAddressType)
    {
        GattDeviceServicesResult result = null;
        int attempt;
        Exception lastException = null;
        for (attempt = 1; attempt <= 10; attempt++)
        {
            try
            {
                _connectStage = "GATT_SERVICES_" + attempt.ToString();
                WriteStatus("INFO\t正在发现GATT服务（" + attempt.ToString() + "/10）……");
                /*
                 * 用户旧版 BleakClient(address) 未指定 services/use_cached_services，
                 * WinRT 后端走 get_gatt_services_async()，不是按FF00 UUID的Uncached查询。
                 */
                result = WaitOperation(_device.GetGattServicesAsync(), 12000, "发现全部GATT服务(第" + attempt.ToString() + "次)");
                if (result != null && result.Status == GattCommunicationStatus.Success && result.Services != null && result.Services.Count > 0)
                {
                    WriteLog("GATT服务发现成功：Count=" + result.Services.Count.ToString() + "，Attempt=" + attempt.ToString());
                    return result;
                }

                string status = result == null ? "NoResult" : result.Status.ToString();
                string sessionStatus = _gattSession == null ? "NO_SESSION" : _gattSession.SessionStatus.ToString();
                string linkStatus = _device == null ? "NO_DEVICE" : _device.ConnectionStatus.ToString();
                WriteLog("GATT服务发现第" + attempt.ToString() + "次未成功：" + status + "，ObservedAddrType=" + AddressTypeToText(observedAddressType) + "，Link=" + linkStatus + "，Session=" + sessionStatus);
                if (result == null || result.Status != GattCommunicationStatus.Unreachable) { break; }
            }
            catch (Exception exception)
            {
                lastException = exception;
                int hresult = exception.HResult;
                WriteLog("GATT服务发现第" + attempt.ToString() + "次异常：HRESULT=0x" + hresult.ToString("X8") + "，" + FlattenException(exception));
                /*
                 * Windows BLE 栈偶发把不可达/命令拒绝作为WinRT异常抛出，而不是
                 * GattCommunicationStatus.Unreachable。旧Bleak对Unreachable会1秒重试；
                 * 这里将0x80070016也视为可恢复的连接期异常。
                 */
                if (hresult != unchecked((int)0x80070016) && hresult != unchecked((int)0x8007048F)) { throw; }
            }

            if (attempt < 10) { Thread.Sleep(1000); }
        }

        if (lastException != null && result == null)
        {
            throw new InvalidOperationException("GATT服务发现连续失败；最近异常=" + FlattenException(lastException), lastException);
        }
        return result;
    }

    private GattDeviceService FindTargetService(GattDeviceServicesResult serviceResult)
    {
        if (serviceResult == null || serviceResult.Services == null) { return null; }
        int index;
        for (index = 0; index < serviceResult.Services.Count; index++)
        {
            GattDeviceService service = serviceResult.Services[index];
            if (service != null && service.Uuid == ServiceUuid) { return service; }
        }
        return null;
    }

    private GattCharacteristicsResult DiscoverCharacteristicsLikeOldBleak(GattDeviceService service)
    {
        GattCharacteristicsResult result = null;
        int attempt;
        for (attempt = 1; attempt <= 5; attempt++)
        {
            try
            {
                _connectStage = "GATT_CHARACTERISTICS_" + attempt.ToString();
                WriteStatus("INFO\tFF00已找到，正在发现FF02/FF01特征（" + attempt.ToString() + "/5）……");
                result = WaitOperation(service.GetCharacteristicsAsync(), 10000, "发现FF00特征(第" + attempt.ToString() + "次)");
                if (result != null && result.Status == GattCommunicationStatus.Success) { return result; }
                string status = result == null ? "NoResult" : result.Status.ToString();
                WriteLog("FF00特征发现第" + attempt.ToString() + "次未成功：" + status);
                if (result == null || result.Status != GattCommunicationStatus.Unreachable) { break; }
            }
            catch (Exception exception)
            {
                int hresult = exception.HResult;
                WriteLog("FF00特征发现第" + attempt.ToString() + "次异常：HRESULT=0x" + hresult.ToString("X8") + "，" + FlattenException(exception));
                if (hresult != unchecked((int)0x80070016) && hresult != unchecked((int)0x8007048F)) { throw; }
            }
            if (attempt < 5) { Thread.Sleep(700); }
        }
        return result;
    }

    private void BeginConnect(string addressHex, int requestedAddressType)
    {
        ulong address;
        if (!ulong.TryParse(addressHex, System.Globalization.NumberStyles.HexNumber, System.Globalization.CultureInfo.InvariantCulture, out address))
        { throw new InvalidOperationException("蓝牙MAC格式无效：" + addressHex); }

        _selectedAddress = address;
        _selectedName = FindScanName(address);
        int observedAddressType = GetScanAddressType(address, requestedAddressType);
        _connectStage = "FRESH_ADVERTISEMENT";

        bool fresh = WaitForFreshAdvertisement(address, 2000);
        WriteLog("连接前广播确认：" + (fresh ? "FRESH" : "STALE") + "，ObservedAddressType=" + AddressTypeToText(observedAddressType));
        StopScan();
        CloseConnection();
        ResetSecurityState();

        _connectStage = "CREATE_DEVICE";
        WriteStatus("INFO\t正在创建BLE设备对象：" + FormatAddress(address));
        _device = CreateBluetoothDevice(address, observedAddressType, "创建设备对象");
        if (_device == null) { throw new InvalidOperationException("Windows 未能创建 BluetoothLEDevice，请确认设备仍在广播且未被其他终端占用。"); }
        _device.ConnectionStatusChanged += OnConnectionStatusChanged;
        if (!string.IsNullOrWhiteSpace(_device.Name)) { _selectedName = _device.Name; }
        WriteLog("BluetoothLEDevice创建成功：Name=" + Sanitize(_selectedName) + "，DeviceAddrType=" + _device.BluetoothAddressType.ToString() + "，Link=" + _device.ConnectionStatus.ToString());

        _connectStage = "CREATE_GATT_SESSION";
        WriteStatus("INFO\tBLE设备对象已创建，正在建立GattSession……");
        try
        {
            _gattSessionActiveEvent.Reset();
            _gattSession = WaitOperation(GattSession.FromDeviceIdAsync(_device.BluetoothDeviceId), 8000, "创建GattSession");
            if (_gattSession != null)
            {
                _gattSession.SessionStatusChanged += OnGattSessionStatusChanged;
                WriteLog("GattSession创建成功：CanMaintainConnection=" + _gattSession.CanMaintainConnection.ToString() + "，Status=" + _gattSession.SessionStatus.ToString());
                if (!_gattSession.CanMaintainConnection) { WriteLog("警告：当前设备/系统报告CanMaintainConnection=false，仍继续服务发现。 "); }
                else { _gattSession.MaintainConnection = true; }
                if (_gattSession.SessionStatus == GattSessionStatus.Active) { _gattSessionActiveEvent.Set(); }
            }
        }
        catch (Exception sessionException)
        {
            WriteLog("GattSession初始化异常，按旧Bleak思路继续通过GATT请求触发物理连接：HRESULT=0x" + sessionException.HResult.ToString("X8") + "，" + FlattenException(sessionException));
        }

        /*
         * 关键：旧程序 BleakClient(address) 的 services=None，因此首先枚举全部GATT服务，
         * 且默认不强制Uncached。获取服务本身会触发Windows真正建立BLE/GATT链路。
         */
        GattDeviceServicesResult serviceResult = DiscoverServicesLikeOldBleak(observedAddressType);
        if (serviceResult == null || serviceResult.Status != GattCommunicationStatus.Success || serviceResult.Services == null || serviceResult.Services.Count == 0)
        {
            string status = serviceResult == null ? "NoResult" : serviceResult.Status.ToString();
            string sessionStatus = _gattSession == null ? "NO_SESSION" : _gattSession.SessionStatus.ToString();
            string linkStatus = _device == null ? "NO_DEVICE" : _device.ConnectionStatus.ToString();
            throw new InvalidOperationException("GATT服务发现失败：" + status + "；ObservedAddressType=" + AddressTypeToText(observedAddressType) + "；Link=" + linkStatus + "；GattSession=" + sessionStatus);
        }

        _connectStage = "FIND_FF00";
        _service = FindTargetService(serviceResult);
        if (_service == null)
        {
            StringBuilder uuids = new StringBuilder();
            int si;
            for (si = 0; si < serviceResult.Services.Count && si < 12; si++)
            {
                if (serviceResult.Services[si] == null) { continue; }
                if (uuids.Length > 0) { uuids.Append(","); }
                uuids.Append(ShortUuid(serviceResult.Services[si].Uuid));
            }
            throw new InvalidOperationException("GATT已连接但未发现FF00服务；实际服务=" + uuids.ToString());
        }
        WriteLog("找到目标FF00服务；总服务数=" + serviceResult.Services.Count.ToString());

        /* Bleak在服务发现完成后等待GattSession进入Active。 */
        if (_gattSession != null && _gattSession.SessionStatus != GattSessionStatus.Active)
        {
            _connectStage = "WAIT_SESSION_ACTIVE";
            WriteStatus("INFO\tFF00服务已发现，正在等待GattSession进入Active……");
            _gattSessionActiveEvent.WaitOne(5000);
            WriteLog("服务发现后GattSession状态=" + _gattSession.SessionStatus.ToString() + "，Link=" + _device.ConnectionStatus.ToString());
        }

        GattCharacteristicsResult charResult = DiscoverCharacteristicsLikeOldBleak(_service);
        if (charResult == null || charResult.Status != GattCommunicationStatus.Success)
        { throw new InvalidOperationException("GATT特征发现失败：" + (charResult == null ? "NoResult" : charResult.Status.ToString())); }

        int charIndex;
        for (charIndex = 0; charIndex < charResult.Characteristics.Count; charIndex++)
        {
            GattCharacteristic characteristic = charResult.Characteristics[charIndex];
            if (characteristic.Uuid == WriteUuid) { _writeCharacteristic = characteristic; }
            else if (characteristic.Uuid == NotifyUuid) { _notifyCharacteristic = characteristic; }
            else if (characteristic.Uuid == NotifyUuidNew) { _notifyCharacteristicFf03 = characteristic; }
        }
        if (_writeCharacteristic == null) { throw new InvalidOperationException("未发现 FF02 写特征。"); }
        if (_notifyCharacteristic == null && _notifyCharacteristicFf03 == null) { throw new InvalidOperationException("未发现 FF01/FF03 通知特征。"); }

        _connectStage = "ENABLE_NOTIFY";
        if (_notifyCharacteristic != null)
        {
            EnableNotify(_notifyCharacteristic, "FF01");
            if (_notifyCharacteristicFf03 != null) { WriteLog("同时发现FF03；按旧上位机行为优先订阅FF01。"); }
        }
        else
        {
            EnableNotify(_notifyCharacteristicFf03, "FF03-FALLBACK");
            WriteLog("目标设备未提供FF01，使用FF03通知兼容回退。");
        }

        _connection = _device;
        _connectPending = true;
        _connectStartedUtc = DateTime.UtcNow;
        _modbusSlaveId = 1;
        _nextDeviceInfoReadUtc = DateTime.UtcNow;
        _deviceTypeText = "--"; _deviceSnText = "--"; _deviceVersionsText = "--";
        _nextPollUtc = DateTime.MaxValue;
        _connectStage = "GATT_READY";
        PublishConnectedStatus(serviceResult.Services.Count);
        WriteDataLine("WAITING\tBLE/GATT已连接，等待旧版协议鉴权；若设备不要求加密将自动进入明文Modbus模式");
    }

    private static string AddressTypeToText(int addressType)
    {
        if (addressType == (int)BluetoothAddressType.Public) { return "Public(0)"; }
        if (addressType == (int)BluetoothAddressType.Random) { return "Random(1)"; }
        return "Unspecified(2)";
    }

    private string FindScanName(ulong address)
    {
        lock (_scanLock)
        {
            ScanRecord record;
            if (_scanRecords.TryGetValue(address, out record) && record != null && !string.IsNullOrWhiteSpace(record.Name)) { return record.Name; }
        }
        return "BLE Device";
    }


    private void EnableNotify(GattCharacteristic characteristic, string name)
    {
        GattClientCharacteristicConfigurationDescriptorValue descriptorValue;
        if ((characteristic.CharacteristicProperties & GattCharacteristicProperties.Notify) != 0)
        {
            descriptorValue = GattClientCharacteristicConfigurationDescriptorValue.Notify;
        }
        else if ((characteristic.CharacteristicProperties & GattCharacteristicProperties.Indicate) != 0)
        {
            descriptorValue = GattClientCharacteristicConfigurationDescriptorValue.Indicate;
        }
        else
        {
            throw new InvalidOperationException(name + " 不支持Notify/Indicate。");
        }

        characteristic.ValueChanged += OnGattValueChanged;
        GattCommunicationStatus status = WaitOperation(characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(descriptorValue), 8000, "订阅" + name);
        if (status != GattCommunicationStatus.Success)
        {
            characteristic.ValueChanged -= OnGattValueChanged;
            throw new InvalidOperationException(name + " 通知订阅失败：" + status.ToString());
        }
        WriteLog(name + " 通知订阅成功，CCCD=" + descriptorValue.ToString());
    }

    private void OnConnectionStatusChanged(BluetoothLEDevice sender, object args)
    {
        WriteLog("BluetoothLEDevice.ConnectionStatus=" + sender.ConnectionStatus.ToString());
        if (sender.ConnectionStatus != BluetoothConnectionStatus.Connected && _connectPending)
        {
            WriteStatus("INFO\tBLE真实链路已断开：" + sender.ConnectionStatus.ToString());
        }
    }

    private void PublishConnectedStatus(int serviceCount)
    {
        string name = string.IsNullOrWhiteSpace(_selectedName) ? "BLE Device" : _selectedName;
        _connectedServiceCount = serviceCount;
        WriteStatus("CONNECTED\t" + Sanitize(name) + "\t" + FormatAddress(_selectedAddress) + "\t" + serviceCount.ToString());
    }

    private void PollConnection()
    {
        if (!_connectPending || _device == null) { return; }
        if (_device.ConnectionStatus != BluetoothConnectionStatus.Connected)
        {
            if (!_gattWaitLogged && (DateTime.UtcNow - _connectStartedUtc).TotalMilliseconds >= 1200.0)
            {
                _gattWaitLogged = true;
                WriteStatus("INFO\tBLE设备对象已建立，但真实连接状态=" + _device.ConnectionStatus.ToString());
            }
            return;
        }
        if (_gattReadyReported) { return; }
        double elapsed = (DateTime.UtcNow - _connectStartedUtc).TotalMilliseconds;
        if (!_authStarted && elapsed >= 6000.0)
        {
            _gattReadyReported = true;
            _encryptionRequired = false;
            _nextPollUtc = DateTime.UtcNow.AddMilliseconds(700.0);
            WriteLog("连接后6秒未收到2A2A01鉴权请求，按旧版上位机行为进入明文Modbus模式。");
            WriteStatus("INFO\tBLE + GATT 已就绪；当前设备未触发旧版加密握手，按明文 Modbus 通信");
        }
        else if (_authStarted && !_encryptionReady && elapsed >= 15000.0 && !_gattWaitLogged)
        {
            _gattWaitLogged = true;
            WriteStatus("INFO\tBLE/GATT已连接，但旧版2A2A/0086加密握手超过15秒仍未完成，请查看BLE_Direct_V1.3.5.log");
        }
    }

    private void OnGattValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        byte[] data;
        try { data = BufferToBytes(args.CharacteristicValue); }
        catch (Exception exception) { WriteLog("读取GATT通知失败：" + FlattenException(exception)); return; }
        if (data == null || data.Length == 0) { return; }
        lock (_notificationQueueLock)
        {
            _notificationQueue.Enqueue(new NotificationItem { Data = data, SourceUuid = sender.Uuid });
        }
        _notificationEvent.Set();
    }

    private void NotificationWorker()
    {
        while (!_exiting)
        {
            _notificationEvent.WaitOne(500);
            while (!_exiting)
            {
                NotificationItem item = null;
                lock (_notificationQueueLock)
                {
                    if (_notificationQueue.Count > 0) { item = _notificationQueue.Dequeue(); }
                }
                if (item == null) { break; }
                try { ProcessNotification(item.Data, item.SourceUuid); }
                catch (Exception exception)
                {
                    WriteLog("处理BLE通知异常：" + FlattenException(exception));
                    WriteStatus("INFO\tBLE通知处理异常：" + FlattenException(exception));
                }
            }
        }
    }

    private void ProcessNotification(byte[] data, Guid sourceUuid)
    {
        lock (_notificationLock)
        {
            _lastAirRx = CloneBytes(data);
            WriteLog("RX-AIR[" + ShortUuid(sourceUuid) + "] " + BytesToHex(data));

            if (_handshakeAccumulator.Count > 0)
            {
                _handshakeAccumulator.AddRange(data);
                if (IsHandshakePacketComplete(_handshakePrefix, _handshakeAccumulator.Count))
                {
                    byte[] full = _handshakeAccumulator.ToArray();
                    string prefix = _handshakePrefix;
                    _handshakeAccumulator.Clear(); _handshakePrefix = string.Empty;
                    DispatchHandshake(prefix, full);
                }
                return;
            }

            string hex = HexNoSpace(data);
            if (hex.StartsWith("2A2A01", StringComparison.OrdinalIgnoreCase)) { HandleAuthRequest(data); return; }
            if (hex.StartsWith("2A2A03", StringComparison.OrdinalIgnoreCase)) { HandleAuthResult(data); return; }
            if (!_keyExchangeStatus && hex.StartsWith("0086", StringComparison.OrdinalIgnoreCase))
            {
                if (!IsHandshakePacketComplete("0086", data.Length)) { _handshakePrefix = "0086"; _handshakeAccumulator.AddRange(data); return; }
                HandleKeyExchange(data); return;
            }
            if (!_keyExchangeStatus && hex.StartsWith("0007", StringComparison.OrdinalIgnoreCase))
            {
                if (!IsHandshakePacketComplete("0007", data.Length)) { _handshakePrefix = "0007"; _handshakeAccumulator.AddRange(data); return; }
                HandleKeyExchangeResult(data); return;
            }

            ProcessBusinessNotification(data);
        }
    }

    private static bool IsHandshakePacketComplete(string prefix, int count)
    {
        if (prefix == "0086") { return count >= 146 && ((count - 2) % 16) == 0; }
        if (prefix == "0007") { return count >= 18 && ((count - 2) % 16) == 0; }
        return true;
    }

    private void DispatchHandshake(string prefix, byte[] data)
    {
        if (prefix == "0086") { HandleKeyExchange(data); }
        else if (prefix == "0007") { HandleKeyExchangeResult(data); }
    }

    private void ProcessBusinessNotification(byte[] data)
    {
        /* OTA Start仍走正常Modbus链路。仅允许设备在Modbus应答前后立即返回的单字节C/ACK/NAK/CAN旁路进入OTA控制队列。 */
        if (_otaStartControlPassthrough && data != null && data.Length == 1 && (data[0] == 0x43 || data[0] == 0x06 || data[0] == 0x15 || data[0] == 0x18))
        {
            lock (_otaControlBytes) { _otaControlBytes.Enqueue(data[0]); }
            _responseEvent.Set();
            WriteLog("RX-OTA-CONTROL[START] " + data[0].ToString("X2"));
            return;
        }

        if (_otaRawReceiveActive)
        {
            if (_otaRawEncrypted)
            {
                /* OTA优先走业务加密，但BOOT阶段若直接返回C/ACK/NAK，也允许裸控制字节通过。 */
                if (data != null && data.Length <= 4 && data.Length > 0 && (data[0] == 0x43 || data[0] == 0x06 || data[0] == 0x15 || data[0] == 0x18))
                {
                    EnqueueResponse(data, data);
                    return;
                }
                _businessAccumulator.AddRange(data);
                while (_businessAccumulator.Count >= 6)
                {
                    int plainLength = (_businessAccumulator[0] << 8) | _businessAccumulator[1];
                    if (plainLength <= 0 || plainLength > 4096)
                    {
                        byte[] rawFallback = _businessAccumulator.ToArray();
                        _businessAccumulator.Clear();
                        EnqueueResponse(rawFallback, rawFallback);
                        return;
                    }
                    int encryptedLength = ((plainLength + 15) / 16) * 16;
                    int totalLength = 6 + encryptedLength;
                    if (_businessAccumulator.Count < totalLength) { return; }
                    byte[] raw = _businessAccumulator.GetRange(0, totalLength).ToArray();
                    _businessAccumulator.RemoveRange(0, totalLength);
                    byte[] plain = DecryptBusinessPacket(raw);
                    EnqueueResponse(raw, plain);
                }
            }
            else
            {
                EnqueueResponse(data, data);
            }
            return;
        }
        if (_encryptionReady)
        {
            _businessAccumulator.AddRange(data);
            while (_businessAccumulator.Count >= 6)
            {
                int plainLength = (_businessAccumulator[0] << 8) | _businessAccumulator[1];
                int encryptedLength = ((plainLength + 15) / 16) * 16;
                int totalLength = 6 + encryptedLength;
                if (plainLength <= 0 || plainLength > 4096) { _businessAccumulator.Clear(); throw new InvalidDataException("加密业务包长度字段异常：" + plainLength.ToString()); }
                if (_businessAccumulator.Count < totalLength) { return; }
                byte[] raw = _businessAccumulator.GetRange(0, totalLength).ToArray();
                _businessAccumulator.RemoveRange(0, totalLength);
                byte[] plain = DecryptBusinessPacket(raw);
                EnqueueResponse(raw, plain);
            }
        }
        else
        {
            _businessAccumulator.AddRange(data);
            int expected = GetPlainModbusExpectedLength(_businessAccumulator);
            if (expected > 0 && _businessAccumulator.Count >= expected)
            {
                byte[] raw = _businessAccumulator.GetRange(0, expected).ToArray();
                _businessAccumulator.RemoveRange(0, expected);
                EnqueueResponse(raw, raw);
            }
            else if (expected < 0)
            {
                byte[] raw = _businessAccumulator.ToArray();
                _businessAccumulator.Clear();
                EnqueueResponse(raw, raw);
            }
        }
    }

    private static int GetPlainModbusExpectedLength(List<byte> data)
    {
        if (data.Count < 2) { return 0; }
        int function = data[1];
        if ((function & 0x80) != 0) { return 5; }
        if (function == 3 || function == 4)
        {
            if (data.Count < 3) { return 0; }
            return 3 + data[2] + 2;
        }
        if (function == 6 || function == 16) { return 8; }
        return -1;
    }

    private void EnqueueResponse(byte[] raw, byte[] plain)
    {
        _lastAirRx = CloneBytes(raw);
        _lastDecodedRx = CloneBytes(plain);
        lock (_responses) { _responses.Enqueue(new ResponsePacket { Raw = CloneBytes(raw), Plain = CloneBytes(plain) }); }
        WriteLog("RX-DECODED " + BytesToHex(plain));
        _responseEvent.Set();
    }

    private void HandleAuthRequest(byte[] data)
    {
        _authStarted = true; _encryptionRequired = true;
        if (data.Length < 8) { throw new InvalidDataException("2A2A01鉴权请求长度不足。"); }
        byte[] random = new byte[4]; System.Buffer.BlockCopy(data, 4, random, 0, 4); Array.Reverse(random);
        _md5Hash = Md5(random);
        _newAesKey = new byte[16];
        int index; for (index = 0; index < 16; index++) { _newAesKey[index] = (byte)(_md5Hash[index] ^ AesKey[index]); }
        byte[] frame = new byte[6]; frame[0] = 0x02; frame[1] = 0x04; System.Buffer.BlockCopy(_md5Hash, 8, frame, 2, 4);
        ushort check = AdditiveChecksum(frame);
        byte[] response = new byte[10]; response[0] = 0x2A; response[1] = 0x2A; System.Buffer.BlockCopy(frame, 0, response, 2, frame.Length); response[8] = (byte)(check >> 8); response[9] = (byte)check;
        WriteLog("收到2A2A01；MD5=" + HexNoSpace(_md5Hash) + "；NEW_AES=" + HexNoSpace(_newAesKey));
        WriteRaw(response, "AUTH-2A2A02");
        WriteStatus("INFO\t收到旧版2A2A01鉴权请求，已发送2A2A02响应");
    }

    private void HandleAuthResult(byte[] data)
    {
        if (data.Length < 5) { throw new InvalidDataException("2A2A03鉴权结果长度不足。"); }
        _authSucceeded = data[4] == 0;
        if (_authSucceeded) { WriteStatus("INFO\t旧版BLE鉴权成功，等待0086 ECDH密钥协商"); WriteLog("2A2A03 result=00"); }
        else { _newAesKey = null; WriteStatus("INFO\t旧版BLE鉴权失败，result=0x" + data[4].ToString("X2")); }
    }

    private void HandleKeyExchange(byte[] data)
    {
        if (!_authSucceeded || _newAesKey == null || _md5Hash == null) { throw new InvalidOperationException("收到0086但2A2A鉴权尚未完成。"); }
        byte[] cipher = Slice(data, 2, data.Length - 2);
        byte[] decrypted = TrimZero(AesCbc(false, _newAesKey, _md5Hash, cipher));
        if (decrypted.Length < 134) { throw new InvalidDataException("0086解密后长度不足：" + decrypted.Length.ToString()); }
        byte[] payload = Slice(decrypted, 4, decrypted.Length - 6);
        if (payload.Length < 128) { throw new InvalidDataException("0086有效载荷不足128字节。"); }
        _iotPublicKey = Slice(payload, 0, 64);
        byte[] signature = Slice(payload, 64, payload.Length - 64);
        if (signature.Length < 64) { throw new InvalidDataException("0086签名长度不足64字节。"); }
        signature = Slice(signature, 0, 64);
        byte[] signData = Concat(_iotPublicKey, _md5Hash);
        if (!VerifyRawSignature(PublicKeyK2, signature, signData)) { throw new InvalidDataException("0086 IoT公钥签名验证失败。"); }
        WriteLog("0086验签成功，IoT PublicKey=" + HexNoSpace(_iotPublicKey));
        SendKeyExchangeResponse();
        WriteStatus("INFO\t0086 ECDH请求验签成功，已发送应用侧公钥和签名");
    }

    private void SendKeyExchangeResponse()
    {
        ECKeyPairGenerator generator = new ECKeyPairGenerator();
        generator.Init(new ECKeyGenerationParameters(_ecDomain, new SecureRandom()));
        AsymmetricCipherKeyPair pair = generator.GenerateKeyPair();
        _ephemeralPrivateKey = (ECPrivateKeyParameters)pair.Private;
        ECPublicKeyParameters publicKey = (ECPublicKeyParameters)pair.Public;
        byte[] encoded = GetEcPointEncodedCompat(publicKey.Q);
        byte[] publicRaw = Slice(encoded, encoded.Length - 64, 64);
        byte[] signData = Concat(publicRaw, _md5Hash);
        byte[] signature = SignRaw(PrivateKeyL1, signData);
        byte[] frameData = Concat(new byte[] { 0x05, 0x80 }, publicRaw, signature);
        ushort check = AdditiveChecksum(frameData);
        byte[] responseData = Concat(new byte[] { 0x2A, 0x2A }, frameData, new byte[] { (byte)(check >> 8), (byte)check });
        byte[] encrypted = AesCbc(true, _newAesKey, _md5Hash, ZeroPad(responseData, 16));
        byte[] air = Concat(new byte[] { 0x00, 0x86 }, encrypted);
        WriteRaw(air, "KEX-0086");
    }

    private void HandleKeyExchangeResult(byte[] data)
    {
        if (_newAesKey == null || _md5Hash == null || _ephemeralPrivateKey == null || _iotPublicKey == null) { throw new InvalidOperationException("收到0007但ECDH上下文不完整。"); }
        byte[] cipher = Slice(data, 2, data.Length - 2);
        byte[] plain = TrimZero(AesCbc(false, _newAesKey, _md5Hash, cipher));
        if (plain.Length < 5 || plain[4] != 0) { throw new InvalidDataException("0007密钥协商结果失败：" + BytesToHex(plain)); }
        ECPublicKeyParameters peer = CreatePublicKey(_iotPublicKey);
        ECDHBasicAgreement agreement = new ECDHBasicAgreement(); agreement.Init(_ephemeralPrivateKey);
        BigInteger shared = agreement.CalculateAgreement(peer);
        _sharedKey = ToFixed(shared, 32);
        _keyExchangeStatus = true; _encryptionReady = true; _gattReadyReported = true;
        byte[] probe = EncryptBusinessPacket(HexToBytesStatic("01030001001015C6"));
        WriteRaw(probe, "POST-KEX-PROBE");
        _nextPollUtc = DateTime.UtcNow.AddMilliseconds(900.0);
        WriteLog("0007密钥协商成功，ECDH ShareKey=" + HexNoSpace(_sharedKey));
        WriteStatus("INFO\tBLE + GATT + 旧版2A2A/ECDH加密链路已完成，Modbus可以发送");
    }

    private byte[] EncryptBusinessPacket(byte[] plain)
    {
        if (_sharedKey == null || _sharedKey.Length != 32) { throw new InvalidOperationException("ECDH共享密钥未建立。"); }
        byte[] padded = ZeroPad(plain, 16);
        byte[] random = new byte[4]; using (RandomNumberGenerator rng = RandomNumberGenerator.Create()) { rng.GetBytes(random); }
        byte[] iv = Md5(random);
        byte[] cipher = AesCbc(true, _sharedKey, iv, padded);
        return Concat(new byte[] { (byte)(plain.Length >> 8), (byte)plain.Length }, random, cipher);
    }

    private byte[] DecryptBusinessPacket(byte[] packet)
    {
        if (packet.Length < 22) { throw new InvalidDataException("加密业务包长度不足。"); }
        int plainLength = (packet[0] << 8) | packet[1];
        byte[] random = Slice(packet, 2, 4);
        byte[] cipher = Slice(packet, 6, packet.Length - 6);
        byte[] plainPadded = AesCbc(false, _sharedKey, Md5(random), cipher);
        if (plainLength < 0 || plainLength > plainPadded.Length) { throw new InvalidDataException("加密业务包明文长度字段无效。"); }
        return Slice(plainPadded, 0, plainLength);
    }

    private void WriteRaw(byte[] data, string stage)
    {
        GattCharacteristic characteristic = _writeCharacteristic;
        if (characteristic == null) { throw new InvalidOperationException("FF02写特征未建立。"); }
        lock (_gattWriteLock)
        {
            _lastAirTx = CloneBytes(data);
            WriteLog("TX-AIR[" + stage + "] " + BytesToHex(data));
            IBuffer buffer = BytesToBuffer(data);
            GattWriteOption writeOption = (characteristic.CharacteristicProperties & GattCharacteristicProperties.Write) != 0 ? GattWriteOption.WriteWithResponse : GattWriteOption.WriteWithoutResponse;
            GattCommunicationStatus status = WaitOperation(characteristic.WriteValueAsync(buffer, writeOption), 8000, "写入FF02");
            if (status != GattCommunicationStatus.Success) { throw new IOException("FF02写入失败：" + status.ToString() + "，模式=" + writeOption.ToString()); }
        }
    }

    private void ResetSecurityState()
    {
        _authStarted = false; _encryptionRequired = false; _authSucceeded = false; _keyExchangeStatus = false; _encryptionReady = false;
        _md5Hash = null; _newAesKey = null; _iotPublicKey = null; _sharedKey = null; _ephemeralPrivateKey = null;
        _handshakeAccumulator.Clear(); _handshakePrefix = string.Empty; _businessAccumulator.Clear();
        lock (_notificationQueueLock) { _notificationQueue.Clear(); }
        lock (_responses) { _responses.Clear(); }
        _lastAirTx = new byte[0]; _lastAirRx = new byte[0]; _lastDecodedRx = new byte[0];
        _gattReadyReported = false; _gattWaitLogged = false;
    }

    private void InitializeEcDomain()
    {
        X9ECParameters parameters = SecNamedCurves.GetByName("secp256r1");
        if (parameters == null) { throw new InvalidOperationException("BouncyCastle不支持secp256r1。"); }
        _ecDomain = new ECDomainParameters(parameters.Curve, parameters.G, parameters.N, parameters.H, parameters.GetSeed());
    }

    private static byte[] GetEcPointEncodedCompat(object point)
    {
        if (point == null) { throw new ArgumentNullException("point"); }
        Type pointType = point.GetType();
        MethodInfo method = pointType.GetMethod("GetEncoded", BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);
        if (method != null)
        {
            byte[] result = method.Invoke(point, null) as byte[];
            if (result != null && result.Length > 0) { return result; }
        }
        method = pointType.GetMethod("GetEncoded", BindingFlags.Public | BindingFlags.Instance, null, new Type[] { typeof(bool) }, null);
        if (method != null)
        {
            byte[] result = method.Invoke(point, new object[] { false }) as byte[];
            if (result != null && result.Length > 0) { return result; }
        }
        throw new MissingMethodException(pointType.FullName, "GetEncoded");
    }

    private static Org.BouncyCastle.Math.EC.ECPoint CreateEcPointCompat(object curve, BigInteger x, BigInteger y)
    {
        if (curve == null) { throw new ArgumentNullException("curve"); }
        Type curveType = curve.GetType();
        MethodInfo method = curveType.GetMethod("CreatePoint", BindingFlags.Public | BindingFlags.Instance, null, new Type[] { typeof(BigInteger), typeof(BigInteger), typeof(bool) }, null);
        if (method != null)
        {
            object point = method.Invoke(curve, new object[] { x, y, false });
            Org.BouncyCastle.Math.EC.ECPoint ecPoint = point as Org.BouncyCastle.Math.EC.ECPoint;
            if (ecPoint != null) { return ecPoint; }
        }
        method = curveType.GetMethod("CreatePoint", BindingFlags.Public | BindingFlags.Instance, null, new Type[] { typeof(BigInteger), typeof(BigInteger) }, null);
        if (method != null)
        {
            object point = method.Invoke(curve, new object[] { x, y });
            Org.BouncyCastle.Math.EC.ECPoint ecPoint = point as Org.BouncyCastle.Math.EC.ECPoint;
            if (ecPoint != null) { return ecPoint; }
        }
        throw new MissingMethodException(curveType.FullName, "CreatePoint");
    }

    private ECPublicKeyParameters CreatePublicKey(byte[] raw64)
    {
        if (raw64 == null || raw64.Length != 64) { throw new ArgumentException("P-256公钥必须为64字节X||Y。"); }
        byte[] x = Slice(raw64, 0, 32); byte[] y = Slice(raw64, 32, 32);
        Org.BouncyCastle.Math.EC.ECPoint point = CreateEcPointCompat(_ecDomain.Curve, new BigInteger(1, x), new BigInteger(1, y));
        return new ECPublicKeyParameters(point, _ecDomain);
    }

    private byte[] SignRaw(byte[] privateKey, byte[] data)
    {
        ECPrivateKeyParameters key = new ECPrivateKeyParameters(new BigInteger(1, privateKey), _ecDomain);
        byte[] hash = Sha256(data);
        ECDsaSigner signer = new ECDsaSigner(); signer.Init(true, new ParametersWithRandom(key, new SecureRandom()));
        BigInteger[] rs = signer.GenerateSignature(hash);
        return Concat(ToFixed(rs[0], 32), ToFixed(rs[1], 32));
    }

    private bool VerifyRawSignature(byte[] publicKey, byte[] signature, byte[] data)
    {
        if (signature == null || signature.Length != 64) { return false; }
        ECPublicKeyParameters key = CreatePublicKey(publicKey);
        BigInteger r = new BigInteger(1, Slice(signature, 0, 32)); BigInteger s = new BigInteger(1, Slice(signature, 32, 32));
        ECDsaSigner signer = new ECDsaSigner(); signer.Init(false, key);
        return signer.VerifySignature(Sha256(data), r, s);
    }

    private static byte[] AesCbc(bool encrypt, byte[] key, byte[] iv, byte[] data)
    {
        if (data == null || (data.Length % 16) != 0) { throw new ArgumentException("AES-CBC数据必须16字节对齐。"); }
        CbcBlockCipher cipher = new CbcBlockCipher(new AesEngine());
        cipher.Init(encrypt, new ParametersWithIV(new KeyParameter(key), iv));
        byte[] output = new byte[data.Length];
        int offset; for (offset = 0; offset < data.Length; offset += 16) { cipher.ProcessBlock(data, offset, output, offset); }
        return output;
    }

    private static byte[] ZeroPad(byte[] data, int blockSize)
    {
        int padding = blockSize - (data.Length % blockSize);
        byte[] result = new byte[data.Length + padding]; System.Buffer.BlockCopy(data, 0, result, 0, data.Length); return result;
    }

    private static byte[] TrimZero(byte[] data)
    {
        int length = data.Length; while (length > 0 && data[length - 1] == 0) { length--; }
        return Slice(data, 0, length);
    }

    private static ushort AdditiveChecksum(byte[] data)
    {
        int sum = 0; int index; for (index = 0; index < data.Length; index++) { sum += data[index]; }
        return (ushort)sum;
    }

    private static byte[] Md5(byte[] data)
    {
        using (MD5 md5 = MD5.Create()) { return md5.ComputeHash(data); }
    }

    private static byte[] Sha256(byte[] data)
    {
        using (SHA256 sha = SHA256.Create()) { return sha.ComputeHash(data); }
    }

    private static byte[] ToFixed(BigInteger value, int length)
    {
        byte[] raw = value.ToByteArrayUnsigned();
        if (raw.Length == length) { return raw; }
        if (raw.Length > length) { return Slice(raw, raw.Length - length, length); }
        byte[] output = new byte[length]; System.Buffer.BlockCopy(raw, 0, output, length - raw.Length, raw.Length); return output;
    }

    private static byte[] Slice(byte[] data, int offset, int count)
    {
        if (data == null) { return new byte[0]; }
        if (offset < 0 || count < 0 || offset + count > data.Length) { throw new ArgumentOutOfRangeException(); }
        byte[] output = new byte[count]; if (count > 0) { System.Buffer.BlockCopy(data, offset, output, 0, count); } return output;
    }

    private static byte[] Concat(params byte[][] arrays)
    {
        int length = 0; int index; for (index = 0; index < arrays.Length; index++) { if (arrays[index] != null) { length += arrays[index].Length; } }
        byte[] output = new byte[length]; int offset = 0;
        for (index = 0; index < arrays.Length; index++) { byte[] item = arrays[index]; if (item != null && item.Length > 0) { System.Buffer.BlockCopy(item, 0, output, offset, item.Length); offset += item.Length; } }
        return output;
    }

    private static byte[] CloneBytes(byte[] data)
    {
        if (data == null) { return new byte[0]; } byte[] copy = new byte[data.Length]; System.Buffer.BlockCopy(data, 0, copy, 0, data.Length); return copy;
    }

    private static string HexNoSpace(byte[] data)
    {
        if (data == null) { return string.Empty; } StringBuilder builder = new StringBuilder(data.Length * 2); int index;
        for (index = 0; index < data.Length; index++) { builder.Append(data[index].ToString("X2")); } return builder.ToString();
    }

    private static byte[] HexToBytesStatic(string hex)
    {
        if (hex == null) { return new byte[0]; } hex = hex.Replace(" ", string.Empty).Replace("-", string.Empty); if ((hex.Length & 1) != 0) { throw new FormatException("HEX长度必须为偶数。"); }
        byte[] data = new byte[hex.Length / 2]; int index; for (index = 0; index < data.Length; index++) { data[index] = Convert.ToByte(hex.Substring(index * 2, 2), 16); } return data;
    }

    private static string ShortUuid(Guid uuid)
    {
        string text = uuid.ToString().ToUpperInvariant(); return text.StartsWith("0000") && text.Length >= 8 ? text.Substring(4, 4) : text;
    }

    private static IBuffer BytesToBuffer(byte[] data)
    {
        DataWriter writer = new DataWriter(); writer.WriteBytes(data); IBuffer buffer = writer.DetachBuffer(); writer.Dispose(); return buffer;
    }

    private static byte[] BufferToBytes(IBuffer buffer)
    {
        if (buffer == null || buffer.Length == 0) { return new byte[0]; }
        DataReader reader = DataReader.FromBuffer(buffer); byte[] data = new byte[(int)buffer.Length]; reader.ReadBytes(data); reader.Dispose(); return data;
    }

    private static T WaitOperation<T>(IAsyncOperation<T> operation, int timeoutMilliseconds, string action)
    {
        if (operation == null) { throw new InvalidOperationException(action + "：WinRT操作对象为空。"); }
        ManualResetEvent done = new ManualResetEvent(false); T result = default(T); Exception error = null;
        operation.Completed = delegate(IAsyncOperation<T> asyncOperation, AsyncStatus status)
        {
            try
            {
                if (status == AsyncStatus.Completed) { result = asyncOperation.GetResults(); }
                else if (status == AsyncStatus.Canceled) { error = new OperationCanceledException(action + "已取消。"); }
                else
                {
                    Exception operationError = asyncOperation.ErrorCode;
                    error = operationError ?? new InvalidOperationException(action + "失败。");
                }
            }
            catch (Exception exception) { error = exception; }
            finally { done.Set(); }
        };
        if (!done.WaitOne(timeoutMilliseconds)) { try { operation.Cancel(); } catch { } done.Close(); throw new TimeoutException(action + "超时。"); }
        done.Close(); if (error != null) { throw error; } return result;
    }

    private void QueueControlWrite(int registerAddress, int value, string name)
    {
        if (!_connectPending || _connection == null)
        {
            WriteStatus("INFO\t蓝牙未连接，无法控制" + name);
            return;
        }
        _pendingWriteAddress = registerAddress;
        _pendingWriteValue = value;
        _pendingWriteName = name;
        WriteStatus("INFO\t正在" + (value != 0 ? "开启" : "关闭") + name + "……");
    }

    private void QueueManualModbus(int slaveId, int functionCode, int registerAddress, int parameter, int timeoutMs)
    {
        if (!_connectPending || _connection == null)
        {
            WriteManualResult(false, "蓝牙未连接", new byte[0], new byte[0], new byte[0], new byte[0], "", BuildBleDiagnosticSnapshot());
            return;
        }
        if (slaveId < 0 || slaveId > 247 || functionCode < 1 || functionCode > 6 || registerAddress < 0 || registerAddress > 65535 || parameter < 0 || parameter > 65535 || timeoutMs < 100 || timeoutMs > 10000)
        {
            WriteManualResult(false, "参数超出有效范围", new byte[0], new byte[0], new byte[0], new byte[0], "", BuildBleDiagnosticSnapshot());
            return;
        }
        _pendingManualSlave = slaveId;
        _pendingManualFunction = functionCode;
        _pendingManualRegister = registerAddress;
        _pendingManualParameter = parameter;
        _pendingManualTimeout = timeoutMs;
        WriteStatus("INFO\t正在发送自定义 Modbus 指令……");
    }

    private void ProcessModbusSchedule()
    {
        if (_otaRunning) { return; }
        if (!_connectPending || _connection == null || !_gattReadyReported)
        {
            return;
        }

        if (_pendingManualFunction >= 0)
        {
            if (Interlocked.CompareExchange(ref _modbusBusy, 1, 0) == 0)
            {
                int slaveId = _pendingManualSlave;
                int functionCode = _pendingManualFunction;
                int registerAddress = _pendingManualRegister;
                int parameter = _pendingManualParameter;
                int timeoutMs = _pendingManualTimeout;
                _pendingManualFunction = -1;
                ThreadPool.QueueUserWorkItem(delegate(object state)
                {
                    try
                    {
                        ExecuteManualModbus(slaveId, functionCode, registerAddress, parameter, timeoutMs);
                    }
                    finally
                    {
                        Interlocked.Exchange(ref _modbusBusy, 0);
                    }
                });
            }
            return;
        }

        if (_pendingWriteAddress >= 0)
        {
            if (Interlocked.CompareExchange(ref _modbusBusy, 1, 0) == 0)
            {
                int offset = _pendingWriteAddress;
                int value = _pendingWriteValue;
                string name = _pendingWriteName;
                _pendingWriteAddress = -1;
                ThreadPool.QueueUserWorkItem(delegate(object state)
                {
                    try
                    {
                        WriteControlRegister(offset, value, name);
                    }
                    finally
                    {
                        Interlocked.Exchange(ref _modbusBusy, 0);
                    }
                });
            }
            return;
        }

        if (DateTime.UtcNow < _nextPollUtc)
        {
            return;
        }
        _nextPollUtc = DateTime.UtcNow.AddSeconds(_pollIntervalSeconds);
        if (Interlocked.CompareExchange(ref _modbusBusy, 1, 0) != 0)
        {
            return;
        }

        ThreadPool.QueueUserWorkItem(delegate(object state)
        {
            try
            {
                PollModbusData();
            }
            finally
            {
                Interlocked.Exchange(ref _modbusBusy, 0);
            }
        });
    }

    private void PollModbusData()
    {
        try
        {
            ushort[] registers;
            int slaveId;
            string error;
            if (!TryReadDashboard(out registers, out slaveId, out error))
            {
                _dataSequence++;
                WriteDataLine("DATAERROR\t" + _dataSequence.ToString() + "\t" + Sanitize(error));
                WriteStatus("INFO\t指定从机" + _modbusSlaveId.ToString() + "未返回有效响应，保持蓝牙连接并继续重试");
                return;
            }

            _modbusSlaveId = slaveId;

            bool readDeviceInfoAfterDashboard = DateTime.UtcNow >= _nextDeviceInfoReadUtc;
            if (readDeviceInfoAfterDashboard)
            {
                TryReadDeviceInformation(slaveId);
            }

            int acState;
            int dcState;
            if (TryReadControlStates(slaveId, out acState, out dcState))
            {
                _lastAcState = acState;
                _lastDcState = dcState;
            }

            int soc = registers[102 - 100];
            uint dcOutputPowerValue = CombineUInt32(registers[140 - 100], registers[141 - 100]);
            uint acOutputPowerValue = CombineUInt32(registers[142 - 100], registers[143 - 100]);
            uint pvInputPowerValue = CombineUInt32(registers[144 - 100], registers[145 - 100]);
            int acInputPower = CombineInt32(registers[146 - 100], registers[147 - 100]);
            int dcOutputPower = ClampUInt32ToInt32(dcOutputPowerValue);
            int acOutputPower = ClampUInt32ToInt32(acOutputPowerValue);
            int pvInputPower = ClampUInt32ToInt32(pvInputPowerValue);

            _dataSequence++;
            WriteDataLine("DATA\t" + _dataSequence.ToString() + "\t" + soc.ToString() + "\t" + acOutputPower.ToString() + "\t" + dcOutputPower.ToString() + "\t" + pvInputPower.ToString() + "\t" + acInputPower.ToString() + "\t" + _lastAcState.ToString() + "\t" + _lastDcState.ToString() + "\t" + DateTime.Now.ToString("HH:mm:ss") + "\tREG100\t" + slaveId.ToString() + "\t" + Sanitize(_deviceTypeText) + "\t" + Sanitize(_deviceSnText) + "\t" + Sanitize(_deviceVersionsText));
            WriteStatus("INFO\tModbus实时数据已更新（从机" + slaveId.ToString() + "，寄存器100～149）");
        }
        catch (Exception exception)
        {
            _dataSequence++;
            string error = FlattenException(exception);
            WriteLog("Modbus轮询异常：" + error);
            WriteDataLine("DATAERROR\t" + _dataSequence.ToString() + "\t" + Sanitize(error));
            WriteStatus("INFO\tModbus轮询异常，保持连接并自动重试");
        }
    }

    private bool TryReadDashboard(out ushort[] registers, out int successfulSlaveId, out string error)
    {
        registers = null;
        successfulSlaveId = _modbusSlaveId;
        error = string.Empty;
        try
        {
            WriteLog("Modbus实时数据读取：从机=" + _modbusSlaveId.ToString() + "，起始=100，数量=50");
            byte[] request = BuildReadHoldingRegistersRequest(_modbusSlaveId, 100, 50);
            byte[] response = SendAndReceiveModbus(request, 1800);
            registers = ParseReadHoldingRegistersResponse(response, _modbusSlaveId, 50);
            return true;
        }
        catch (Exception exception)
        {
            error = "从机" + _modbusSlaveId.ToString() + "/寄存器100～149：" + FlattenException(exception);
            WriteLog("Modbus实时数据读取失败：" + error);
            return false;
        }
    }

    private bool TryReadControlStates(int slaveId, out int acState, out int dcState)
    {
        acState = _lastAcState;
        dcState = _lastDcState;
        try
        {
            byte[] request = BuildReadHoldingRegistersRequest(slaveId, 2011, 2);
            byte[] response = SendAndReceiveModbus(request, 1800);
            ushort[] values = ParseReadHoldingRegistersResponse(response, slaveId, 2);
            acState = values[0] == 0 ? 0 : 1;
            dcState = values[1] == 0 ? 0 : 1;
            return true;
        }
        catch (Exception exception)
        {
            WriteLog("读取AC/DC开关回显失败（2011～2012）：" + FlattenException(exception));
            return false;
        }
    }

    private void WriteControlRegister(int registerAddress, int value, string name)
    {
        try
        {
            int slaveId = _modbusSlaveId;
            WriteLog(name + "写寄存器：从机=" + slaveId.ToString() + "，地址=" + registerAddress.ToString() + "，值=" + value.ToString());
            byte[] request = BuildWriteSingleRegisterRequest(slaveId, registerAddress, value);
            byte[] response = SendAndReceiveModbus(request, 1800);
            ValidateWriteSingleRegisterResponse(response, request);
            if (registerAddress == 2011) { _lastAcState = value == 0 ? 0 : 1; }
            if (registerAddress == 2012) { _lastDcState = value == 0 ? 0 : 1; }
            WriteStatus("INFO\t" + name + (value != 0 ? "已开启" : "已关闭") + "（从机" + slaveId.ToString() + "）");
            Thread.Sleep(500);
            _nextPollUtc = DateTime.UtcNow;
        }
        catch (Exception exception)
        {
            WriteLog(name + "写寄存器失败（从机" + _modbusSlaveId.ToString() + "，地址" + registerAddress.ToString() + "）：" + FlattenException(exception));
            WriteStatus("INFO\t" + name + "控制失败：" + FlattenException(exception));
            _nextPollUtc = DateTime.UtcNow;
        }
    }

    private void TryReadDeviceInformation(int slaveId)
    {
        _nextDeviceInfoReadUtc = DateTime.UtcNow.AddSeconds(30.0);
        try
        {
            WriteLog("读取1100设备信息段：从机=" + slaveId.ToString() + "，起始=1100，数量=31");
            byte[] request = BuildReadHoldingRegistersRequest(slaveId, 1100, 31);
            byte[] response = SendAndReceiveModbus(request, 1800);
            ushort[] values = ParseReadHoldingRegistersResponse(response, slaveId, 31);
            DecodeDeviceInformation(values);
            _nextDeviceInfoReadUtc = DateTime.UtcNow.AddMinutes(10.0);
            WriteLog("1100设备信息读取成功：类型=" + _deviceTypeText + "，SN=" + _deviceSnText + "，版本=" + _deviceVersionsText);
        }
        catch (Exception exception)
        {
            WriteLog("1100设备信息读取失败：" + FlattenException(exception));
        }
    }

    private void DecodeDeviceInformation(ushort[] values)
    {
        if (values == null || values.Length < 31)
        {
            throw new InvalidDataException("1100设备信息寄存器数量不足。");
        }

        StringBuilder raw = new StringBuilder();
        int index;
        for (index = 0; index < values.Length; index++)
        {
            if (index > 0) { raw.Append(' '); }
            raw.Append(values[index].ToString("X4"));
        }
        WriteLog("1100～1130寄存器原始值：" + raw.ToString());

        /*
         * 设备类型寄存器按低字节在前保存。V1.2.5按高字节优先会把
         * AP200解析成PA020；这里只修正1101~1106设备类型ASCII字节序。
         */
        _deviceTypeText = DecodeAsciiRegistersLowByteFirst(values, 1101 - 1100, 6);
        if (string.IsNullOrWhiteSpace(_deviceTypeText))
        {
            _deviceTypeText = "--";
        }

        ulong sn = (ulong)values[1107 - 1100]
            | ((ulong)values[1108 - 1100] << 16)
            | ((ulong)values[1109 - 1100] << 32)
            | ((ulong)values[1110 - 1100] << 48);
        _deviceSnText = sn == 0ul ? "--" : sn.ToString();

        int softwareNumber = Math.Min(6, (int)values[1112 - 1100]);
        List<string> versions = new List<string>();
        for (index = 0; index < softwareNumber; index++)
        {
            int baseIndex = (1113 - 1100) + index * 3;
            int softwareType = values[baseIndex];
            uint version = CombineUInt32(values[baseIndex + 1], values[baseIndex + 2]);
            string softwareName = GetSoftwareTypeName(softwareType);

            /*
             * 9位版本号按 XXXXX.XX.XX 理解。中间两位（第6~7位）为00时，
             * 该条目属于BOOT版本。例如100670001 => 10067.00.01。
             * 6位版本号（如110404/908411）保持原样，不参与此判断。
             */
            if (IsBootVersionByDecimalLayout(version) && softwareName.IndexOf("-BOOT", StringComparison.OrdinalIgnoreCase) < 0)
            {
                softwareName += "-BOOT";
            }
            versions.Add(softwareName + " " + version.ToString());
        }
        _deviceVersionsText = versions.Count > 0 ? JoinStrings(versions, "  |  ") : "--";
    }

    private static string DecodeAsciiRegistersLowByteFirst(ushort[] values, int startIndex, int registerCount)
    {
        StringBuilder builder = new StringBuilder(registerCount * 2);
        int index;
        for (index = 0; index < registerCount; index++)
        {
            ushort value = values[startIndex + index];
            byte low = (byte)(value & 0xFF);
            byte high = (byte)((value >> 8) & 0xFF);
            if (low != 0 && low != 0xFF) { builder.Append((char)low); }
            if (high != 0 && high != 0xFF) { builder.Append((char)high); }
        }
        return builder.ToString().Trim(' ', '\0', '\t', '\r', '\n');
    }

    private static uint CombineUInt32(ushort low, ushort high)
    {
        return (uint)low | ((uint)high << 16);
    }

    private static int CombineInt32(ushort low, ushort high)
    {
        return unchecked((int)CombineUInt32(low, high));
    }

    private static int ClampUInt32ToInt32(uint value)
    {
        return value > int.MaxValue ? int.MaxValue : (int)value;
    }

    private static string GetSoftwareTypeName(int softwareType)
    {
        bool boot = softwareType >= 1000;
        int baseType = boot ? softwareType - 1000 : softwareType;
        string name;
        switch (baseType)
        {
            case 0: name = "IOT"; break;
            case 1: name = "INV_ARM"; break;
            case 2: name = "INV_DSP"; break;
            case 3: name = "BMS"; break;
            case 4: name = "BA"; break;
            case 5: name = "PACK_BCU"; break;
            case 6: name = "PACK_BMU"; break;
            case 7: name = "PACK_BMS"; break;
            case 8: name = "PACK_M1"; break;
            case 9: name = "PACK_SAFE"; break;
            case 10: name = "PACK_HV"; break;
            case 11: name = "HMI"; break;
            case 12: name = "HMI2"; break;
            case 13: name = "RF"; break;
            case 14: name = "DC-HUB"; break;
            case 15: name = "AC-HUB"; break;
            case 16: name = "DC-DC"; break;
            case 17: name = "ATS-ARM"; break;
            case 18: name = "PANEL-ARM"; break;
            case 19: name = "PARALLEL-BOX"; break;
            case 20: name = "INV_DSP2"; break;
            case 21: name = "HMI-BOOT"; break;
            case 22: name = "HMI-KERNEL"; break;
            case 23: name = "HMI-FILESYSTEM"; break;
            case 24: name = "HMI-APP-UI"; break;
            case 25: name = "HMI-APP-BASE"; break;
            case 40: name = "THIRD-PARTY-1"; break;
            case 41: name = "THIRD-PARTY-2"; break;
            default: name = "TYPE-" + baseType.ToString(); break;
        }
        return boot ? name + "-BOOT" : name;
    }

    private static bool IsBootVersionByDecimalLayout(uint version)
    {
        /* 仅处理恰好9位的十进制版本号：XXXXX.XX.XX */
        if (version < 100000000u || version > 999999999u) { return false; }
        return ((version / 100u) % 100u) == 0u;
    }

    private static string JoinStrings(List<string> values, string separator)
    {
        StringBuilder builder = new StringBuilder();
        int index;
        for (index = 0; index < values.Count; index++)
        {
            if (index > 0) { builder.Append(separator); }
            builder.Append(values[index]);
        }
        return builder.ToString();
    }



    private bool WaitForModbusIdle(int timeoutMs)
    {
        DateTime deadline = DateTime.UtcNow.AddMilliseconds(Math.Max(500, timeoutMs));
        while (DateTime.UtcNow < deadline)
        {
            if (Interlocked.CompareExchange(ref _modbusBusy, 0, 0) == 0) { return true; }
            Thread.Sleep(20);
        }
        return Interlocked.CompareExchange(ref _modbusBusy, 0, 0) == 0;
    }

    private void StartBleOta(string manifestPath)
    {
        if (_otaRunning) { WriteOtaStatus(null, "RUNNING", 0, 0, 0, 0, 0, 0, "OTA任务已经在运行", 0); return; }
        if (string.IsNullOrWhiteSpace(manifestPath) || !File.Exists(manifestPath)) { WriteOtaStatus(null, "ERROR", 0, 0, 0, 0, 0, 0, "OTA清单文件不存在", 0); return; }

        // 点击“开始一键升级”的这一刻立即锁住普通Modbus调度，避免后续SOC/版本查询插入XMODEM数据流。
        _otaCancelRequested = false;
        _otaRunning = true;
        _pendingManualFunction = -1;
        _pendingWriteAddress = -1;
        _nextPollUtc = DateTime.MaxValue;
        _nextDeviceInfoReadUtc = DateTime.MaxValue;
        WriteLog("OTA已开始：立即暂停SOC/版本/AC/DC/手动Modbus调度，等待当前事务收尾。 ");

        _otaThread = new Thread(delegate()
        {
            try
            {
                if (!WaitForModbusIdle(5000)) { throw new TimeoutException("进入OTA前仍有普通Modbus事务未结束，为避免与XMODEM冲突已停止本次OTA。"); }
                ExecuteBleOtaManifest(manifestPath);
            }
            catch (Exception exception)
            {
                WriteLog("BLE OTA异常：" + FlattenException(exception));
                WriteOtaStatus(null, "ERROR", 0, 0, 0, 0, 0, 0, FlattenException(exception), 0);
            }
            finally
            {
                _otaRawReceiveActive = false;
                _otaRunning = false;
                _otaCancelRequested = false;
                _nextDeviceInfoReadUtc = DateTime.MinValue;
                _nextPollUtc = DateTime.UtcNow.AddMilliseconds(900.0);
                WriteLog("OTA任务结束：恢复普通SOC/版本/AC/DC Modbus轮询。 ");
            }
        });
        _otaThread.IsBackground = true;
        _otaThread.Name = "BLUETTI-BLE-OTA";
        _otaThread.SetApartmentState(ApartmentState.MTA);
        _otaThread.Start();
    }

    private OtaManifest LoadOtaManifest(string manifestPath)
    {
        OtaManifest manifest = new OtaManifest();
        string[] lines = File.ReadAllLines(manifestPath, Encoding.Unicode);
        int lineIndex;
        for (lineIndex = 0; lineIndex < lines.Length; lineIndex++)
        {
            string line = lines[lineIndex];
            if (string.IsNullOrWhiteSpace(line)) { continue; }
            string[] parts = line.Split(new char[] { '\t' }, 9, StringSplitOptions.None);
            if (parts.Length >= 8 && string.Equals(parts[0], "CONFIG", StringComparison.OrdinalIgnoreCase))
            {
                int.TryParse(parts[1], out manifest.GapSeconds);
                int.TryParse(parts[2], out manifest.FailureTimeoutSeconds);
                int.TryParse(parts[3], out manifest.ChannelMode);
                int.TryParse(parts[4], out manifest.SlaveId);
                ulong.TryParse(parts[5], System.Globalization.NumberStyles.HexNumber, System.Globalization.CultureInfo.InvariantCulture, out manifest.Address);
                int.TryParse(parts[6], out manifest.AddressType);
                manifest.GapSeconds = Math.Max(0, Math.Min(300, manifest.GapSeconds));
                manifest.FailureTimeoutSeconds = Math.Max(5, Math.Min(300, manifest.FailureTimeoutSeconds));
                manifest.ChannelMode = Math.Max(0, Math.Min(2, manifest.ChannelMode));
            }
            else if (parts.Length >= 9 && string.Equals(parts[0], "ITEM", StringComparison.OrdinalIgnoreCase))
            {
                OtaManifestItem item = new OtaManifestItem();
                int.TryParse(parts[1], out item.Index);
                int.TryParse(parts[2], out item.Chip);
                int firmwareType; int.TryParse(parts[3], out firmwareType); item.FirmwareType = (byte)firmwareType;
                uint.TryParse(parts[4], out item.Version);
                uint.TryParse(parts[5], out item.ImageSize);
                long.TryParse(parts[6], out item.FileSize);
                item.Name = parts[7];
                item.Path = parts[8];
                item.State = 0; item.Progress = 0; item.PcProgress = 0; item.DeviceProgress = 0; item.DistributionSlot = -1; item.DistributionDepth = 0; item.DistributionError = 0; item.OtaGroup = 0; item.Message = "等待升级";
                manifest.Items.Add(item);
            }
        }
        if (manifest.Items.Count == 0) { throw new InvalidDataException("OTA清单中没有固件。 "); }
        return manifest;
    }

    private void ExecuteBleOtaManifest(string manifestPath)
    {
        OtaManifest manifest = LoadOtaManifest(manifestPath);
        int total = manifest.Items.Count;
        int completed = 0;
        int successCount = 0;
        int failureCount = 0;
        _modbusSlaveId = manifest.SlaveId;
        WriteOtaStatus(manifest, "RUNNING", 0, total, 0, 0, successCount, failureCount, "准备 BLE OTA", 0);

        int index;
        for (index = 0; index < total; index++)
        {
            OtaManifestItem item = manifest.Items[index];
            if (_otaCancelRequested) { item.State = 3; item.Message = "用户终止"; break; }
            item.State = 1; item.Progress = 0; item.Message = "准备连接设备";
            WriteOtaStatus(manifest, "RUNNING", index + 1, total, 0, CalculateProcessProgress(manifest, completed, 0), successCount, failureCount, item.Message, 0);

            bool connectionReady = EnsureOtaConnection(manifest.Address, manifest.AddressType, manifest.FailureTimeoutSeconds, manifest.ChannelMode);
            bool success = false;
            if (connectionReady)
            {
                try
                {
                    success = ExecuteOneBleOta(item, manifest, completed, successCount, failureCount);
                }
                catch (Exception exception)
                {
                    item.Message = FlattenException(exception);
                    WriteLog("OTA文件失败：" + item.Name + "；" + item.Message);
                    success = false;
                }
            }
            else { item.Message = "设备重连超时"; }
            _otaRawReceiveActive = false;
            _otaRawEncrypted = false;

            completed++;
            if (success) { item.Progress = 100; item.PcProgress = 100; item.DeviceProgress = 100; item.State = 2; item.Message = "完整升级成功，等待设备重启"; successCount++; }
            else { item.State = 3; if (string.IsNullOrWhiteSpace(item.Message)) { item.Message = "升级失败"; } failureCount++; }
            WriteOtaStatus(manifest, "RUNNING", index + 1, total, item.Progress, CalculateProcessProgress(manifest, completed, 0), successCount, failureCount, item.Message, 0);

            if (index + 1 < total && !_otaCancelRequested)
            {
                int remaining;
                for (remaining = manifest.GapSeconds; remaining > 0 && !_otaCancelRequested; remaining--)
                {
                    WriteOtaStatus(manifest, "WAITING", index + 1, total, 100, CalculateProcessProgress(manifest, completed, 0), successCount, failureCount, "设备重启/固件间隔，等待 " + remaining.ToString() + " 秒", remaining);
                    Thread.Sleep(1000);
                }
                /* 不假设一定断开；下一包开始前统一验证真实链路，不可用则扫描并重连同一MAC。 */
            }
        }

        string state = _otaCancelRequested ? "STOPPED" : (failureCount == 0 ? "DONE" : "DONE_WITH_FAILURE");
        string message = _otaCancelRequested ? "OTA任务已终止" : (failureCount == 0 ? "全部固件升级成功" : "OTA队列完成，存在失败固件");
        WriteOtaStatus(manifest, state, total, total, 100, 100, successCount, failureCount, message, 0);
    }

    private int CalculateProcessProgress(OtaManifest manifest, int completedBefore, int currentPercent)
    {
        if (manifest == null || manifest.Items.Count <= 0) { return 0; }
        long totalWeight = 0;
        long doneWeight = 0;
        int i;
        for (i = 0; i < manifest.Items.Count; i++)
        {
            OtaManifestItem item = manifest.Items[i];
            long weight = item.ImageSize > 0 ? (long)item.ImageSize : item.FileSize;
            if (weight <= 0) { weight = 1; }
            totalWeight += weight;
            if (i < completedBefore) { doneWeight += weight; }
            else if (i == completedBefore) { doneWeight += weight * Math.Max(0, Math.Min(100, currentPercent)) / 100L; }
        }
        if (totalWeight <= 0) { return 0; }
        return (int)Math.Max(0L, Math.Min(100L, doneWeight * 100L / totalWeight));
    }

    private bool EnsureOtaConnection(ulong address, int addressType, int timeoutSeconds, int channelMode)
    {
        DateTime deadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
        while (!_otaCancelRequested && DateTime.UtcNow < deadline)
        {
            if (_connectPending && _device != null && _device.ConnectionStatus == BluetoothConnectionStatus.Connected && _writeCharacteristic != null)
            {
                if (_gattReadyReported)
                {
                    if (channelMode != 1 || _encryptionReady) { return true; }
                }
            }
            try
            {
                CloseConnection();
                StartScan();
                DateTime scanDeadline = DateTime.UtcNow.AddMilliseconds(Math.Min(7000.0, Math.Max(1000.0, (deadline - DateTime.UtcNow).TotalMilliseconds)));
                bool seen = false;
                while (!_otaCancelRequested && DateTime.UtcNow < scanDeadline)
                {
                    lock (_scanLock)
                    {
                        ScanRecord record;
                        if (_scanRecords.TryGetValue(address, out record) && record != null && (DateTime.UtcNow - record.LastSeenUtc).TotalSeconds < 3.0) { seen = true; addressType = record.AddressType; }
                    }
                    if (seen) { break; }
                    Thread.Sleep(120);
                }
                StopScan();
                if (!seen) { Thread.Sleep(400); continue; }
                BeginConnect(address.ToString("X12"), addressType);
                DateTime readyDeadline = DateTime.UtcNow.AddMilliseconds(Math.Min(15000.0, Math.Max(1000.0, (deadline - DateTime.UtcNow).TotalMilliseconds)));
                while (!_otaCancelRequested && DateTime.UtcNow < readyDeadline)
                {
                    if (_connectPending && _device != null && _device.ConnectionStatus == BluetoothConnectionStatus.Connected && _gattReadyReported)
                    {
                        if (channelMode != 1 || _encryptionReady) { return true; }
                    }
                    Thread.Sleep(100);
                }
            }
            catch (Exception exception)
            {
                WriteLog("OTA自动重连失败，将继续尝试：" + FlattenException(exception));
                CloseConnection();
                Thread.Sleep(500);
            }
        }
        return false;
    }

    private bool ExecuteOneBleOta(OtaManifestItem item, OtaManifest manifest, int completedBefore, int successCount, int failureCount)
    {
        byte[] firmware = File.ReadAllBytes(item.Path);
        if (firmware == null || firmware.Length == 0) { throw new IOException("固件文件为空"); }
        if (firmware.LongLength > UInt32.MaxValue) { throw new IOException("固件文件超过4GB"); }
        int packetTotal = (firmware.Length + 1023) / 1024;
        if (packetTotal <= 0 || packetTotal > 65535) { throw new IOException("XMODEM 1K包数量超过协议范围"); }

        /* 已经由实机验证自动加密路径可用。V1.3.5 固定使用现有 BLE AES-CBC 业务通道，
           不再提供明文/强制/自动三选一，避免误操作造成不同固件走不同链路。 */
        if (!_encryptionReady) { throw new InvalidOperationException("BLE加密通道尚未完成认证，不能启动OTA"); }
        bool useEncryption = true;

        _otaRawReceiveActive = false;
        _otaRawEncrypted = false;
        _otaStartControlPassthrough = false;
        ClearOtaResponses();
        item.PcProgress = 0;
        item.DeviceProgress = 0;
        item.DistributionSlot = -1;
        item.DistributionDepth = 0;
        item.DistributionError = 0;
        item.Progress = 0;
        item.Message = "阶段1/2：PC → IOT，发送 OTA Start";
        WriteOtaStatus(manifest, "RUNNING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);

        bool startOk = SendBleOtaStartAndWaitC(item, useEncryption, manifest.FailureTimeoutSeconds);
        if (!startOk) { item.Message = "OTA Start发送失败/等待C超时"; return false; }

        int packetIndex;
        for (packetIndex = 0; packetIndex < packetTotal; packetIndex++)
        {
            if (_otaCancelRequested) { item.Message = "用户终止"; return false; }
            item.Message = "阶段1/2：PC → IOT · XMODEM-1K " + (packetIndex + 1).ToString() + "/" + packetTotal.ToString();
            WriteOtaStatus(manifest, "RUNNING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
            if (!SendBleXmodemPacket(firmware, packetIndex, packetTotal, useEncryption)) { item.Message = "XMODEM数据包未收到ACK"; return false; }

            item.PcProgress = (int)((long)(packetIndex + 1) * 100L / packetTotal);
            item.Progress = item.PcProgress / 2; /* 第一阶段只占总进度0~50% */
            WriteOtaStatus(manifest, "RUNNING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
        }

        item.Message = "阶段1/2：PC → IOT · 发送 EOT";
        WriteOtaStatus(manifest, "RUNNING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
        bool eotOk = SendBleEot(useEncryption);
        _otaRawReceiveActive = false;
        _otaRawEncrypted = false;
        if (!eotOk) { item.Message = "PC → IOT EOT未收到ACK"; return false; }

        /* EOT ACK只表示 PC 到 IOT 的文件传输完成，不代表目标MCU升级完成。 */
        item.PcProgress = 100;
        item.DeviceProgress = 0;
        item.Progress = 50;
        item.Message = "阶段1完成：PC → IOT 100% · 等待IOT分发目标MCU";
        WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);

        return WaitForIotDistributionProgress(item, manifest, completedBefore, successCount, failureCount);
    }

    private bool TryReadOtaDistributionProgress(OtaManifestItem item, out int progress, out int errorCode, out int depth, out int stateFlag, out int slot, out ushort otaGroup)
    {
        progress = 0; errorCode = 0; depth = 0; stateFlag = 0; slot = -1; otaGroup = 0;
        byte[] request = BuildReadHoldingRegistersRequest(_modbusSlaveId, 720, 49);
        byte[] response = SendAndReceiveModbus(request, 2500);
        ushort[] values = ParseReadHoldingRegistersResponse(response, _modbusSlaveId, 49);
        otaGroup = values[0];

        int selected = item.DistributionSlot;
        if (selected < 0 || selected >= 16)
        {
            int bestScore = -1;
            int index;
            for (index = 0; index < 16; index++)
            {
                int baseIndex = 1 + index * 3;
                ushort stateValue = values[baseIndex];
                ushort fileValue = values[baseIndex + 1];
                ushort pctValue = values[baseIndex + 2];
                int fileType = (fileValue >> 8) & 0xFF;
                if (fileType != item.FirmwareType) { continue; }
                int active = (stateValue >> 8) & 0xFF;
                int stateDepth = stateValue & 0xFF;
                int fileDepth = fileValue & 0xFF;
                int pct = (pctValue >> 8) & 0xFF;
                int err = pctValue & 0xFF;
                int candidateDepth = stateDepth != 0 ? stateDepth : fileDepth;
                int score = pct;
                if (active == 1) { score += 1000; }
                if (candidateDepth == 1 || candidateDepth == 2) { score += 200; }
                if (err != 0) { score += 100; }
                if (score > bestScore) { bestScore = score; selected = index; }
            }
            if (selected >= 0) { item.DistributionSlot = selected; }
        }

        if (selected < 0 || selected >= 16) { return false; }
        int offset = 1 + selected * 3;
        ushort stateRegister = values[offset];
        ushort fileRegister = values[offset + 1];
        ushort pctRegister = values[offset + 2];
        int selectedType = (fileRegister >> 8) & 0xFF;
        if (selectedType != item.FirmwareType && item.DistributionSlot < 0) { return false; }

        stateFlag = (stateRegister >> 8) & 0xFF;
        int stateDepthValue = stateRegister & 0xFF;
        int fileDepthValue = fileRegister & 0xFF;
        depth = stateDepthValue != 0 ? stateDepthValue : fileDepthValue;
        progress = Math.Max(0, Math.Min(100, (pctRegister >> 8) & 0xFF));
        errorCode = pctRegister & 0xFF;
        slot = selected;
        return true;
    }

    private static string OtaDistributionDepthText(int depth)
    {
        if (depth == 1) { return "IOT → 设备"; }
        if (depth == 2) { return "设备 → 子设备"; }
        if (depth == 3) { return "服务器 → IOT"; }
        return "IOT分发";
    }

    private bool WaitForIotDistributionProgress(OtaManifestItem item, OtaManifest manifest, int completedBefore, int successCount, int failureCount)
    {
        int timeoutSeconds = Math.Max(5, manifest.FailureTimeoutSeconds);
        DateTime noProgressDeadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
        int lastProgress = -1;
        int lastDepth = -1;
        int lastSlot = -1;
        bool everSeen = false;

        while (!_otaCancelRequested)
        {
            if (DateTime.UtcNow >= noProgressDeadline)
            {
                item.Message = everSeen ? "IOT分发进度超过" + timeoutSeconds.ToString() + "秒无变化" : "未找到当前固件的IOT分发进度，等待超时";
                return false;
            }

            if (!EnsureOtaConnection(manifest.Address, manifest.AddressType, Math.Min(timeoutSeconds, 12), 1))
            {
                item.Message = "等待BLE加密链路恢复后读取IOT分发进度";
                WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
                Thread.Sleep(500);
                continue;
            }

            try
            {
                int progress; int errorCode; int depth; int stateFlag; int slot; ushort otaGroup;
                bool found = TryReadOtaDistributionProgress(item, out progress, out errorCode, out depth, out stateFlag, out slot, out otaGroup);
                item.OtaGroup = otaGroup;
                if (found)
                {
                    everSeen = true;
                    item.DistributionSlot = slot;
                    item.DistributionDepth = depth;
                    item.DistributionError = errorCode;
                    item.DeviceProgress = progress;
                    item.Progress = 50 + progress / 2; /* 第二阶段占总进度50~100% */

                    if (progress != lastProgress || depth != lastDepth || slot != lastSlot)
                    {
                        noProgressDeadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
                        lastProgress = progress; lastDepth = depth; lastSlot = slot;
                    }

                    int group = (otaGroup >> 8) & 0xFF;
                    int groupId = otaGroup & 0xFF;
                    string pathText = OtaDistributionDepthText(depth);
                    if (errorCode != 0)
                    {
                        item.Message = "阶段2失败：" + pathText + " · Slot" + slot.ToString() + " · 故障码0x" + errorCode.ToString("X2");
                        WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
                        return false;
                    }

                    item.Message = "阶段2/2：" + pathText + " " + progress.ToString() + "% · Group=" + group.ToString() + "/ID=" + groupId.ToString() + " · Slot" + slot.ToString();
                    WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
                    WriteLog("OTA-DISTRIBUTION slot=" + slot.ToString() + " state=" + stateFlag.ToString() + " depth=" + depth.ToString() + " type=" + item.FirmwareType.ToString() + " pct=" + progress.ToString() + " err=0x" + errorCode.ToString("X2") + " group=0x" + otaGroup.ToString("X4"));

                    if (progress >= 100)
                    {
                        item.DeviceProgress = 100;
                        item.Progress = 100;
                        item.Message = "升级完成：PC → IOT 100% · " + pathText + " 100%";
                        WriteOtaStatus(manifest, "RUNNING", item.Index + 1, manifest.Items.Count, 100, CalculateProcessProgress(manifest, completedBefore, 100), successCount, failureCount, item.Message, 0);
                        return true;
                    }
                }
                else
                {
                    item.Message = "阶段2/2：等待寄存器720~768出现当前FileType=" + item.FirmwareType.ToString() + "的分发任务";
                    WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
                }
            }
            catch (Exception exception)
            {
                WriteLog("读取IOT分发进度失败，将继续重试：" + FlattenException(exception));
                item.Message = "读取720~768进度暂时失败，继续等待：" + FlattenException(exception);
                WriteOtaStatus(manifest, "DISTRIBUTING", item.Index + 1, manifest.Items.Count, item.Progress, CalculateProcessProgress(manifest, completedBefore, item.Progress), successCount, failureCount, item.Message, 0);
            }

            Thread.Sleep(1000);
        }
        item.Message = "用户终止";
        return false;
    }

    private byte[] BuildBleOtaStartRequest(OtaManifestItem item)
    {
        /* 旧版已验证 OTA 逻辑：OtaFileSize 使用实际固件文件长度计算 1K 包数，
           Version 默认将十进制最后两位替换为 99，INV 单播 Group 为 0x0100。 */
        if (item.FileSize <= 0 || item.FileSize > UInt32.MaxValue) { throw new InvalidDataException("OTA固件实际文件大小无效"); }
        uint packetCount = ((uint)item.FileSize + 1023u) / 1024u;
        if (packetCount == 0 || packetCount > 65535u) { throw new InvalidDataException("OtaFileSize超出16位1K包计数范围"); }
        uint otaVersion = (item.Version / 100u) * 100u + 99u;

        byte[] request = new byte[19];
        request[0] = (byte)_modbusSlaveId;
        request[1] = 0x10; request[2] = 0x02; request[3] = 0xBC; request[4] = 0x00; request[5] = 0x06; request[6] = 0x0C;
        request[7] = 0x00; request[8] = 0x01;
        request[9] = 0x00; request[10] = item.FirmwareType;
        request[11] = (byte)((otaVersion >> 8) & 0xFF); request[12] = (byte)(otaVersion & 0xFF);
        request[13] = (byte)((otaVersion >> 24) & 0xFF); request[14] = (byte)((otaVersion >> 16) & 0xFF);
        request[15] = (byte)((packetCount >> 8) & 0xFF); request[16] = (byte)(packetCount & 0xFF);
        request[17] = 0x01; request[18] = 0x00;
        return request;
    }

    private bool IsValidOtaStartResponse(byte[] response)
    {
        if (response == null || response.Length < 8) { return false; }
        if (response[0] != (byte)_modbusSlaveId || response[1] != 0x10 || response[2] != 0x02 || response[3] != 0xBC || response[4] != 0x00 || response[5] != 0x06) { return false; }
        return CheckModbusCrc(response, 8);
    }

    private bool SendBleOtaStartAndWaitC(OtaManifestItem item, bool useEncryption, int failureTimeoutSeconds)
    {
        byte[] request = BuildBleOtaStartRequest(item);
        byte[] logicalRtu = AppendModbusCrc(request);
        DateTime deadline = DateTime.UtcNow.AddSeconds(failureTimeoutSeconds);
        int attempt;

        /* 严格复用旧上位机 OTA 启动行为：
           1) OTA Start 本身仍是与 SOC/AC/DC 相同的 Modbus over BLE 加密/FF02 链路；
           2) 发送后不等待 0x10 Modbus 应答，而是直接等待设备进入 XMODEM 后返回 'C'(0x43)；
           3) 每次最多等待 5 秒，失败后重发 OTA Start，最多 5 次。 */
        _otaStartControlPassthrough = false;
        _otaRawReceiveActive = true;
        _otaRawEncrypted = useEncryption;
        WriteLog("OTA-START-PLAIN " + BytesToHex(logicalRtu));

        for (attempt = 1; attempt <= 5 && !_otaCancelRequested && DateTime.UtcNow < deadline; attempt++)
        {
            ClearOtaResponses();
            try
            {
                byte[] air = useEncryption ? EncryptBusinessPacket(logicalRtu) : logicalRtu;
                WriteRaw(air, useEncryption ? "OTA-START-ENC" : "OTA-START-PLAIN");
                WriteLog("OTA Start第" + attempt.ToString() + "次已发送，直接等待 C(0x43)");
                int waitMs = Math.Min(5000, Math.Max(1, (int)(deadline - DateTime.UtcNow).TotalMilliseconds));
                int result = WaitForOtaControl(0x43, waitMs);
                if (result == 1)
                {
                    WriteLog("OTA Start成功：收到 C(0x43)，进入 XMODEM-1K");
                    return true;
                }
                if (result == 2) { WriteLog("OTA Start收到 NAK(0x15)，准备立即重发"); continue; }
                if (result < 0) { WriteLog("OTA Start收到 CAN(0x18)，终止当前固件"); return false; }
                WriteLog("OTA Start第" + attempt.ToString() + "次等待 C 超时，准备重发");
            }
            catch (Exception exception)
            {
                WriteLog("OTA Start第" + attempt.ToString() + "次发送失败：" + FlattenException(exception));
            }
        }
        return false;
    }

    private bool SendBleXmodemPacket(byte[] firmware, int packetIndex, int packetTotal, bool useEncryption)
    {
        byte[] packet = new byte[1029];
        byte sequence = (byte)((packetIndex + 1) & 0xFF);
        packet[0] = 0x02; packet[1] = sequence; packet[2] = (byte)(0xFF - sequence);
        int i; for (i = 0; i < 1024; i++) { packet[3 + i] = 0x1A; }
        int offset = packetIndex * 1024; int copy = Math.Min(1024, Math.Max(0, firmware.Length - offset));
        if (copy > 0) { System.Buffer.BlockCopy(firmware, offset, packet, 3, copy); }
        ushort crc = CalculateCrc16Xmodem(packet, 3, 1024); packet[1027] = (byte)(crc >> 8); packet[1028] = (byte)(crc & 0xFF);
        int attempt;
        for (attempt = 1; attempt <= 5 && !_otaCancelRequested; attempt++)
        {
            ClearOtaResponses();
            WriteOtaBusiness(packet, useEncryption, "XMODEM-" + (packetIndex + 1).ToString() + "-TRY" + attempt.ToString());
            int result = WaitForOtaControl(0x06, 5000);
            if (result == 1) { return true; }
            if (result == 2) { WriteLog("XMODEM包" + (packetIndex + 1).ToString() + "收到NAK，重发"); }
        }
        return false;
    }

    private bool SendBleEot(bool useEncryption)
    {
        int attempt;
        for (attempt = 1; attempt <= 5 && !_otaCancelRequested; attempt++)
        {
            ClearOtaResponses();
            byte[] eotPlain = new byte[] { 0x04 };
            byte[] eotAir = useEncryption ? EncryptBusinessPacket(eotPlain) : eotPlain;
            WriteRaw(eotAir, "XMODEM-EOT-" + attempt.ToString() + (useEncryption ? "-ENC" : "-PLAIN"));
            int result = WaitForOtaControl(0x06, 5000);
            if (result == 1) { return true; }
        }
        return false;
    }

    private void WriteOtaBusiness(byte[] plain, bool useEncryption, string stage)
    {
        byte[] air = useEncryption ? EncryptBusinessPacket(plain) : plain;
        WriteOtaAirChunks(air, stage + (useEncryption ? "-ENC" : "-PLAIN"));
    }

    private void WriteOtaAirChunks(byte[] data, string stage)
    {
        GattCharacteristic characteristic = _writeCharacteristic;
        if (characteristic == null) { throw new InvalidOperationException("FF02写特征未建立"); }
        /* 旧上位机固定按 MTU=247 处理 OTA 数据，单次 FF02 数据块为 247-3=244 Bytes。 */
        int chunkSize = 244;
        lock (_gattWriteLock)
        {
            GattWriteOption writeOption = (characteristic.CharacteristicProperties & GattCharacteristicProperties.Write) != 0 ? GattWriteOption.WriteWithResponse : GattWriteOption.WriteWithoutResponse;
            int offset = 0;
            while (offset < data.Length)
            {
                if (_otaCancelRequested) { throw new OperationCanceledException("OTA已终止"); }
                int count = Math.Min(chunkSize, data.Length - offset);
                byte[] chunk = new byte[count]; System.Buffer.BlockCopy(data, offset, chunk, 0, count);
                IBuffer buffer = BytesToBuffer(chunk);
                GattCommunicationStatus status = WaitOperation(characteristic.WriteValueAsync(buffer, writeOption), 8000, "OTA写入FF02");
                if (status != GattCommunicationStatus.Success) { throw new IOException("OTA FF02写入失败：" + status.ToString()); }
                offset += count;
                if (writeOption == GattWriteOption.WriteWithoutResponse) { Thread.Sleep(3); }
            }
        }
        _lastAirTx = CloneBytes(data);
        WriteLog("TX-OTA[" + stage + "] " + data.Length.ToString() + "B");
    }

    private void ClearOtaResponses()
    {
        lock (_responses) { _responses.Clear(); }
        lock (_otaControlBytes) { _otaControlBytes.Clear(); }
        _businessAccumulator.Clear();
        while (_responseEvent.WaitOne(0)) { }
    }

    private byte[] WaitOtaResponseChunk(int timeoutMs)
    {
        if (timeoutMs <= 0) { return null; }
        if (!_responseEvent.WaitOne(timeoutMs)) { return null; }
        ResponsePacket packet = null;
        lock (_responses) { if (_responses.Count > 0) { packet = _responses.Dequeue(); } }
        return packet == null ? null : packet.Plain;
    }

    private bool WaitForOtaModbusStartResponse(int timeoutMs)
    {
        List<byte> accumulator = new List<byte>();
        DateTime deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
        while (!_otaCancelRequested && DateTime.UtcNow < deadline)
        {
            byte[] chunk = WaitOtaResponseChunk(Math.Max(1, (int)(deadline - DateTime.UtcNow).TotalMilliseconds));
            if (chunk == null) { break; }
            accumulator.AddRange(chunk);
            int i;
            for (i = 0; i + 7 < accumulator.Count; i++)
            {
                if (accumulator[i] == 0x01 && accumulator[i + 1] == 0x10 && accumulator[i + 2] == 0x02 && accumulator[i + 3] == 0xBC && accumulator[i + 4] == 0x00 && accumulator[i + 5] == 0x06)
                {
                    byte[] frame = accumulator.GetRange(i, 8).ToArray();
                    if (CheckModbusCrc(frame, 8))
                    {
                        int j; lock (_otaControlBytes) { for (j = i + 8; j < accumulator.Count; j++) { _otaControlBytes.Enqueue(accumulator[j]); } }
                        return true;
                    }
                }
            }
        }
        return false;
    }

    private int WaitForOtaControl(byte expected, int timeoutMs)
    {
        DateTime deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
        while (!_otaCancelRequested && DateTime.UtcNow < deadline)
        {
            lock (_otaControlBytes)
            {
                while (_otaControlBytes.Count > 0)
                {
                    byte value = _otaControlBytes.Dequeue();
                    if (value == expected) { return 1; }
                    if ((expected == 0x06 || expected == 0x43) && value == 0x15) { return 2; }
                    if (value == 0x18) { return -1; }
                }
            }
            byte[] chunk = WaitOtaResponseChunk(Math.Max(1, (int)(deadline - DateTime.UtcNow).TotalMilliseconds));
            if (chunk == null) { break; }
            int i; for (i = 0; i < chunk.Length; i++)
            {
                byte value = chunk[i];
                if (value == expected) { return 1; }
                if ((expected == 0x06 || expected == 0x43) && value == 0x15) { return 2; }
                if (value == 0x18) { return -1; }
            }
        }
        return 0;
    }

    private static ushort CalculateCrc16Xmodem(byte[] data, int offset, int length)
    {
        ushort crc = 0;
        int i; for (i = 0; i < length; i++)
        {
            crc ^= (ushort)(data[offset + i] << 8);
            int bit; for (bit = 0; bit < 8; bit++) { crc = (ushort)(((crc & 0x8000) != 0) ? ((crc << 1) ^ 0x1021) : (crc << 1)); }
        }
        return crc;
    }

    private void WriteOtaStatus(OtaManifest manifest, string state, int currentIndex, int total, int currentPercent, int processPercent, int successCount, int failureCount, string message, int waitRemaining)
    {
        try
        {
            _otaStatusSequence++;
            int successPercent = total > 0 ? (successCount * 100 / total) : 0;
            List<string> lines = new List<string>();
            int currentPc = 0; int currentDevice = 0; int currentDepth = 0; int currentError = 0; int currentSlot = -1; int currentGroup = 0;
            if (manifest != null && currentIndex > 0 && currentIndex <= manifest.Items.Count)
            {
                OtaManifestItem currentItem = manifest.Items[currentIndex - 1];
                currentPc = currentItem.PcProgress; currentDevice = currentItem.DeviceProgress; currentDepth = currentItem.DistributionDepth; currentError = currentItem.DistributionError; currentSlot = currentItem.DistributionSlot; currentGroup = currentItem.OtaGroup;
            }
            lines.Add("OTA\t" + _otaStatusSequence.ToString() + "\t" + state + "\t" + currentIndex.ToString() + "\t" + total.ToString() + "\t" + currentPercent.ToString() + "\t" + processPercent.ToString() + "\t" + successPercent.ToString() + "\t" + successCount.ToString() + "\t" + failureCount.ToString() + "\t" + waitRemaining.ToString() + "\t" + Sanitize(message) + "\t" + currentPc.ToString() + "\t" + currentDevice.ToString() + "\t" + currentDepth.ToString() + "\t" + currentError.ToString() + "\t" + currentSlot.ToString() + "\t" + currentGroup.ToString());
            if (manifest != null)
            {
                int i; for (i = 0; i < manifest.Items.Count; i++)
                {
                    OtaManifestItem item = manifest.Items[i];
                    lines.Add("ITEM\t" + i.ToString() + "\t" + item.State.ToString() + "\t" + item.Progress.ToString() + "\t" + Sanitize(item.Message) + "\t" + Sanitize(item.Name) + "\t" + item.PcProgress.ToString() + "\t" + item.DeviceProgress.ToString() + "\t" + item.DistributionDepth.ToString() + "\t" + item.DistributionError.ToString() + "\t" + item.DistributionSlot.ToString());
                }
            }
            WriteAtomicLines(_otaStatusPath, lines.ToArray());
        }
        catch { }
    }

    private void ExecuteManualModbus(int slaveId, int functionCode, int registerAddress, int parameter, int timeoutMs)
    {
        byte[] request = BuildGenericModbusRequest(slaveId, functionCode, registerAddress, parameter);
        byte[] plainTx = AppendModbusCrc(request);
        try
        {
            byte[] response = SendAndReceiveModbus(request, timeoutMs);
            string parsed = InterpretManualResponse(response, slaveId, functionCode, registerAddress, parameter);
            string diagnostic = BuildBleDiagnosticSnapshot() + "；实际空口TX/RX均由Direct WinRT BLE捕获；加密算法与用户旧版上位机2A2A/0086/ECDH流程一致。";
            WriteManualResult(true, "请求成功", plainTx, _lastAirTx, _lastAirRx, response, parsed, diagnostic);
            WriteStatus("INFO\t自定义 Modbus 指令已收到响应");
            _nextPollUtc = DateTime.UtcNow.AddMilliseconds(600.0);
        }
        catch (Exception exception)
        {
            string error = FlattenException(exception); if (slaveId == 0) { error += "；从机地址0通常为广播地址，设备可能不应答"; }
            WriteManualResult(false, error, plainTx, _lastAirTx, _lastAirRx, _lastDecodedRx, "", BuildBleDiagnosticSnapshot());
            WriteStatus("INFO\t自定义 Modbus 指令失败：" + error);
        }
    }

    private string BuildBleDiagnosticSnapshot()
    {
        string link = _device == null ? "NO_DEVICE" : _device.ConnectionStatus.ToString();
        string mode = _encryptionReady ? "ECDH-AES-CBC" : (_authStarted ? "HANDSHAKE" : "PLAINTEXT/PENDING");
        string notifyPath = _notifyCharacteristic != null ? "FF01" : (_notifyCharacteristicFf03 != null ? "FF03-FALLBACK" : "NONE");
        return "DirectWinRT=YES，Link=" + link + "，FF00=" + (_service != null ? "READY" : "WAIT") + "，FF02=" + (_writeCharacteristic != null ? "READY" : "WAIT") + "，FF01=" + (_notifyCharacteristic != null ? "PRESENT" : "WAIT") + "，FF03=" + (_notifyCharacteristicFf03 != null ? "PRESENT" : "OPTIONAL") + "，NotifyPath=" + notifyPath + "，AuthStarted=" + _authStarted.ToString() + "，AuthOK=" + _authSucceeded.ToString() + "，KEX=" + _keyExchangeStatus.ToString() + "，Mode=" + mode + "，ShareKey=" + (_sharedKey == null ? "0" : _sharedKey.Length.ToString()) + "B";
    }

    private byte[] SendAndReceiveModbus(byte[] request, int timeoutMilliseconds)
    {
        if (!_connectPending || _device == null || _writeCharacteristic == null) { throw new InvalidOperationException("蓝牙通信对象未建立。"); }
        if (!_gattReadyReported) { throw new InvalidOperationException("BLE/GATT或加密握手尚未完成。"); }
        lock (_sendLock)
        {
            lock (_responses) { _responses.Clear(); }
            while (_responseEvent.WaitOne(0)) { }
            _lastAirRx = new byte[0]; _lastDecodedRx = new byte[0];
            byte[] plain = AppendModbusCrc(request);
            byte[] air = _encryptionReady ? EncryptBusinessPacket(plain) : plain;
            _lastAirTx = CloneBytes(air);
            WriteLog("MODBUS-TX-PLAIN " + BytesToHex(plain));
            WriteRaw(air, _encryptionReady ? "MODBUS-ENC" : "MODBUS-PLAIN");
            DateTime started = DateTime.UtcNow;
            while ((DateTime.UtcNow - started).TotalMilliseconds < timeoutMilliseconds)
            {
                int remain = Math.Max(1, timeoutMilliseconds - (int)(DateTime.UtcNow - started).TotalMilliseconds);
                if (!_responseEvent.WaitOne(remain)) { break; }
                ResponsePacket packet = null;
                lock (_responses) { if (_responses.Count > 0) { packet = _responses.Dequeue(); } }
                if (packet != null && packet.Plain != null && packet.Plain.Length > 0)
                {
                    _lastAirRx = CloneBytes(packet.Raw); _lastDecodedRx = CloneBytes(packet.Plain);
                    return packet.Plain;
                }
            }
            throw new TimeoutException("设备未返回 Modbus 响应。");
        }
    }

    private void WriteManualResult(bool success, string message, byte[] plainTx, byte[] encryptedTx, byte[] rawRx, byte[] decryptedRx, string parsed, string diagnostic)
    {
        _manualSequence++;
        WriteAtomicLines(_manualModbusPath, new string[]
        {
            "MANUAL\t" + _manualSequence.ToString() + "\t" + (success ? "OK" : "ERROR") + "\t" + Sanitize(message) + "\t" + BytesToHex(plainTx) + "\t" + BytesToHex(encryptedTx) + "\t" + BytesToHex(rawRx) + "\t" + BytesToHex(decryptedRx) + "\t" + Sanitize(parsed) + "\t" + Sanitize(diagnostic)
        });
    }

    private static byte[] BuildGenericModbusRequest(int slaveId, int functionCode, int registerAddress, int parameter)
    {
        return new byte[]
        {
            (byte)slaveId, (byte)functionCode,
            (byte)((registerAddress >> 8) & 0xFF), (byte)(registerAddress & 0xFF),
            (byte)((parameter >> 8) & 0xFF), (byte)(parameter & 0xFF)
        };
    }

    private static byte[] AppendModbusCrc(byte[] request)
    {
        ushort crc = CalculateModbusCrc(request, request.Length);
        byte[] result = new byte[request.Length + 2];
        System.Buffer.BlockCopy(request, 0, result, 0, request.Length);
        result[result.Length - 2] = (byte)(crc & 0xFF);
        result[result.Length - 1] = (byte)((crc >> 8) & 0xFF);
        return result;
    }

    private static string InterpretManualResponse(byte[] response, int slaveId, int functionCode, int registerAddress, int parameter)
    {
        if (response == null || response.Length == 0) { return "无响应数据"; }
        if (response[0] != (byte)slaveId) { return "响应从机地址=" + response[0].ToString(); }
        if (response.Length > 2 && (response[1] & 0x80) != 0) { return "Modbus异常码=" + response[2].ToString(); }
        StringBuilder builder = new StringBuilder();
        if ((functionCode == 0x03 || functionCode == 0x04) && response.Length >= 3)
        {
            int byteCount = Math.Min((int)response[2], Math.Max(0, response.Length - 3));
            int registerCount = byteCount / 2;
            int index;
            for (index = 0; index < registerCount && index < 64; index++)
            {
                ushort value = (ushort)((response[3 + index * 2] << 8) | response[4 + index * 2]);
                if (builder.Length > 0) { builder.Append("，"); }
                builder.Append("R"); builder.Append(registerAddress + index); builder.Append("="); builder.Append(value); builder.Append("(0x"); builder.Append(value.ToString("X4")); builder.Append(")");
            }
            if (registerCount > 64) { builder.Append("，其余寄存器未展开"); }
        }
        else if ((functionCode == 0x01 || functionCode == 0x02) && response.Length >= 3)
        {
            int byteCount = Math.Min((int)response[2], Math.Max(0, response.Length - 3));
            int bitCount = Math.Min(parameter, byteCount * 8);
            int index;
            for (index = 0; index < bitCount && index < 128; index++)
            {
                int value = (response[3 + index / 8] >> (index % 8)) & 1;
                if (builder.Length > 0) { builder.Append("，"); }
                builder.Append("B"); builder.Append(registerAddress + index); builder.Append("="); builder.Append(value);
            }
            if (bitCount > 128) { builder.Append("，其余位未展开"); }
        }
        else if (functionCode == 0x05 || functionCode == 0x06)
        {
            builder.Append("写入回显：寄存器="); builder.Append(registerAddress); builder.Append("，值="); builder.Append(parameter); builder.Append(" (0x"); builder.Append(parameter.ToString("X4")); builder.Append(")");
        }
        else
        {
            builder.Append("已返回功能码0x"); builder.Append(response.Length > 1 ? response[1].ToString("X2") : "--"); builder.Append("，请查看原始RX帧");
        }
        return builder.ToString();
    }

    private static string BytesToHex(byte[] data)
    {
        if (data == null || data.Length == 0) { return "--"; }
        StringBuilder builder = new StringBuilder(data.Length * 3);
        int index;
        for (index = 0; index < data.Length; index++)
        {
            if (index > 0) { builder.Append(' '); }
            builder.Append(data[index].ToString("X2"));
        }
        return builder.ToString();
    }


    private static byte[] BuildReadHoldingRegistersRequest(int slaveId, int startAddress, int count)
    {
        return new byte[]
        {
            (byte)slaveId, 0x03,
            (byte)((startAddress >> 8) & 0xFF), (byte)(startAddress & 0xFF),
            (byte)((count >> 8) & 0xFF), (byte)(count & 0xFF)
        };
    }

    private static byte[] BuildWriteSingleRegisterRequest(int slaveId, int address, int value)
    {
        return new byte[]
        {
            (byte)slaveId, 0x06,
            (byte)((address >> 8) & 0xFF), (byte)(address & 0xFF),
            (byte)((value >> 8) & 0xFF), (byte)(value & 0xFF)
        };
    }

    private static ushort[] ParseReadHoldingRegistersResponse(byte[] response, int slaveId, int count)
    {
        int expectedBytes = count * 2;
        if (response.Length < 3)
        {
            throw new InvalidDataException("Modbus响应长度不足。");
        }
        if (response[0] != (byte)slaveId)
        {
            throw new InvalidDataException("Modbus从机地址不匹配。");
        }
        if ((response[1] & 0x80) != 0)
        {
            int exceptionCode = response.Length > 2 ? response[2] : -1;
            throw new InvalidDataException("Modbus异常响应，异常码=" + exceptionCode.ToString());
        }
        if (response[1] != 0x03 || response[2] < expectedBytes || response.Length < 3 + expectedBytes)
        {
            throw new InvalidDataException("Modbus功能码或字节数不正确。");
        }
        if (response.Length >= 3 + response[2] + 2 && !CheckModbusCrc(response, 3 + response[2] + 2))
        {
            throw new InvalidDataException("Modbus CRC16校验失败。");
        }

        ushort[] values = new ushort[count];
        int index;
        for (index = 0; index < count; index++)
        {
            values[index] = (ushort)((response[3 + index * 2] << 8) | response[4 + index * 2]);
        }
        return values;
    }

    private static void ValidateWriteSingleRegisterResponse(byte[] response, byte[] request)
    {
        if (response.Length < 6)
        {
            throw new InvalidDataException("写寄存器响应长度不足。");
        }
        if ((response[1] & 0x80) != 0)
        {
            throw new InvalidDataException("写寄存器异常响应，异常码=" + (response.Length > 2 ? response[2].ToString() : "未知"));
        }
        int index;
        for (index = 0; index < 6; index++)
        {
            if (response[index] != request[index])
            {
                throw new InvalidDataException("写寄存器响应回显不一致。");
            }
        }
        if (response.Length >= 8 && !CheckModbusCrc(response, 8))
        {
            throw new InvalidDataException("写寄存器响应 CRC16 校验失败。");
        }
    }

    private static ushort CalculateModbusCrc(byte[] data, int length)
    {
        ushort crc = 0xFFFF;
        int index;
        int bit;
        for (index = 0; index < length; index++)
        {
            crc ^= data[index];
            for (bit = 0; bit < 8; bit++)
            {
                crc = (ushort)(((crc & 1) != 0) ? ((crc >> 1) ^ 0xA001) : (crc >> 1));
            }
        }
        return crc;
    }

    private static bool CheckModbusCrc(byte[] data, int length)
    {
        if (data == null || length < 4 || data.Length < length)
        {
            return false;
        }
        ushort crc = 0xFFFF;
        int index;
        int bit;
        for (index = 0; index < length - 2; index++)
        {
            crc ^= data[index];
            for (bit = 0; bit < 8; bit++)
            {
                crc = (ushort)(((crc & 1) != 0) ? ((crc >> 1) ^ 0xA001) : (crc >> 1));
            }
        }
        return data[length - 2] == (byte)(crc & 0xFF) && data[length - 1] == (byte)((crc >> 8) & 0xFF);
    }



    private void WriteDataLine(string text)
    {
        WriteAtomicLines(_dataPath, new string[] { text });
    }

    private void CloseConnection()
    {
        _connectPending = false; _connectedServiceCount = 0; _gattReadyReported = false;
        try { if (_notifyCharacteristic != null) { _notifyCharacteristic.ValueChanged -= OnGattValueChanged; } } catch { }
        try { if (_notifyCharacteristicFf03 != null) { _notifyCharacteristicFf03.ValueChanged -= OnGattValueChanged; } } catch { }
        try { if (_device != null) { _device.ConnectionStatusChanged -= OnConnectionStatusChanged; } } catch { }
        try { if (_gattSession != null) { _gattSession.SessionStatusChanged -= OnGattSessionStatusChanged; _gattSession.MaintainConnection = false; _gattSession.Dispose(); } } catch { }
        try { if (_service != null) { _service.Dispose(); } } catch { }
        try { if (_device != null) { _device.Dispose(); } } catch { }
        _notifyCharacteristic = null; _notifyCharacteristicFf03 = null; _writeCharacteristic = null; _service = null; _gattSession = null; _device = null; _connection = null;
        ResetSecurityState();
    }

    private void ExitBridge()
    {
        if (_exiting) { return; } _exiting = true;
        try { StopScan(); } catch { } try { CloseConnection(); } catch { }
        try { _notificationEvent.Set(); } catch { }
        try { if (_timer != null) { _timer.Stop(); _timer.Dispose(); } } catch { }
        Application.ExitThread();
    }

    private static string FormatAddress(ulong address)
    {
        string value = address.ToString("X12");
        return value.Substring(0, 2) + ":" + value.Substring(2, 2) + ":" + value.Substring(4, 2) + ":" + value.Substring(6, 2) + ":" + value.Substring(8, 2) + ":" + value.Substring(10, 2);
    }

    private static string Sanitize(string text)
    {
        if (string.IsNullOrEmpty(text)) { return string.Empty; }
        return text.Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ').Trim();
    }

    private void WriteStatus(string text)
    {
        try
        {
            string output = text;

            /*
             * 状态文件只有一个槽位。连接成功后后台很快又会写入鉴权/Modbus INFO，
             * 主界面若恰好没读到瞬时 CONNECTED，就会一直停在“正在连接”，即使
             * SOC/Modbus 已经正常通信。连接真实存在时，将后续 INFO 继续封装成
             * CONNECTED 状态，使“已连接”成为持久状态而不是一次性脉冲。
             * 这里只改变进程间状态发布，不改 BLE/GATT/鉴权/加密/Modbus 链路。
             */
            if (!string.IsNullOrEmpty(text) && text.StartsWith("INFO\t", StringComparison.OrdinalIgnoreCase) &&
                _connectPending && _device != null && _device.ConnectionStatus == BluetoothConnectionStatus.Connected)
            {
                string name = string.IsNullOrWhiteSpace(_selectedName) ? "BLE Device" : _selectedName;
                string detail = text.Length > 5 ? Sanitize(text.Substring(5)) : string.Empty;
                output = "CONNECTED\t" + Sanitize(name) + "\t" + FormatAddress(_selectedAddress) + "\t" + _connectedServiceCount.ToString() + "\t" + detail;
            }
            WriteAtomicLines(_statusPath, new string[] { output });
        }
        catch { }
    }

    private static void WriteAtomicLines(string path, string[] lines)
    {
        string directory = Path.GetDirectoryName(path); if (!string.IsNullOrEmpty(directory)) { Directory.CreateDirectory(directory); }
        string temp = path + ".tmp." + Thread.CurrentThread.ManagedThreadId.ToString();
        File.WriteAllLines(temp, lines, Encoding.Unicode);
        try
        {
            if (File.Exists(path)) { File.Replace(temp, path, null); }
            else { File.Move(temp, path); }
        }
        catch
        {
            try { if (File.Exists(path)) { File.Delete(path); } } catch { }
            try { File.Move(temp, path); } catch { try { File.Copy(temp, path, true); File.Delete(temp); } catch { } }
        }
    }

    private void WriteLog(string text)
    {
        try { File.AppendAllText(_logPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff") + "] " + text + Environment.NewLine, Encoding.UTF8); } catch { }
    }

    private void OnThreadException(object sender, ThreadExceptionEventArgs eventArgs)
    {
        WriteLog("ThreadException：" + FlattenException(eventArgs.Exception));
    }

    private void OnUnhandledException(object sender, UnhandledExceptionEventArgs eventArgs)
    {
        Exception exception = eventArgs.ExceptionObject as Exception;
        WriteLog("UnhandledException：" + (exception == null ? Convert.ToString(eventArgs.ExceptionObject) : FlattenException(exception)));
    }

    private static string FlattenException(Exception exception)
    {
        if (exception == null) { return "Unknown"; }
        StringBuilder builder = new StringBuilder(); Exception current = exception; int depth = 0;
        while (current != null && depth < 8)
        {
            if (depth > 0) { builder.Append(" -> "); }
            builder.Append(current.GetType().Name); builder.Append(": "); builder.Append(current.Message);
            current = current.InnerException; depth++;
        }
        return builder.ToString();
    }
}

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        if (args == null || args.Length < 8) { return; }
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        BluettiBleBridge bridge = new BluettiBleBridge(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        bridge.Start(); Application.Run();
    }
}
