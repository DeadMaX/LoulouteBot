/*
 * Copyright (C) 2018-2024 MC2 Technologies - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Alexandre Martins <alexandre.martins@mc2-technologies.com>
 */

#include "configuration.h"

#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

#ifdef WIN32
#include <windows.h>
#endif

thread_local std::string::size_type logger_h_offset{0};

static const std::string_view get_prefix(LogLevel l)
{
  switch (l)
  {
  case LogLevel::Emergency:
    return "[fat] ";

  case LogLevel::Alert:
    return "[fat] ";

  case LogLevel::Critical:
    return "[fat] ";

  case LogLevel::Error:
    return "[err] ";

  case LogLevel::Warning:
    return "[war] ";

  case LogLevel::Notice:
    return "[not] ";

  case LogLevel::Informational:
    return "[inf] ";

  default:
    [[fallthrough]];
  case LogLevel::Debugging:
    return "[deb] ";
  }
}

static std::ostream &write_time_and_prefix(
    std::ostream &os,
    LogLevel l,
    const std::chrono::system_clock::time_point &now = std::chrono::system_clock::now())
{
  auto t = std::chrono::system_clock::to_time_t(now);
  auto sec_part = std::chrono::system_clock::from_time_t(t);
  auto milis = std::chrono::duration_cast<std::chrono::milliseconds>(now - sec_part);
  auto tm = std::localtime(&t);
  std::ostream::sentry ios_saver{os};
  os << std::put_time(tm, "[%y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
     << milis.count() << "] " << get_prefix(l);
  return os;
}

StdlogBackend::StdlogBackend()
{
#ifdef WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);
#endif
}

void StdlogBackend::write(LogLevel l, std::string_view sv)
{
  static const char *colors[] = {"\033[0;33;41;5;1m",
                                 "\033[0;37;41;5;1m",
                                 "\033[0;37;43;5;1m",
                                 "\033[0;31;1m",
                                 "\033[0;33;1m",
                                 "\033[0;1m",
                                 "\033[0m",
                                 "\033[0m"};
  static const char *norm = "\033[0m";
  static std::mutex log_mutex;

  std::unique_lock lk{log_mutex};
  const char *c = colors[0];

  switch (l)
  {
  case LogLevel::Emergency:
    c = colors[0];
    break;
  case LogLevel::Alert:
    c = colors[1];
    break;
  case LogLevel::Critical:
    c = colors[2];
    break;
  case LogLevel::Error:
    c = colors[3];
    break;
  case LogLevel::Warning:
    c = colors[4];
    break;
  case LogLevel::Notice:
    c = colors[5];
    break;
  case LogLevel::Informational:
    c = colors[6];
    break;
  case LogLevel::Debugging:
    c = colors[7];
    break;
  }
  write_time_and_prefix(std::cout, l) << c << sv << norm << '\n';
}

