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
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import androidx.lifecycle.lifecycleScope
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
    private var suppressConnectCallback = false
    private var scoWanted = false
    private lateinit var audioManager: AudioManager
    private val writeLock = Any()
    private val lineTimeFmt = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)

    private var fwVersion: String = "—"
    private var bicrosOn: Boolean? = null
    private var knobsSummary: String = "—"
    private var lastSavedNote: String = "—"

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
            appendDevLog("[phone_sco] ACTION_SCO_AUDIO_STATE_UPDATED → $label")
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

        binding.refreshButton.setOnClickListener { refreshDevices() }
        binding.refreshStatusButton.setOnClickListener { sendStatus() }
        binding.clearButton.setOnClickListener {
            logLines.clear()
            renderLog()
        }
        binding.shareButton.setOnClickListener { shareLog() }
        binding.scoButton.setOnClickListener { togglePhoneSco() }
        binding.applyCfgButton.setOnClickListener { onApplyClicked() }
        binding.getCfgButton.setOnClickListener {
            sendTotaString("cros get")
            sendStatus()
        }
        binding.helpButton.setOnClickListener { showHelp() }
        binding.connectSwitch.setOnCheckedChangeListener { _, checked ->
            if (suppressConnectCallback) return@setOnCheckedChangeListener
            if (checked) {
                startConnect()
            } else {
                stopConnect(userMessage = "Disconnected")
            }
        }
        binding.devLogSwitch.setOnCheckedChangeListener { _, checked ->
            binding.devPanel.visibility = if (checked) View.VISIBLE else View.GONE
            if (checked) renderLog()
        }

        wireKnobLabels()
        renderStatusBanner()
        ensurePermissions()
        refreshDevices()
        maybeShowFirstRunDisclaimer()
    }

    private fun prefs() = getSharedPreferences(PREFS, MODE_PRIVATE)

    private fun maybeShowFirstRunDisclaimer() {
        if (!prefs().getBoolean(PREF_DISCLAIMER_OK, false)) {
            showDisclaimer()
        }
    }

    /** First launch only — not shown again after I understand. */
    private fun showDisclaimer() {
        AlertDialog.Builder(this)
            .setTitle(R.string.disclaimer_title)
            .setMessage(R.string.disclaimer_body)
            .setPositiveButton(R.string.disclaimer_accept) { _, _ ->
                prefs().edit().putBoolean(PREF_DISCLAIMER_OK, true).apply()
            }
            .setCancelable(false)
            .show()
    }

    private fun showHelp() {
        val scroll = android.widget.ScrollView(this)
        val text = android.widget.TextView(this).apply {
            text = getString(R.string.faq_body)
            setPadding(48, 32, 48, 32)
            textSize = 14f
            setTextIsSelectable(true)
        }
        scroll.addView(text)
        AlertDialog.Builder(this)
            .setTitle(R.string.faq_title)
            .setView(scroll)
            .setPositiveButton(android.R.string.ok, null)
            .show()
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
        binding.noiseSeek.setOnSeekBarChangeListener(labelUpdater)
        refreshKnobLabels()
    }

    private fun mixDb(): Int =
        (MIX_DB_MIN + binding.mixSeek.progress * 2).coerceAtMost(MIX_DB_MAX)
    private fun bassDb(): Int = binding.bassSeek.progress + EQ_DB_MIN
    private fun trebleDb(): Int = binding.trebleSeek.progress + EQ_DB_MIN
    private fun scoLevel(): Int = binding.volSeek.progress
    private fun noiseLevel(): Int = binding.noiseSeek.progress
    private fun poorSide(): String =
        if (binding.poorLeft.isChecked) "left" else "right"

    private fun refreshKnobLabels() {
        binding.mixLabel.text = "Local ear mix ${mixDb()} dB"
        binding.bassLabel.text = "Bass ${bassDb()} dB"
        binding.trebleLabel.text = "Treble ${trebleDb()} dB"
        binding.volLabel.text = "CROS path level ${scoLevel()} / 15"
        binding.noiseLabel.text = if (noiseLevel() == 0) {
            "Link hiss filter 0 (off)"
        } else {
            "Link hiss filter ${noiseLevel()} / 5"
        }
    }

    private fun onApplyClicked() {
        if (binding.poorLeft.isChecked) {
            AlertDialog.Builder(this)
                .setTitle(R.string.poor_warn_title)
                .setMessage(R.string.poor_warn_body)
                .setPositiveButton(R.string.poor_warn_apply) { _, _ -> sendCfgSet() }
                .setNegativeButton(R.string.poor_warn_cancel, null)
                .show()
        } else {
            sendCfgSet()
        }
    }

    private fun sendCfgSet() {
        val cmd = "cros set poor=${poorSide()} mix=${mixDb()} bass=${bassDb()} " +
            "treble=${trebleDb()} vol=${scoLevel()} noise=${noiseLevel()}"
        sendTotaString(cmd)
    }

    private fun sendStatus() {
        sendTotaString("cros status")
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
                    appendDevLog("[phone] → $text")
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendDevLog("[phone] send failed: ${e.message}")
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
        stopConnect(userMessage = null)
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
            appendDevLog("[phone_sco] isBluetoothScoAvailableOffCall=false")
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
        appendDevLog("[phone_sco] startBluetoothSco()")
        try {
            audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
            audioManager.startBluetoothSco()
            audioManager.isBluetoothScoOn = true
        } catch (e: Exception) {
            scoWanted = false
            binding.scoButton.text = getString(R.string.sco_start)
            appendDevLog("[phone_sco] start failed: ${e.message}")
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
            appendDevLog("[phone_sco] stop failed: ${e.message}")
        }
        if (userMessage != null) {
            appendDevLog(userMessage)
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
        appendDevLog("Found ${bonded.size} bonded device(s)")
    }

    @SuppressLint("MissingPermission")
    private fun startConnect() {
        val idx = binding.deviceSpinner.selectedItemPosition
        if (idx < 0 || idx >= bonded.size) {
            toast("Pick a bonded device")
            setConnectChecked(false)
            return
        }
        if (!hasConnectPermission()) {
            toast("Bluetooth permission required")
            setConnectChecked(false)
            return
        }
        closeSession()

        val device = bonded[idx]
        binding.statusText.text = getString(R.string.status_connecting)
        appendDevLog("Connecting SPP to ${device.name} (${device.address})…")
        readerJob = lifecycleScope.launch(Dispatchers.IO) {
            try {
                val sock = openSpp(device)
                socket = sock
                withContext(Dispatchers.Main) {
                    appendDevLog("SPP connected")
                    binding.statusText.text = getString(R.string.status_connected)
                    renderStatusBanner()
                    sendTotaString("cros get")
                    sendStatus()
                }
                readLoop(BufferedInputStream(sock.inputStream))
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendDevLog("Connect failed: ${e.message}")
                    binding.statusText.text = getString(R.string.status_idle)
                    setConnectChecked(false)
                    renderStatusBanner()
                }
                closeQuietly()
            }
        }
    }

    private fun stopConnect(userMessage: String?) {
        closeSession()
        binding.statusText.text = getString(R.string.status_idle)
        setConnectChecked(false)
        bicrosOn = null
        renderStatusBanner()
        if (userMessage != null) {
            appendDevLog(userMessage)
        }
    }

    private fun closeSession() {
        readerJob?.cancel()
        readerJob = null
        closeQuietly()
    }

    private fun setConnectChecked(checked: Boolean) {
        if (binding.connectSwitch.isChecked == checked) return
        suppressConnectCallback = true
        binding.connectSwitch.isChecked = checked
        suppressConnectCallback = false
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
                appendDevLog("SDP SPP failed (${first.message}); trying channel $TOTA_RFCOMM_CHANNEL")
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
                    appendDevLog("SPP closed by peer")
                    binding.statusText.text = getString(R.string.status_idle)
                    setConnectChecked(false)
                    bicrosOn = null
                    renderStatusBanner()
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
                onBudLine(text)
            }
        }
    }

    /** Always parse for status/knobs; only buffer UI log when support log is on. */
    private fun onBudLine(line: String) {
        appendDevLog(line)
        maybeSyncKnobsFromLog(line)
        updateStatusFromLine(line)
    }

    private fun updateStatusFromLine(line: String) {
        Regex("""init v([\d.]+)""").find(line)?.groupValues?.getOrNull(1)?.let {
            fwVersion = it
        }
        when {
            line.contains("[cros_cue] ENABLED") ||
                line.contains("BiCROS GOOD/RX") ||
                line.contains("CROS shape POOR/TX") -> bicrosOn = true
            line.contains("[cros_cue] DISABLED") ||
                line.contains("[cros_tws] DISABLE") ||
                line.contains("[cros_cue] READY") -> bicrosOn = false
            line.contains("[cros_cfg] status enabled=") -> {
                val en = Regex("""enabled=(\d+)""").find(line)?.groupValues?.getOrNull(1)
                bicrosOn = en == "1"
                Regex("""fw=([\d.]+)""").find(line)?.groupValues?.getOrNull(1)?.let {
                    fwVersion = it
                }
            }
        }
        if (line.contains("[cros_cfg] NV save")) {
            lastSavedNote = "Saved on buds"
            toast("Saved on buds")
        }
        if (line.contains("[cros_cfg] NV load") || line.contains("[cros_cfg] get") ||
            line.contains("[cros_cfg] status")
        ) {
            val poor = Regex("""poor=(RIGHT|LEFT)""").find(line)?.groupValues?.getOrNull(1)
            val mix = Regex("""mix=(-?\d+)""").find(line)?.groupValues?.getOrNull(1)
            val sco = Regex("""(?:sco|vol)=(\d+)""").find(line)?.groupValues?.getOrNull(1)
            val noise = Regex("""noise=(\d+)""").find(line)?.groupValues?.getOrNull(1)
            if (poor != null || mix != null) {
                knobsSummary = "poor=${poor ?: "?"} mix=${mix ?: "?"} sco=${sco ?: "?"} noise=${noise ?: "?"}"
                if (line.contains("NV load")) {
                    lastSavedNote = "Loaded from bud NV"
                } else if (line.contains("[cros_cfg] get")) {
                    lastSavedNote = "Loaded via Get"
                }
            }
        }
        renderStatusBanner()
    }

    private fun renderStatusBanner() {
        val connected = socket?.isConnected == true
        val link = if (connected) "Connected (fw $fwVersion)" else "Not connected"
        val cros = when (bicrosOn) {
            true -> "On"
            false -> "Off"
            null -> "—"
        }
        binding.statusBanner.text =
            "Buds: $link\nBiCROS: $cros\nKnobs: $knobsSummary\nLast: $lastSavedNote"
    }

    private fun maybeSyncKnobsFromLog(line: String) {
        if (!line.contains("[cros_cfg]")) return
        val poor = Regex("""poor=(RIGHT|LEFT)""").find(line)?.groupValues?.getOrNull(1)
        val mix = Regex("""mix=(-?\d+)""").find(line)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val bass = Regex("""bass=(-?\d+)""").find(line)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val treble = Regex("""treble=(-?\d+)""").find(line)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val vol = Regex("""(?:sco|vol)=(\d+)""").find(line)?.groupValues?.getOrNull(1)?.toIntOrNull()
        val noise = Regex("""noise=(\d+)""").find(line)?.groupValues?.getOrNull(1)?.toIntOrNull()
        if (poor == null && mix == null && vol == null && noise == null) return

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
            appendLine("fw=$fwVersion bicros=$bicrosOn knobs=$knobsSummary")
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
            appendDevLog("Share file failed (${e.message}); sent as plain text")
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

    /** Buffer + show only when support log is enabled. */
    private fun appendDevLog(line: String) {
        if (!binding.devLogSwitch.isChecked) return
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
        if (binding.devLogSwitch.isChecked) {
            binding.logScroll.post {
                binding.logScroll.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    private fun hasConnectPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
                PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
    }

    private fun toast(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }

    companion object {
        private const val REQ_BT = 42
        private const val MAX_LINES = 400
        private const val OP_TOTA_STRING = 0x1000
        private const val TOTA_RFCOMM_CHANNEL = 12
        private const val MIX_DB_MIN = -30
        private const val MIX_DB_MAX = -12
        private const val EQ_DB_MIN = -6
        private const val PREFS = "cros_control"
        private const val PREF_DISCLAIMER_OK = "disclaimer_ok"
        private val SPP_UUID: UUID =
            UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        private fun u16le(b0: Byte, b1: Byte): Int =
            (b0.toInt() and 0xff) or ((b1.toInt() and 0xff) shl 8)
    }
}
