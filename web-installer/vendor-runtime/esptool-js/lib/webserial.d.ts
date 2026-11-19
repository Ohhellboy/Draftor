/// <reference types="w3c-web-serial" />
/**
 * Options for device serialPort.
 * @interface SerialOptions
 *
 * Note: According to the documentation of the Web Serial API, 'baudRate' is a
 * 'required' field as part of serial options. However, we are currently
 * maintaining 'baudRate' as a separate parameter outside the options
 * dictionary, and it is effectively used in the code. For now, we are
 * keeping it optional in the dictionary to avoid conflicts.
 */
export interface SerialOptions {
    /**
     * A positive, non-zero value indicating the baud rate at which serial communication should be established.
     * @type {number | undefined}
     */
    baudRate?: number | undefined;
    /**
     * The number of data bits per frame. Either 7 or 8.
     * @type {number | undefined}
     */
    dataBits?: 7 | 8 | undefined;
    /**
     * The number of stop bits at the end of a frame. Either 1 or 2.
     * @type {number | undefined}
     */
    stopBits?: 1 | 2 | undefined;
    /**
     * The parity mode: none, even or odd
     * @type {ParityType | undefined}
     */
    parity?: ParityType | undefined;
    /**
     * A positive, non-zero value indicating the size of the read and write buffers that should be created.
     * @type {number | undefined}
     */
    bufferSize?: number | undefined;
    /**
     * The flow control mode: none or hardware.
     * @type {FlowControlType | undefined}
     */
    flowControl?: FlowControlType | undefined;
}
/**
 * Optional capability exposed by serial-port implementations which can change
 * baud rate without closing the port. This is not part of the Web Serial API,
 * but can be implemented by WebUSB adapters using device-specific control
 * transfers.
 */
export interface BaudRateConfigurablePort extends SerialPort {
    setBaudRate?: (baudRate: number) => Promise<void>;
}
/**
 * Wrapper class around Webserial API to communicate with the serial device.
 * @param {typeof import("w3c-web-serial").SerialPort} device - Requested device prompted by the browser.
 *
 * ```
 * const port = await navigator.serial.requestPort();
 * ```
 */
declare class Transport {
    device: SerialPort;
    tracing: boolean;
    slipReaderEnabled: boolean;
    baudrate: number;
    private traceLog;
    private lastTraceTime;
    private reader;
    private buffer;
    private onDeviceLostCallback;
    constructor(device: SerialPort, tracing?: boolean, enableSlipReader?: boolean);
    /**
     * Set callback for when device is lost
     * @param {Function} callback Function to call when device is lost
     */
    setDeviceLostCallback(callback: (() => void) | null): void;
    /**
     * Update the device reference (used when re-selecting device after reset)
     * @param {typeof import("w3c-web-serial").SerialPort} newDevice New SerialPort device
     */
    updateDevice(newDevice: SerialPort): void;
    /**
     * Request the serial device vendor ID and Product ID as string.
     * @returns {string} Return the device VendorID and ProductID from SerialPortInfo as formatted string.
     */
    getInfo(): string;
    /**
     * Request the serial device vendor id from SerialPortInfo.
     * @returns {number | undefined} Return the vendor ID.
     */
    getVid(): number | undefined;
    /**
     * Request the serial device product id from SerialPortInfo.
     * @returns {number | undefined} Return the product ID.
     */
    getPid(): number | undefined;
    /**
     * Format received or sent data for tracing output.
     * @param {string} message Message to format as trace line.
     */
    trace(message: string): void;
    returnTrace(): Promise<void>;
    hexify(s: Uint8Array): string;
    hexConvert(uint8Array: Uint8Array, autoSplit?: boolean): string;
    /**
     * Format data packet using the Serial Line Internet Protocol (SLIP).
     * @param {Uint8Array} data Binary unsigned 8 bit array data to format.
     * @returns {Uint8Array} Formatted unsigned 8 bit data array.
     */
    slipWriter(data: Uint8Array): Uint8Array;
    /**
     * Write binary data to device using the WebSerial device writable stream.
     * @param {Uint8Array} data 8 bit unsigned data array to write to device.
     */
    write(data: Uint8Array): Promise<void>;
    /**
     * Append a buffer array after another buffer array
     * @param {Uint8Array} arr1 - First array buffer.
     * @param {Uint8Array} arr2 - magic hex number to select ROM.
     * @returns {Uint8Array} Return a 8 bit unsigned array.
     */
    appendArray(arr1: Uint8Array, arr2: Uint8Array): Uint8Array;
    /**
     * Read from serial device and append to buffer
     */
    readLoop(): Promise<void>;
    flushInput(): void;
    /**
     * Actively drain the input buffer: wait for in-flight bytes to arrive
     * (the ROM repeats an unsupported command response 8 times) and discard them.
     * Flushing alone only drops the bytes already buffered.
     * @param {number} quietMs Time without new bytes before considering the stream drained
     * @param {number} maxMs Upper bound on the total wait
     */
    drainInput(quietMs?: number, maxMs?: number): Promise<void>;
    flushOutput(): Promise<void>;
    inWaiting(): number;
    peek(): Uint8Array;
    /**
     * Detect if the data read from device is a Fatal or Guru meditation error.
     * @param {Uint8Array} input Data read from device
     */
    private detectPanicHandler;
    private SLIP_END;
    private SLIP_ESC;
    private SLIP_ESC_END;
    private SLIP_ESC_ESC;
    /**
     * Take a data array and return the first well formed packet after
     * replacing the escape sequence. Reads at least 8 bytes.
     * @param {number} timeout Timeout read data.
     * @returns {Uint8Array} Formatted packet using SLIP escape sequences.
     */
    read(timeout: number): Promise<Uint8Array>;
    /**
     * Read from serial device without SLIP formatting. Calls onData for each chunk.
     * Stops when isClosed() returns true, the stream ends/errors, or disconnect() is called.
     *
     * The reader is stored on the instance so that disconnect() can cancel a pending
     * read: isClosed() is only evaluated after each chunk arrives, so on an idle
     * device a function-local reader would keep the stream locked forever and make
     * disconnect() block in waitForUnlock().
     * @param {Function} onData Callback for each chunk of data read
     * @param {Function} isClosed Function that returns true when reading should stop (e.g. when console is closed)
     */
    rawRead(onData: (data: Uint8Array) => void, isClosed: () => boolean): Promise<void>;
    _DTR_state: boolean;
    /**
     * Send the RequestToSend (RTS) signal to given state
     * # True for EN=LOW, chip in reset and False EN=HIGH, chip out of reset
     * @param {boolean} state Boolean state to set the signal
     */
    setRTS(state: boolean): Promise<void>;
    /**
     * Send the dataTerminalReady (DTS) signal to given state
     * # True for IO0=LOW, chip in reset and False IO0=HIGH
     * @param {boolean} state Boolean state to set the signal
     */
    setDTR(state: boolean): Promise<void>;
    /**
     * Set the device signals to given states
     * # True for IO0=LOW, chip in reset and False IO0=HIGH
     * @param {boolean} dtr dataTerminalReady (DTS) signal boolean state
     * @param {boolean} rts requestToSend (RTS) signal boolean state
     * @param {boolean} breakSignal break signal boolean state
     */
    setSignals(dtr?: boolean, rts?: boolean, breakSignal?: boolean): Promise<void>;
    /**
     * Connect to serial device using the Webserial open method.
     * @param {number} baud Number baud rate for serial connection. Default is 115200.
     * @param {typeof import("w3c-web-serial").SerialOptions} serialOptions Serial Options for WebUSB SerialPort class.
     */
    connect(baud?: number, serialOptions?: SerialOptions): Promise<void>;
    /**
     * Change the host-side baud rate, preferring an in-place capability when the
     * serial-port implementation provides one.
     * @param {number} baud New baud rate.
     * @param {SerialOptions} serialOptions Options to preserve when a reopen is required.
     * @returns {boolean} True when the port had to be closed and reopened.
     */
    changeBaudrate(baud: number, serialOptions?: SerialOptions): Promise<boolean>;
    /**
     * Wait for a given timeout ms for serial device unlock.
     * @param {number} timeout Timeout time in milliseconds (ms) to sleep
     */
    waitForUnlock(timeout: number): Promise<void>;
    /**
     * Disconnect from serial device by running SerialPort.close() after streams unlock.
     */
    disconnect(): Promise<void>;
}
export { Transport };
