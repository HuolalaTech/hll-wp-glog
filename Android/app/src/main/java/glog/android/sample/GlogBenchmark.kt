package glog.android.sample

import android.content.Context
import android.os.CountDownTimer
import android.os.Process
import android.os.SystemClock
import com.dianping.logan.Logan
import glog.android.Glog
import java.io.File
import java.io.InputStreamReader
import java.text.DecimalFormat
import java.util.concurrent.TimeUnit
import kotlin.math.log10
import kotlin.math.pow
import com.tencent.mars.xlog.Log as XLogger

/**
 * Created by issac on 2021/3/1.
 */
const val XLOG_FILE_PREFIX = "X"

// generate by generate-ecc-key.py
const val SVR_PUB_KEY =
    "41B5F5F9A53684A1C09B931B7BDF7D7C3959BC7FB31827ADBE1524DDC8F2D90AD4978891385D956CE817B293FC57CF07A4EC3DAF03F63852D75A32A956B84176"
const val SVR_PRIV_KEY = "9C8B23A406216B7B93AA94C66AA5451CCE41DD57A8D5ADBCE8D9F1E7F3D33F45"

enum class LogQuantity(val desc: String, val value: Int) {
    _10K("10k", 10_000),
    _100K("100k", 100_000);
}

private const val TAG = "Benchmark"

class GlogBenchmark(
    context: Context,
    private val logDirectory: String,
    private val logCatView: LogCatView
) {
    private val rawLogs10k = mutableListOf<String>()
    private val rawLogs100k = mutableListOf<String>()

    init {
        var fileSize10k = 0
        var fileSize100k = 0
        context.assets.open(RAW_FILE_NAME_10K).use { input ->
            fileSize10k = input.available()
            InputStreamReader(input).use { reader ->
                reader.readLines().forEach { line -> rawLogs10k.add(line) }
            }
        }
        context.assets.open(RAW_FILE_NAME_100K).use { input ->
            fileSize100k = input.available()
            InputStreamReader(input).use { reader ->
                reader.readLines().forEach { line -> rawLogs100k.add(line) }
            }
        }
        File(logDirectory).listFiles()?.forEach { // 为了便于测试日志体积, 每次启动清理上次产生的日志
            it.delete()
        }
        logCatView.i(
            TAG,
            "测试数据准备完成, 1万条测试数据文件体积:" + fileSize10k.toLong().toReadableSize()
                    + ", 10万条测试数据文件体积:" + fileSize100k.toLong().toReadableSize()
        )
    }

    fun glogWrite(context: Context, async: Boolean, quantity: LogQuantity, incremental: Boolean) {
        val protoName = "glog-benchmark-$async-${quantity.desc}-$incremental"
        val glog = Glog.Builder(context)
            .async(async)
            .compressMode(Glog.CompressMode.Zlib)
            .incrementalArchive(incremental)
            .key(SVR_PUB_KEY)
            .encryptMode(Glog.EncryptMode.AES)
            .rootDirectory(logDirectory)
            .protoName(protoName)
            .build()

        logCatView.d(TAG, "glog 开始写入, quantity:" + quantity.desc)
        val rawLogs = if (quantity == LogQuantity._10K) rawLogs10k else rawLogs100k
        var totalTime = 0L
        var originalTotalBytes = 0L

        for (i in 1..quantity.value) {
            val data = rawLogs[i - 1].toByteArray()
            originalTotalBytes += data.size
            val begin = SystemClock.elapsedRealtimeNanos()
            glog.write(data)
            totalTime += SystemClock.elapsedRealtimeNanos() - begin
        }

        logCatView.d(
            TAG,
            "glog写入完成 ${protoName}，耗时:" + TimeUnit.NANOSECONDS.toMillis(totalTime) + " 毫秒"
        )

        val printFileSizeBlock = {
            var size = 0L
            File(logDirectory)
                .listFiles { file -> file.name.contains(protoName) }!!
                .forEach { file -> size += file.length() }
            logCatView.i(
                TAG,
                " 日志文件体积:" + size.toReadableSize() + ", 原始数据体积:" + originalTotalBytes.toReadableSize()
            )
        }

        if (async) {
            logCatView.i(TAG, "计算日志文件体积...")
            object : CountDownTimer(10_000, 1000) {
                override fun onTick(millisUntilFinished: Long) {
                    logCatView.i(TAG, "等待: ${millisUntilFinished / 1000}s")
                }

                override fun onFinish() {
                    // 等待异步写入完成后计算文件体积
                    printFileSizeBlock()
                }
            }.start()
        } else {
            printFileSizeBlock()
        }
    }

    fun glogProtobufWrite(
        context: Context,
        async: Boolean,
        quantity: LogQuantity,
        incremental: Boolean
    ) {
        val protoName = "glog-benchmark-pb-$async-${quantity.desc}-$incremental"
        val glog = Glog.Builder(context)
            .async(async)
            .compressMode(Glog.CompressMode.Zlib)
            .incrementalArchive(incremental)
            .key(SVR_PUB_KEY)
            .encryptMode(Glog.EncryptMode.AES)
            .rootDirectory(logDirectory)
            .protoName(protoName)
            .build()

        logCatView.d(TAG, "glog pb 开始写入, quantity:" + quantity.desc)
        val rawLogs = if (quantity == LogQuantity._10K) rawLogs10k else rawLogs100k
        val pid = Process.myPid()
        val tid = Thread.currentThread().id
        var totalTime = 0L
        var originalTotalBytes = 0L

        for ((seq, i) in (1..quantity.value).withIndex()) {
            val timestamp = System.currentTimeMillis().toString()
            val begin = SystemClock.elapsedRealtimeNanos()

            LogProtos.Log.newBuilder()
                .setLogLevel(LogProtos.Log.Level.INFO)
                .setSequence(seq.toLong())
                .setTimestamp(timestamp)
                .setPid(pid)
                .setTid(tid.toString())
                .setTag("")
                .setMsg(rawLogs[i - 1])
                .build()
                .toByteArray()
                .also {
                    originalTotalBytes += it.size
                    glog.write(it)
                }

            totalTime += SystemClock.elapsedRealtimeNanos() - begin
        }

        logCatView.d(
            TAG,
            "glog pb 写入完成 ${protoName}，耗时:" + TimeUnit.NANOSECONDS.toMillis(totalTime) + " 毫秒"
        )

        val printFileSizeBlock = {
            var size = 0L
            File(logDirectory)
                .listFiles { file -> file.name.contains(protoName) }!!
                .forEach { file -> size += file.length() }
            logCatView.i(
                TAG,
                " 日志文件体积:" + size.toReadableSize() + ", 原始数据体积:" + originalTotalBytes.toReadableSize()
            )
        }

        if (async) {
            logCatView.i(TAG, "计算日志文件体积...")
            object : CountDownTimer(10_000, 1000) {
                override fun onTick(millisUntilFinished: Long) {
                    logCatView.i(TAG, "等待: ${millisUntilFinished / 1000}s")
                }

                override fun onFinish() {
                    // 等待异步写入完成后计算文件体积
                    printFileSizeBlock()
                }
            }.start()
        } else {
            printFileSizeBlock()
        }
    }

    fun xlogWrite(context: Context, quantity: LogQuantity) {
        logCatView.d(TAG, "xlog 开始写入, quantity:" + quantity.desc)
        val rawLogs = if (quantity == LogQuantity._10K) rawLogs10k else rawLogs100k
        var totalTime = 0L
        var originalTotalBytes = 0L

        for (i in 1..quantity.value) {
            val data = rawLogs[i - 1]
            originalTotalBytes += data.length
            val begin = SystemClock.elapsedRealtimeNanos()
            XLogger.i("", data)
            totalTime += SystemClock.elapsedRealtimeNanos() - begin
        }
//        XLogger.appenderFlushSync(true)

        logCatView.d(
            TAG,
            "xlog 写入完成，耗时:" + TimeUnit.NANOSECONDS.toMillis(totalTime) + " 毫秒"
        )
        // xlog 没有异步模式，直接打印体积
        var size = 0L
        File(logDirectory)
            .listFiles { file -> file.name.contains(XLOG_FILE_PREFIX) }
            ?.forEach { file -> size += file.length() }
        logCatView.i(
            TAG,
            " 日志文件体积:" + size.toReadableSize() + ", 原始数据体积:" + originalTotalBytes.toReadableSize()
        )
    }

    fun loganWrite(context: Context, quantity: LogQuantity) {
        logCatView.d(TAG, "logan 开始写入, quantity:" + quantity.desc)
        val rawLogs = if (quantity == LogQuantity._10K) rawLogs10k else rawLogs100k
        var totalTime = 0L
        var originalTotalBytes = 0L

        for (i in 1..quantity.value) {
            val data = rawLogs[i - 1]
            originalTotalBytes += data.length
            val begin = SystemClock.elapsedRealtimeNanos()
            Logan.w(data, 9)
            totalTime += SystemClock.elapsedRealtimeNanos() - begin
        }

        logCatView.d(
            TAG,
            "logan 写入完成，耗时:" + TimeUnit.NANOSECONDS.toMillis(totalTime) + " 毫秒"
        )

        val printFileSizeBlock = {
            var size = 0L
            File(context.getExternalFilesDir(null)!!.absolutePath + "/$LOGAN_DIR_NAME")
                .listFiles { file -> file.isFile && file.length() > 0 }!!
                .forEach { file -> size += file.length() }
            logCatView.i(
                TAG,
                " 日志文件体积:" + size.toReadableSize() + ", 原始数据体积:" + originalTotalBytes.toReadableSize()
            )
        }
        logCatView.i(TAG, "计算日志文件体积...")
        object : CountDownTimer(10_000, 1000) {
            override fun onTick(millisUntilFinished: Long) {
                logCatView.i(TAG, "等待: ${millisUntilFinished / 1000}s")
            }

            override fun onFinish() {
                // 等待异步写入完成后计算文件体积
                printFileSizeBlock()
            }
        }.start()
    }

    private fun Long.toReadableSize(): String {
        return if (this <= 0) {
            "0"
        } else {
            val fileUnit = arrayOf("B", "KB", "MB", "GB", "TB")
            val group = (log10(this.toDouble()) / log10(1024.0)).toInt()
            "${DecimalFormat("#,##0.#").format(this / 1024.0.pow(group.toDouble()))} ${fileUnit[group]}"
        }
    }
}