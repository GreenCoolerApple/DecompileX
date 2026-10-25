package com.decompilex.app;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import java.io.Closeable;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class IsoReader implements Closeable {
    private static final int SECTOR = 2048;
    private static final long MAX_IMAGE = 50L * 1024 * 1024 * 1024;
    private static final long MAX_EXTRACT = 50L * 1024 * 1024 * 1024;
    private static final int MAX_ENTRIES = 100000;
    private static final int MAX_DEPTH = 64;
    private static final int BUFFER_SIZE = 1024 * 1024;

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

    private final ParcelFileDescriptor descriptor;
    private final FileInputStream stream;
    private final FileChannel channel;
    private final long imageSize;
    private final List<Entry> entries = new ArrayList<>();
    private String volume;
    private long totalBytes;
    private int fileCount;
    private int directoryCount;

    public IsoReader(ContentResolver resolver, Uri uri) throws IOException {
        descriptor = resolver.openFileDescriptor(uri, "r");
        if (descriptor == null) throw new IOException("Cannot open ISO document.");
        stream = new FileInputStream(descriptor.getFileDescriptor());
        channel = stream.getChannel();
        try {
            imageSize = channel.size();
            if (imageSize < 17L * SECTOR) throw new IOException("ISO image is too small.");
            if (imageSize > MAX_IMAGE) throw new IOException("ISO exceeds the 50 GiB image limit.");
            byte[] primary = readAt(16L * SECTOR, SECTOR);
            if ((primary[0] & 255) != 1 || !ascii(primary, 1, 5).equals("CD001") || (primary[6] & 255) != 1)
                throw new IOException("Unsupported ISO: expected ISO 9660 primary volume.");
            if ((primary[156] & 255) < 34) throw new IOException("Invalid ISO root record.");
            volume = ascii(primary, 40, 32).trim();
            walk("", u32(primary, 158), u32(primary, 166), 0, new HashSet<String>());
        } catch (IOException | RuntimeException error) {
            try { close(); } catch (IOException ignored) { }
            throw error;
        }
    }

    private static String ascii(byte[] data, int offset, int length) {
        return new String(data, offset, length, StandardCharsets.US_ASCII);
    }

    private static long u32(byte[] data, int offset) {
        return ((long)data[offset] & 255L) | (((long)data[offset + 1] & 255L) << 8)
            | (((long)data[offset + 2] & 255L) << 16) | (((long)data[offset + 3] & 255L) << 24);
    }

    private void checkRange(long offset, long length) throws IOException {
        if (offset < 0 || length < 0 || offset > imageSize || length > imageSize - offset)
            throw new IOException("ISO contains an invalid or truncated extent.");
    }

    private byte[] readAt(long offset, int length) throws IOException {
        checkRange(offset, length);
        ByteBuffer buffer = ByteBuffer.allocate(length);
        long position = offset;
        while (buffer.hasRemaining()) {
            if (Thread.currentThread().isInterrupted()) throw new IOException("Operation interrupted.");
            int n = channel.read(buffer, position);
            if (n <= 0) throw new IOException("ISO source is not seekable or is truncated. Use a local document provider.");
            position += n;
        }
        return buffer.array();
    }

    private void walk(String prefix, long extent, long size, int depth, Set<String> visited) throws IOException {
        if (depth > MAX_DEPTH) throw new IOException("ISO directory nesting limit exceeded.");
        long start = extent * SECTOR;
        checkRange(start, size);
        String key = extent + ":" + size;
        if (!visited.add(key)) throw new IOException("Repeated ISO directory extent detected.");
        Set<String> names = new HashSet<>();
        long position = 0;
        while (position < size) {
            byte[] lengthByte = readAt(start + position, 1);
            int length = lengthByte[0] & 255;
            if (length == 0) {
                position = ((position / SECTOR) + 1) * SECTOR;
                continue;
            }
            if (length < 34 || length > SECTOR - (position % SECTOR) || length > size - position)
                throw new IOException("Invalid ISO directory record.");
            byte[] record = readAt(start + position, length);
            position += length;
            int nameLength = record[32] & 255;
            if (33 + nameLength > length) throw new IOException("Invalid ISO filename length.");
            if (nameLength == 1 && ((record[33] & 255) == 0 || (record[33] & 255) == 1)) continue;
            if ((record[25] & 0x80) != 0) throw new IOException("Multi-extent ISO files are not supported.");
            String name = ascii(record, 33, nameLength);
            int version = name.indexOf(';');
            if (version >= 0) name = name.substring(0, version);
            if (name.endsWith(".")) name = name.substring(0, name.length() - 1);
            if (name.isEmpty() || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\") || name.indexOf('\0') >= 0 || name.indexOf(':') >= 0 || name.chars().anyMatch(c -> c < 32 || c == 127))
                throw new IOException("Unsafe ISO filename.");
            if (!names.add(name)) throw new IOException("Duplicate ISO entry: " + name);
            boolean directory = (record[25] & 2) != 0;
            long childExtent = u32(record, 2);
            long childSize = u32(record, 10);
            checkRange(childExtent * SECTOR, childSize);
            if (entries.size() >= MAX_ENTRIES) throw new IOException("ISO entry count limit exceeded.");
            String path = prefix.isEmpty() ? name : prefix + "/" + name;
            entries.add(new Entry(path, childExtent, childSize, directory));
            if (directory) {
                directoryCount++;
                walk(path, childExtent, childSize, depth + 1, visited);
            } else {
                if (childSize > MAX_EXTRACT - totalBytes) throw new IOException("ISO exceeds the 50 GiB extraction limit.");
                totalBytes += childSize;
                fileCount++;
            }
        }
    }

    public String report() {
        StringBuilder result = new StringBuilder("ISO 9660 IMAGE\nVolume: ").append(volume)
            .append("\nImage bytes: ").append(imageSize)
            .append("\nFiles: ").append(fileCount)
            .append("\nDirectories: ").append(directoryCount)
            .append("\nExtractable bytes: ").append(totalBytes).append("\n\n");
        for (Entry entry : entries) {
            result.append(entry.directory ? "[DIR] " : "[FILE] ").append(entry.path);
            if (!entry.directory) result.append(" (").append(entry.size).append(" bytes)");
            result.append('\n');
            if (result.length() > 100000) {
                result.append("... listing truncated; extraction still includes all validated entries.");
                break;
            }
        }
        return result.toString();
    }

    public int extract(ContentResolver resolver, Uri destinationTree) throws IOException {
        String rootId = DocumentsContract.getTreeDocumentId(destinationTree);
        Uri root = DocumentsContract.buildDocumentUriUsingTree(destinationTree, rootId);
        Map<String, Uri> folders = new HashMap<>();
        folders.put("", root);
        int count = 0;
        byte[] bytes = new byte[BUFFER_SIZE];
        for (Entry entry : entries) {
            if (Thread.currentThread().isInterrupted()) throw new IOException("Extraction interrupted.");
            int slash = entry.path.lastIndexOf('/');
            String parent = slash < 0 ? "" : entry.path.substring(0, slash);
            String name = slash < 0 ? entry.path : entry.path.substring(slash + 1);
            Uri parentUri = folders.get(parent);
            if (parentUri == null) throw new IOException("Missing parent directory: " + parent);
            Uri created = DocumentsContract.createDocument(resolver, parentUri,
                entry.directory ? DocumentsContract.Document.MIME_TYPE_DIR : "application/octet-stream", name);
            if (created == null) throw new IOException("Unable to create: " + entry.path);
            if (entry.directory) {
                folders.put(entry.path, created);
                continue;
            }
            try (OutputStream target = resolver.openOutputStream(created, "w")) {
                if (target == null) throw new IOException("Unable to write: " + entry.path);
                long remaining = entry.size;
                long offset = entry.extent * SECTOR;
                while (remaining > 0) {
                    int n = (int)Math.min(bytes.length, remaining);
                    ByteBuffer buffer = ByteBuffer.wrap(bytes, 0, n);
                    while (buffer.hasRemaining()) {
                        if (Thread.currentThread().isInterrupted()) throw new IOException("Extraction interrupted.");
                        int read = channel.read(buffer, offset);
                        if (read <= 0) throw new IOException("Unable to read ISO extent: " + entry.path);
                        offset += read;
                    }
                    target.write(bytes, 0, n);
                    remaining -= n;
                }
            }
            count++;
        }
        return count;
    }

    @Override
    public void close() throws IOException {
        try { stream.close(); } finally { descriptor.close(); }
    }
}
