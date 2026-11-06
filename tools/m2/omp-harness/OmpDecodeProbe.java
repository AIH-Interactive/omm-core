import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;

import top.aihmc.omnimorph.avatar.ysm.YsmMemoryResourceSource;
import top.aihmc.omnimorph.avatar.ysm.YsmResourceLimits;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryModel;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryReader;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmDecodeBudget;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmLegacyDecoder;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmV3Decoder;

/**
 * Differential reference: decodes encrypted .ysm files with the unmodified OMP Java decoders
 * and prints only sizes and SHA-256 digests. Decrypted content is never written anywhere.
 *
 * Output lines (tab separated):
 *   file    NAME  legacy-v<n> | v3  ...
 *   entry   NAME  PATH  BYTES  SHA256          (legacy containers)
 *   payload NAME  BYTES  SHA256  FORMAT_DWORD  (v3 decrypted+decompressed payload)
 *   binary  NAME  FORMAT_VERSION  DIGEST | ERROR message   (see BinaryDigest)
 */
public final class OmpDecodeProbe {
    private OmpDecodeProbe() {
    }

    private static String sha256(byte[] data) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data));
    }

    private static YsmDecodeBudget budget() {
        return new YsmDecodeBudget(YsmResourceLimits.DEFAULT, () -> false, 60000);
    }

    private static boolean isLegacy(byte[] bytes) {
        if (bytes.length < 8 || bytes[0] != 'Y' || bytes[1] != 'S' || bytes[2] != 'G' || bytes[3] != 'P') {
            return false;
        }
        int version = ByteBuffer.wrap(bytes, 4, 4).order(ByteOrder.BIG_ENDIAN).getInt();
        return version == 1 || version == 2;
    }

    public static void main(String[] args) throws Exception {
        for (String argument : args) {
            Path path = Path.of(argument);
            String name = path.getFileName().toString();
            byte[] bytes = Files.readAllBytes(path);
            try {
                if (isLegacy(bytes)) {
                    int version = bytes[7];
                    System.out.println("file\t" + name + "\tlegacy-v" + version);
                    try (YsmMemoryResourceSource source = YsmLegacyDecoder.decode(bytes, name, budget())) {
                        for (String entry : source.resourcePaths()) {
                            byte[] data = source.readBytes(entry);
                            System.out.println("entry\t" + name + "\t" + entry + "\t" + data.length + "\t" + sha256(data));
                        }
                    }
                } else {
                    System.out.println("file\t" + name + "\tv3");
                    byte[] payload = YsmV3Decoder.decode(bytes, budget());
                    int formatDword = payload.length >= 4
                            ? ByteBuffer.wrap(payload, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt()
                            : -1;
                    System.out.println("payload\t" + name + "\t" + payload.length + "\t" + sha256(payload) + "\t" + formatDword);
                    try (YsmBinaryModel model = YsmBinaryReader.readFilePayload(payload, budget())) {
                        System.out.println("binary\t" + name + "\t" + model.formatVersion + "\t" + BinaryDigest.of(model));
                    } catch (Exception exception) {
                        System.out.println("binary\t" + name + "\t" + formatDword + "\tERROR " + exception.getMessage());
                    }
                }
            } catch (Exception exception) {
                System.out.println("error\t" + name + "\t" + exception.getMessage());
            }
        }
    }
}
