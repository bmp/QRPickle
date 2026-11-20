#pragma once
// Setup over USB: reads "QRPSETUP ..." lines from the USB serial port (main loop, non-blocking)
// and applies the settings the web installer page sends, like a web console save. Protocol:
// setup_serial_proto.h; design: docs/SETUP.md "Set up over USB".
namespace services {
    void setup_serial_update();
}
