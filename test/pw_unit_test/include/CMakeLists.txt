target_sources(pw_unit_test PUBLIC
  FILE_SET HEADERS
  FILES
    gtest/gtest.h
    pw_unit_test/config.h
    pw_unit_test/event_handler.h
    pw_unit_test/googletest_handler_adapter.h
    pw_unit_test/googletest_style_event_handler.h
    pw_unit_test/logging_event_handler.h
    pw_unit_test/printf_event_handler.h
    pw_unit_test/simple_printing_event_handler.h
    pw_unit_test/static_library_support.h
    pw_unit_test/unit_test_service.h
    pw_unit_test/internal/framework.h
)
