/// <reference types="w3c-web-usb" />
/// <reference types="w3c-web-serial" />
import { BaudRateConfigurablePort, SerialOptions } from "./webserial.js";
/**
 * SerialPort-compatible WebUSB adapter for WCH CH340/CH341.
 * Implements in-place baud changes with vendor request 0x9A so Transport
 * does not need to close and reopen the port.
 */
export declare class WebUSBSerialPort {
    private usbDevice;
    readable: ReadableStream<Uint8Array> | null;
    writable: WritableStream<Uint8Array> | null;
    private interfaceNumber;
    private endpointIn;
    private endpointOut;
    private currentDtr;
    private currentRts;
    private controlQueue;
    private readLoopRunning;
    /**
     * Wrap an already-selected USBDevice.
     * @param {object} usbDevice CH340/CH341 USB device.
     */
    constructor(usbDevice: USBDevice);
    /**
     * Prompt for a WCH USB-serial device (VID 0x1a86). CH343 (PID 0x55d3) is rejected.
     * @returns {Promise<WebUSBSerialPort>} Adapter for the selected device.
     */
    static requestPort(): Promise<WebUSBSerialPort>;
    /**
     * USB vendor and product identifiers.
     * @returns {object} Vendor and product IDs.
     */
    getInfo(): SerialPortInfo;
    /**
     * Open the device, claim the vendor bulk interface, and initialize CH340 UART.
     * @param {SerialOptions} options Serial options. Only baudRate is applied on CH340.
     */
    open(options?: SerialOptions): Promise<void>;
    /**
     * Change baud rate in place using CH340 vendor request 0x9A.
     * @param {number} baudRate New baud rate.
     */
    setBaudRate(baudRate: number): Promise<void>;
    /**
     * Set DTR/RTS via CH340 handshake request 0xA4.
     * @param {object} signals Control-line values.
     * @param {boolean} [signals.dataTerminalReady] DTR line.
     * @param {boolean} [signals.requestToSend] RTS line.
     */
    setSignals(signals: {
        dataTerminalReady?: boolean;
        requestToSend?: boolean;
    }): Promise<void>;
    /**
     * Release the USB interface and close the device.
     */
    close(): Promise<void>;
    /**
     * Use with Transport, which is typed against the Web Serial SerialPort.
     * @returns {BaudRateConfigurablePort} This adapter as a SerialPort-like object.
     */
    asSerialPort(): BaudRateConfigurablePort;
    /**
     * Locate the vendor class 0xFF bulk IN/OUT interface.
     */
    private claimBulkInterface;
    /**
     * Serialize vendor control transfers.
     * @param {number} request bRequest.
     * @param {number} value wValue.
     * @param {number} index wIndex.
     */
    private vendorOut;
    /**
     * Create ReadableStream / WritableStream over bulk endpoints.
     */
    private createStreams;
}
