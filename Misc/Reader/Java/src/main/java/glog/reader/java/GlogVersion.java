package glog.reader.java;

public interface GlogVersion {
    /**
     * @deprecated initial version
     */
    int GlogInitialVersion = 0x1;

    /**
     * @deprecated fix incorrect initial write position
     */
    int GlogFixPositionVersion = 0x2;

    /**
     * add sync marker to each log, support recover from broken file
     */
    int GlogRecoveryVersion = 0x3;

    /**
     * store iv and public key in each log to support AES CFB-128 encrypt
     */
    int GlogCipherVersion = 0x4;
}
