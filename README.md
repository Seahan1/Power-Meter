This project aims to design a current detection system based on the MAX44284H high-precision current sensing chip and the STM32G474 microcontroller, enabling accurate acquisition and real-time monitoring of DC or AC current signals. The system adopts a co-design approach between hardware and software. The lower-level device (STM32G474) is responsible for signal conditioning, data acquisition, and protocol encapsulation, while the upper-level device is developed using C++ and the Qt framework, featuring serial communication management, data framing and parsing, filtering and smoothing, real-time waveform plotting, and CSV data export functions. It is suitable for applications such as industrial inspection, power monitoring, and motor control.

## System Architecture
1. Hardware Part
Core Control Chip: STM32G474RBT6, featuring a high-performance ARM Cortex-M4 core with floating-point unit support, meeting the requirements for high-precision data processing.
Current Sensing Chip: MAX44284H, supports ±20A current measurement, includes an internal PGA amplifier and high-precision ADC interface, with low noise and high common-mode rejection ratio (CMRR), suitable for high-current applications.
Signal Conditioning Circuit: Includes filter networks, current limiting protection, and level-shifting modules to ensure input signals are within the operational range of the MAX44284H.
Communication Interface: Uses UART (USART1) for serial communication with the upper-level device, supporting configurable baud rates (recommended 115200 bps or higher).
Power Supply Design: Employs an independent power module to provide stable voltage for both MAX44284H and STM32G474, minimizing interference.
2. Software Part
Lower-Level Firmware (STM32G474): Configured using STM32CubeMX, with peripheral drivers developed using HAL or LL libraries.
Upper-Level Application: Developed using Qt 5.15+ with C++ language, implementing a graphical user interface and data processing functions.
## Core Function Module Descriptions
### Serial Communication Management Module
Ensures stable communication between the upper-level and STM32G474.
Supports configuration of serial parameters: baud rate, data bits, stop bits, parity, and flow control.
Provides automatic serial port detection and connection status indication, supporting detection of multiple serial devices.
Implements send and receive buffer management to prevent overflow.
Supports disconnection reconnection and communication timeout detection, enhancing system robustness.
### Data Framing and Parsing
Defines a custom communication protocol frame structure: including start flag, device ID, data length, data payload, checksum (e.g., CRC8 or XOR), and end marker.
The lower-level device packages and transmits current sampling data in frames, each containing optional timestamp, sampled values (in floating-point or fixed-point format), and status flags.
The upper-level device performs checksum verification upon receiving complete frames; if verification fails, the frame is discarded and retransmission may be requested.
Extracts valid data after parsing for subsequent processing, ensuring data integrity and correctness.
### Data Filtering and Smoothing
Adopts a multi-stage filtering strategy to enhance data stability:
a. Hardware Level: An RC low-pass filter is added before the ADC to suppress high-frequency noise.
b. Software Level:
Uses sliding average filtering (e.g., window size of 10) to reduce random fluctuations.
Applies median filtering for sudden signal changes to suppress pulse interference.
Optionally includes Kalman filtering or first-order IIR filters to further optimize dynamic response and noise suppression.
Supports selection and parameter adjustment of filtering algorithms, facilitating adaptation to different operating conditions.
### Waveform Plotting
Real-time plotting of current variation over time, supporting multi-channel display (e.g., single-channel current or simultaneous display of voltage and current).
Chart supports zooming, panning, and scrolling through historical data.
Allows setting time axis ranges (e.g., 1s, 10s, 1min) and displays sampling rate.
Uses Qt Charts library for high-performance plotting, with real-time refresh rate recommended at ≥20Hz.
Provides peak marking, peak/trough annotation, and threshold lines to assist in analyzing abnormal current behavior.
### CSV Export
Supports exporting current or historical collected data to standard CSV file format.
Exported content includes: timestamp, raw sampled values, filtered data, device ID, sampling frequency, etc.
Users can customize export range (e.g., all data, last 10 seconds, user-defined start/end time).
Export path is configurable, with automatic file naming support (e.g., current_log_20250405_1430.csv).
After export completion, a prompt message is displayed to facilitate subsequent import into Excel, MATLAB, or other software for analysis.
## Development Environment and Toolchain
Lower-Level Development: STM32CubeIDE / Keil MDK / IAR Embedded Workbench
Upper-Level Development: Qt Creator + C++17 standard + Qt 5.15+
Communication Protocol: Custom binary frame protocol (based on UART)
Debugging Tools: Serial terminal software (e.g., SSCOM, Tera Term), logic analyzer, oscilloscope
