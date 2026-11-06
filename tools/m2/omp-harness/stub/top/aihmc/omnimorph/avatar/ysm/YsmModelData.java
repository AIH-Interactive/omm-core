package top.aihmc.omnimorph.avatar.ysm;

import java.io.IOException;
import java.nio.file.Path;
import java.util.function.BooleanSupplier;

/** Harness stub: never reached (YsmEncryptedSource.hasMagic is always false here). */
public final class YsmModelData implements AutoCloseable {
    private YsmModelData() {
    }

    public static YsmModelData read(Path path, BooleanSupplier cancelled) throws IOException {
        throw new IOException("harness stub");
    }

    public YsmManifest manifest() {
        throw new UnsupportedOperationException("harness stub");
    }

    @Override
    public void close() {
    }
}
