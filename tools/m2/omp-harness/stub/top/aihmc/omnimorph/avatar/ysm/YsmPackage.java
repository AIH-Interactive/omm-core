package top.aihmc.omnimorph.avatar.ysm;

import java.util.ArrayList;
import java.util.List;

/** Harness stub: only the path normalization used by the legacy decoder (copied from OMP YsmPackage). */
public final class YsmPackage {
    private YsmPackage() {
    }

    /** Directory source indexed in Files.walk order, as OMP YsmPackage.indexDirectory does. */
    public static YsmResourceSource open(java.nio.file.Path root) throws java.io.IOException {
        java.util.Map<String, byte[]> files = new java.util.LinkedHashMap<>();
        try (java.util.stream.Stream<java.nio.file.Path> paths = java.nio.file.Files.walk(root)) {
            for (java.nio.file.Path file : (Iterable<java.nio.file.Path>) paths.filter(java.nio.file.Files::isRegularFile)::iterator) {
                files.put(normalize(root.relativize(file).toString()), java.nio.file.Files.readAllBytes(file));
            }
        }
        return new YsmMemoryResourceSource(root.getFileName().toString(), files);
    }

    public static String normalize(String relativePath) {
        String normalized = normalizeRelative(relativePath);
        return normalized == null ? "" : normalized;
    }

    private static String normalizeRelative(String relativePath) {
        if (relativePath == null)
            return null;
        String path = relativePath.replace('\\', '/');
        if (path.isBlank() || path.startsWith("/") || path.startsWith("//") || path.matches("^[A-Za-z]:/.*$"))
            return path.isBlank() ? "" : null;
        List<String> segments = new ArrayList<>();
        for (String segment : path.split("/+")) {
            if (segment.isBlank() || segment.equals("."))
                continue;
            if (segment.equals("..") || segment.contains(":") || segment.indexOf('\0') >= 0)
                return null;
            segments.add(segment);
        }
        return String.join("/", segments);
    }
}
