import os
import serial
import threading
import signal
import traceback
import forge
import time
import subprocess
from typing import Any, Optional


class SerialTerminal:
  """"
  Prints serial data to the console.
  """

  def __init__(self, serial_device: str, baud_rate: int) -> None:
    self.serial_device = serial_device
    self.baud_rate = baud_rate
    self.ser: Optional[serial.Serial] = None
    self.thread: Optional[threading.Thread] = None

    if not SerialTerminal.is_serial_device_available(self.serial_device):
      forge.error(f"Serial device {self.serial_device} is already in use!")

    self.ser = serial.Serial(self.serial_device, self.baud_rate)

  @staticmethod
  def is_serial_device_available(serial_device: str) -> bool:
    """Check if a serial port is being used by another process."""
    try:
      result = subprocess.run(["lsof", serial_device], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
      # If there's no output, the port is available.
      return not bool(result.stdout)
    except FileNotFoundError:
      print("lsof command not found. Install it with: sudo apt install lsof")
      return False  # Assume port is free if lsof is unavailable

  def readline(self) -> str:
    """
    Blocking read and return one line of serial data.
    """
    assert self.ser is not None, "Serial port not initialized"
    return self.ser.readline().decode('utf-8').strip('\r\n')

  def read(self) -> None:
    """
    Blocking continuously read and print serial data.
    """
    try:
      while self.ser and self.ser.is_open:
        line = self.readline()
        print(line)
    except Exception as e:
      if self.ser and self.ser.is_open:
        print(f"Serial error: {e}")
        traceback.print_exc()
    finally:
      if self.ser and self.ser.is_open:
        self.ser.close()
        print("Serial connection closed.")

  def read_background(self) -> None:
    """
    Reads the serial terminal in a background thread.
    """
    forge.print_green("Reading serial terminal in the background...")
    # daemon=True so a readline() blocked on a closed port can't keep
    # the process alive after the main thread exits.
    self.thread = threading.Thread(target=self.read, daemon=True)
    self.thread.start()
    time.sleep(.1)  # For is_serial_device_available() to work immediately.
    signal.signal(signal.SIGINT, self.signal_handler)
    print("Ctrl+c to exit serial terminal.")

  def signal_handler(self, sig: int, frame: Any) -> None:
    """
    Handle Ctrl+C and exit cleanly.
    """
    print("\nClosing serial terminal...")
    self.cleanup()
    # os._exit instead of sys.exit: sys.exit raises SystemExit which has
    # to unwind, and during that unwind threading._shutdown can re-enter
    # this handler (re-printing the close message and dumping a noisy
    # "Exception ignored ... SystemExit" trace). The port is already
    # closed and the read thread is a daemon, so terminating immediately
    # is correct.
    os._exit(0)

  def __del__(self) -> None:
    """
    Calls cleanup.
    """
    self.cleanup()

  def cleanup(self) -> None:
    """
    Cleans up resources.
    """
    if self.ser and self.ser.is_open:
      self.ser.close()
