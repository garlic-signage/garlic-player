package com.sagiadinos.garlic.player.java;


import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

import android.os.Handler;
import android.util.Log;

/**
 * Socket Client needed for communicating SICP commands to Philips Launcher
 */
public class SocketClient
{
    private static final String TAG = "SocketClient";
    public static final String address = "127.0.0.1";
    public static final int port = 5000;
    private Socket mClientSocket;
    private final SocketResponse mSocketResponse;
    protected DataInputStream mDataInputStream;
    private BufferedOutputStream mBufferedOutputStream;

    private static boolean bStopped;

    public static interface SocketResponse
    {
        public void sockResponse(byte[] ret);
    }

    public SocketClient(SocketResponse socketResponse) {
        mSocketResponse = socketResponse;
    }

    public void start()
    {
        bStopped = false;

        new Thread(new Runnable()
        {
            @Override
            public void run() {
                mClientSocket = new Socket();
                InetSocketAddress isa = new InetSocketAddress(address, port);
                try
                {
                    mClientSocket.connect(isa, 10000);
                    mDataInputStream = new DataInputStream(mClientSocket.getInputStream());
                    mBufferedOutputStream = new BufferedOutputStream(mClientSocket.getOutputStream());
                    Log.w(TAG, "Socket started");
                }
                catch (IOException e)
                {
                    e.printStackTrace();
                }

                new SocketReplyThread(mClientSocket, mDataInputStream, mSocketResponse).start();
            }
        }).start();
    }

    public void stop()
    {
        bStopped = true;
        new Handler().postDelayed(new Runnable()
        {
            @Override
            public void run()
            {
                try {
                    mBufferedOutputStream.close();
                    mBufferedOutputStream = null;

                    mDataInputStream.close();
                    mDataInputStream = null;

                    mClientSocket.close();
                    mClientSocket = null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }, 1000);
    }

    public void sendSICPCommand(final byte[] data)
    {
        new SocketSendCommandThread(mClientSocket, mBufferedOutputStream, data).start();
    }

    public static class SocketSendCommandThread extends Thread
    {
        private Socket clientSocket;
        private BufferedOutputStream bufferedOutputStream;
        private byte[] data;

        public SocketSendCommandThread(Socket clientSocket, BufferedOutputStream bufferedOutputStream, byte[] data) {
            this.clientSocket = clientSocket;
            this.bufferedOutputStream = bufferedOutputStream;
            this.data = data;
        }

        @Override
        public void run() {
            if (clientSocket == null || clientSocket.isClosed()) {
                Log.w(TAG, "SocketSendCommandThread, clientSocket is invalid. clientSocket: " + clientSocket);
                return;
            }

            if (bufferedOutputStream == null) {
                Log.w(TAG, "SocketSendCommandThread, null pointer. bufferedOutputStream: " + bufferedOutputStream);
                return;
            }

            try {
                bufferedOutputStream.write(data);
                bufferedOutputStream.flush();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public static class SocketReplyThread extends Thread
    {
        private Socket clientSocket;
        private SocketResponse socketResponse;
        private DataInputStream dataInputStream;
        private byte[] byteBuf = new byte[64];
        private byte nCmdLen;

        public SocketReplyThread(Socket clientSocket, DataInputStream dataInputStream, SocketResponse socketResponse) {
            this.clientSocket = clientSocket;
            this.socketResponse = socketResponse;
            this.dataInputStream = dataInputStream;
        }

        @Override
        public void run() {
            if (clientSocket == null || clientSocket.isClosed()) {
                Log.w(TAG, "SocketReplyThread, clientSocket is invalid. clientSocket: " + clientSocket);
                return;
            }

            if (socketResponse == null || dataInputStream == null) {
                Log.w(TAG, "SocketReplyThread, null pointer. socketResponse: " + socketResponse + ", bufferedReader: "
                        + dataInputStream);
                return;
            }
            while (!bStopped && !clientSocket.isClosed()) {
                try {
                    int nRead = dataInputStream.read(byteBuf);
                    int nOffset = 0;
                    while (!bStopped && nRead > 0) {
                        nCmdLen = byteBuf[0];
                        nOffset = 0;
                        Log.d(TAG, "nCmdLen= " + nCmdLen);

                        if (nCmdLen > 0 && (nCmdLen <= nRead)) {
                            byte[] command = new byte[nCmdLen];
                            System.arraycopy(byteBuf, nOffset, command, 0, nCmdLen);
                            if (socketResponse != null) {
                                socketResponse.sockResponse(command);
                            }
                        }

                        nRead = nRead - nCmdLen;
                        if (nRead <= 0) {
                            break;
                        } else {
                            nOffset = nOffset + nCmdLen;
                        }
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

}
