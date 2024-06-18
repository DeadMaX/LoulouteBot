/*
 * Copyright (C) 2018-2024 MC2 Technologies - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Alexandre Martins <alexandre.martins@mc2-technologies.com>
 */

#include "configuration.h"

#include <iostream>
#include <set>

using namespace std;

const Configuration::mapped_type Configuration::noconf{""};

Configuration::Configuration(std::istream &localFile, std::istream &globalFile)
{
  parse(&no_section, global_store, globalFile);
  parse(&no_section, local_store, localFile);
}

Configuration::Configuration(std::istream &localFile)
{
  parse(&no_section, local_store, localFile);
}

void Configuration::serialize(std::ostream &localFile)
{
  write(local_store, localFile);
}

void Configuration::serialize(std::ostream &localFile, std::ostream &globalFile)
{
  write(global_store, globalFile);
  write(local_store, localFile);
}

void Configuration::parse(ConfigurationSection *no_section,
                               Storage &store,
                               std::istream &stream)
{
  auto current_section = no_section;
  while (!stream.eof())
  {
    string line;

    // read line
    if (!getline(stream, line))
    {
      break;
    }

    // trim white space
    trim(line);

    if (line.empty())
    {
      continue;
    }

    if (*std::begin(line) == '[' && *rbegin(line) == ']')
    {
      // change current section
      line.pop_back();
      line.erase(std::begin(line));
      auto l = store.find(line);
      if (l == std::end(store))
      {
        auto n = store.emplace(line, line);
        current_section = &n.first->second;
      }
      else
      {
        current_section = &l->second;
      }
      continue;
    }

    auto it = std::find_if(std::begin(line), std::end(line), [](int ch) { return ch == '='; });

    if (it == std::end(line))
    {
      continue;
    }
    string key{std::begin(line), it};
    string value{it + 1, std::end(line)};
    trim(key);
    trim(value);
    if (!value.empty())
    {
      current_section->operator[](key) = value;
    }
  }
}

void Configuration::write(const Storage &store, std::ostream &stream)
{
  for (auto &i : store)
  {
    bool first_line{false};
    for (auto &j : i.second)
    {
      if (!j.second.empty())
      {
        if (!first_line)
        {
          stream << '[' << i.first << "]\n";
        }
        first_line = true;
        stream << j.first << " = " << j.second << '\n';
      }
    }
    stream << '\n';
  }
}

std::set<const Configuration::Storage::key_type *, Configuration::Compare>
Configuration::get_key_list() const
{
  std::set<const Storage::key_type *, Compare> list;
  for (auto &i : local_store)
  {
    list.emplace(&i.first);
  }

  for (auto &i : global_store)
  {
    list.emplace(&i.first);
  }

  return list;
}

Configuration::size_type Configuration::size() const
{
  return get_key_list().size();
}

std::set<std::string> Configuration::names() const
{
  auto list = get_key_list();
  std::set<std::string> result;

  std::transform(std::begin(list),
                 std::end(list),
                 std::inserter(result, std::end(result)),
                 [](const std::string *value) -> std::string { return *value; });
  return result;
}

std::ostream &operator<<(std::ostream &stream, const Configuration &conf)
{
  std::set<std::string> section_name = conf.names();

  std::set<std::string>::iterator itLoop = section_name.begin();
  while (itLoop != section_name.end())
  {
    const ConfigurationSection &section = conf[*itLoop];

    stream << "[" << *itLoop << "]\n";

    std::map<std::string, std::string>::const_iterator itLoopValues = section.begin();
    while (itLoopValues != section.end())
    {
      stream << itLoopValues->first << " = " << itLoopValues->second << "\n";

      itLoopValues++;
    }
    stream << "\n";

    itLoop++;
  }
  return stream;
}
