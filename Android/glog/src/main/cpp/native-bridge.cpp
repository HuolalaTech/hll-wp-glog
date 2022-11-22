#include "Glog.h"
#include "GlogBuffer.h"
#include "GlogReader.h"
#include <jni.h>
#include "InternalLog.h"
#include "android-log.h"
#include <sys/stat.h>
#include <thread>

using namespace glog;
using namespace std;

static jclass g_cls = nullptr;
static JavaVM *g_currentJVM = nullptr;

static string jstring2string(JNIEnv *env, jstring str);
static jstring string2jstring(JNIEnv *env, const string &str);

static int registerNativeMethods(JNIEnv *env, jclass cls);

//static void internalLog(InternalLogLevel level,
//                        const char *file,
//                        int line,
//                        const char *function,
//                        const std::string &message) {
//    const string format = "[%s] <%s:%d::%s> %s";
//    switch (level) {
//    case InternalLogLevelDebug:LOGD(format.c_str(), "D", file, line, function, message.c_str());
//        break;
//    case InternalLogLevelInfo:LOGI(format.c_str(), "I", file, line, function, message.c_str());
//        break;
//    case InternalLogLevelWarning:LOGW(format.c_str(), "W", file, line, function, message.c_str());
//        break;
//    case InternalLogLevelError:LOGE(format.c_str(), "E", file, line, function, message.c_str());
//        break;
//    default:break;
//    }
//}

extern "C" JNIEXPORT JNICALL
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_currentJVM = vm;
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    if (g_cls) {
        env->DeleteGlobalRef(g_cls);
    }
    static const char *clsName = "glog/android/Glog";
    jclass instance = env->FindClass(clsName);
    if (!instance) {
        InternalError("fail to locate class: %s", clsName);
        return -2;
    }
    g_cls = reinterpret_cast<jclass>(env->NewGlobalRef(instance));
    if (!g_cls) {
        InternalError("fail to create global reference for %s", clsName);
        return -3;
    }
    int ret = registerNativeMethods(env, g_cls);
    if (ret != 0) {
        InternalError("fail to register native methods for class %s, ret = %d", clsName, ret);
        return -4;
    }
    return JNI_VERSION_1_6;
}

static string jstring2string(JNIEnv *env, jstring str) {
    if (str) {
        const char *kstr = env->GetStringUTFChars(str, nullptr);
        if (kstr) {
            string result(kstr);
            env->ReleaseStringUTFChars(str, kstr);
            return result;
        }
    }
    return "";
}

static jstring string2jstring(JNIEnv *env, const string &str) {
    return env->NewStringUTF(str.c_str());
}

static bool isFileExists(const std::string &path) {
    struct stat st{};
    return (stat(path.c_str(), &st) == 0);
}

namespace glog_jni {

static void jniInitialize(JNIEnv *env, jobject obj, jint logLevel) {
    Glog::initialize(static_cast<InternalLogLevel>(logLevel));
//    Glog::registerLogHandler(internalLog);
}

static jlong
jniMaybeCreateWithConfig(JNIEnv *env,
                         jobject obj,
                         jstring rootDirectory,
                         jstring protoName,
                         jboolean incrementalArchive,
                         jboolean async,
                         jint expireSeconds,
                         jint totalArchiveSizeLimit,
                         jint compressMode,
                         jint encryptMode,
                         jstring key) {
    Glog *glPtr = nullptr;
    if (!rootDirectory || !protoName) {
        return reinterpret_cast<jlong>(glPtr);
    }
    string svrKey = jstring2string(env, key);
    if (encryptMode == (uint8_t) format::GlogEncryptMode::AES && svrKey.empty()) {
        return reinterpret_cast<jlong>(glPtr);
    }
    string root = jstring2string(env, rootDirectory);
    string proto = jstring2string(env, protoName);

    if (root.empty() || proto.empty()) {
        return reinterpret_cast<jlong>(glPtr);
    }

    glPtr = Glog::maybeCreateWithConfig(
        GlogConfig{
            .m_protoName = proto,
            .m_rootDirectory = root,
            .m_incrementalArchive = incrementalArchive == JNI_TRUE,
            .m_async = async == JNI_TRUE,
            .m_expireSeconds = expireSeconds,
            .m_totalArchiveSizeLimit = static_cast<size_t>(totalArchiveSizeLimit),
            .m_compressMode = static_cast<format::GlogCompressMode>(compressMode),
            .m_encryptMode = static_cast<format::GlogEncryptMode>(encryptMode),
            .m_serverPublicKey = &svrKey
        }
    );
    return reinterpret_cast<jlong>(glPtr);
}

static jboolean jniWrite(JNIEnv *env,
                         jobject obj,
                         jlong ptr,
                         jbyteArray bytes,
                         jint offset,
                         jint len) {
    Glog *glPtr = reinterpret_cast<Glog *>(ptr);
    if (glPtr) {
        jbyte *data = env->GetByteArrayElements(bytes, nullptr);
        jboolean ret = false;
        if (data) {
            ret = glPtr->write(GlogBuffer(data + offset, len));
            env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
        } else {
            InternalError("fail to alloc array, size:%d", len);
        }
        return ret;
    }
    return false;
}

static void jniFlush(JNIEnv *env, jobject obj, jlong ptr) {
    Glog *glPtr = reinterpret_cast<Glog *>(ptr);
    if (glPtr) {
        glPtr->flush();
    }
}

static void jniDestroy(JNIEnv *env, jobject obj, jstring protoName) {
    string proto = jstring2string(env, protoName);
    if (!proto.empty())
        Glog::destroy(proto);
}

static void jniDestroyAll(JNIEnv *env, jobject obj) {
    Glog::destroyAll();
}

static jstring jniGetArchiveSnapshot(JNIEnv *env,
                                     jobject obj,
                                     jlong ptr,
                                     jobject outFiles,
                                     jboolean flush,
                                     jlong minLogNum,
                                     jlong totalLogSize,
                                     jint order) {
    Glog *glPtr = reinterpret_cast<Glog *>(ptr);
    if (glPtr) {
        ArchiveCondition cond =
            {flush == JNI_TRUE,
             static_cast<size_t>(minLogNum),
             static_cast<size_t>(totalLogSize),
            };
        vector<string> archives;
        string message = glPtr->getArchiveSnapshot(archives, cond, static_cast<FileOrder>(order));

        if (archives.empty()) {
            return string2jstring(env, message); // message already contains files num
        }

        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        if (!arrayListClass) {
            string error = "fail to locate java/util/ArrayList";
            InternalError(error.c_str());
            return string2jstring(env, message += "\n" + error);
        }

        jmethodID addMethodID = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
        if (!addMethodID) {
            string error = "fail to locate java/util/ArrayList.add()";
            InternalError(error.c_str());
            return string2jstring(env, message += "\n" + error);
        }

        for (auto &i : archives) {
            jstring archive = string2jstring(env, i);
            env->CallBooleanMethod(outFiles, addMethodID, archive);
            env->DeleteLocalRef(archive);
        }
        return string2jstring(env, message);
    }
    return string2jstring(env, "glPtr == null");
}

static jobjectArray jniGetArchivesOfDate(JNIEnv *env,
                                         jobject obj,
                                         jlong ptr,
                                         jlong epochSeconds) {
    Glog *glPtr = reinterpret_cast<Glog *>(ptr);
    if (glPtr) {
        vector<string> archives;
        glPtr->getArchivesOfDate(epochSeconds, archives);
        jobjectArray result =
            env->NewObjectArray(archives.size(), env->FindClass("java/lang/String"), nullptr);
        for (int i = 0; i < archives.size(); ++i) {
            env->SetObjectArrayElement(result, i, env->NewStringUTF(archives.at(i).c_str()));
        }
        return result;
    }
    return nullptr;
}

static jlong jniOpenReader(JNIEnv *env,
                           jobject obj,
                           jlong glPtr,
                           jstring archiveFile,
                           jstring key) {
    Glog *glogPtr = reinterpret_cast<Glog *>(glPtr);
    GlogReader *readerPtr = nullptr;

    if (glogPtr) {
        if (!archiveFile) {
            return reinterpret_cast<jlong>(readerPtr);
        }
        string filepath = jstring2string(env, archiveFile);

        if (filepath.empty() || !isFileExists(filepath)) {
            return reinterpret_cast<jlong>(readerPtr);
        }
        string svrKey = jstring2string(env, key);
        readerPtr = glogPtr->openReader(filepath, &svrKey);
    }
    return reinterpret_cast<jlong>(readerPtr);
}

static jint jniRead(JNIEnv *env, jobject obj, jlong ptr, jbyteArray buf, jint offset, jint len) {
    auto *readerPtr = reinterpret_cast<GlogReader *>(ptr);
    jint result = -1;

    if (readerPtr) {
        if (len < SINGLE_LOG_CONTENT_MAX_LENGTH) {
            InternalWarning(
                "reader buffer:%d less than max log length:%d , may read ONLY part of log");
        }
        jbyte *jOutBuf = env->GetByteArrayElements(buf, nullptr);
        GlogBuffer outBuffer(jOutBuf + offset, len);
        result = readerPtr->read(outBuffer);
        if (jOutBuf)
            env->ReleaseByteArrayElements(buf, jOutBuf, 0);
    }
    return result;
}

static jint jniGetCurrentPosition(JNIEnv *env, jobject obj, jlong ptr) {
    auto *readerPtr = reinterpret_cast<GlogReader *>(ptr);
    if (readerPtr) {
        return readerPtr->getPosition();
    }
    return -1;
}

static jboolean jniSeek(JNIEnv *env, jobject obj, jlong ptr, jint offset) {
    auto *readerPtr = reinterpret_cast<GlogReader *>(ptr);
    if (readerPtr) {
        return readerPtr->seek(offset);
    }
    return false;
}

static void jniCloseReader(JNIEnv *env, jobject obj, jlong glPtr, jlong readerPtr) {
    auto *glog = reinterpret_cast<Glog *>(glPtr);
    auto *reader = reinterpret_cast<GlogReader *>(readerPtr);
    if (glPtr && reader) {
        glog->closeReader(reader);
    }
}

static jlong jniGetSystemPageSize(JNIEnv *env, jobject obj) {
    return glog::SYS_PAGE_SIZE;
}

static jint jniGetSingleLogMaxLength(JNIEnv *env, jobject obj) {
    return glog::SINGLE_LOG_CONTENT_MAX_LENGTH;
}

static jboolean jniResetExpireSeconds(JNIEnv *env, jobject obj, jlong ptr, jint seconds) {
    auto *gPtr = reinterpret_cast<Glog *>(ptr);
    if (gPtr) {
        return gPtr->resetExpireSeconds(seconds);
    }
    return false;
}

static jint jniGetCacheSize(JNIEnv *env, jobject obj, jlong ptr) {
    auto *gPtr = reinterpret_cast<Glog *>(ptr);
    if (gPtr) {
        return gPtr->getCacheSize();
    }
    return -1;
}

static jstring jniGetCacheFileName(JNIEnv *env, jobject obj, jlong ptr) {
    auto *gPtr = reinterpret_cast<Glog *>(ptr);
    if (gPtr) {
        string filename = gPtr->getCacheFileName();
        return string2jstring(env, filename);
    }
    return string2jstring(env, "");
}

static void jniRemoveAll(JNIEnv *env,
                         jobject obj,
                         jlong ptr,
                         jboolean removeReadingFiles,
                         jboolean reloadCache) {
    auto *gPtr = reinterpret_cast<Glog *>(ptr);
    if (gPtr) {
        gPtr->removeAll(removeReadingFiles, reloadCache);
    }
}

static void jniRemoveArchiveFile(JNIEnv *env, jobject obj, jlong ptr, jstring archiveFile) {
    auto *gPtr = reinterpret_cast<Glog *>(ptr);
    if (gPtr) {
        string file = jstring2string(env, archiveFile);
        if (!file.empty()) {
            gPtr->removeArchiveFile(file, false);
        }
    }
}

}

static JNINativeMethod g_methods[] = {
    {"jniInitialize", "(I)V", (void *) glog_jni::jniInitialize},
    {"jniMaybeCreateWithConfig", "(Ljava/lang/String;Ljava/lang/String;ZZIIIILjava/lang/String;)J",
     (void *) glog_jni::jniMaybeCreateWithConfig},
    {"jniWrite", "(J[BII)Z", (void *) glog_jni::jniWrite},
    {"jniFlush", "(J)V", (void *) glog_jni::jniFlush},
    {"jniDestroy", "(Ljava/lang/String;)V", (void *) glog_jni::jniDestroy},
    {"jniDestroyAll", "()V", (void *) glog_jni::jniDestroyAll},
    {"jniGetArchiveSnapshot", "(JLjava/util/ArrayList;ZJJI)Ljava/lang/String;",
     (void *) glog_jni::jniGetArchiveSnapshot},
    {"jniGetArchivesOfDate", "(JJ)[Ljava/lang/String;", (void *) glog_jni::jniGetArchivesOfDate},
    {"jniOpenReader", "(JLjava/lang/String;Ljava/lang/String;)J", (void *) glog_jni::jniOpenReader},
    {"jniRead", "(J[BII)I", (void *) glog_jni::jniRead},
    {"jniGetCurrentPosition", "(J)I", (void *) glog_jni::jniGetCurrentPosition},
    {"jniSeek", "(JI)Z", (void *) glog_jni::jniSeek},
    {"jniCloseReader", "(JJ)V", (void *) glog_jni::jniCloseReader},
    {"jniGetSystemPageSize", "()J", (void *) glog_jni::jniGetSystemPageSize},
    {"jniGetSingleLogMaxLength", "()I", (void *) glog_jni::jniGetSingleLogMaxLength},
    {"jniResetExpireSeconds", "(JI)Z", (void *) glog_jni::jniResetExpireSeconds},
    {"jniGetCacheSize", "(J)I", (void *) glog_jni::jniGetCacheSize},
    {"jniRemoveAll", "(JZZ)V", (void *) glog_jni::jniRemoveAll},
    {"jniRemoveArchiveFile", "(JLjava/lang/String;)V", (void *) glog_jni::jniRemoveArchiveFile},
    {"jniGetCacheFileName", "(J)Ljava/lang/String;", (void *) glog_jni::jniGetCacheFileName}
};

static int registerNativeMethods(JNIEnv *env, jclass cls) {
    return env->RegisterNatives(cls, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
}