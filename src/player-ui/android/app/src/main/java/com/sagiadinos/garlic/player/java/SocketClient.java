package com.sagiadinos.garlic.player.java;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.Arrays;

/**
 * The SICP connection to the Philips firmware on 127.0.0.1:5000. Connects at the first command and again whenever the
 * connection is gone (the firmware restarted its service, a write failed): the next command connects anew, so SICP
 * does not stay dead until the player restarts. Commands are written by the caller's thread (PhilipsLauncher sends
 * them one after the other); a reader thread per connection hands every complete answer to SocketResponse.
 */
public class SocketClient
{
    private static final String TAG        = "SocketClient";
    public static final String  address    = "127.0.0.1";
    public static final int     port       = 5000;
    private static final int    CONNECT_MS = 2000;

    public interface SocketResponse
    {
        void sockResponse(byte[] ret);
    }

    private final SocketResponse response;
    private Socket       socket; // null: not connected
    private OutputStream out;

    public SocketClient(SocketResponse socketResponse)
    {
        response = socketResponse;
    }

    /// Writes a command, connecting first if needed; a failed write connects once more and writes again.
    public synchronized void sendSICPCommand(final byte[] data)
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            if (socket == null && !connect())
                return;
            try
            {
                out.write(data);
                out.flush();
                return;
            }
            catch (IOException e)
            {
                Log.w(TAG, "SICP write failed, connecting again: " + e.getMessage());
                close(socket);
            }
        }
    }

    private boolean connect()
    {
        Socket next = new Socket();
        try
        {
            next.connect(new InetSocketAddress(address, port), CONNECT_MS);
            socket = next;
            out    = next.getOutputStream();
            InputStream in = next.getInputStream();
            new Thread(() -> read(next, in), "SICP reader").start();
            Log.i(TAG, "SICP connected");
            return true;
        }
        catch (IOException e)
        {
            Log.w(TAG, "SICP not connected: " + e.getMessage());
            try
            {
                next.close();
            }
            catch (IOException ignored)
            {
            }
            return false;
        }
    }

    /// Closes the connection if it is still the current one (a reader of an old one closes only its own)
    private synchronized void close(Socket which)
    {
        if (which == null)
            return;
        try
        {
            which.close();
        }
        catch (IOException ignored)
        {
        }
        if (which == socket)
        {
            socket = null;
            out    = null;
        }
    }

    /**
     * The answers of one connection until it ends (end of stream or an error), then the connection is closed: the next
     * command connects again. An answer starts with its size; a read may hold several answers or a part of one.
     */
    private void read(Socket which, InputStream in)
    {
        byte[]                chunk   = new byte[256];
        ByteArrayOutputStream pending = new ByteArrayOutputStream();
        try
        {
            int n;
            while ((n = in.read(chunk)) > 0)
            {
                pending.write(chunk, 0, n);
                byte[] bytes  = pending.toByteArray();
                int    offset = 0;
                while (offset < bytes.length)
                {
                    int size = bytes[offset] & 0xFF;
                    if (size < 4) // no answer is that short: out of step, the rest is dropped
                    {
                        Log.w(TAG, "SICP answer with size " + size + ", dropped");
                        offset = bytes.length;
                        break;
                    }
                    if (offset + size > bytes.length)
                        break; // the rest comes with the next read
                    response.sockResponse(Arrays.copyOfRange(bytes, offset, offset + size));
                    offset += size;
                }
                pending.reset();
                pending.write(bytes, offset, bytes.length - offset);
            }
            Log.w(TAG, "SICP connection closed by the display");
        }
        catch (IOException e)
        {
            Log.w(TAG, "SICP connection lost: " + e.getMessage());
        }
        close(which);
    }
}
