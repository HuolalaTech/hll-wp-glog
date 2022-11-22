package glog.android.sample

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import glog.android.Glog
import kotlinx.android.synthetic.main.activity_sample.*
import java.util.*
import java.util.concurrent.atomic.AtomicLong

/**
 * Created by issac on 2022/7/25.
 */
private const val TAG = "Sample"

class SampleActivity : AppCompatActivity() {
    private val incrementalGlog: Glog by lazy { // 日志按天归档
        Glog.Builder(this.applicationContext)
            .protoName("glog-incremental")
            .encryptMode(Glog.EncryptMode.AES) // 可选
            .key(SVR_PUB_KEY) // 可选
            .incrementalArchive(true)
            .build()
    }

    private val nonIncrementalGlog: Glog by lazy { // 重命名缓存文件的方式归档
        Glog.Builder(this.applicationContext)
            .protoName("glog-non-incremental")
            .encryptMode(Glog.EncryptMode.AES) // 可选
            .key(SVR_PUB_KEY) // 可选
            .incrementalArchive(false)
            .build()
    }

    private val seq = AtomicLong()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_sample)
        log_cat_view.v(TAG, getString(R.string.slogan_txt))
        write_button.setOnClickListener { write() }
        read_button.setOnClickListener {
            readIncremental()
            readNonIncremental()
        }
        benchmark_button.setOnClickListener {
            startActivity(
                Intent(
                    this,
                    BenchmarkActivity::class.java
                )
            )
        }
    }

    private fun write() {
        log_cat_view.i(TAG, "开始写入")
        for (i in 1..100) {
            val bytes = serializeLog()
            incrementalGlog.write(bytes)
            nonIncrementalGlog.write(bytes)
        }
        log_cat_view.i(TAG, "写入完成")
    }

    private fun readIncremental() {
        incrementalGlog.flush()
        val logArchiveFiles = incrementalGlog.getArchivesOfDate(Date().time / 1000)
        val inBuf = ByteArray(Glog.getSingleLogMaxLength())

        log_cat_view.i(TAG, "开始读取当天日志, 文件列表:" + Arrays.toString(logArchiveFiles))

        for (file in logArchiveFiles) {
            incrementalGlog.openReader(file, SVR_PRIV_KEY).use { reader ->
                while (true) {
                    val count = reader.read(inBuf)
                    if (count < 0) { // 读取结束
                        break
                    } else if (count == 0) { // 破损恢复
                        continue
                    }
                    val outBuf = ByteArray(count)
                    System.arraycopy(inBuf, 0, outBuf, 0, count)
                    deserializeLog(outBuf)
                }
            }
        }
        log_cat_view.i(TAG, "读取结束")
    }

    private fun readNonIncremental() {
        val logArchiveFiles = arrayListOf<String>()
        // 如果缓存中日志条数 > 0 或 缓存中日志体积 > 0 则 flush 缓存中日志到归档文件, 返回归档相关状态信息
        val statusMessage = nonIncrementalGlog.getArchiveSnapshot(logArchiveFiles, 0, 0)
        val inBuf = ByteArray(Glog.getSingleLogMaxLength())

        log_cat_view.i(TAG, "开始读取日志, 文件列表:$logArchiveFiles")
        log_cat_view.i(TAG, "状态信息:$statusMessage")

        for (file in logArchiveFiles) {
            nonIncrementalGlog.openReader(file, SVR_PRIV_KEY).use { reader ->
                while (true) {
                    val count = reader.read(inBuf)
                    if (count < 0) { // 读取结束
                        break
                    } else if (count == 0) { // 破损恢复
                        continue
                    }
                    val outBuf = ByteArray(count)
                    System.arraycopy(inBuf, 0, outBuf, 0, count)
                    deserializeLog(outBuf)
                }
            }
        }
        log_cat_view.i(TAG, "读取结束")
    }

    private fun serializeLog(): ByteArray {
        return LogProtos.Log.newBuilder()
            .setLogLevel(LogProtos.Log.Level.INFO)
            .setSequence(seq.getAndIncrement())
            .setTimestamp(System.currentTimeMillis().toString())
            .setPid(android.os.Process.myPid())
            .setTid(Thread.currentThread().id.toString())
            .setTag("GlogSample")
            .setMsg("your_message")
            .build()
            .toByteArray()
    }

    private fun deserializeLog(bytes: ByteArray) {
        val log = LogProtos.Log.parseFrom(bytes)
        log_cat_view.d(TAG, log.string())
    }

    private fun LogProtos.Log.string(): String {
        return "Log{" +
                "sequence=" + sequence +
                ", timestamp='" + timestamp + '\'' +
                ", logLevel=" + logLevel +
                ", pid=" + pid +
                ", tid='" + tid + '\'' +
                ", tag='" + tag + '\'' +
                ", msg='" + msg + '\'' +
                '}'
    }
}