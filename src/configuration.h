/*
 * Copyright (C) 2018-2024 MC2 Technologies - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Alexandre Martins <alexandre.martins@mc2-technologies.com>
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <iterator>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief SFINAE test class used to know if the class has a toString method
 */
template <typename T> class has_toString {
  using one = char;
  using two = std::array<char, 2>;

  template <typename C> static one test(decltype(&C::toString));
  template <typename C> static two test(...);

public:
  enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

/**
 * @brief Syntaxic sugar fo the has_toString SFINAE check
 */
template <typename T>
inline constexpr bool has_toString_v = has_toString<T>::value;

/**
 * @brief SFINAE test class used to know if the class has a toString method
 */
template <typename T> class has_to_string {
  using one = char;
  using two = std::array<char, 2>;

  template <typename C> static one test(decltype(&C::to_string));
  template <typename C> static two test(...);

public:
  enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

/**
 * @brief Syntaxic sugar for the has_toString SFINAE check
 */
template <typename T>
inline constexpr bool has_to_string_v = has_to_string<T>::value;

template <typename T, typename... Args>
[[noreturn]] constexpr void throw_exception(Args... args) {
  throw T(std::forward<Args>(args)...);
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return std::isprint(ch) && !std::isspace(ch);
          }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
  ltrim(s);
  return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
  rtrim(s);
  return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
  trim(s);
  return s;
}

class SwitchCLocale {
  std::string old_locale{};

public:
  SwitchCLocale() : old_locale{std::setlocale(LC_NUMERIC, nullptr)} {
    std::setlocale(LC_NUMERIC, "C");
  }
  ~SwitchCLocale() {
    if (!old_locale.empty())
      std::setlocale(LC_NUMERIC, old_locale.c_str());
  }
  SwitchCLocale(const SwitchCLocale &) = delete;
  SwitchCLocale(SwitchCLocale &&) = default;
  SwitchCLocale &operator=(const SwitchCLocale &) = delete;
  SwitchCLocale &operator=(SwitchCLocale &&) = default;
};

enum class LogLevel {
  Emergency,
  Alert,
  Critical,
  Error,
  Warning,
  Notice,
  Informational,
  Debugging
};

class LogBackend {
protected:
  LogBackend() {}
  LogBackend(const LogBackend &) = delete;
  LogBackend(LogBackend &&) = delete;
  LogBackend &operator=(const LogBackend &) = delete;
  LogBackend &operator=(LogBackend &&) = delete;

public:
  virtual ~LogBackend() noexcept = default;
  virtual void write(LogLevel, std::string_view) = 0;
};

class StdlogBackend : public LogBackend {
protected:
  StdlogBackend();

public:
  static StdlogBackend &instance() {
    static StdlogBackend sb;
    return sb;
  }
  virtual void write(LogLevel, std::string_view) override;
  virtual ~StdlogBackend() noexcept override = default;
};

template <LogLevel L> class Log;

class LogBase {
public:
  static LogLevel &level() {
    static LogLevel the_level{LogLevel::Notice};
    return the_level;
  }
  static LogBackend *&backend() {
    static LogBackend *the_backend = &StdlogBackend::instance();
    return the_backend;
  }

  template <LogLevel L2, typename T>
  friend Log<L2> &operator<<(Log<L2> &l, T &&v);
  template <LogLevel L2, typename T>
  friend Log<L2> &operator<<(Log<L2> &l, const T &v);
  template <LogLevel L2>
  friend Log<L2> &operator<<(Log<L2> &l, const std::wstring &v);

  static void setBackend(LogBackend *b) {
    if (!b)
      throw_exception<std::runtime_error>("Invalid log backend");
    backend() = b;
  }

  static void setLevel(LogLevel l) { level() = l; }
};

template <LogLevel L> class Log : public std::ostream, protected LogBase {
protected:
  std::basic_stringbuf<std::ostream::char_type> buffer;

  Log(const Log &) = delete;
  Log(Log &&) = delete;
  Log &operator=(const Log &) = delete;
  Log &operator=(Log &&) = delete;

public:
  using LogBase::setBackend;
  using LogBase::setLevel;

  Log() : std::ostream{&buffer} {}

  virtual ~Log() override {
    assert(backend());
    if (buffer.view().size())
      backend()->write(L, buffer.view());
  }
};

template <LogLevel L, typename T> Log<L> &operator<<(Log<L> &l, T &&v) {
  if (L <= LogBase::level()) {
    std::ostream &os{l};
    os << std::forward<T>(v);
  }
  return l;
}

template <LogLevel L, typename T> Log<L> &operator<<(Log<L> &l, const T &v) {
  if (L <= LogBase::level()) {
    std::ostream &os{l};

    if constexpr (std::is_enum_v<T>) {
      os << "enum(" << static_cast<typename std::underlying_type_t<T>>(v)
         << ')';
    } else
      os << v;
  }
  return l;
}

template <LogLevel L> Log<L> &operator<<(Log<L> &l, const std::wstring &v) {
  if (L <= LogBase::level()) {
    std::ostream &os{l};
    const std::locale locale("");
    typedef std::codecvt<wchar_t, char, std::mbstate_t> converter_type;
    const converter_type &converter = std::use_facet<converter_type>(locale);
    std::vector<char> to(v.length() * converter.max_length());
    std::mbstate_t state{};
    const wchar_t *from_next;
    char *to_next;
    const converter_type::result result =
        converter.out(state, v.data(), v.data() + v.length(), from_next, &to[0],
                      &to[0] + to.size(), to_next);
    if (result == converter_type::ok || result == converter_type::noconv) {
      const std::string s(&to[0], to_next);
      os << s;
    }
  }
  return l;
}

struct LogEmergency final : public Log<LogLevel::Emergency>
{};

struct LogAlert final : public Log<LogLevel::Alert>
{};

struct LogCritical final : public Log<LogLevel::Critical>
{};

struct LogError final : public Log<LogLevel::Error>
{};

struct LogWarning final : public Log<LogLevel::Warning>
{};

struct LogNotice final : public Log<LogLevel::Notice>
{};

struct LogInformational final : public Log<LogLevel::Informational>
{};

struct LogAlwaysDebug final : public Log<LogLevel::Debugging>
{};

struct NoLog final
{};

template <typename T>
NoLog &operator<<(NoLog &l, T &&)
{
  return l;
}

template <typename T>
NoLog &operator<<(NoLog &l, const T &)
{
  return l;
}

template <typename T>
NoLog &&operator<<(NoLog &&l, T &&)
{
  return std::forward<NoLog>(l);
}

template <typename T>
NoLog &&operator<<(NoLog &&l, const T &)
{
  return std::forward<NoLog>(l);
}

#ifndef NDEBUG
using LogDebugging = LogAlwaysDebug;
#else
using LogDebugging = NoLog;
#endif


/**
 * @brief Contains all the pair token/value for a given section
 *
 */
class ConfigurationSection {
  using Storage = std::map<std::string, std::string>;
  static inline char sep{','};
  static inline char escape{'\\'};

  static std::vector<std::string> cut_and_unescape(const std::string &list) {
    std::string toparse = sep + list + sep;
    std::vector<std::string> results;

    auto the_end = std::end(toparse);
    auto last = std::prev(the_end);
    auto start = std::begin(toparse);
    do {
      auto pos = std::find_if(std::next(start), the_end,
                              [is_escape = false](const auto &c) mutable {
                                if (is_escape) {
                                  is_escape = false;
                                  return false;
                                }
                                if (c == escape) {
                                  is_escape = true;
                                  return false;
                                }
                                return (c == sep);
                              });

      std::string res;
      std::istringstream{std::string{start, pos}} >>
          std::quoted(res, sep, escape);
      trim(res);
      start = pos;
      results.emplace_back(std::move(res));
    } while (start != last);

    return results;
  }

  static std::string escape_and_group(const std::vector<std::string> &list) {
    std::ostringstream oss;
    for (auto &i : list) {
      oss << std::quoted(i, sep, escape);
      oss.seekp(-1, oss.cur);
    }
    std::string results{oss.str()};
    results.erase(results.begin());
    results.erase(std::prev(results.end()));
    return results;
  }

  ConfigurationSection(const ConfigurationSection &other) = default;

  std::string name;

public:
  using iterator = Storage::iterator;
  using const_iterator = Storage::const_iterator;
  using size_type = Storage::size_type;
  using key_type = Storage::key_type;
  using mapped_type = Storage::mapped_type;
  using value_type = Storage::value_type;

  ConfigurationSection(std::string n) : name{std::move(n)} {}
  ConfigurationSection(ConfigurationSection &&other) = default;
  ConfigurationSection &operator=(const ConfigurationSection &other) = delete;
  ConfigurationSection &operator=(ConfigurationSection &&other) = default;
  ~ConfigurationSection() noexcept = default;

  ConfigurationSection copy() const { return *this; }
  const std::string getName() const { return name; }

  template <typename V, typename T,
            typename = std::enable_if_t<std::is_convertible_v<V, std::string> &&
                                        std::is_arithmetic_v<T>>>
  static bool convert_to_num(const V &value, T &val, T default_value = T{0},
                             int base = 10) {
    SwitchCLocale locale_keep;

    if constexpr (std::is_same_v<T, bool>) {
      std::istringstream ss{value};
      ss >> std::setbase(base) >> std::showbase >> std::boolalpha >> val;
      return false;
    } else {
      try {
        std::size_t pos{0};
        if constexpr (std::is_signed_v<T>) {
          if constexpr (sizeof(T) < sizeof(int)) {
            int vv = std::stoi(value, &pos, base);
            if (vv > std::numeric_limits<T>::max())
              throw_exception<std::out_of_range>("");
            if (vv < std::numeric_limits<T>::lowest())
              throw_exception<std::out_of_range>("");
            val = static_cast<T>(vv);
          } else if constexpr (std::is_same_v<T, int>) {
            val = std::stoi(value, &pos, base);
          } else if constexpr (std::is_same_v<T, long>) {
            val = std::stol(value, &pos, base);
          } else if constexpr (std::is_same_v<T, long long>) {
            val = std::stoll(value, &pos, base);
          } else if constexpr (std::is_same_v<T, float>) {
            val = std::stof(value, &pos);
          } else if constexpr (std::is_same_v<T, double>) {
            val = std::stod(value, &pos);
          } else if constexpr (std::is_same_v<T, long double>) {
            val = std::stold(value, &pos);
          } else
            throw_exception<std::logic_error>("");
        } else {
          if constexpr (sizeof(T) < sizeof(unsigned long)) {
            unsigned long vv = std::stoul(value, &pos, base);
            if (vv > std::numeric_limits<T>::max())
              throw_exception<std::out_of_range>("");
            val = static_cast<T>(vv);
          } else if constexpr (std::is_same_v<T, unsigned long> ||
                               (std::is_same_v<T, unsigned int> &&
                                sizeof(unsigned long) ==
                                    sizeof(unsigned int))) {
            val = std::stoul(value, &pos, base);
          } else if constexpr (std::is_same_v<T, unsigned long long>) {
            val = std::stoull(value, &pos, base);
          } else
            throw_exception<std::logic_error>("");
        }

        if constexpr (std::is_same_v<V, std::string>) {
          if (pos != value.length())
            throw_exception<std::invalid_argument>("");
        } else if constexpr (std::is_same_v<V, const char *> ||
                             std::is_same_v<V, char *>) {
          if (pos != std::char_traits<char>::length(value))
            throw_exception<std::invalid_argument>("");
        } else
          throw_exception<std::logic_error>("");
        return false;
      } catch (const std::invalid_argument &) {
      } catch (const std::out_of_range &) {
      } catch (...) {
      }

      val = default_value;
      return true;
    }
  }

  /**
   * @brief Access to the token. Create it if not found
   *
   * @param key the token to access
   * @return The value as string
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, std::string>>>
  [[nodiscard]] mapped_type &operator[](T &&key) {
    return store[std::forward<T>(key)];
  }

  /**
   * @brief Get the count of token
   *
   * @return return count of token
   */
  [[nodiscard]] size_type size() const { return store.size(); }

  /**
   * @brief Check if there is configuration elements
   *
   * @return true is there is no configuration, else false
   */
  [[nodiscard]] bool empty() const { return store.empty(); }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename... Args,
            typename =
                std::enable_if_t<std::is_convertible_v<const std::string &, T>>>
  [[nodiscard]] T get(const key_type &key, Args &&...args) const {
    auto itr = store.find(key);
    if (itr != end()) {
      return T{itr->second};
    }
    return T{std::forward<Args>(args)...};
  }

  bool rem(const key_type &key) {
    auto itr = store.find(key);
    if (itr == end()) {
      return false;
    }

    store.erase(itr);
    return true;
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @param f the conversion function
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename Func, typename... Args,
            typename = std::enable_if_t<
                std::is_invocable_r_v<T, Func, const std::string &>>>
  [[nodiscard]] T get(const key_type &key, Func &&f, Args &&...args) const {
    auto itr = store.find(key);
    if (itr != end()) {
      return f(itr->second);
    }
    return T{std::forward<Args>(args)...};
  }

  /**
   * @brief Return the content of the token converted to the given number type
   * If the token is not found, the default value is 0 if not specified
   *
   * @param key the token to access
   * @param default_value The default value if not found
   * @param base The base representation of the data. If 0, it will be guessed
   * @return The converted number
   */
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  [[nodiscard]] T get(const key_type &key, T default_value = T{0},
                      int base = 10) const {
    T val{default_value};
    auto itr = store.find(key);
    if (itr != end())
      convert_to_num(itr->second, val, default_value, base);
    return val;
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename = std::enable_if_t<
                            std::is_convertible_v<const std::string &, T>>>
  [[nodiscard]] std::vector<T> getVector(const key_type &key) const {
    auto itr = store.find(key);
    if (itr == end())
      return {};

    auto strings = cut_and_unescape(itr->second);
    std::vector<T> results;

    for (const auto &j : strings)
      results.emplace_back(j);

    return results;
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @param f the conversion function
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename Func,
            typename = std::enable_if_t<
                std::is_invocable_r_v<T, Func, const std::string &>>>
  [[nodiscard]] std::vector<T> getVector(const key_type &key, Func &&f) const {
    auto itr = store.find(key);
    if (itr == end())
      return {};

    auto strings = cut_and_unescape(itr->second);
    std::vector<T> results;

    for (const auto &j : strings)
      results.emplace_back(f(j));

    return results;
  }

  /**
   * @brief Return the content of the token converted to the given number type
   * If the token is not found, the default value is 0 if not specified
   *
   * @param key the token to access
   * @param base The base representation of the data. If 0, it will be guessed
   * @return The converted number
   */
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  [[nodiscard]] std::vector<T> getVector(const key_type &key,
                                         int base = 10) const {
    std::vector<T> results;

    auto itr = store.find(key);
    if (itr == end())
      return results;

    auto strings = cut_and_unescape(itr->second);

    for (const auto &j : strings) {
      T val{};
      if (convert_to_num(j, val, T{0}, base))
        continue;

      results.emplace_back(val);
    }

    return results;
  }

  /**
   * @brief define the token from the type converted to a string
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_assignable_v<std::string &, T> &&
                                !std::is_arithmetic_v<T>,
                            const std::string &>
  set(const key_type &key, const T &data) {
    auto &value = store[key];
    value = data;
    return value;
  }

  /**
   * @brief define the token from the type converted to a string with a member
   * toString fonction
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_toString_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  set(const key_type &key, const T &data) {
    auto &value = store[key];
    value = data.toString();
    return value;
  }

  /**
   * @brief define the token from the type converted to a string with a member
   * to_string fonction
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the object
   */
  template <typename T>
  typename std::enable_if_t<has_to_string_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  set(const key_type &key, const T &data) {
    auto &value = store[key];
    value = data.to_string();
    return value;
  }

  /**
   * @brief define the token from the given number
   *
   * @param key The key to create/access
   * @param data The number to be converted to a string
   * @param base The base representation of the data
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  set(const key_type &key, const T &data, int base = 10) {
    std::ostringstream ss;
    ss << std::setbase(base) << std::showbase << std::boolalpha << data;
    auto &value = store[key];
    value = ss.str();
    return value;
  }

  /**
   * @brief define the token from the given number
   *
   * @param key The key to create/access
   * @param data The number to be converted to a string
   * @param base The base representation of the data
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  set(const key_type &key, T &&data, int base = 10) {
    std::ostringstream ss;
    ss << std::setbase(base) << std::showbase << std::boolalpha
       << std::forward<T>(data);
    auto &value = store[key];
    value = ss.str();
    return value;
  }

  /**
   * @brief define the token from the type converted to a string
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_assignable_v<std::string &, T> &&
                                !std::is_arithmetic_v<T>,
                            const std::string &>
  setVector(const key_type &key, const std::vector<T> &data) {
    auto &value = store[key];
    std::vector<std::string> list;
    std::transform(std::begin(data), std::end(data), std::back_inserter(list),
                   [](const T &v) {
                     std::string str = v;
                     return str;
                   });
    value = escape_and_group(list);
    return value;
  }

  /**
   * @brief define the token from the type converted to a string with a member
   * toString fonction
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_toString_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  setVector(const key_type &key, const std::vector<T> &data) {
    auto &value = store[key];
    std::vector<std::string> list;
    std::transform(std::begin(data), std::end(data), std::back_inserter(list),
                   [](const T &v) {
                     std::string str = v.toString();
                     return str;
                   });
    value = escape_and_group(list);
    return value;
  }

  /**
   * @brief define the token from the type converted to a string with a member
   * to_string fonction
   *
   * @param key The key to create/access
   * @param data The type to convert to a string and assign to token
   * @return the string representation of the object
   */
  template <typename T>
  typename std::enable_if_t<has_to_string_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  setVector(const key_type &key, const std::vector<T> &data) {
    auto &value = store[key];
    std::vector<std::string> list;
    std::transform(std::begin(data), std::end(data), std::back_inserter(list),
                   [](const T &v) {
                     std::string str = v.to_string();
                     return str;
                   });
    value = escape_and_group(list);
    return value;
  }

  /**
   * @brief define the token from the given number
   *
   * @param key The key to create/access
   * @param data The number to be converted to a string
   * @param base The base representation of the data
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  setVector(const key_type &key, const std::vector<T> &data, int base = 10) {
    auto &value = store[key];
    std::vector<std::string> list;
    std::transform(std::begin(data), std::end(data), std::back_inserter(list),
                   [base](const T &v) {
                     std::ostringstream ss;
                     ss << std::setbase(base) << std::showbase << std::boolalpha
                        << v;
                     std::string str = ss.str();
                     return str;
                   });
    value = escape_and_group(list);
    return value;
  }

  /**
   * @brief get an iterator to the begining of the content
   *
   * @return an iterator
   */
  [[nodiscard]] iterator begin() { return store.begin(); }

  /**
   * @brief get an iterator to the end of the content
   *
   * @return an iterator
   */
  [[nodiscard]] iterator end() { return store.end(); }

  /**
   * @brief get an iterator to the begining of the content
   *
   * @return an iterator
   */
  [[nodiscard]] const_iterator begin() const { return store.begin(); }

  /**
   * @brief get an iterator to the end of the content
   *
   * @return an iterator
   */
  [[nodiscard]] const_iterator end() const { return store.end(); }

  /**
   * @brief get an iterator to the specified content
   *
   * @return an iterator
   */
  [[nodiscard]] const_iterator find(const key_type &key) const {
    return store.find(key);
  }

private:
  Storage store; /** The content of the section */
};

/**
 * @brief Hold a copy of a local and a global configuration file
 * The file is an "ini" file
 * No comments are allowed
 */
class Configuration {
  using Storage = std::map<std::string, ConfigurationSection>;
  struct Compare {
    bool operator()(const Storage::key_type *const &lhs,
                    const Storage::key_type *const &rhs) const {
      return *lhs < *rhs;
    }
  };

  std::set<const Storage::key_type *, Compare> get_key_list() const;

  ConfigurationSection no_section{""};

public:
  using iterator = Storage::iterator;
  using const_iterator = Storage::const_iterator;
  using size_type = Storage::size_type;
  using key_type = Storage::key_type;
  using mapped_type = Storage::mapped_type;
  enum class destination { local, global };

  /**
   * @brief empty configuration
   */
  explicit Configuration() = default;
  /**
   * @brief read the content of the given source as local configuration
   */
  explicit Configuration(std::istream &localFile);

  /**
   * @brief read the content of the given sources as local and global
   * configuration
   */
  Configuration(std::istream &localFile, std::istream &globalFile);

  explicit Configuration(ConfigurationSection &&);

  Configuration(const Configuration &other) = delete;
  Configuration(Configuration &&other) noexcept = default;
  Configuration &operator=(const Configuration &other) = delete;
  Configuration &operator=(Configuration &&other) noexcept = default;
  ~Configuration() noexcept = default;

  const ConfigurationSection &get_no_section() const { return no_section; }

  /**
   * @brief write the content of the local configuration to the given
   * destination
   */
  void serialize(std::ostream &localFile);

  /**
   * @brief write the content of the local and global configuration to the given
   * destinations
   */
  void serialize(std::ostream &localFile, std::ostream &globalFile);

  /**
   * @brief Get the count of unique section in both local and global
   * configuration
   *
   * @return the count of sections
   */
  [[nodiscard]] size_type size() const;

  /**
   * @brief Check if the configuration is empty;
   *
   * @return true if empty, else false
   */
  [[nodiscard]] bool empty() const {
    return local_store.empty() && global_store.empty();
  }

  /**
   * @brief Get the list of section currently available in the configuration
   *
   * @return the set of section names
   */
  [[nodiscard]] std::set<std::string> names() const;

  /**
   * @brief Access to the given section. Create it if not found
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] mapped_type &operator[](T &&k) {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item->second;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item->second;
    // create it
    return local_store.emplace(key, key).first->second;
  }

  /**
   * @brief Access to the given section. Create it if not found
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] const mapped_type &operator[](T &&k) const {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item->second;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item->second;
    return noconf;
  }

  /**
   * @brief Access to the given section.
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] mapped_type &at(T &&k) {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item->second;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item->second;
    return local_store.at(key);
  }

  /**
   * @brief Access to the given section. Create it if not found
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] const mapped_type &at(T &&k) const {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item->second;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item->second;
    return noconf;
  }

  /**
   * @brief Access to the given section. Create it if not found
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] iterator emplace(T &&k, destination d = destination::local) {
    key_type key{std::forward<T>(k)};
    if (d == destination::local) {
      return local_store.emplace(key, key).first;
    } else if (d == destination::global) {
      return global_store.emplace(key, key).first;
    }
    throw_exception<std::runtime_error>(
        "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  iterator end() { return std::end(local_store); }

  const_iterator end() const { return std::end(local_store); }

  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] iterator find(T &&k) {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item;
    return end();
  }

  /**
   * @brief Access to the given section. Create it if not found
   *
   * @param k The name to access
   * @return The section as ConfigurationSection
   */
  template <typename T,
            typename = std::enable_if_t<std::is_convertible_v<T, key_type>>>
  [[nodiscard]] const_iterator find(T &&k) const {
    key_type key{std::forward<T>(k)};
    auto local_item = local_store.find(key);
    if (local_item != std::end(local_store))
      return local_item;
    auto global_item = global_store.find(key);
    if (global_item != std::end(global_store))
      return global_item;
    return end();
  }

  /**
   * @brief Helper function to load a file and parse it as local configuration
   *
   * @param localFile The local file name
   * @return The parsed file
   */
  template <typename T, typename = std::enable_if_t<
                            std::is_convertible_v<T, std::filesystem::path>>>
  [[nodiscard]] static Configuration from_file(T &&localFile,
                                               bool *no_file = nullptr) {
    std::filesystem::path local_file_path{std::forward<T>(localFile)};
    std::ifstream local_stream{local_file_path};
    if (!local_stream.is_open()) {
      LogWarning{} << "Unable to open configuration file " << local_file_path;
      if (no_file)
        *no_file = true;
    } else {
      if (no_file)
        *no_file = false;
    }

    return Configuration{local_stream};
  }

  /**
   * @brief Helper function to load two files and parse it as local and global
   * configuration
   *
   * @param localFile The local file name
   * @param globalFile The global file name
   * @return The parsed files
   */
  template <typename T, typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<T, std::filesystem::path> &&
                std::is_convertible_v<U, std::filesystem::path>>>
  [[nodiscard]] static Configuration from_file(T &&localFile, U &&globalFile) {
    std::filesystem::path local_file_path{std::forward<T>(localFile)};
    std::filesystem::path global_file_path{std::forward<U>(globalFile)};
    std::ifstream local_stream{local_file_path};
    std::ifstream global_stream{global_file_path};
    if (!local_stream.is_open())
      LogWarning{} << "Unable to open configuration file " << local_file_path;
    if (!global_stream.is_open())
      LogWarning{} << "Unable to open configuration file " << global_file_path;
    return Configuration{local_stream, global_stream};
  }

  /**
   * @brief Helper function to write a file as local configuration
   *
   * @param localFile The local file name
   */
  template <typename T, typename = std::enable_if_t<
                            std::is_convertible_v<T, std::filesystem::path>>>
  static bool to_file(const Configuration &c, T &&localFile) {
    std::ofstream local_stream{
        static_cast<std::filesystem::path>(std::forward<T>(localFile))};
    if (!local_stream.is_open() || !local_stream.good()) {
      return false;
    }
    write(c.local_store, local_stream);
    return true;
  }

  /**
   * @brief Helper function to write two files as local and global configuration
   *
   * @param localFile The local file name
   * @param globalFile The global file name
   */
  template <typename T, typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<T, std::filesystem::path> &&
                std::is_convertible_v<U, std::filesystem::path>>>
  static bool to_file(const Configuration &c, T &&localFile, U &&globalFile) {
    std::ofstream local_stream{std::forward<T>(localFile)};
    if (!local_stream.is_open() || !local_stream.good()) {
      return false;
    }
    std::ofstream global_stream{std::forward<U>(globalFile)};
    if (!global_stream.is_open() || !global_stream.good()) {
      return false;
    }
    write(c.global_store, global_stream);
    write(c.local_store, local_stream);
    return true;
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename... Args,
            typename =
                std::enable_if_t<std::is_convertible_v<const std::string &, T>>>
  [[nodiscard]] T get(const key_type &section, const key_type &key,
                      Args &&...args) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.get<T>(key);
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.get<T>(key);
    return T{std::forward<Args>(args)...};
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @param f the conversion function
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename Func, typename... Args,
            typename = std::enable_if_t<
                std::is_invocable_r_v<T, Func, const std::string &>>>
  [[nodiscard]] T get(const key_type &section, const key_type &key, Func &&f,
                      Args &&...args) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.get<T>(key, std::forward<Func>(f));
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.get<T>(key, std::forward<Func>(f));
    return T{std::forward<Args>(args)...};
  }

  /**
   * @brief Return the content of the token converted to the given number type
   * If the token is not found, the default value is 0 if not specified
   *
   * @param key the token to access
   * @param default_value The default value if not found
   * @param base The base representation of the data. If 0, it will be guessed
   * @return The converted number
   */
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  [[nodiscard]] T get(const key_type &section, const key_type &key,
                      T default_value = T{0}, int base = 10) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.get<T>(key, default_value, base);
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.get<T>(key, default_value, base);
    return default_value;
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename = std::enable_if_t<
                            std::is_convertible_v<const std::string &, T>>>
  [[nodiscard]] std::vector<T> getVector(const key_type &section,
                                         const key_type &key) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.getVector<T>(key);
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.getVector<T>(key);
    return {};
  }

  /**
   * @brief Return the content of the token converted to the given class
   * If the token is not found, the type created with is default constructor
   *
   * @param key the token to access
   * @param f the conversion function
   * @return T constructed with the given value or default constructed
   */
  template <typename T, typename Func,
            typename = std::enable_if_t<
                std::is_invocable_r_v<T, Func, const std::string &>>>
  [[nodiscard]] std::vector<T> getVector(const key_type &section,
                                         const key_type &key, Func &&f) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.getVector<T>(key, std::forward<Func>(f));
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.getVector<T>(key, std::forward<Func>(f));
    return {};
  }

  /**
   * @brief Return the content of the token converted to the given number type
   * If the token is not found, the default value is 0 if not specified
   *
   * @param key the token to access
   * @param default_value The default value if not found
   * @param base The base representation of the data. If 0, it will be guessed
   * @return The converted number
   */
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  [[nodiscard]] std::vector<T>
  getVector(const key_type &section, const key_type &key, int base = 10) const {
    auto local_item = local_store.find(section);
    if (local_item != std::end(local_store))
      return local_item->second.getVector<T>(key, base);
    auto global_item = global_store.find(section);
    if (global_item != std::end(global_store))
      return global_item->second.getVector<T>(key, base);
    return {};
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_assignable_v<std::string &, T> &&
                                !std::is_arithmetic_v<T>,
                            const std::string &>
  set(const key_type &section, const ConfigurationSection::key_type &key,
      const T &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    }
    throw_exception<std::runtime_error>(
        "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_toString_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  set(const key_type &section, const ConfigurationSection::key_type &key,
      const T &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_to_string_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  set(const key_type &section, const ConfigurationSection::key_type &key,
      const T &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @param base Write the number to the given base
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  set(const key_type &section, const ConfigurationSection::key_type &key,
      const T &data, destination d = destination::local, int base = 10) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data, base);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, data, base);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @param base Write the number to the given base
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  set(const key_type &section, const ConfigurationSection::key_type &key,
      T &&data, destination d = destination::local, int base = 10) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, std::forward<T>(data), base);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.set(key, std::forward<T>(data), base);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_assignable_v<std::string &, T> &&
                                !std::is_arithmetic_v<T>,
                            const std::string &>
  setVector(const key_type &section, const ConfigurationSection::key_type &key,
            const std::vector<T> &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    }
    throw_exception<std::runtime_error>(
        "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_toString_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  setVector(const key_type &section, const ConfigurationSection::key_type &key,
            const std::vector<T> &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<has_to_string_v<T> &&
                                !std::is_assignable_v<std::string &, T>,
                            const std::string &>
  setVector(const key_type &section, const ConfigurationSection::key_type &key,
            const std::vector<T> &data, destination d = destination::local) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

  /**
   * @brief Define or reset the value to the given value
   *
   * @param section The section name
   * @param key The token
   * @param data The data to set
   * @param d Where to put the value
   * @param base Write the number to the given base
   * @return The string representation of the data
   */
  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T>, const std::string &>
  setVector(const key_type &section, const ConfigurationSection::key_type &key,
            const std::vector<T> &data, destination d = destination::local,
            int base = 10) {
    if (d == destination::local) {
      auto s = local_store.find(section);
      if (s == std::end(local_store))
        s = local_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data, base);
    } else if (d == destination::global) {
      auto s = global_store.find(section);
      if (s == std::end(global_store))
        s = global_store.emplace(section, section).first;
      auto &sec = s->second;
      return sec.setVector(key, data, base);
    } else
      throw_exception<std::runtime_error>(
          "Invalid Configuration destination"); // LCOV_EXCL_LINE
  }

protected:
  static void parse(ConfigurationSection *no_section, Storage &store,
                    std::istream &);
  static void write(const Storage &store, std::ostream &);

  Storage local_store;
  Storage global_store;
  static const mapped_type noconf;
};

std::ostream &operator<<(std::ostream &stream, const Configuration &conf);

#endif // CONFIGURATION_H
