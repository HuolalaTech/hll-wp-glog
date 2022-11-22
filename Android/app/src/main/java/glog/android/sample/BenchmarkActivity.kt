package glog.android.sample

import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.PopupMenu
import kotlinx.android.synthetic.main.activity_benchmark.*
import java.io.File
import java.util.zip.CRC32
import kotlin.concurrent.thread

const val LOG_DIR_NAME = "log"
const val LOGAN_DIR_NAME = "logan"

private const val TAG = "BenchmarkActivity"

private enum class Logger { Glog, Xlog, Logan }
class BenchmarkActivity : AppCompatActivity() {
    private val logDirectory: String by lazy {
        "${getExternalFilesDir(null)!!.absolutePath}/$LOG_DIR_NAME"
    }
    private val rawDataGenerator: RawDataGenerator by lazy {
        RawDataGenerator("${getExternalFilesDir(null)!!.absolutePath}/raw-log")
    }
    private val benchmark: GlogBenchmark by lazy {
        GlogBenchmark(applicationContext, logDirectory, log_cat_view)
    }
    private var logger = Logger.Glog
    private var quantity = LogQuantity._10K

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_benchmark)
        log_cat_view.v(TAG, getString(R.string.slogan_txt))
        if (!verifyRaw("1c0cadbc", RAW_FILE_NAME_10K)
            || !verifyRaw("31a367b6", RAW_FILE_NAME_100K)
        ) {
            Toast.makeText(this, "raw file crc mismatch", Toast.LENGTH_LONG).show()
            finish()
            return
        }

        ArrayAdapter.createFromResource(
            this,
            R.array.logger_names,
            android.R.layout.simple_spinner_item
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            logger_spinner.adapter = adapter
            logger_spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(
                    parent: AdapterView<*>?,
                    view: View?,
                    position: Int,
                    id: Long
                ) {
                    logger = Logger.values()[position]
                    protobuf_switch.isEnabled = logger == Logger.Glog
                    incremental_switch.isEnabled = logger == Logger.Glog
                    async_mode_switch.isEnabled = logger == Logger.Glog
                }

                override fun onNothingSelected(parent: AdapterView<*>?) {
                }
            }
        }
        ArrayAdapter.createFromResource(
            this,
            R.array.quantity_types,
            android.R.layout.simple_spinner_item
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            log_quantity_spinner.adapter = adapter
            log_quantity_spinner.onItemSelectedListener =
                object : AdapterView.OnItemSelectedListener {
                    override fun onItemSelected(
                        parent: AdapterView<*>?,
                        view: View?,
                        position: Int,
                        id: Long
                    ) {
                        quantity = if (position == 0) {
                            LogQuantity._10K
                        } else {
                            LogQuantity._100K
                        }
                    }

                    override fun onNothingSelected(parent: AdapterView<*>?) {
                    }
                }
        }
        start_write.setOnClickListener {
            if (glog.android.BuildConfig.DEBUG) {
                Toast.makeText(this, "建议在 Release 模式下运行基准测试", Toast.LENGTH_LONG).show()
            }
            when (logger) {
                Logger.Glog -> {
                    if (protobuf_switch.isChecked) {
                        benchmark.glogProtobufWrite(
                            applicationContext,
                            async_mode_switch.isChecked,
                            quantity,
                            incremental_switch.isChecked
                        )
                    } else {
                        benchmark.glogWrite(
                            applicationContext,
                            async_mode_switch.isChecked,
                            quantity,
                            incremental_switch.isChecked
                        )
                    }
                }
                Logger.Xlog -> benchmark.xlogWrite(applicationContext, quantity)
                Logger.Logan -> benchmark.loganWrite(applicationContext, quantity)
            }
        }

        more_button.setOnClickListener { v ->
            PopupMenu(this, v).also { popMenu ->
                popMenu.inflate(R.menu.more_menu)
                popMenu.show()
                popMenu.setOnMenuItemClickListener { item ->
                    when (item.itemId) {
                        R.id.generate_data -> thread {
                            runOnUiThread { log_cat_view.i(TAG, "开始生成测试数据") }
                            val start = System.currentTimeMillis()
                            rawDataGenerator.generate(applicationContext)
                            runOnUiThread {
                                log_cat_view.i(
                                    TAG,
                                    "生成测试数据完成, 耗时: ${System.currentTimeMillis() - start} millis"
                                )
                            }
                        }
                        R.id.cleanup -> {
                            File(logDirectory).listFiles()?.forEach {
                                it.delete()
                            }
                            log_cat_view.i(TAG, "清理完成")
                        }
                    }
                    true
                }
            }
        }
    }

    /**
     * 校验测试数据一致性
     */
    private fun verifyRaw(expected: String, raw: String): Boolean {
        var ret = false

        assets.open(raw).use { input ->
            ret = CRC32().apply {
                val buffer = ByteArray(8192)
                var n: Int

                while (true) {
                    n = input.read(buffer)
                    if (n < 0) {
                        break
                    }
                    update(buffer, 0, n)
                }
            }.value.toString(16) == expected
        }

        return ret
    }
}