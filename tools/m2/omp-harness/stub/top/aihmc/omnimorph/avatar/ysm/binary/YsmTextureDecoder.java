package top.aihmc.omnimorph.avatar.ysm.binary;

import top.aihmc.omnimorph.avatar.ysm.crypto.YsmDecodeBudget;

/** Harness stub: keeps encoded bytes (the native mapper does not transcode to PNG either). */
public final class YsmTextureDecoder {
    private YsmTextureDecoder() {
    }

    public static byte[] toPng(byte[] data, int format, int width, int height, YsmDecodeBudget budget) {
        return data.clone();
    }
}
