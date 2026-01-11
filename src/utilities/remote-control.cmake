find_package(magic_args CONFIG REQUIRED)
include(MagicArgs)

add_utility_executable(
  OpenKneeboard-RemoteControl
  WIN32
  remote-control.cpp
  simple-remotes.cpp
  simple-remotes.hpp
  remote-traceprovider.cpp
)
target_link_libraries(
  OpenKneeboard-RemoteControl
  PRIVATE
  OpenKneeboard-APIEvent
  OpenKneeboard-UserAction
  OpenKneeboard-dprint
  OpenKneeboard-tracing
  magic_args::magic_args
)
magic_args_enumerate_subcommands(
  OpenKneeboard-RemoteControl
  HARDLINKS_DIR "$<TARGET_FILE_DIR:OpenKneeboard-RemoteControl>"
  TEXT_FILE "$<TARGET_FILE_DIR:OpenKneeboard-RemoteControl>/$<TARGET_FILE_BASE_NAME:OpenKneeboard-RemoteControl>-aliases.txt"
)
