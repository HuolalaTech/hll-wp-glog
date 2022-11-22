package glog.reader.java;


import javax.crypto.*;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.io.FileInputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.security.*;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.*;
import java.util.Arrays;

/*
 * Glog file format v4 (all length store as little endian)
 *
 * sync marker: synchronization marker, 8 random bytes
 *
 * optional:
 * (write iv and key when the log should be encrypted)
 * AES random iv: AES crypt CFB-128 random initial vector
 * client ecc public key: make shared key with server private key
 *
+-----------------------------------------------------------------+
|                         magic number (4)                        |
+----------------+-------------------------------+----------------+
|   version (1)  |     proto name length (2)     |
+----------------+-------------------------------+----------------+
|       proto name (0...)       ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+================+================================================+
|  mode set (1)  |          optional: AES random iv (16)
+----------------+------------------------------------------------+
|                              ... AES random iv                  |
+-----------------------------------------------------------------+
|       optional: client ecc public key (64)
+-----------------------------------------------------------------+
|                              ... client ecc public key          |
+-------------------------------+---------------------------------+
|        log length (2)         |
+-------------------------------+----------------------------------
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+================+================================================+
|  mode set (1)  |          optional: AES random iv (16)
+----------------+------------------------------------------------+
|                              ... AES random iv                  |
+-----------------------------------------------------------------+
|       optional: client ecc public key (64)
+-----------------------------------------------------------------+
|                              ... client ecc public key          |
+-------------------------------+---------------------------------+
|        log length (2)         |
+-------------------------------+----------------------------------
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+-----------------------------------------------------------------+
|                              ...
+-----------------------------------------------------------------+
*/
public class FileReaderV4 extends FileReader {
    private enum CompressMode {
        None(1),
        Zlib(2);

        private final int value;

        CompressMode(int value) {
            this.value = value;
        }
    }

    private enum EncryptMode {
        None(1),
        AES(2);

        private final int value;

        EncryptMode(int value) {
            this.value = value;
        }
    }

    private static final char[] HEX_TABLE = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};


    private final String svrPriKey;
    private final ECPrivateKey svrEcPriKey;

    protected FileReaderV4(FileInputStream input, String key) throws IOException {
        super(input);
        if (key == null || key.isEmpty()) {
            this.svrPriKey = null;
            this.svrEcPriKey = null;
        } else {
            this.svrPriKey = key;
            svrEcPriKey = prepareSvrPriKey(key);
        }
    }

    @Override
    protected void readRemainHeader() throws IOException {
        final int protoNameLen = readU16Le();
        byte[] name = new byte[protoNameLen];
        readSafely(input, protoNameLen, name);

        final String protoName = new String(name);
        logger.info("Proto name:" + protoName);

        byte[] syncMarker = new byte[8];
        readSafely(input, 8, syncMarker);

        if (!Arrays.equals(syncMarker, GlogReader.SYNC_MARKER)) {
            throw new FileCorruptException("Sync marker mismatch");
        }
        position += 4 + 1 + 2 + protoNameLen + 8;
    }

    @Override
    public int read(byte[] outBuf, int outBufLen) throws IOException {
        if (input.available() < 2 + 1 + 8) {
            return -1;
        }
        final int ms = input.read();
        CompressMode compressMode;
        EncryptMode encryptMode;
        switch (ms >> 4) {
            case 1:
                compressMode = CompressMode.None;
                break;
            case 2:
                compressMode = CompressMode.Zlib;
                break;
            default:
                logger.warning("Illegal compress mode:" + (ms >> 4)); // todo recover
                return -2;
        }

        switch (ms & 0x0F) {
            case 1:
                encryptMode = EncryptMode.None;
                break;
            case 2:
                encryptMode = EncryptMode.AES;
                break;
            default:
                logger.warning("Illegal encrypt mode:" + (ms & 0x0F)); // todo recover
                return -3;
        }
        logger.info("compress mode:" + compressMode + ", encrypt mode:" + encryptMode);

        if (encryptMode == EncryptMode.AES && (svrPriKey == null || svrPriKey.isEmpty())) {
            throw new RuntimeException("cipher not ready");
        }

        position += 1;

        int logLength = 0;
        if (encryptMode == EncryptMode.AES) {
            byte[] iv = new byte[16];
            readSafely(input, 16, iv);

            byte[] clientPubKey = new byte[64];
            readSafely(input, 64, clientPubKey);

            position += 16 + 64;

            logLength = readU16Le();
            logger.info("log length:" + logLength);

            if (logLength <= 0 || logLength > GlogReader.SINGLE_LOG_CONTENT_MAX_LENGTH) {
                return -4; // todo recover
            }

            position += 2 + logLength;

            byte[] buf = new byte[logLength];
            readSafely(input, logLength, buf);

            byte[] plain = decrypt(clientPubKey, iv, buf);
            if (plain == null || plain.length == 0) {
                return -5; // todo recover
            }

            if (compressMode == CompressMode.Zlib) {
                logLength = decompress(plain, logLength, outBuf, outBufLen); // todo recover
            } else {
                logLength = plain.length;
                System.arraycopy(plain, 0, outBuf, 0, logLength);
            }
        } else {
            logLength = readU16Le();
            logger.info("log length:" + logLength);

            if (logLength <= 0 || logLength > GlogReader.SINGLE_LOG_CONTENT_MAX_LENGTH) {
                return -6; // todo recover
            }

            position += 2 + logLength;

            byte[] buf = new byte[logLength];
            readSafely(input, logLength, buf);

            if (compressMode == CompressMode.Zlib) {
                logLength = decompress(buf, logLength, outBuf, outBufLen); // todo recover
            } else {
                System.arraycopy(buf, 0, outBuf, 0, logLength);
            }
        }

        byte[] syncMarker = new byte[8];
        readSafely(input, 8, syncMarker);

        if (!Arrays.equals(syncMarker, GlogReader.SYNC_MARKER)) {
            return -7; // todo recover
        }
        position += 8;

        return logLength;
    }

    private byte[] decrypt(byte[] clientPubKey, byte[] iv, byte[] encrypt) {
        byte[] aesUsrKey = getSharedKey(clientPubKey);
        aesUsrKey = Arrays.copyOfRange(aesUsrKey, 0, 16); // AES_KEY_BITSET_LEN = 128

        try {
            IvParameterSpec ivSpec = new IvParameterSpec(iv);
            SecretKeySpec keySpec = new SecretKeySpec(aesUsrKey, "AES");
            Cipher cipher = Cipher.getInstance("AES/CFB/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, keySpec, ivSpec);
            return cipher.doFinal(encrypt);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | InvalidAlgorithmParameterException
                | InvalidKeyException | IllegalBlockSizeException | BadPaddingException e) {
            e.printStackTrace();
        }
        return null;
    }

    private byte[] getSharedKey(byte[] clientPubKey) {
        ECPublicKey clientEcPubKey = prepareClientPubKey(clientPubKey);
        try {
            KeyAgreement keyAgreement = KeyAgreement.getInstance("ECDH");
            keyAgreement.init(svrEcPriKey);
            keyAgreement.doPhase(clientEcPubKey, true);
            return keyAgreement.generateSecret();
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            e.printStackTrace();
        }
        return null;
    }

    private ECPrivateKey prepareSvrPriKey(String svrPriKey) {
        try {
            AlgorithmParameters parameters = AlgorithmParameters.getInstance("EC");
            parameters.init(new ECGenParameterSpec("secp256k1"));
            ECParameterSpec ecParameterSpec = parameters.getParameterSpec(ECParameterSpec.class);

            byte[] ecPrivateKeyBytes = string2Hex(svrPriKey);
            ECPrivateKeySpec ecPrivateKeySpec = new ECPrivateKeySpec(new BigInteger(ecPrivateKeyBytes), ecParameterSpec);

            return (ECPrivateKey) KeyFactory.getInstance("EC").generatePrivate(ecPrivateKeySpec);
        } catch (NoSuchAlgorithmException | InvalidParameterSpecException | InvalidKeySpecException e) {
            e.printStackTrace();
        }
        return null;
    }

    private ECPublicKey prepareClientPubKey(byte[] clientPubKey) {
        byte[] x = Arrays.copyOfRange(clientPubKey, 0, clientPubKey.length / 2);
        byte[] y = Arrays.copyOfRange(clientPubKey, clientPubKey.length / 2, clientPubKey.length);
        ECPoint ecPoint = new ECPoint(new BigInteger(x), new BigInteger(y));

        try {
            AlgorithmParameters parameters = AlgorithmParameters.getInstance("EC");
            parameters.init(new ECGenParameterSpec("secp256k1"));
            ECParameterSpec ecParameterSpec = parameters.getParameterSpec(ECParameterSpec.class);

            ECPublicKeySpec publicKeySpec = new ECPublicKeySpec(ecPoint, ecParameterSpec);
            return (ECPublicKey) KeyFactory.getInstance("EC").generatePublic(publicKeySpec);
        } catch (NoSuchAlgorithmException | InvalidParameterSpecException | InvalidKeySpecException e) {
            e.printStackTrace();
        }
        return null;
    }

    private int logStoreSize(int len, boolean cipher) {
        return 1 + 2 + len + 8 + (cipher ? 16 + 64 : 0);
    }

    private byte[] string2Hex(String s) {
        final int len = s.length();
        if (len == 0 || len % 2 != 0) {
            return null;
        }
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((char2int(s.charAt(i)) << 4) | char2int(s.charAt(i + 1)));
        }
        return data;
    }

    private String hex2Str(byte[] data) {
        final int len = data.length;
        char[] chs = new char[len * 2];

        for (int i = 0; i < len; ++i) {
            chs[2 * i] = HEX_TABLE[(data[i] & 0xF0) >> 4];
            chs[2 * i + 1] = HEX_TABLE[data[i] & 0x0F];
        }
        return new String(chs);
    }

    private int char2int(char input) {
        if (input >= '0' && input <= '9')
            return input - '0';
        if (input >= 'A' && input <= 'F')
            return input - 'A' + 10;
        if (input >= 'a' && input <= 'f')
            return input - 'a' + 10;
        throw new IllegalArgumentException("Invalid input string");
    }
}
