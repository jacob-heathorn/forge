// sample_pw_ut_main.cpp
//
// Copy this file, adapt it for your platform (e.g. threadx entry function), and it to the
// pw_unit_test target library, so that when you link it while authoring unit tests, main is
// defined.

#include "gtest/gtest.h"

#include <cstdio>

// pw_unit_test:light requires an event handler to be configured.
#include "pw_unit_test/simple_printing_event_handler.h"

void WriteString(const std::string_view& string, bool newline) {
  (void)string;
  printf("%s", string.data());
  if (newline) {
    printf("\r\n");
  }
}

int main() {
  // // The following line has no effect with pw_unit_test_light, but makes this
  // // test compatible with upstream GoogleTest.
  // testing::InitGoogleTest();
  printf("\r\nRunning Pigweed Unit Tests! For the baddies.\r\n");

  // Since we are using pw_unit_test:light, set up an event handler.
  pw::unit_test::SimplePrintingEventHandler handler(WriteString);
  pw::unit_test::RegisterEventHandler(&handler);
  return RUN_ALL_TESTS();
}
