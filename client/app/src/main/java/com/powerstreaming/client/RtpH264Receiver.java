package com.powerstreaming.client;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.view.Surface;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.util.Arrays;

final class RtpH264Receiver extends Thread {
    private final Surface surface; private final String bindAddress; private final int port;
    private volatile boolean running = true; private DatagramSocket socket; private MediaCodec codec;
    private byte[] sps, pps; private byte[] accessUnit = new byte[0]; private long pts;

    RtpH264Receiver(Surface surface, String address, int port) { this.surface = surface; bindAddress = address; this.port = port; }
    void shutdown() { running = false; if (socket != null) socket.close(); interrupt(); }

    @Override public void run() {
        try {
            socket = new DatagramSocket(port); socket.setReceiveBufferSize(2 * 1024 * 1024);
            byte[] receive = new byte[65535];
            while (running) { DatagramPacket packet = new DatagramPacket(receive, receive.length); socket.receive(packet); handle(packet.getData(), packet.getLength()); }
        } catch (IOException ignored) { }
        finally { if (codec != null) { codec.stop(); codec.release(); codec = null; } if (socket != null) socket.close(); }
    }

    private void handle(byte[] p, int length) {
        if (length < 12 || (p[0] & 0xC0) != 0x80) return;
        int cc = p[0] & 0x0F, header = 12 + cc * 4; if (header > length) return;
        boolean marker = (p[1] & 0x80) != 0; int type = p[header] & 0x1F;
        byte[] nal;
        if (type == 28) {
            if (length < header + 2) return; int fu = p[header + 1] & 0xFF;
            boolean start = (fu & 0x80) != 0, end = (fu & 0x40) != 0;
            if (start) { accessUnit = new byte[] {0,0,0,1, (byte)((p[header] & 0xE0) | (fu & 0x1F))}; }
            append(p, header + 2, length - header - 2);
            if (!end) return;
            nal = accessUnit; accessUnit = new byte[0];
        } else {
            nal = new byte[length - header + 4]; nal[0]=0; nal[1]=0; nal[2]=0; nal[3]=1; System.arraycopy(p, header, nal, 4, length-header);
        }
        int nalType = nal[4] & 0x1F;
        if (nalType == 7) sps = Arrays.copyOfRange(nal, 4, nal.length);
        else if (nalType == 8) pps = Arrays.copyOfRange(nal, 4, nal.length);
        append(nal, 0, nal.length); pts = ((p[4]&255L)<<24)|((p[5]&255L)<<16)|((p[6]&255L)<<8)|(p[7]&255L);
        if (marker) { if (codec == null && sps != null && pps != null) startCodec(); if (codec != null) queue(accessUnit, pts * 1000000L / 90000L); accessUnit = new byte[0]; }
    }

    private void startCodec() {
        try {
            codec = MediaCodec.createDecoderByType("video/avc");
            MediaFormat f = MediaFormat.createVideoFormat("video/avc", 1920, 1080);
            f.setByteBuffer("csd-0", java.nio.ByteBuffer.wrap(sps)); f.setByteBuffer("csd-1", java.nio.ByteBuffer.wrap(pps));
            codec.configure(f, surface, null, 0); codec.start();
        } catch (IOException e) { codec = null; }
    }
    private void queue(byte[] data, long timestampUs) {
        try { int i = codec.dequeueInputBuffer(5000); if (i < 0) return; java.nio.ByteBuffer b = codec.getInputBuffer(i); b.clear(); b.put(data); codec.queueInputBuffer(i,0,data.length,timestampUs,0);
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo(); while (codec.dequeueOutputBuffer(info,0) >= 0) { /* surface output is rendered by MediaCodec */ }
        } catch (Exception ignored) { }
    }
    private void append(byte[] data, int offset, int count) { byte[] n = Arrays.copyOf(accessUnit, accessUnit.length + count); System.arraycopy(data, offset, n, accessUnit.length, count); accessUnit = n; }
}
