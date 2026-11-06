package org.figuramc.figura.utils;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/** Harness stub: the two IOUtils helpers the YSM manifest code uses. */
public final class IOUtils {
    private IOUtils() {
    }

    public static String getFileNameOrEmpty(Path path) {
        Path name = path == null ? null : path.getFileName();
        return name == null ? "" : name.toString();
    }

    public static String readFile(Path path) throws IOException {
        return new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
    }
}
