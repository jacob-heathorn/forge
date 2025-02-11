import subprocess
import os
import forge

PROJECT_ROOT = os.environ.get("PROJECT_ROOT")

# =================================================================================================
# Tests


def test_clean():
  "Clean the .bin directory"
  args = ['rip', '--clean']
  subprocess.check_call(args)
  assert not os.path.exists(os.path.join(PROJECT_ROOT, '.bin'))
  assert not os.path.exists(os.path.join(PROJECT_ROOT, '.vscode', 'launch.json'))


def test_build_debug():
  """
  Build in DEBUG mode.
  """
  with forge.pushd(PROJECT_ROOT):
    args = ['cmake', '--workflow', '--preset', 'native-debug']
    subprocess.run(args, capture_output=True, text=True)
    assert os.path.exists(
        os.path.join(
            PROJECT_ROOT,
            '.bin',
            'native-debug',
            'forge',
            'test',
            'native',
            'hello-world'))


def test_build_release():
  """
  Build in RELEASE mode.
  """
  with forge.pushd(PROJECT_ROOT):
    args = ['cmake', '--workflow', '--preset', 'native-release']
    subprocess.run(args, capture_output=True, text=True)
  assert os.path.exists(
      os.path.join(
          PROJECT_ROOT,
          '.bin',
          'native-release',
          'forge',
          'test',
          'native',
          'hello-world'))


def test_debug_hello_world():
  """
  Verifies that debugging hello-world generates the vscode launch script.
  """
  forge.NativeApplication("native-debug:hello-world").debug()
  assert os.path.exists(os.path.join(PROJECT_ROOT, '.vscode', 'launch.json'))


def test_run_hello_world():
  """
  Verifies that running hello-world generates expected output.
  """
  args = ['rip', '--run', 'native-debug:hello-world']
  result = subprocess.run(args, capture_output=True, text=True)
  assert "Hello World!" in result.stdout


def test_ctest():
  """
  Uses ctest to verify both pigweed and googletest unit tests.
  """
  with forge.pushd(os.path.join(PROJECT_ROOT, '.bin', 'native-debug')):
    result = subprocess.run('ctest', capture_output=True, text=True)
    print(result.stdout)
    assert "100% tests passed" in result.stdout
    assert not result.stderr
