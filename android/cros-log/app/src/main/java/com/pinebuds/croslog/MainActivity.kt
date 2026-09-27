package com.pinebuds.croslog

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import androidx.lifecycle.lifecycleScope
import com.google.android.material.tabs.TabLayout
import com.pinebuds.croslog.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedInputStream
import java.io.File
import java.io.IOException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.UUID

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private var socket: BluetoothSocket? = null
    private var readerJob: Job? = null
    private val logLines = ArrayDeque<String>(MAX_LINES)
    private lateinit var deviceAdapter: ArrayAdapter<String>
    private var bonded: List<BluetoothDevice> = emptyList()
    /** Ignore programmatic switch updates while we sync UI after connect/fail. */
    private var suppressSwitchCallback = false
    private var scoWanted = false
    private lateinit var audioManager: AudioManager
    private val writeLock = Any()
    private val lineTimeFmt = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)

    private val scoReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action != AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED) return
            val state = intent.getIntExtra(
                AudioManager.EXTRA_SCO_AUDIO_STATE,
                AudioManager.SCO_AUDIO_STATE_ERROR,
            )
            val label = when (state) {
                AudioManager.SCO_AUDIO_STATE_CONNECTED -> "CONNECTED"
                AudioManager.SCO_AUDIO_STATE_CONNECTING -> "CONNECTING"
                AudioManager.SCO_AUDIO_STATE_DISCONNECTED -> "DISCONNECTED"
                AudioManager.SCO_AUDIO_STATE_ERROR -> "ERROR"
                else -> "state=$state"
            }
            appendUi("[phone_sco] ACTION_SCO_AUDIO_STATE_UPDATED → $label")
            if (state == AudioManager.SCO_AUDIO_STATE_DISCONNECTED ||
                state == AudioManager.SCO_AUDIO_STATE_ERROR
            ) {
                scoWanted = false
                binding.scoButton.text = getString(R.string.sco_start)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        audioManager = getSystemService(AudioManager::class.java)

        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, mutableListOf())
        binding.deviceSpinner.adapter = deviceAdapter

        setupTabs()
        binding.refreshButton.setOnClickListener { refreshDevices() }
        binding.clearButton.setOnClickListener {
            logLines.clear()
            renderLog()
        }
        binding.shareButton.setOnClickListener { shareLog() }
        binding.scoButton.setOnClickListener { togglePhoneSco() }
        binding.applyCfgButton.setOnClickListener { sendCfgSet() }
        binding.getCfgButton.setOnClickListener { sendCfgGet() }
        binding.loggingSwitch.setOnCheckedChangeListener { _, checked ->
            if (suppressSwitchCallback) return@setOnCheckedChangeListener
            if (checked) {
                startLogging()
            } else {
                stopLogging(userMessage = "Disconnected — SPP closed (sniff free for ear test)")
            }
        }

        wireKnobLabels()
        ensurePermissions()
        refreshDevices()
    }

    private fun setupTabs() {
        binding.tabLayout.addTab(binding.tabLayout.newTab().setText(R.string.tab_controls))
        binding.tabLayout.addTab(binding.tabLayout.newTab().setText(R.string.tab_logs))
        binding.tabLayout.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab) {
                showTab(tab.position)
            }
            override fun onTabUnselected(tab: TabLayout.Tab) {}
            override fun onTabReselected(tab: TabLayout.Tab) {}
        })
        showTab(0)
    }

    private fun showTab(position: Int) {
        val controls = position == 0
        binding.controlsPanel.visibility = if (controls) View.VISIBLE else View.GONE
        binding.logsPanel.visibility = if (controls) View.GONE else View.VISIBLE
        if (!controls) {
            binding.logScroll.post {
                binding.logScroll.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    private fun wireKnobLabels() {
        val labelUpdater = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                refreshKnobLabels()
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }
        binding.mixSeek.setOnSeekBarChangeListener(labelUpdater)
        binding.bassSeek.setOnSeekBarChangeListener(labelUpdater)
        binding.trebleSeek.setOnSeekBarChangeListener(labelUpdater)
        binding.volSeek.setOnSeekBarChangeListener(labelUpdater)
        binding.a2dpSeek.setOnSeekBarChangeListener(labelUpdater)
        binding.noiseSeek.setOnSeekBarChangeListener(labelUpdater)
        refreshKnobLabels()
    }

    private fun mixDb(): Int =
        (MIX_DB_MIN + binding.mixSeek.progress * 2).coerceAtMost(MIX_DB_MAX)
    private fun bassDb(): Int = binding.bassSeek.progress + EQ_DB_MIN
    private fun trebleDb(): Int = binding.trebleSeek.progress + EQ_DB_MIN
    private fun scoLevel(): Int = binding.volSeek.progress
    private fun a2dpLevel(): Int = binding.a2dpSeek.progress
    private fun noiseLevel(): Int = binding.noiseSeek.progress
    private fun poorSide(): String =
        if (binding.poorLeft.isChecked) "left" else "right"

    private fun refreshKnobLabels() {
        binding.mixLabel.text = "Mix (local mic) ${mixDb()} dB"
        binding.bassLabel.text = "Bass ${bassDb()} dB"
        binding.trebleLabel.text = "Treble ${trebleDb()} dB"
        binding.volLabel.text = "SCO DAC gain ${scoLevel()} / 15"
        binding.a2dpLabel.text = "Music (A2DP) ${a2dpLevel()} / 15"
        binding.noiseLabel.text = if (noiseLevel() == 0) {
            "Link noise filter 0 (off)"
        } else {
            "Link noise filter ${noiseLevel()} / 5"
        }
    }

    private fun sendCfgSet() {
        val cmd = "cros set poor=${poorSide()} mix=${mixDb()} bass=${bassDb()} " +
            "treble=${trebleDb()} vol=${scoLevel()} a2dp=${a2dpLevel()} " +
            "noise=${noiseLevel()}"
        sendTotaString(cmd)
    }

    private fun sendCfgGet() {
        sendTotaString("cros get")
    }

    private fun sendTotaString(text: String) {
        val sock = socket
        if (sock == null || !sock.isConnected) {
            toast(getString(R.string.connect_first))
            return
        }
        val payload = text.toByteArray(Charsets.UTF_8)
        if (payload.size > 640) {
            toast("Command too long")
            return
        }
        val frame = ByteArray(4 + payload.size)
        frame[0] = (OP_TOTA_STRING and 0xff).toByte()
        frame[1] = ((OP_TOTA_STRING shr 8) and 0xff).toByte()
        frame[2] = (payload.size and 0xff).toByte()
        frame[3] = ((payload.size shr 8) and 0xff).toByte()
        System.arraycopy(payload, 0, frame, 4, payload.size)
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                synchronized(writeLock) {
                    sock.outputStream.write(frame)
                    sock.outputStream.flush()
                }
                withContext(Dispatchers.Main) {
                    appendUi("[phone] → $text")
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendUi("[phone] send failed: ${e.message}")
                    toast("Send failed")
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        val filter = IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(scoReceiver, filter, RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(scoReceiver, filter)
        }
    }

    override fun onStop() {
        try {
            unregisterReceiver(scoReceiver)
        } catch (_: IllegalArgumentException) {
        }
        super.onStop()
    }

    override fun onDestroy() {
        stopPhoneSco(userMessage = null)
        stopLogging(userMessage = null)
        super.onDestroy()
    }

    private fun ensurePermissions() {
        val needed = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) {
                needed += Manifest.permission.BLUETOOTH_CONNECT
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) {
                needed += Manifest.permission.BLUETOOTH_SCAN
            }
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED
        ) {
            needed += Manifest.permission.RECORD_AUDIO
        }
        if (needed.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), REQ_BT)
        }
    }

    @Suppress("DEPRECATION")
    private fun togglePhoneSco() {
        if (scoWanted) {
            stopPhoneSco(userMessage = "Phone SCO off")
            return
        }
        if (!audioManager.isBluetoothScoAvailableOffCall) {
            toast("Phone reports SCO unavailable off-call")
            appendUi("[phone_sco] isBluetoothScoAvailableOffCall=false")
            return
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED
        ) {
            toast("Microphone permission required for SCO")
            ensurePermissions()
            return
        }
        scoWanted = true
        binding.scoButton.text = getString(R.string.sco_stop)
        appendUi("[phone_sco] startBluetoothSco() — watch bud for BTEVENT_SCO_*")
        try {
            audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
            audioManager.startBluetoothSco()
            audioManager.isBluetoothScoOn = true
        } catch (e: Exception) {
            scoWanted = false
            binding.scoButton.text = getString(R.string.sco_start)
            appendUi("[phone_sco] start failed: ${e.message}")
        }
    }

    @Suppress("DEPRECATION")
    private fun stopPhoneSco(userMessage: String?) {
        if (!scoWanted && !audioManager.isBluetoothScoOn) {
            binding.scoButton.text = getString(R.string.sco_start)
            return
        }
        scoWanted = false
        binding.scoButton.text = getString(R.string.sco_start)
        try {
            audioManager.stopBluetoothSco()
            audioManager.isBluetoothScoOn = false
            audioManager.mode = AudioManager.MODE_NORMAL
        } catch (e: Exception) {
            appendUi("[phone_sco] stop failed: ${e.message}")
        }
        if (userMessage != null) {
            appendUi(userMessage)
        }
    }

    @SuppressLint("MissingPermission")
    private fun refreshDevices() {
        val mgr = getSystemService(BluetoothManager::class.java)
        val adapter = mgr?.adapter
        if (adapter == null || !adapter.isEnabled) {
            toast("Enable Bluetooth first")
            return
        }
        if (!hasConnectPermission()) {
            toast("Bluetooth permission required")
            ensurePermissions()
            return
        }
        bonded = adapter.bondedDevices.orEmpty().sortedBy { it.name ?: it.address }
        deviceAdapter.clear()
        bonded.forEach { d ->
            deviceAdapter.add("${d.name ?: "?"}  ${d.address}")
        }
        deviceAdapter.notifyDataSetChanged()
        appendUi("Found ${bonded.size} bonded device(s)")
    }

    @SuppressLint("MissingPermission")
    private fun startLogging() {
        val idx = binding.deviceSpinner.selectedItemPosition
        if (idx < 0 || idx >= bonded.size) {
            toast("Pick a bonded device")
            setSwitchChecked(false)
            return
        }
        if (!hasConnectPermission()) {
            toast("Bluetooth permission required")
            setSwitchChecked(false)
            return
        }
        closeSession()

        val device = bonded[idx]
        binding.statusText.text = getString(R.string.status_connecting)
        appendUi("Connecting SPP to ${device.name} (${device.address})…")
        readerJob = lifecycleScope.launch(Dispatchers.IO) {
            try {
                val sock = openSpp(device)
                socket = sock
                withContext(Dispatchers.Main) {
                    appendUi("SPP connected — knobs + OP_TOTA_STRING (0x1000)")
                    binding.statusText.text = getString(R.string.status_connected)
                    sendTotaString("cros get")
                }
                readLoop(BufferedInputStream(sock.inputStream))
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendUi("Connect failed: ${e.message}")
                    binding.statusText.text = getString(R.string.status_idle)
                    setSwitchChecked(false)
                }
                closeQuietly()
            }
        }
    }

    private fun stopLogging(userMessage: String?) {
        closeSession()
        binding.statusText.text = getString(R.string.status_idle)
        setSwitchChecked(false)
        if (userMessage != null) {
            appendUi(userMessage)
        }
    }

    private fun closeSession() {
        readerJob?.cancel()
        readerJob = null
        closeQuietly()
    }

    private fun setSwitchChecked(checked: Boolean) {
        if (binding.loggingSwitch.isChecked == checked) return
        suppressSwitchCallback = true
        binding.loggingSwitch.isChecked = checked
        suppressSwitchCallback = false
    }

    @SuppressLint("MissingPermission")
    private suspend fun openSpp(device: BluetoothDevice): BluetoothSocket {
        val viaSdp = device.createRfcommSocketToServiceRecord(SPP_UUID)
        return try {
            getSystemService(BluetoothManager::class.java)?.adapter?.cancelDiscovery()
            viaSdp.connect()
            viaSdp
        } catch (first: IOException) {
            viaSdp.close()
            withContext(Dispatchers.Main) {
                appendUi("SDP SPP failed (${first.message}); trying channel $TOTA_RFCOMM_CHANNEL")
            }
            val ctor = device.javaClass.getMethod(
                "createRfcommSocket",
                Int::class.javaPrimitiveType,
            )
            @Suppress("UNCHECKED_CAST")
            val chSock = ctor.invoke(device, TOTA_RFCOMM_CHANNEL) as BluetoothSocket
            chSock.connect()
            chSock
        }
    }

    private suspend fun readLoop(input: BufferedInputStream) {
        val buf = ByteArray(1024)
        val acc = ArrayList<Byte>(512)
        while (true) {
            val n = try {
                input.read(buf)
            } catch (_: IOException) {
                -1
            }
            if (n < 0) {
                withContext(Dispatchers.Main) {
                    appendUi("SPP closed by peer")
                    binding.statusText.text = getString(R.string.status_idle)
                    setSwitchChecked(false)
                }
                break
            }
            if (n == 0) continue
            for (i in 0 until n) acc.add(buf[i])
            drainFrames(acc)
        }
    }

    private suspend fun drainFrames(acc: ArrayList<Byte>) {
        while (acc.size >= 4) {
            val cmd = u16le(acc[0], acc[1])
            val len = u16le(acc[2], acc[3])
            if (cmd != OP_TOTA_STRING) {
                acc.removeAt(0)
                continue
            }
            if (len > 660) {
                acc.removeAt(0)
                continue
            }
            if (acc.size < 4 + len) return
            val textBytes = ByteArray(len) { i -> acc[4 + i] }
            repeat(4 + len) { acc.removeAt(0) }
            val text = textBytes.toString(Charsets.UTF_8)
            withContext(Dispatchers.Main) {
                appendUi(text)
                maybeSyncKnobsFromLog(text)
            }
        }
    }

    private fun maybeSyncKnobsFromLog(line: String) {
        // Strip optional timestamp prefix before matching.
        val body = line.substringAfter("] ", line).let {
            if (it.startsWith("[") || it.contains("[cros_cfg]")) it else line
        }
        if (!body.contains("[cros_cfg]") && !line.contains("[cros_cfg]")) return
        val src = if (line.contains("[cros_cfg]")) line else body
        val poor = Regex("""poor=(RIGHT|LEFT)""").find(src)?.groupValues?.getOrNull(1)
        val mix = Regex("""mix=(-?\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val bass = Regex("""bass=(-?\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val treble = Regex("""treble=(-?\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val vol = Regex("""(?:sco|vol)=(\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val a2dp = Regex("""a2dp=(\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val noise = Regex("""noise=(\d+)""").find(src)?.groupValues?.getOrNull(1)?.toIntOrNull()
        if (poor == "LEFT") {
            binding.poorLeft.isChecked = true
        } else if (poor == "RIGHT") {
            binding.poorRight.isChecked = true
        }
        if (mix != null) {
            val snapped = ((mix / 2) * 2).coerceIn(MIX_DB_MIN, MIX_DB_MAX)
            val prog = ((snapped - MIX_DB_MIN) / 2).coerceIn(0, binding.mixSeek.max)
            binding.mixSeek.progress = prog
        }
        if (bass != null) {
            binding.bassSeek.progress = (bass - EQ_DB_MIN).coerceIn(0, binding.bassSeek.max)
        }
        if (treble != null) {
            binding.trebleSeek.progress = (treble - EQ_DB_MIN).coerceIn(0, binding.trebleSeek.max)
        }
        if (vol != null) {
            binding.volSeek.progress = vol.coerceIn(0, binding.volSeek.max)
        }
        if (a2dp != null) {
            binding.a2dpSeek.progress = a2dp.coerceIn(0, binding.a2dpSeek.max)
        }
        if (noise != null) {
            binding.noiseSeek.progress = noise.coerceIn(0, binding.noiseSeek.max)
        }
        refreshKnobLabels()
    }

    private fun shareLog() {
        if (logLines.isEmpty()) {
            toast(getString(R.string.share_empty))
            return
        }
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val body = buildString {
            appendLine("CROS Log export $stamp")
            appendLine("device=${selectedDeviceLabel()}")
            appendLine("---")
            logLines.forEach { appendLine(it) }
        }
        try {
            val dir = File(cacheDir, "exports").apply { mkdirs() }
            val file = File(dir, "cros-log-$stamp.txt")
            file.writeText(body)
            val uri = FileProvider.getUriForFile(this, "$packageName.files", file)
            val send = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_SUBJECT, getString(R.string.share_title))
                putExtra(Intent.EXTRA_TEXT, body)
                putExtra(Intent.EXTRA_STREAM, uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            startActivity(Intent.createChooser(send, getString(R.string.share)))
        } catch (e: Exception) {
            val send = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_SUBJECT, getString(R.string.share_title))
                putExtra(Intent.EXTRA_TEXT, body)
            }
            startActivity(Intent.createChooser(send, getString(R.string.share)))
            appendUi("Share file failed (${e.message}); sent as plain text")
        }
    }

    @SuppressLint("MissingPermission")
    private fun selectedDeviceLabel(): String {
        val idx = binding.deviceSpinner.selectedItemPosition
        return if (idx in bonded.indices) {
            val d = bonded[idx]
            "${d.name ?: "?"} ${d.address}"
        } else {
            "(none)"
        }
    }

    private fun closeQuietly() {
        try {
            socket?.close()
        } catch (_: IOException) {
        }
        socket = null
    }

    private fun appendUi(line: String) {
        val stamped = "${lineTimeFmt.format(Date())}  $line"
        while (logLines.size >= MAX_LINES) logLines.removeFirst()
        logLines.addLast(stamped)
        renderLog()
    }

    private fun renderLog() {
        binding.logView.text = logLines.joinToString("\n")
        binding.logCountText.text = if (logLines.isEmpty()) {
            getString(R.string.log_count_zero)
        } else {
            getString(R.string.log_count, logLines.size)
        }
        // Badge-ish hint on Logs tab when new lines arrive while on Controls.
        val logsTab = binding.tabLayout.getTabAt(1)
        if (logsTab != null) {
            logsTab.text = if (logLines.isEmpty()) {
                getString(R.string.tab_logs)
            } else {
                "${getString(R.string.tab_logs)} (${logLines.size})"
            }
        }
        if (binding.logsPanel.visibility == View.VISIBLE) {
            binding.logScroll.post { binding.logScroll.fullScroll(View.FOCUS_DOWN) }
        }
    }

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()

    private fun hasConnectPermission(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return true
        return ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
            PackageManager.PERMISSION_GRANTED
    }

    companion object {
        private const val REQ_BT = 42
        private const val MAX_LINES = 400
        private const val OP_TOTA_STRING = 0x1000
        private const val TOTA_RFCOMM_CHANNEL = 12
        private const val MIX_DB_MIN = -30
        /* Must match firmware CROS_MIX_DB_MAX — 0 dB howls. */
        private const val MIX_DB_MAX = -12
        private const val EQ_DB_MIN = -6
        private val SPP_UUID: UUID =
            UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        private fun u16le(b0: Byte, b1: Byte): Int =
            (b0.toInt() and 0xff) or ((b1.toInt() and 0xff) shl 8)
    }
}
