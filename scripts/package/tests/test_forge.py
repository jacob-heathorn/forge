import pytest
import forge

# =================================================================================================
# Tests


def test1():
  """
  TODO.
  """
  forge.NativeApplication("native-debug:hello-world").run()

  print("hello world")
  assert True
