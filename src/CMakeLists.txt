
set(BOOTKEY "" CACHE STRING "La clé du bot")

if(BOOTKEY MATCHES "^$")
	message(FATAL_ERROR "Pas de clé de bot défini")
endif()

add_executable(LoulouteBot main.cpp configuration.cpp logger.cpp)
target_compile_definitions(LoulouteBot PRIVATE BOT_TOKEN="${BOOTKEY}")
target_link_libraries(LoulouteBot PUBLIC LIBDPP)

