package glog.reader.java;

import java.io.EOFException;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

import static glog.reader.java.GlogReader.SINGLE_LOG_CONTENT_MAX_LENGTH;
import static glog.reader.java.GlogReader.SYNC_MARKER;

/*
 * Glog file format v3 (all length store as little endian)
 *
 * sync marker: synchronization marker, 8 random bytes
 *
+-----------------------------------------------------------------+
|                         magic number (4)                        |
+----------------+----------------+-------------------------------+
|   version (1)  |  mode set (1)  |     proto name length (2)     |
+----------------+----------------+-------------------------------+
|       proto name (0...)       ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+=================================+===============================+
|         log length (2)          |
+---------------------------------+-------------------------------+
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+-----------------------------------------------------------------+
|         log length (2)         |
+--------------------------------+--------------------------------+
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+------------------------------------------------------------------
|                              ...
+-----------------------------------------------------------------+
*/
public class FileReaderV3 extends FileReader {
    private enum CompressMode {
        None(0),
        Zlib(1);

        private final int value;

        CompressMode(int value) {
            this.value = value;
        }
    }

    private enum EncryptMode {
        None(0),
        AES(1);

        private final int value;

        EncryptMode(int value) {
            this.value = value;
        }
    }

    private CompressMode compressMode;
    private EncryptMode encryptMode;

    protected FileReaderV3(FileInputStream input) throws IOException {
        super(input);
    }

    @Override
    protected void readRemainHeader() throws IOException {
        final int ms = input.read();
        switch (ms >> 4) {
            case 0:
                compressMode = CompressMode.None;
                break;
            case 1:
                compressMode = CompressMode.Zlib;
                break;
            default:
                throw new FileCorruptException("Illegal compress mode");
        }

        switch (ms & 0x0F) {
            case 0:
                encryptMode = EncryptMode.None;
                break;
            case 1:
                encryptMode = EncryptMode.AES;
                break;
            default:
                throw new FileCorruptException("Illegal encrypt mode");
        }
        logger.info("compress mode:" + compressMode + ", encrypt mode:" + encryptMode);

        final int protoNameLen = readU16Le();

        logger.info("Proto name len:" + protoNameLen);

        if (input.available() < protoNameLen + 8) {
            throw new EOFException("Too small header");
        }

        byte[] name = new byte[protoNameLen];
        readSafely(input, protoNameLen, name);

        final String protoName = new String(name);
        logger.info("Proto name:" + protoName);

        byte[] syncMarker = new byte[8];
        readSafely(input, 8, syncMarker);

        if (!Arrays.equals(syncMarker, SYNC_MARKER)) {
            throw new FileCorruptException("Sync marker mismatch");
        }

        position += 4 + 1 + 1 + 2 + 8 + protoNameLen;
        logger.info("Read header done, position:" + position);
    }

    @Override
    public int read(byte[] outBuf, int outBufLen) throws IOException {
        if (input.available() < logStoreSize(1)) {
            return -1;
        }

        final int logLength = readU16Le();

        position += 2;

        if (input.available() < logLength + 8) {
            throw new EOFException("Not enough space for length:" + logLength + " at position:" + position);
        }

        if (logLength <= 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
            return -2; // todo recover
        }

        logger.info("logLength:" + logLength);

        byte[] buf = new byte[logLength];
        readSafely(input, logLength, buf);

        position += logLength;

        // todo handle not compressed log
        int uncompressedLength = decompress(buf, logLength, outBuf, outBufLen);

        byte[] syncMarker = new byte[8];
        readSafely(input, 8, syncMarker);

        if (!Arrays.equals(syncMarker, SYNC_MARKER)) {
            return -3; // todo recover
        }
        position += 8;

        return uncompressedLength;
    }

    private int logStoreSize(int len) {
        return 2 + len + 8;
    }
}
