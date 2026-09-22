package com.pinebuds.croslog

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.pinebuds.croslog.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedInputStream
import java.io.IOException
import java.util.UUID

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private var socket: BluetoothSocket? = null
    private var readerJob: Job? = null
    private val logLines = ArrayDeque<String>(MAX_LINES)
    private lateinit var deviceAdapter: ArrayAdapter<String>
    private var bonded: List<BluetoothDevice> = emptyList()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, mutableListOf())
        binding.deviceSpinner.adapter = deviceAdapter

        binding.refreshButton.setOnClickListener { refreshDevices() }
        binding.connectButton.setOnClickListener { connectSelected() }
        binding.disconnectButton.setOnClickListener { disconnect() }
        binding.clearButton.setOnClickListener {
            logLines.clear()
            binding.logView.text = ""
        }

        ensurePermissions()
        refreshDevices()
    }

    override fun onDestroy() {
        disconnect()
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
        if (needed.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), REQ_BT)
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
    private fun connectSelected() {
        val idx = binding.deviceSpinner.selectedItemPosition
        if (idx < 0 || idx >= bonded.size) {
            toast("Pick a bonded device")
            return
        }
        if (!hasConnectPermission()) {
            toast("Bluetooth permission required")
            return
        }
        disconnect()
        val device = bonded[idx]
        appendUi("Connecting SPP to ${device.name} (${device.address})…")
        readerJob = lifecycleScope.launch(Dispatchers.IO) {
            try {
                val sock = openSpp(device)
                socket = sock
                withContext(Dispatchers.Main) {
                    appendUi("SPP connected — waiting for OP_TOTA_STRING (0x1000)")
                    binding.statusText.text = getString(R.string.status_connected)
                }
                readLoop(BufferedInputStream(sock.inputStream))
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendUi("Connect failed: ${e.message}")
                    binding.statusText.text = getString(R.string.status_idle)
                }
                closeQuietly()
            }
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun openSpp(device: BluetoothDevice): BluetoothSocket {
        // Prefer SDP resolution of Serial Port UUID (0x1101).
        val viaSdp = device.createRfcommSocketToServiceRecord(SPP_UUID)
        return try {
            getSystemService(BluetoothManager::class.java)?.adapter?.cancelDiscovery()
            viaSdp.connect()
            viaSdp
        } catch (first: IOException) {
            viaSdp.close()
            // Fallback: BES TOTA is RFCOMM channel 12 (RFCOMM_CHANNEL_3 = 10+2).
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
                // Resync: drop one byte (unknown framing / encrypted junk).
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
            withContext(Dispatchers.Main) { appendUi(text) }
        }
    }

    private fun disconnect() {
        readerJob?.cancel()
        readerJob = null
        closeQuietly()
        binding.statusText.text = getString(R.string.status_idle)
    }

    private fun closeQuietly() {
        try {
            socket?.close()
        } catch (_: IOException) {
        }
        socket = null
    }

    private fun appendUi(line: String) {
        while (logLines.size >= MAX_LINES) logLines.removeFirst()
        logLines.addLast(line)
        binding.logView.text = logLines.joinToString("\n")
        binding.logScroll.post { binding.logScroll.fullScroll(android.view.View.FOCUS_DOWN) }
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
        private val SPP_UUID: UUID =
            UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        private fun u16le(b0: Byte, b1: Byte): Int =
            (b0.toInt() and 0xff) or ((b1.toInt() and 0xff) shl 8)
    }
}
