import contextlib
import os

# Prints green
def print_green(text):
  print(f"\033[32m{text}\033[0m")

# Prints red
def print_red(text):
  print(f"\033[91m{text}\033[0m")

# Prints and raises an exception
def Error(message: str):
  print_red(message)
  raise(Exception(message))


# Context manager for pushd. Example from
# (https://stackoverflow.com/questions/6194499/pushd-through-os-system)
@contextlib.contextmanager
def pushd(new_dir):
  previous_dir = os.getcwd()
  
  # Create the directoy if it doesn't exsits (not exactly pushd)
  if not os.path.exists(new_dir):
    os.makedirs(new_dir)
  
  os.chdir(new_dir)
  try:
      yield
  finally:
      os.chdir(previous_dir)
