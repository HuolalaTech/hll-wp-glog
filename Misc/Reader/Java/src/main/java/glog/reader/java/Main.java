package glog.reader.java;


import glog.android.sample.Log;

import java.io.IOException;
import java.util.Arrays;

public class Main {
    private static final String SVR_PUB_KEY = "41B5F5F9A53684A1C09B931B7BDF7D7C3959BC7FB31827ADBE1524DDC8F2D90AD4978891385D956CE817B293FC57CF07A4EC3DAF03F63852D75A32A956B84176";
    private static final String SVR_PRIV_KEY = "9C8B23A406216B7B93AA94C66AA5451CCE41DD57A8D5ADBCE8D9F1E7F3D33F45";

    public static void main(String[] args) {
        String filePath = "your-glog-file-path";
        try {
            readLogs(filePath);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void readLogs(String filePath) throws IOException {
        try (GlogReader reader = new GlogReader(filePath, SVR_PRIV_KEY)) {
            int logNum = 0;
            final int bufLen = GlogReader.SINGLE_LOG_CONTENT_MAX_LENGTH;
            byte[] buf = new byte[bufLen];

            while (true) {
                int len = reader.read(buf, bufLen);

                if (len <= 0) { // todo recovery
                    GlogReader.logger.info("read len=" + len);
                    break;
                }

                byte[] content = Arrays.copyOfRange(buf, 0, len);

                GlogReader.logger.info("content:" + new String(content));
                logNum++;

                Log log = Log.ADAPTER.decode(content);
                GlogReader.logger.info("log=" + log);
            }

            GlogReader.logger.info("total log num:" + logNum);
        }
    }
}
