import serial
import threading
import signal
import traceback
import forge
import time
import subprocess


class SerialTerminal:
  """"
  Prints serial data to the console.
  """

  def __init__(self, serial_device, baud_rate):
    self.serial_device = serial_device
    self.baud_rate = baud_rate
    self.ser = None
    self.thread = None

  @staticmethod
  def is_serial_device_available(serial_device):
    """Check if a serial port is being used by another process."""
    try:
      result = subprocess.run(["lsof", serial_device], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
      # If there's no output, the port is available.
      return not bool(result.stdout)
    except FileNotFoundError:
      print("lsof command not found. Install it with: sudo apt install lsof")
      return False  # Assume port is free if lsof is unavailable

  def read_serial(self):
    """
    Continuously read and print serial data.
    """
    if not SerialTerminal.is_serial_device_available(self.serial_device):
      forge.error(f"Serial device {self.serial_device} is already in use!")

    else:
      forge.print_green("Starting serial terminal...")
      self.ser = serial.Serial(self.serial_device, self.baud_rate)
      try:
        while self.ser and self.ser.is_open:
          line = self.ser.readline().decode('utf-8').strip('\r\n')
          print(line)
      except Exception as e:
        if self.ser and self.ser.is_open:
          print(f"Serial error: {e}")
          traceback.print_exc()
      finally:
        if self.ser and self.ser.is_open:
          self.ser.close()
          print("Serial connection closed.")

  def read_background(self):
    """
    Reads the serial terminal in a background thread.
    """
    self.thread = threading.Thread(target=self.read_serial, daemon=False)
    self.thread.start()
    time.sleep(.1)  # For is_serial_device_available() to work immediately.
    signal.signal(signal.SIGINT, self.signal_handler)
    print("Ctrl+c to exit serial terminal.")

  def signal_handler(self, sig, frame):
    """
    Handle Ctrl+C and exit cleanly.
    """
    print("\nClosing serial terminal...")
    self.cleanup()

  def __del__(self):
    """
    Calls cleanup.
    """
    self.cleanup()

  def cleanup(self):
    """
    Cleans up resources.
    """
    if self.ser and self.ser.is_open:
      self.ser.close()
