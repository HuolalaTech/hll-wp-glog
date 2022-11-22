package glog.android;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Created by issac on 2021/1/27.
 */
public final class Glog {
    public static final int DEFAULT_EXPIRES_SECS = 7 * 24 * 60 * 60; // 7 day
    public static final int DEFAULT_TOTAL_ARCHIVE_SIZE_LIMIT = 16 * 1024 * 1024; // 16 MB

    public enum InternalLogLevel {
        InternalLogLevelDebug(0), // not available for release/product build
        InternalLogLevelInfo(1),  // default level
        InternalLogLevelWarning(2),
        InternalLogLevelError(3),
        InternalLogLevelNone(4); // special level used to disable all log messages

        private final int value;

        InternalLogLevel(int value) {
            this.value = value;
        }

        public int value() {
            return value;
        }
    }

    public enum FileOrder {
        None(0),
        CreateTimeAscending(1),
        CreateTimeDescending(2);

        private final int value;

        FileOrder(int value) {
            this.value = value;
        }

        public int value() {
            return value;
        }
    }

    public enum CompressMode {
        None(1),
        Zlib(2);

        private final int value;

        CompressMode(int value) {
            this.value = value;
        }

        public int value() {
            return value;
        }
    }

    public enum EncryptMode {
        None(1),
        AES(2);

        private final int value;

        EncryptMode(int value) {
            this.value = value;
        }

        public int value() {
            return value;
        }
    }

    public interface LibraryLoader {
        void loadLibrary(String libName);
    }

    private static final AtomicBoolean sInitialized = new AtomicBoolean();
    private static int sSingleLogMaxLength;

    @VisibleForTesting
    final long nativePtr;

    private final String protoName;
    private final String rootDirectory;
    private final boolean async;
    private final int cacheSize;
    private int expireSeconds;
    private final int totalArchiveSizeLimit;
    private final CompressMode compressMode;
    private final boolean incrementalArchive;
    private final EncryptMode encryptMode;
    private final String key;

    private Glog(Builder builder) {
        if (!sInitialized.get()) {
            throw new IllegalStateException("glog not initialized");
        }
        this.protoName = builder.protoName;
        this.rootDirectory = builder.rootDirectory;
        this.async = builder.async;
        this.expireSeconds = builder.expireSeconds;
        this.totalArchiveSizeLimit = builder.totalArchiveSizeLimit;
        this.compressMode = builder.compressMode;
        this.incrementalArchive = builder.incrementalArchive;
        this.encryptMode = builder.encryptMode;
        this.key = builder.key;
        this.nativePtr = jniMaybeCreateWithConfig(rootDirectory, protoName, incrementalArchive, async,
                expireSeconds, totalArchiveSizeLimit, compressMode.value(), encryptMode.value(), key);
        this.cacheSize = jniGetCacheSize(nativePtr);
    }

    public static void initialize(InternalLogLevel level) {
        initialize(level, null);
    }

    public static void initialize(InternalLogLevel level, LibraryLoader libraryLoader) {
        if (sInitialized.get()) {
            return;
        }

        if (libraryLoader == null) {
            System.loadLibrary("glog");
        } else {
            libraryLoader.loadLibrary("glog");
        }

        jniInitialize(level.value());
        sSingleLogMaxLength = jniGetSingleLogMaxLength();
        sInitialized.set(true);
    }

    public static long getSystemPageSize() {
        if (!sInitialized.get()) {
            throw new IllegalStateException("glog not initialized");
        }
        return jniGetSystemPageSize();
    }

    public static int getSingleLogMaxLength() {
        if (!sInitialized.get()) {
            throw new IllegalStateException("glog not initialized");
        }
        return sSingleLogMaxLength;
    }

    public boolean write(byte[] buf) {
        if (buf == null) {
            throw new NullPointerException();
        }
        return write(buf, 0, buf.length);
    }

    public boolean write(byte[] buf, int offset, int len) {
        if (buf == null) {
            throw new NullPointerException();
        } else if ((offset < 0) || (offset > buf.length) || (len < 0) ||
                ((offset + len) > buf.length) || ((offset + len) < 0)) {
            throw new IndexOutOfBoundsException();
        } else if (len == 0 || len > sSingleLogMaxLength) {
            return false;
        }
        return jniWrite(nativePtr, buf, offset, len);
    }

    public void flush() {
        jniFlush(nativePtr);
    }

    public int getCacheSize() {
        return cacheSize;
    }

    public int getExpireSeconds() {
        return expireSeconds;
    }

    public void destroy() {
        jniDestroy(protoName);
    }

    /**
     * destroy all native glog instance
     */
    public static void destroyAll() {
        if (!sInitialized.get()) {
            throw new IllegalStateException("glog not initialized");
        }
        jniDestroyAll();
    }

    public String getProtoName() {
        return protoName;
    }

    public String getCacheFileName() {
        return jniGetCacheFileName(nativePtr);
    }

    public String getRootDirectory() {
        return rootDirectory;
    }

    public boolean isAsync() {
        return async;
    }

    public int getTotalArchiveSizeLimit() {
        return totalArchiveSizeLimit;
    }

    public CompressMode getCompressMode() {
        return compressMode;
    }

    public boolean isIncrementalArchive() {
        return incrementalArchive;
    }

    public EncryptMode getEncryptMode() {
        return encryptMode;
    }

    public String getKey() {
        return key;
    }

    /**
     * reset expire seconds, clean archives if new value < old value, return true if do clean
     */
    public boolean resetExpireSeconds(int expireSeconds) {
        if (expireSeconds < 0) {
            throw new IllegalArgumentException("expire seconds should >= 0");
        }
        if (expireSeconds == this.expireSeconds) {
            return false;
        }
        return jniResetExpireSeconds(nativePtr, this.expireSeconds = expireSeconds);
    }

    public void removeAll() {
        removeAll(false, false);
    }

    /**
     * remove all archive files, if removeReadingFiles = true, remove files which is being read.
     * if reloadCache = true, remove the log content in cache file.
     */
    public void removeAll(boolean removeReadingFiles, boolean reloadCache) {
        jniRemoveAll(nativePtr, removeReadingFiles, reloadCache);
    }

    public void removeArchiveFile(String archiveFile) throws IOException {
        if (archiveFile == null || archiveFile.trim().isEmpty()) {
            throw new IOException("file path should not be empty");
        }
        jniRemoveArchiveFile(nativePtr, archiveFile);
    }

    public String getArchiveSnapshot(ArrayList<String> outFiles, int minLogNum, int totalLogSize) {
        if (outFiles == null) {
            throw new NullPointerException("outFiles == null");
        }
        return jniGetArchiveSnapshot(nativePtr, outFiles, true, minLogNum, totalLogSize, FileOrder.CreateTimeDescending.value());
    }

    /**
     * get archive files snapshot, if flush == true, either total log number in cache >= minLogNum or
     * total log size in cache >= totalLogSize will trigger flush.
     */
    public String getArchiveSnapshot(ArrayList<String> outFiles, boolean flush, int minLogNum, int totalLogSize, FileOrder order) {
        if (outFiles == null) {
            throw new NullPointerException("outFiles == null");
        }
        return jniGetArchiveSnapshot(nativePtr, outFiles, flush, minLogNum, totalLogSize, order.value());
    }

    public String[] getArchivesOfDate(long epochSeconds) {
        return jniGetArchivesOfDate(nativePtr, epochSeconds);
    }

    public Reader openReader(String archiveFile) throws IOException {
        return openReader(archiveFile, null);
    }

    public Reader openReader(String archiveFile, String key) throws IOException {
        if (archiveFile == null || archiveFile.trim().isEmpty()) {
            throw new IOException("file path should not be empty");
        }
        File file = new File(archiveFile);
        if (!file.isFile()) {
            throw new FileNotFoundException("file:" + archiveFile + " not found");
        }
        if (!file.canRead()) {
            throw new FileNotFoundException("file:" + archiveFile + " have no read permission");
        }
        return new Reader(nativePtr, archiveFile, key);
    }

    public static final class Builder {
        private String protoName;
        private String rootDirectory;
        private boolean async;
        private int expireSeconds;
        private int totalArchiveSizeLimit;
        private CompressMode compressMode;
        private boolean incrementalArchive;
        private EncryptMode encryptMode;
        private String key;

        public Builder(Context context) {
            if (context == null) {
                throw new NullPointerException();
            }

            rootDirectory = context.getFilesDir().getAbsolutePath() + "/glog";
            async = true;
            expireSeconds = DEFAULT_EXPIRES_SECS;
            totalArchiveSizeLimit = DEFAULT_TOTAL_ARCHIVE_SIZE_LIMIT;
            compressMode = CompressMode.Zlib;
            incrementalArchive = false;
            encryptMode = EncryptMode.None;
            key = null;
        }

        public Builder protoName(String protoName) {
            if (protoName == null || protoName.trim().isEmpty()) {
                throw new IllegalArgumentException("proto should not be empty");
            }
            this.protoName = protoName;
            return this;
        }

        public Builder rootDirectory(String rootDirectory) {
            if (rootDirectory == null || rootDirectory.trim().isEmpty()) {
                throw new IllegalArgumentException("root directory should not be empty");
            }
            this.rootDirectory = rootDirectory;
            return this;
        }

        public Builder async(boolean async) {
            this.async = async;
            return this;
        }

        public Builder expireSeconds(int expireSeconds) {
            if (expireSeconds < 0) {
                throw new IllegalArgumentException("expire seconds should >= 0");
            }
            this.expireSeconds = expireSeconds;
            return this;
        }

        public Builder totalArchiveSizeLimit(int totalArchiveSizeLimit) {
            if (totalArchiveSizeLimit < 0) {
                throw new IllegalArgumentException("totalArchiveSizeLimit should >= 0");
            }
            this.totalArchiveSizeLimit = totalArchiveSizeLimit;
            return this;
        }

        public Builder compressMode(CompressMode compressMode) {
            if (compressMode == null) {
                throw new NullPointerException();
            }
            this.compressMode = compressMode;
            return this;
        }

        public Builder incrementalArchive(boolean incrementalArchive) {
            this.incrementalArchive = incrementalArchive;
            return this;
        }

        public Builder encryptMode(EncryptMode encryptMode) {
            if (encryptMode == null) {
                throw new NullPointerException();
            }
            this.encryptMode = encryptMode;
            return this;
        }

        public Builder key(String key) {
            if (key == null || key.trim().isEmpty()) {
                throw new IllegalArgumentException("key should not be empty");
            }
            this.key = key;
            return this;
        }

        public Glog build() {
            if (protoName == null) {
                throw new IllegalArgumentException("should set proto name explicitly");
            }
            if (encryptMode == EncryptMode.AES && (key == null || key.trim().isEmpty())) {
                throw new IllegalArgumentException("should provide key while encrypt mode = AES");
            }
            return new Glog(this);
        }
    }

    public static class Reader implements Closeable {
        private final long nativePtr;
        private final long glPtr;
        private final String archiveFile;

        private Reader(long glPtr, String archiveFile, String key) {
            this.archiveFile = archiveFile;
            nativePtr = jniOpenReader(this.glPtr = glPtr, archiveFile, key);
        }

        public int read(byte[] buf) {
            if (buf == null) {
                throw new NullPointerException();
            }
            return read(buf, 0, buf.length);
        }

        public int read(byte[] buf, int offset, int len) {
            if (buf == null) {
                throw new NullPointerException();
            } else if ((offset < 0) || (offset > buf.length) || (len < 0) ||
                    ((offset + len) > buf.length) || ((offset + len) < 0)) {
                throw new IndexOutOfBoundsException();
            } else if (len == 0) {
                return -1;
            }
            return jniRead(nativePtr, buf, offset, len);
        }

        public int getCurrentPosition() {
            return jniGetCurrentPosition(nativePtr);
        }

        public boolean seek(int offset) {
            if (offset < 0) {
                throw new IndexOutOfBoundsException();
            } else if (offset == 0) {
                return true;
            } else {
                return jniSeek(nativePtr, offset);
            }
        }

        @Override
        public void close() {
            jniCloseReader(glPtr, nativePtr);
        }

        public String getArchiveFile() {
            return archiveFile;
        }
    }

    // jni function start
    private static native void jniInitialize(int level);

    private static native long jniGetSystemPageSize();

    private static native int jniGetSingleLogMaxLength();

    private static native long jniMaybeCreateWithConfig(String rootDirectory, String protoName,
                                                        boolean incrementalArchive,
                                                        boolean async, int expireSeconds,
                                                        int totalArchiveSizeLimit, int compressMode,
                                                        int encryptMode, String key);

    private static native boolean jniWrite(long ptr, byte[] b, int offset, int len);

    private static native void jniFlush(long ptr);

    private static native void jniDestroy(String protoName);

    private static native void jniDestroyAll();

    private static native boolean jniResetExpireSeconds(long ptr, int seconds);

    private static native int jniGetCacheSize(long ptr);

    private static native String jniGetCacheFileName(long ptr);

    private static native void jniRemoveAll(long ptr, boolean removeReadingFiles, boolean reloadCache);

    private static native void jniRemoveArchiveFile(long ptr, String archiveFile);

    private static native String jniGetArchiveSnapshot(long ptr, ArrayList<String> outFiles, boolean flush,
                                                       long minLogNum, long totalLogSize, int order);

    private static native String[] jniGetArchivesOfDate(long ptr, long epochSeconds);

    private static native long jniOpenReader(long glPtr, String archiveFile, String key);

    private static native int jniRead(long ptr, byte[] buf, int offset, int len);

    private static native int jniGetCurrentPosition(long ptr);

    private static native boolean jniSeek(long ptr, int offset);

    private static native void jniCloseReader(long glPtr, long readerPtr);

}
