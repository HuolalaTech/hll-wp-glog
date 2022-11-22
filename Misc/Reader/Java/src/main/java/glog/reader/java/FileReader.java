package glog.reader.java;

import com.jcraft.jzlib.Inflater;
import com.jcraft.jzlib.JZlib;

import java.io.*;
import java.util.logging.Logger;

import static com.jcraft.jzlib.JZlib.*;

public abstract class FileReader implements Closeable {
    protected final Logger logger = Logger.getLogger(this.getClass().getSimpleName());

    protected FileInputStream input;
    protected final Inflater inflater = new Inflater(MAX_WBITS, JZlib.WrapperType.NONE);
    protected long position;
    protected final long size;

    protected FileReader(FileInputStream input) throws IOException {
        this.input = input;
        size = input.available();
    }

    protected abstract void readRemainHeader() throws IOException;

    public abstract int read(byte[] outBuf, int outBufLen) throws IOException;

    @Override
    public void close() throws IOException {
        input.close();
    }

    protected int decompress(byte[] inBuf, int inBufLen, byte[] outBuf, int outBufLen) throws IOException {
        inflater.avail_in = inBufLen;
        inflater.next_in = inBuf;
        inflater.next_in_index = 0;

        inflater.avail_out = outBufLen;
        inflater.next_out = outBuf;
        inflater.next_out_index = 0;

        int ret = inflater.inflate(Z_SYNC_FLUSH);

        if (ret != Z_OK) {
            throw new FileCorruptException("inflate ret:" + ret + ", msg:" + inflater.msg);
        }
        return outBufLen - inflater.avail_out;
    }

    public static int readSafely(InputStream input, int expected, byte[] filled) throws IOException {
        if (input.available() < expected) {
            throw new EOFException("Expect:" + expected + " bytes, but available:" + input.available() + " bytes");
        }
        int n = input.read(filled, 0, expected);
        if (n != expected) {
            throw new EOFException("Expect:" + expected + " bytes, but read:" + n + " bytes");
        }
        return n;
    }

    protected long spaceLeft() {
        if (size <= position) {
            return 0;
        }
        return size - position;
    }

    protected int readU16Le() throws IOException {
        if (input.available() < 2) {
            throw new EOFException();
        }
        return input.read() | (input.read() << 8);
    }
}
