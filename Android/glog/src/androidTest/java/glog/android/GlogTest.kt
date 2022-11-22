package glog.android

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.Matchers.*
import org.junit.*
import org.junit.Assert.*
import org.junit.runner.RunWith
import java.io.File
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread


/**
 * Created by issac on 2021/2/24.
 */
@RunWith(AndroidJUnit4::class)
class GlogTest {
    companion object {
        private lateinit var sRootDirectory: File

        @BeforeClass
        @JvmStatic
        fun initialize() {
            Glog.initialize(Glog.InternalLogLevel.InternalLogLevelInfo)
            val appContext = InstrumentationRegistry.getInstrumentation().targetContext
            sRootDirectory = File(appContext.filesDir.absolutePath + "/glog")
            sRootDirectory.listFiles()?.run {
                for (file in this) {
                    file.delete()
                }
            }
        }

        @AfterClass
        @JvmStatic
        fun destroy() {
            Glog.destroyAll()
            sRootDirectory.listFiles()?.run {
                for (file in this) {
                    file.delete()
                }
            }
        }
    }

    @Test
    fun multiInstanceWithSameProtoNameShareSameNativePtr() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        var ptr: Long = -1
        val protoName = "glog-test-share-ptr"
        for (i in 0..9) {
            val glog = Glog.Builder(appContext)
                .protoName(protoName)
                .expireSeconds(0)
                .totalArchiveSizeLimit(0)
                .build()

            assertTrue(glog.nativePtr > 0)
            if (ptr == -1L) {
                ptr = glog.nativePtr
            } else {
                assertEquals(ptr, glog.nativePtr)
            }
        }
    }

    @Test
    fun illegalLogLength() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val maxLength = Glog.getSingleLogMaxLength()
        val glog = Glog.Builder(appContext)
            .protoName("glog-test-max-log-len")
            .expireSeconds(0)
            .totalArchiveSizeLimit(0)
            .build()

        var bytes = ByteArray(maxLength)
        assertTrue(glog.write(bytes))

        bytes = ByteArray(0)
        assertFalse(glog.write(bytes))

        bytes = ByteArray(maxLength + 1)
        assertFalse(glog.write(bytes))
    }

    @Test
    fun archiveIfCacheRemainSpaceNotEnough() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val protoName = "glog-archive-cache-insufficient-space"
        val glog = Glog.Builder(appContext)
            .protoName(protoName)
            .expireSeconds(500)
            .totalArchiveSizeLimit(128 * 1024 * 1024)
            .compressMode(Glog.CompressMode.None)
            .build()

        glog.write(byteArrayOf(1, 2, 3))

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, 0, 0)

        assertEquals(1, files.size)

        assertEquals(
            2,
            sRootDirectory.listFiles { file -> file.name.contains(protoName) }?.size
        ) // 1(cache) + 1(archive)
//        Thread.sleep(3_000)

        val size = glog.cacheSize / 11 // every log contains 2 bytes of length +  8 byte sync marker

        for (i in 1..size) {
            glog.write(byteArrayOf(1)) // trigger archive, generate 1 file
        }

        Thread.sleep(800)
        assertEquals(
            3,
            sRootDirectory.listFiles { file -> file.name.contains(protoName) }?.size
        ) // 1(cache) + 1(archive) + 1(archive before 3 sec)
    }

    @Test
    @Ignore("expire file deleted by daemon")
    fun verifyExpireTime() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val protoName = "verify-expire-time"
        val glog = Glog.Builder(appContext)
            .protoName(protoName)
            .expireSeconds(3)
            .build()

        glog.write(byteArrayOf(1, 2, 3))
        glog.getArchiveSnapshot(arrayListOf(), 0, 0) // generate 1 archive

        Thread.sleep(4_000)

        glog.write(byteArrayOf(1, 2, 3))
        glog.getArchiveSnapshot(arrayListOf(), 0, 0) // generate 1 archive, delete 1 overdue archive

        assertEquals(2, sRootDirectory.listFiles { file -> file.name.contains(protoName) }?.size)
    }

    @Test
    fun verifyArchiveSizeLimit() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val protoName = "verify-archive-size-limit"
        val limit = 200 * 1024 * 1024L // 2 MB
        val glog = Glog.Builder(appContext)
            .protoName(protoName)
            .async(false)
            .expireSeconds(300)
            .totalArchiveSizeLimit(limit.toInt())
            .compressMode(Glog.CompressMode.None)
            .build()

        // write 5 MB
        for (i in 1..(5 * 1024 * 1024 / Glog.getSingleLogMaxLength())) {
            assertTrue(glog.write(ByteArray(Glog.getSingleLogMaxLength())))
        }
        Thread.sleep(3_000)

        val total = sRootDirectory
            .listFiles { file -> file.name.contains(protoName) }
            ?.map { file -> file.length() }
            ?.sum()

        assertThat(total!! - glog.cacheSize, lessThanOrEqualTo(limit))
    }

    @Test
    fun syncBatchWriteRead() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val glog = Glog.Builder(appContext)
            .protoName("glog-test-sync-batch-write")
            .rootDirectory(sRootDirectory.absolutePath)
            .async(false)
            .expireSeconds(24 * 60 * 60) // ensure native code do not clean archives
            .totalArchiveSizeLimit(128 * 1024 * 1024)
            .build()

        val logNum = 10000
        val content =
            "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hqurfj920qhfhafjp0h2iofh09hijafa(^(*^%$%^q29hfq89uhvqwwhfqwhef892hfw8qfhq8w9efhq89hf89q2fy89qh43f892hq89"
        for (i in 1..logNum) {
            assertTrue(glog.write("$i$content".toByteArray()))
        }

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, true, 0, 0, Glog.FileOrder.CreateTimeAscending)
        var index = 0
        files.forEach { file ->
            val reader = glog.openReader(file)
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                index++
                assertEquals("$index$content", String(buf, 0, n))
            }
            reader.close()
        }
        assertEquals(logNum, index)
    }

    @Test
    fun asyncBatchWriteRead() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val glog = Glog.Builder(appContext)
            .protoName("glog-test-async-batch-write")
            .rootDirectory(sRootDirectory.absolutePath)
            .async(true)
            .expireSeconds(24 * 60 * 60) // ensure native code do not clean archives
            .totalArchiveSizeLimit(128 * 1024 * 1024)
            .build()

        val logNum = 10000
        val content =
            "_log_header_89h4fuv82h02h9jadnvp(**$^#&>chinese29hfq9rhf982hqurfj920qhfhafjp0h2iofh09hijafa(^(*^%$%^q29hfq89uhvqwwhfqwhef892hfw8qfhq8w9efhq89hf89q2fy89qh43f892hq89"
        for (i in 1..logNum) {
            assertTrue(glog.write("$i$content".toByteArray()))
        }

        Thread.sleep(3_000) // wait 10s because async write returned prior to actual work done

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, true, 0, 0, Glog.FileOrder.CreateTimeAscending)
        var readLogNum = 0

        files.forEach { file ->
            val reader = glog.openReader(file)
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                readLogNum++
                assertEquals("$readLogNum$content", String(buf, 0, n))
            }
            reader.close()
        }
        assertEquals(readLogNum, logNum)
    }

    @Test
    fun multiThreadSyncBatchWriteRead() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val glog = Glog.Builder(appContext)
            .protoName("glog-test-multi-sync-batch-write")
            .rootDirectory(sRootDirectory.absolutePath)
            .async(false)
            .totalArchiveSizeLimit(128 * 1024 * 1024) // ensure native code do not clean archives
            .build()

        val logNum = 1000
        val threadNum = 100
        val content =
            "_log_header_89h4fuv82h02h9jadnvp(**$^#&>chinese29hfq9rhf982hqurfj920qhfhafjp0h2iofh09hijafa(^(*^%$%^q29hfq89uhvqwwhfqwhef892hfw8qfhq8w9efhq89hf89q2fy89qh43f892hq89"
        val index = AtomicInteger(1)
        for (t in 1..threadNum) {
            thread {
                for (i in 1..logNum) {
                    assertTrue(glog.write("${index.getAndIncrement()}$content".toByteArray()))
                }
            }.join()
        }

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, true, 0, 0, Glog.FileOrder.CreateTimeAscending)
        var readLogNum = 0

        files.forEach { file ->
            val reader = glog.openReader(file)
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                readLogNum++
                assertEquals("$readLogNum$content", String(buf, 0, n))
            }
            reader.close()
        }
        assertEquals(readLogNum, logNum * threadNum)
    }

    @Test
    fun multiThreadAsyncBatchWriteRead() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val glog = Glog.Builder(appContext)
            .protoName("glog-test-multi-async-batch-write")
            .rootDirectory(sRootDirectory.absolutePath)
            .totalArchiveSizeLimit(128 * 1024 * 1024) // ensure native code do not clean archives
            .build()

        val logNum = 1000
        val threadNum = 100
        val content =
            "_log_header_89h4fuv82h02h9jadnvp(**$^#&>chinese29hfq9rhf982hqurfj920qhfhafjp0h2iofh09hijafa(^(*^%$%^q29hfq89uhvqwwhfqwhef892hfw8qfhq8w9efhq89hf89q2fy89qh43f892hq89"
        val index = AtomicInteger(1)
        for (t in 1..threadNum) {
            thread {
                for (i in 1..logNum) {
                    assertTrue(glog.write("${index.getAndIncrement()}$content".toByteArray()))
                }
            }.join()
        }
        Thread.sleep(3_000) // wait 10s because async write returned prior to actual work done

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, true, 0, 0, Glog.FileOrder.CreateTimeAscending)
        var readLogNum = 0

        files.forEach { file ->
            val reader = glog.openReader(file)
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                readLogNum++
                assertEquals("$readLogNum$content", String(buf, 0, n))
            }
            reader.close()
        }
        assertEquals(readLogNum, logNum * threadNum)
    }

    @Test
    fun verifyExpireTimeAfterReset() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val protoName = "reset-expire-time"
        val glog = Glog.Builder(appContext)
            .protoName(protoName)
            .expireSeconds(3)
            .build()

        glog.write(byteArrayOf(1, 2, 3, 4, 5, 6))
        glog.getArchiveSnapshot(arrayListOf(), 0, 0) // generate 1 archive

        glog.resetExpireSeconds(5)
        Thread.sleep(4_000)

        glog.write(byteArrayOf(1, 2, 3, 4, 5, 6))
        glog.getArchiveSnapshot(arrayListOf(), 0, 0) // generate 1 archive, not delete

        assertEquals(3, sRootDirectory.listFiles { file -> file.name.contains(protoName) }?.size)

        glog.resetExpireSeconds(3) // delete file created 3 secs ago

        assertEquals(2, sRootDirectory.listFiles { file -> file.name.contains(protoName) }?.size)
    }

    @Test
    fun verifyDailyLogFileContent() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val protoName = "verify-daily-log"
        val glog = Glog.Builder(appContext)
            .incrementalArchive(true)
            .rootDirectory(sRootDirectory.absolutePath)
            .protoName(protoName)
            .build()

        val log =
            "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f" +
                    "_log_header_89h4fuv82h02h9jadnvp(**\$^#&>chinese29hfq9rhf982hq3ej98h9wa8fh98ewfhgw89fhuw98h89whe9888h9f"

        for (i in 1..10000) {
            glog.write((i.toString() + log).toByteArray())
        }

        glog.flush()

        val filename = protoName + "-" + SimpleDateFormat("yyyyMMdd").format(Date()) + ".glog"
        val file = File(sRootDirectory, filename)

        assertTrue(file.exists())
        assertTrue(file.length() > 128 * 1024)

        var readLogNum = 0
        glog.openReader(file.absolutePath).use { reader ->
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                readLogNum++
                assertEquals("$readLogNum$log", String(buf, 0, n))
            }
            assertEquals(10000, readLogNum)
        }
    }

    @Test
    fun asyncBatchWriteReadWithAesCipher() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val svrPubKey =
            "9CECA66EB62760F5A93406A6F98C1489EC69089C0A1F2608B18B72B0BF428CC9BAEB30CB946208A2BFC2255D0E77968CB07FE0E7E7042A3E638CDB553A94C650"
        val priKey = "00E11DED269138A3F834AF1948C0DE17363C9B78D979662C30E0BBFF7085B6CC"

        val glog = Glog.Builder(appContext)
            .protoName("glog-test-async-batch-write")
            .rootDirectory(sRootDirectory.absolutePath)
            .async(true)
            .expireSeconds(24 * 60 * 60) // ensure native code do not clean archives
            .totalArchiveSizeLimit(128 * 1024 * 1024)
            .encryptMode(Glog.EncryptMode.AES)
            .key(svrPubKey)
            .build()

        val logNum = 10000
        val content =
            "_log_header_89h4fuv82h02h9jadnvp(**$^#&>chinese29hfq9rhf982hqurfj920qhfhafjp0h2iofh09hijafa(^(*^%$%^q29hfq89uhvqwwhfqwhef892hfw8qfhq8w9efhq89hf89q2fy89qh43f892hq89"
        for (i in 1..logNum) {
            assertTrue(glog.write("$i$content".toByteArray()))
        }

        Thread.sleep(3_000) // wait 3s because async write returned prior to actual work done

        val files = arrayListOf<String>()
        val message = glog.getArchiveSnapshot(files, true, 0, 0, Glog.FileOrder.CreateTimeAscending)
        var readLogNum = 0

        files.forEach { file ->
            val reader = glog.openReader(file, priKey)
            while (true) {
                val buf = ByteArray(Glog.getSingleLogMaxLength())
                val n = reader.read(buf)

                if (n <= 0) break

                readLogNum++
                assertEquals("$readLogNum$content", String(buf, 0, n))
            }
            reader.close()
        }
        assertEquals(readLogNum, logNum)
    }
}