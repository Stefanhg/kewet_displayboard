"""
Python interface for communicating with the Kewet display over Serial connection.

Available commands: See led_step_test.ino file for a list of commands.
"""

import serial
import time


class KewetDisplay:
    def __init__(self, port="/dev/ttyUSB0", baudrate=9600, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # Wait for the arduino to reset and be ready
    
    def send(self, command):
        """Send a command string to the Kewet display."""
        if not command.endswith('\n'):
            command += '\n'
        self.ser.write(command.encode('utf-8'))

    def read_response(self):
        """Read a response from the Kewet display."""
        response = self.ser.readline().decode('utf-8').strip()
        return response

    def close(self):
        """Close the serial connection."""
        self.ser.close()

    def wait_cfm(self, exp_cfm_status=0, timeout=5):
        """Waits for CFM OK (0) response from Kewet display."""
        while True:
            if timeout <= 0:
                raise TimeoutError("Timeout waiting for CFM OK response.")
            
            response = self.read_response()
            if int(response) == exp_cfm_status:
                break

    def send_cmd(self, cmd: str):
        """Send a command and wait for OK cmd."""
        self.send(cmd)
        self.wait_cfm(exp_cfm_status=0)

    def _test_set_all_on(self):
        """Custom test function to set all LEDs on."""
        self.send_cmd("X")
    
    def _test_clear_all(self):
        """Custom test function to clear all LEDs."""
        self.send_cmd("x")

    def set_speed(self, speed: int):
        self.send_cmd(f"S{speed}")
