import forge


def run(preset_application: str):
  """
  TODO
  """
  preset, application = forge.resolve_application(preset_application)

  # if preset.startswith("cm7"):
  #   flasher = mimxrt1170evk.Core0Flasher()
  #   flasher.flash(application)
  # else:
  #   forge.error("You can only flash core0, which is cortex-m7 architecture")