package glog.reader.java;

import java.io.*;
import java.util.Arrays;
import java.util.logging.Logger;

public class GlogReader implements Closeable {
    public final static byte[] MAGIC_NUMBER = {0x1B, (byte) 0xAD, (byte) 0xC0, (byte) 0xDE};
    public final static byte[] SYNC_MARKER = {(byte) 0xB7, (byte) 0xDB, (byte) 0xE7, (byte) 0xDB, (byte) 0x80, (byte) 0xAD, (byte) 0xD9, 0x57};
    public final static int SINGLE_LOG_CONTENT_MAX_LENGTH = 16 * 1024;

    public static final Logger logger = Logger.getLogger(GlogReader.class.getSimpleName());
    private final String key;
    private FileReader fileReader;

    public GlogReader(String filePath) throws IOException {
        this(filePath, null);
    }

    public GlogReader(String filePath, String key) throws IOException {
        this.key = key;
        File logFile = new File(filePath);
        readerHeader(new FileInputStream(logFile));
    }

    private void readerHeader(FileInputStream input) throws IOException {
        byte[] magic = new byte[4];
        FileReader.readSafely(input, 4, magic);

        if (!Arrays.equals(magic, MAGIC_NUMBER)) {
            throw new FileCorruptException("Magic number mismatch");
        }

        switch (input.read()) {
            case GlogVersion.GlogRecoveryVersion:
                fileReader = new FileReaderV3(input);
                break;
            case GlogVersion.GlogCipherVersion:
                fileReader = new FileReaderV4(input, key);
                break;
            default:
                throw new FileCorruptException("Version code mismatch");
        }
        fileReader.readRemainHeader();
    }

    public int read(byte[] outBuf, int outBufLen) throws IOException {
        return fileReader.read(outBuf, outBufLen);
    }

    @Override
    public void close() throws IOException {
        fileReader.close();
    }
}
