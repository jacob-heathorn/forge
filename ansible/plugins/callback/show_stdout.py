from ansible.plugins.callback import CallbackBase


class CallbackModule(CallbackBase):
  CALLBACK_VERSION = 2.0
  CALLBACK_TYPE = 'aggregate'
  CALLBACK_NAME = 'show_stdout'

  def __init__(self):
    super(CallbackModule, self).__init__()

  def v2_runner_on_ok(self, result):

    task_vars = result._task_fields.get('vars', {})
    show_stdout = task_vars.get('show_stdout', False)

    if show_stdout and result._result.get('stdout'):

      for line in result._result['stdout'].splitlines():
        self._display.display(line)
