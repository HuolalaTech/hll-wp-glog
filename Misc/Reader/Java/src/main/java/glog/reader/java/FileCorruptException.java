package glog.reader.java;

import java.io.IOException;

public class FileCorruptException extends IOException {
    public FileCorruptException(String message) {
        super(message);
    }
}