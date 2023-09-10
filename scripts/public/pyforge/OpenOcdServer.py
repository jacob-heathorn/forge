import telnetlib
import os
import subprocess
import time
import signal

class Telnet:
  def __init__(self, host, port):
    self.connection_ = None
    self.host_ = host
    self.port_ = port
    self.timeout_ = 5

  def __enter__(self):
    self.connection_ = telnetlib.Telnet(self.host_, self.port_ , self.timeout_)
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    if self.connection_:
      self.connection_.close()

  def write(self, command):
    # Write command
    self.connection_.write((command + "\n").encode("ascii"))

def VerifyTelnet(host, port):
  num_attempts = 5
  for attempt in range(num_attempts):
    try:
      # Connect to the Telnet server
      with Telnet(host, port) as telnet:
        # Successful.
        return
    
    except ConnectionRefusedError:
        # Sleep and try again.
        print("try again")
        time.sleep(.05)
        pass
    
  raise Exception(f"Failed to connect to telnet after {num_attempts} attempts!")

class OpenOcdServer:
  TELNET_HOST = "localhost"
  TELNET_PORT = 4444

  def __init__(self, openocd_install_dir, config_fullfile, log_fullfile):
    self.server = None
    self.log_fullfile = log_fullfile
    self.config_fullfile = config_fullfile
    self.openocd_install_dir = openocd_install_dir
    self.executable = os.path.join(self.openocd_install_dir, 'bin', 'openocd')
  
  def __enter__(self):
    # Kill any pre-existing openocd server
    try:
      args = ["pkill", 'openocd', '--echo']
      subprocess.check_call(args)
    except:
      pass

    # Create log directory
    os.makedirs(os.path.dirname(self.log_fullfile), exist_ok=True)

    # Start the server and redirect output to the log file.
    args = f"{self.executable} \
      -c \"set OPENOCD_DIR {self.openocd_install_dir}\" \
      -f {self.config_fullfile} \
      -l {self.log_fullfile}"
    print(f"Starting OpenOCD server and redirecting output to:\n{self.log_fullfile}")
    print(f"\n>> {args}\n")
    self.server = subprocess.Popen(args, shell=True, preexec_fn=os.setsid)
    
    # Verify telnet can connect to the server.
    VerifyTelnet(OpenOcdServer.TELNET_HOST, OpenOcdServer.TELNET_PORT)
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    print("Killing OpenOCD server")

    self.wait_for_last_telnet()
    
    # Alrighty, let's kill it.
    os.killpg(os.getpgid(self.server.pid), signal.SIGTERM)

  def run(self, executable_fullfile):
    # Connect through telnet
    with Telnet(OpenOcdServer.TELNET_HOST, OpenOcdServer.TELNET_PORT) as telnet:
      telnet.write(f"program {executable_fullfile} verify reset")
    
    self.wait_for_last_telnet()
  
  def debug(self, executable_fullfile):
    # Connect through telnet
    with Telnet(OpenOcdServer.TELNET_HOST, OpenOcdServer.TELNET_PORT) as telnet:
      # Flash the executable, but don't reset
      telnet.write(f"program {executable_fullfile} verify")
    
    self.wait_for_last_telnet()

  def wait_for_last_telnet(self):
    # This implementation is a hack to make sure the server isn't still
    # processing previous telnet commands. If we can connect to it, it's not
    # busy.
    VerifyTelnet(OpenOcdServer.TELNET_HOST, OpenOcdServer.TELNET_PORT)
