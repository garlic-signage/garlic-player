package com.sagiadinos.garlic.player.java;

import android.util.Log;

/**
 * Definitions for SICP command Philips Launcher
 */
public class SICPDef
{
    private static final String TAG = "PdScalarSICPDef";

    public enum SICPCommand
    {
        SICP_COMMAND_GET_POWERSTATE,
        SICP_COMMAND_SET_POWERSTATE_SCREEN_OFF,
        SICP_COMMAND_GET_MODEL_NUMBER,
        SICP_COMMAND_GET_FIRMWARE_BUILD,
        SICP_COMMAND_GET_SERIAL_CODE,
        SICP_COMMAND_SET_BACKLIGHT_ENABLE,
        SICP_COMMAND_SET_BACKLIGHT_DISABLE
    }

    final private static byte EMPTY_CHECKSUM = 0x00;
    final private static byte[] COMMAND_GET_POWERSTATE = { 0x05, 0x01, 0x00, 0x19, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_SET_POWERSTATE_SCREENOFF = { 0x06, 0x01, 0x00, 0x18, 0x1, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_GET_MODEL_NUMBER = { 0x06, 0x01, 0x00, (byte) 0xA1, 0x00, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_GET_FIRMWARE_BUILD = { 0x06, 0x01, 0x00, (byte) 0xA1, 0x01, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_GET_SERIAL_CODE = { 0x05, 0x01, 0x00, 0x15, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_SET_BACKLIGHT_ENABLE = { 0x06, 0x01, 0x00, 0x72, 0x00, EMPTY_CHECKSUM };
    final private static byte[] COMMAND_SET_BACKLIGHT_DISABLE = { 0x06, 0x01, 0x00, 0x72, 0x01, EMPTY_CHECKSUM };

    public static byte[] getSICPCommand(SICPCommand cmd)
    {
        byte[] sicpCmd;
        switch (cmd)
        {
            case SICP_COMMAND_GET_POWERSTATE:
                sicpCmd = COMMAND_GET_POWERSTATE;
                break;
            case SICP_COMMAND_SET_POWERSTATE_SCREEN_OFF:
                sicpCmd = COMMAND_SET_POWERSTATE_SCREENOFF;
                break;
            case SICP_COMMAND_GET_MODEL_NUMBER:
                sicpCmd = COMMAND_GET_MODEL_NUMBER;
                break;
            case SICP_COMMAND_GET_FIRMWARE_BUILD:
                sicpCmd = COMMAND_GET_FIRMWARE_BUILD;
                break;
            case SICP_COMMAND_GET_SERIAL_CODE:
                sicpCmd = COMMAND_GET_SERIAL_CODE;
                break;
            case SICP_COMMAND_SET_BACKLIGHT_ENABLE:
                sicpCmd = COMMAND_SET_BACKLIGHT_ENABLE;
                break;
            case SICP_COMMAND_SET_BACKLIGHT_DISABLE:
                sicpCmd = COMMAND_SET_BACKLIGHT_DISABLE;
                break;
            default:
                return null;
        }

        // update check sum
        int length = sicpCmd.length;
        byte checkSum = getDataCheckSum(sicpCmd);
        sicpCmd[length -1] = checkSum;
        return sicpCmd;
    }

    public static byte getDataCheckSum(byte[] data)
    {
        int nCheckSum = 0;
        if (data != null)
        {
            nCheckSum = (data[0] ^ data[1]) & 0xff;
            // skip original checkSum, so "data.length -1"
            for (int i = 2; i < data.length -1; i++)
            {
                nCheckSum = (nCheckSum ^ data[i]) & 0xff;
            }
            Log.d(TAG, "getDataCheckSum: checksum is " + nCheckSum);
        }
        else
        {
            Log.e(TAG, "getDataCheckSum: ERR! data array is null!");
        }
        return (byte) (nCheckSum & 0xff);
    }

    public static void toHexString(byte[] sicpData)
    {
        Log.d(TAG, "========== toHexString ==========");
        Log.d(TAG, "data size = " + sicpData.length);
        for (int i = 0; i < sicpData.length; i++)
        {
            Log.d(TAG, "sicpData[" + i + "]: 0x" + convertByteToHexString(sicpData[i]));
        }
        Log.d(TAG, "=====================================");
    }

    public static String convertByteToHexString(byte sicpByte)
    {
        return String.format("%02X", sicpByte & 0xFF);
    }

    public static String convertBytesToString(byte[] sicpData)
    {
        // use full data begins from sicpData[44]
        StringBuilder sb = new StringBuilder();
        for (int i = 4; i < sicpData.length - 1 ; i++)
        {
            sb.append(Character.toString((char) sicpData[i]));
        }
        return sb.toString();
    }

}
