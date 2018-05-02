include(cmake/configs/posix_sitl_default.cmake)

list(APPEND config_module_list
	# checkpoint
	examples/checkpoint
)

set(UREBOOT_HOME "$ENV{UREBOOT_HOME}")
if(UREBOOT_HOME)
	message("Building with microreboot support")
else()
  message(FATAL_ERROR "UREBOOT_HOME environment variable not set")
endif()
