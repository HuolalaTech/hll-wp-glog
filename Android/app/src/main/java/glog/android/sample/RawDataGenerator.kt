package glog.android.sample

import android.content.Context
import android.os.Handler
import android.widget.Toast
import java.io.File

/**
 * Created by issac on 2021/3/4.
 */
const val RAW_FILE_NAME_10K = "raw_10k.log"
const val RAW_FILE_NAME_100K = "raw_100k.log"

const val NUM_10K = 10_000
const val NUM_100K = 100_000

/**
 * 生成测试数据, app assets 中已经有一份通过下面代码生成的数据
 */
class RawDataGenerator(private val rawFileRoot: String) {
    fun generate(context: Context) {
        generate(context, RAW_FILE_NAME_10K, NUM_10K)
        generate(context, RAW_FILE_NAME_100K, NUM_100K)
    }

    // generate test log data, for idempotent test
    private fun generate(context: Context, fileName: String, logNum: Int) {
        val rawFile = File("$rawFileRoot/$fileName")
        rawFile.parentFile?.mkdirs()
        if (rawFile.exists()) {
            Handler(context.mainLooper).post {
                Toast.makeText(context, "file [$rawFile] already exists", Toast.LENGTH_LONG).show()
            }
            return
        }

        println("=== generate file:$fileName")

        // random string length 1 - 200, write to file line by line
        val alphas = CharRange('A', 'Z')
        val lens = IntRange(1, 200)
        var size = 0

        for (i in 1..logNum) {
            val len = lens.random()
            val sb = StringBuilder()

            for (j in 1..len) {
                sb.append(alphas.random())
            }
            sb.append("\n")
            val line = sb.toString()
            println("=== write line:$line")
            size += line.length
            rawFile.appendText(line)
        }
        println("=== write total size:$size")
    }
}