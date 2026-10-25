package com.decompilex.app;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class IsoReader {
    private static final int SECTOR = 2048;
    private static final int MAX_ENTRIES = 10000;
    private static final int MAX_DEPTH = 32;
    private static final long MAX_EXTRACT = 1024L * 1024 * 1024;

    public static final class Entry {
        public final String path;
        public final long extent;
        public final long size;
        public final boolean directory;
        Entry(String path, long extent, long size, boolean directory) {
            this.path = path;
            this.extent = extent;
            this.size = size;
            this.directory = directory;
        }
    }

    private final File file;
    private final List<Entry> entries = new ArrayList<>();
    private String volume;
    private long totalBytes;

    public IsoReader(File file) throws IOException {
        this.file = file;
        try (RandomAccessFile source = new RandomAccessFile(file, "r")) {
            if (source.length() < 17L * SECTOR) throw new IOException("ISO image is too small.");
            byte[] descriptor = new byte[SECTOR];
            source.seek(16L * SECTOR);
            source.readFully(descriptor);
            if ((descriptor[0] & 255) != 1 || !ascii(descriptor, 1, 5).equals("CD001") || (descriptor[6] & 255) != 1)
                throw new IOException("Not a supported ISO 9660 primary volume.");
            volume = ascii(descriptor, 40, 32).trim();
            long rootExtent = u32(descriptor, 156 + 2);
            long rootSize = u32(descriptor, 156 + 10);
            if ((descriptor[156] & 255) < 34) throw new IOException("Invalid ISO root record.");
            Set<String> visited = new HashSet<>();
            walk(source, "", rootExtent, rootSize, 0, visited);
        }
    }

    private static String ascii(byte[] b, int offset, int length) {
        return new String(b, offset, length, StandardCharsets.US_ASCII);
    }

    private static long u32(byte[] b, int offset) {
        return ((long)b[offset] & 255) | (((long)b[offset + 1] & 255) << 8)
            | (((long)b[offset + 2] & 255) << 16) | (((long)b[offset + 3] & 255) << 24);
    }

    private static void checkRange(long offset, long length, long fileLength) throws IOException {
        if (offset < 0 || length < 0 || offset > fileLength || length > fileLength - offset)
            throw new IOException("Invalid ISO extent or truncated image.");
    }

    private void walk(RandomAccessFile source, String prefix, long extent, long size,
                      int depth, Set<String> visited) throws IOException {
        if (depth > MAX_DEPTH) throw new IOException("ISO directory depth limit exceeded.");
        String key = extent + ":" + size;
        if (!visited.add(key)) throw new IOException("ISO directory cycle detected.");
        long start = extent * SECTOR;
        checkRange(start, size, source.length());
        long position = 0;
        Set<String> names = new HashSet<>();
        while (position < size) {
            source.seek(start + position);
            int length = source.readUnsignedByte();
            if (length == 0) {
                position = ((position / SECTOR) + 1) * SECTOR;
                continue;
            }
            if (length < 34 || length > SECTOR - (position % SECTOR) || length > size - position)
                throw new IOException("Invalid ISO directory record.");
            byte[] record = new byte[length];
            source.seek(start + position);
            source.readFully(record);
            position += length;
            int nameLength = record[32] & 255;
            if (33 + nameLength > length) throw new IOException("Invalid ISO filename.");
            if (nameLength == 1 && ((record[33] & 255) == 0 || (record[33] & 255) == 1)) continue;
            String name = ascii(record, 33, nameLength);
            int version = name.indexOf(';');
            if (version >= 0) name = name.substring(0, version);
            if (name.endsWith(".")) name = name.substring(0, name.length() - 1);
            if (name.isEmpty() || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\") || name.indexOf('\0') >= 0)
                throw new IOException("Unsafe ISO filename.");
            if (!names.add(name)) throw new IOException("Duplicate ISO entry: " + name);
            boolean directory = (record[25] & 2) != 0;
            long childExtent = u32(record, 2);
            long childSize = u32(record, 10);
            checkRange(childExtent * SECTOR, childSize, source.length());
            String path = prefix.isEmpty() ? name : prefix + "/" + name;
            if (entries.size() >= MAX_ENTRIES) throw new IOException("ISO entry limit exceeded.");
            entries.add(new Entry(path, childExtent, childSize, directory));
            if (!directory) {
                if (childSize > MAX_EXTRACT - totalBytes) throw new IOException("ISO extraction size limit exceeded.");
                totalBytes += childSize;
            } else {
                walk(source, path, childExtent, childSize, depth + 1, visited);
            }
        }
    }

    public String report() {
        StringBuilder result = new StringBuilder("ISO 9660 IMAGE\nVolume: ").append(volume)
            .append("\nEntries: ").append(entries.size()).append("\nExtractable bytes: ")
            .append(totalBytes).append("\n\n");
        for (Entry entry : entries) {
            result.append(entry.directory ? "[DIR] " : "[FILE] ").append(entry.path);
            if (!entry.directory) result.append(" (").append(entry.size).append(" bytes)");
            result.append('\n');
            if (result.length() > 100000) { result.append("... listing truncated"); break; }
        }
        return result.toString();
    }

    public int extract(ContentResolver resolver, Uri destinationTree) throws Exception {
        String rootId = DocumentsContract.getTreeDocumentId(destinationTree);
        Uri root = DocumentsContract.buildDocumentUriUsingTree(destinationTree, rootId);
        java.util.Map<String, Uri> folders = new java.util.HashMap<>();
        folders.put("", root);
        int count = 0;
        try (RandomAccessFile source = new RandomAccessFile(file, "r")) {
            for (Entry entry : entries) {
                if (Thread.currentThread().isInterrupted()) throw new IOException("Extraction interrupted.");
                int slash = entry.path.lastIndexOf('/');
                String parent = slash < 0 ? "" : entry.path.substring(0, slash);
                String name = slash < 0 ? entry.path : entry.path.substring(slash + 1);
                Uri parentUri = folders.get(parent);
                if (parentUri == null) throw new IOException("Missing parent folder.");
                Uri created = DocumentsContract.createDocument(resolver, parentUri,
                    entry.directory ? DocumentsContract.Document.MIME_TYPE_DIR : "application/octet-stream", name);
                if (created == null) throw new IOException("Unable to create: " + entry.path);
                if (entry.directory) {
                    folders.put(entry.path, created);
                } else {
                    source.seek(entry.extent * SECTOR);
                    try (OutputStream target = resolver.openOutputStream(created, "w")) {
                        if (target == null) throw new IOException("Unable to write: " + entry.path);
                        byte[] buffer = new byte[65536];
                        long remaining = entry.size;
                        while (remaining > 0) {
                            if (Thread.currentThread().isInterrupted()) throw new IOException("Extraction interrupted.");
                            int n = (int)Math.min(buffer.length, remaining);
                            source.readFully(buffer, 0, n);
                            target.write(buffer, 0, n);
                            remaining -= n;
                        }
                    }
                    count++;
                }
            }
        }
        return count;
    }
}
