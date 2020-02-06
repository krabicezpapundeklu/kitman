cmake_minimum_required(VERSION 3.16)
project(kitman)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(kitman
	db.cpp
	db.hpp
	exception.cpp
	exception.hpp
	http_listener.cpp
	http_listener.hpp
	http_session.cpp
	http_session.hpp
	json.hpp
	kitman.cpp
	kitman.hpp
	main.cpp
	mime.cpp
	mime.hpp
	shell.c
	sqlite3.c
	sqlite3.h
)

set_property(TARGET kitman PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_compile_definitions(kitman PRIVATE _WIN32_WINNT=0x0601 BOOST_DATE_TIME_NO_LIB BOOST_REGEX_NO_LIB SQLITE_SHELL_IS_UTF8=1)
target_link_libraries(kitman PRIVATE Boost::program_options)
