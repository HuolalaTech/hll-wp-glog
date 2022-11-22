package glog.android.sample

import android.app.Application
import android.util.Log
import glog.android.Glog
import glog.android.sample.BuildConfig
import com.dianping.logan.Logan
import com.dianping.logan.LoganConfig
import com.getkeepsafe.relinker.ReLinker
import com.tencent.mars.xlog.Xlog
import java.lang.reflect.Field
import com.tencent.mars.xlog.Log as XLogger


/**
 * Created by issac on 2021/3/1.
 */

class SampleApplication : Application() {
    override fun onCreate() {
        super.onCreate()

        glogInit()
        xlogInit()
        loganInit()
    }

    private fun glogInit() {
        try {
            Glog.initialize(
                if (BuildConfig.DEBUG)
                    Glog.InternalLogLevel.InternalLogLevelDebug
                else
                    Glog.InternalLogLevel.InternalLogLevelInfo
            ) { libName ->
                ReLinker.log {
                    Log.d("ReLinker", it)
                }
//                .force()
//                .recursively()
                    .loadLibrary(this@SampleApplication, libName)
                Log.d("ReLinker", "load ${System.mapLibraryName(libName)} success")
            }
            Log.d("glog", "glog initialize success")
        } catch (t: Throwable) {
            Log.e("glog", "glog initialize failed", t)
        }
    }

    private fun xlogInit() {
//        System.loadLibrary("c++_shared")
//        System.loadLibrary("marsxlogxx")

        val logPath = getExternalFilesDir(null)!!.absolutePath + "/$LOG_DIR_NAME"

        Xlog.open(
            true, Xlog.LEVEL_ALL, Xlog.AppednerModeAsync, logPath + "/xlogcache",
            logPath, XLOG_FILE_PREFIX, SVR_PUB_KEY
        )

        XLogger.setLogImp(Xlog())
//        XLogger.setConsoleLogOpen(true)
//        XLogger.appenderOpen(
//            Xlog.LEVEL_ALL,
//            Xlog.AppednerModeAsync,
//            logPath+"/xlogcache",
//            logPath,
//            XLOG_FILE_PREFIX,
//            9999
//        )
    }

    private fun loganInit() {
        val config = LoganConfig.Builder()
            .setCachePath(getExternalFilesDir(null)!!.absolutePath + "/$LOGAN_DIR_NAME")
            .setPath(getExternalFilesDir(null)!!.absolutePath + "/$LOGAN_DIR_NAME")
            .setEncryptKey16("0123456789012345".toByteArray())
            .setEncryptIV16("0123456789012345".toByteArray())
            .build()

        // modify capacity of logan's async write queue, otherwise, logs may be lost
        val mMaxQueueField: Field = LoganConfig::class.java.getDeclaredField("mMaxQueue")
        mMaxQueueField.isAccessible = true
        val oldMaxQueue = mMaxQueueField.getLong(config)
        println("=== oldMaxQueue:$oldMaxQueue")
        mMaxQueueField.setLong(config, Long.MAX_VALUE)

        Logan.init(config)
    }
}