#include "configuration.h"
#include <dpp/dpp.h>

#include <concepts>

#ifndef BOT_TOKEN
#error Pas de token de bot defini
#endif

std::filesystem::path g_config_file{"config.ini"};

class GuildConfig {
  Configuration guilds_config;

public:
  GuildConfig() = default;

  Configuration &operator=(Configuration &&lhs) {
    guilds_config = std::forward<Configuration>(lhs);
    return guilds_config;
  }

  template <class F>
    requires std::invocable<F, dpp::snowflake, dpp::snowflake>
  void get_guild_goodbye_channel(dpp::cluster &bot, dpp::snowflake guild_id,
                                 F &&callback) {
    const auto &guild_config = guilds_config[guild_id.str()];

    auto channel_id = guild_config.get<dpp::snowflake>("goodbye_channel", "0");

    if (!channel_id.empty()) {
      return callback(guild_id, channel_id);
    }

    bot.channels_get(
        guild_id, [this, guild_id, callback = std::forward<F>(callback)](
                      const dpp::confirmation_callback_t &ccb) {
          if (ccb.is_error()) {
            return dpp::utility::log_error()(ccb);
          }
          const auto &channels = ccb.get<dpp::channel_map>();
          for (auto &i : channels) {
            if (i.second.get_type() == dpp::CHANNEL_TEXT) {
              auto &new_guild_config = guilds_config[guild_id.str()];
              new_guild_config.set("goodbye_channel", i.first.str());
              Configuration::to_file(guilds_config, g_config_file);
              return callback(guild_id, i.first);
            }
          }
        });
  }

  void clear_guild_goodbye_channel(dpp::snowflake guild_id) {
    guilds_config.set(guild_id.str(), "goodbye_channel", "0");
    Configuration::to_file(guilds_config, g_config_file);
  }

  void set_guild_charte_message(dpp::snowflake guild_id, dpp::snowflake channel,
                                dpp::snowflake message) {
    auto &c = guilds_config[guild_id.str()];
    c.set("charte_channel", channel.str());
    c.set("charte_message", message.str());

    Configuration::to_file(guilds_config, g_config_file);
  }

  void set_guild_charte_reaction_valider(dpp::snowflake guild_id,
                                         const std::string &reaction) {
    auto &c = guilds_config[guild_id.str()];
    c.set("charte_reaction_valider", reaction);

    Configuration::to_file(guilds_config, g_config_file);
  }

  void set_guild_charte_role(dpp::snowflake guild_id, const std::string &role) {
    auto &c = guilds_config[guild_id.str()];
    c.set("charte_role", role);

    Configuration::to_file(guilds_config, g_config_file);
  }

  std::string get_guild_charte_role(dpp::snowflake guild_id) const {
    auto &c = guilds_config[guild_id.str()];
    return c.get<std::string>("charte_role", "");
  }

  std::string get_guild_charte_reaction_valider(dpp::snowflake guild_id) const {
    auto &c = guilds_config[guild_id.str()];
    return c.get<std::string>("charte_reaction_valider");
  }

  std::pair<std::string, std::string>
  get_guild_charte_message(dpp::snowflake guild_id) const {
    std::pair<std::string, std::string> res;
    auto &c = guilds_config[guild_id.str()];
    res.first = c.get<std::string>("charte_channel");
    res.second = c.get<std::string>("charte_message");
    return res;
  }
};

template <typename T, typename U> struct default_second {
  std::pair<T, U> value;
  operator std::pair<T, U> &() { return value; }
  operator const std::pair<T, U> &() const { return value; }

  default_second(const T &left) : value{left, U{}} {}
  default_second(const T &left, const U &right) : value{left, right} {}
};

struct GlobalCommand {
  std::string_view help;
  void (*handle)(dpp::cluster &, const dpp::slashcommand_t &);

  std::vector<dpp::command_option> options;
  dpp::permission permissions;
  bool registered{false};

  void operator()(dpp::cluster &b, const dpp::slashcommand_t &e) {
    handle(b, e);
  }

  template <size_t N>
  GlobalCommand(
      const char (&str)[N],
      void (*h)(dpp::cluster &, const dpp::slashcommand_t &),
      std::initializer_list<
          default_second<dpp::command_option,
                         std::initializer_list<dpp::command_option_choice>>>
          l = {},
      dpp::permission p = dpp::p_use_application_commands)
      : help{str, N - 1}, handle{h}, permissions{p} {
    options.reserve(l.size());
    std::ranges::transform(
        l, std::back_inserter(options),
        [](const default_second<
            dpp::command_option,
            std::initializer_list<dpp::command_option_choice>> &p)
            -> dpp::command_option {
          auto res{p.value.first};
          res.choices.reserve(p.value.second.size());
          std::ranges::transform(
              p.value.second, std::back_inserter(res.choices),
              [](const dpp::command_option_choice &c)
                  -> dpp::command_option_choice { return c; });
          return res;
        });
  }
};

static void global_help(dpp::cluster &, const dpp::slashcommand_t &event);
static void global_setup(dpp::cluster &, const dpp::slashcommand_t &event);
static void global_test(dpp::cluster &, const dpp::slashcommand_t &event);
static std::unordered_map<std::string, GlobalCommand> g_global_commands{
    {"help", {"Au secours!", &global_help}},
    {"test",
     {"Test une action",
      &global_test,
      {{{dpp::co_string, "action", "l'action a tester", true},
        {{"Envoyer goodbye", "goodbye"}}},
       {{dpp::co_string, "param", "paramètre de l'action", true}}},
      0}},
    {"setup",
     {"Configuration (Admin)",
      &global_setup,
      {{{dpp::co_string, "param", "Paramètre a modifier", true}, {}},
       {{dpp::co_string, "value", "Valeur a définir", true}}},
      dpp::p_administrator}},
};

static GuildConfig g_guild_configs;

static void global_help(dpp::cluster &, const dpp::slashcommand_t &event) {
  std::ostringstream oss;
  oss << R"string(Ne te noie pas !
Voici la liste des commandes disponibles:)string";
  for (auto &i : g_global_commands) {
    oss << "\n- /" << i.first << ": " << i.second.help;
    for (auto &j : i.second.options)
      oss << "\n - /" << j.name << ": " << j.description;
  }
  event.reply(oss.str());
}

static void global_setup(dpp::cluster &bot, const dpp::slashcommand_t &event) {
  auto value = event.get_parameter("value");
  auto param = event.get_parameter("param");
  auto value_str = std::get_if<std::string>(&value);
  auto param_str = std::get_if<std::string>(&param);

  if (!value_str || !param_str)
    return event.reply("Même pas en rêve !");

  if (*param_str == "charte_role") {

    if (value_str->empty())
      return event.reply("Pas de role donné !");

    return event.thinking(true, [&bot, event, name = *value_str](
                                    const dpp::confirmation_callback_t &ccb) {
      if (ccb.is_error()) {
        return event.edit_original_response(dpp::message("Erreur"));
      }

      bot.roles_get(
          event.command.guild_id,
          [event, name](const dpp::confirmation_callback_t &callback) {
            if (callback.is_error()) {
              event.edit_original_response(dpp::message("Role non trouvé"));
              LogError{} << "role non trouvé: " << callback.get_error().message;
              return;
            }

            const auto &m = callback.get<dpp::role_map>();

            auto r = std::ranges::find_if(
                m, [&name](const auto &r) { return r.second.name == name; });

            if (r == end(m)) {
              event.edit_original_response(dpp::message("Role non trouvé"));
              LogError{} << "role non trouvé: " << callback.get_error().message;
              return;
            }

            g_guild_configs.set_guild_charte_role(event.command.guild_id,
                                                  r->first.str());
            return event.edit_original_response(dpp::message("Okay"));
          });
    });

  } else if (*param_str == "charte_reaction_valider") {

    if (value_str->empty())
      return event.reply("Pas de réaction donné !");

    g_guild_configs.set_guild_charte_reaction_valider(event.command.guild_id,
                                                      *value_str);

    return event.reply("Okay");
  } else if (*param_str == "charte_message") {

    auto split{*value_str | std::views::split('/') |
               std::views::transform([](auto r) {
                 return std::string_view{r.data(), r.size()};
               })};
    auto v = std::vector<std::string_view>{split.begin(), split.end()};

    if (v.size() < 3) {
      return event.reply("Pas une url");
    }

    if (event.command.guild_id.str() != v[v.size() - 3]) {
      return event.reply("Pas pour ce serveur");
    }

    return event.thinking(true, [&bot, event,
                                 chan = std::string{v[v.size() - 2]},
                                 mess = std::string{v[v.size() - 1]}](
                                    const dpp::confirmation_callback_t &ccb) {
      if (ccb.is_error()) {
        return event.edit_original_response(dpp::message("Erreur"));
      }

      bot.message_get(
          mess, chan,
          [event, mess, chan](const dpp::confirmation_callback_t &callback) {
            if (callback.is_error()) {
              event.edit_original_response(dpp::message("message non trouvé"));
              LogError{} << "message non trouvé: "
                         << callback.get_error().message;
              return;
            }

            const auto &m = callback.get<dpp::message>();

            if (m.reactions.empty()) {
              return event.edit_original_response(
                  dpp::message("Pas de réaction trouvé"));
            }

            auto reaction_valider =
                g_guild_configs.get_guild_charte_reaction_valider(
                    event.command.guild_id);

            if (std::ranges::find_if(
                    m.reactions, [&reaction_valider](const dpp::reaction &r) {
                      return r.emoji_name == reaction_valider;
                    }) == end(m.reactions)) {
              return event.edit_original_response(
                  dpp::message("Réaction de validation non trouvé"));
            }

            g_guild_configs.set_guild_charte_message(event.command.guild_id,
                                                     chan, mess);
            event.edit_original_response(dpp::message("Effectué"));
          });
    });

  } else {
    return event.reply("paramètre inconnu");
  }
  event.reply("Effectué");
}

static void send_goodbye(dpp::cluster &bot,
                         const dpp::guild_member_remove_t &event) {
  g_guild_configs.get_guild_goodbye_channel(
      bot, event.guild_id,
      [&bot, event](dpp::snowflake guild_id,
                    dpp::snowflake goodbye_channel_id) {
        std::ostringstream oss;
        oss << "Bye bye on t'aimait bien " << event.removed.username;
        auto msg =
            dpp::message(oss.str()).set_guild_id(guild_id).set_channel_id(
                goodbye_channel_id);
        bot.message_create(msg, [&bot, guild_id, event](
                                    const dpp::confirmation_callback_t &ccb) {
          if (!ccb.is_error())
            return;
          g_guild_configs.clear_guild_goodbye_channel(guild_id);
          send_goodbye(bot, event);
        });
      });
}

static void register_bot(dpp::cluster &bot) {

  bot.global_commands_get([&bot](const dpp::confirmation_callback_t &ccb) {
    if (ccb.is_error()) {
      return dpp::utility::log_error()(ccb);
    }
    const auto &list = ccb.get<dpp::slashcommand_map>();
    for (auto &i : list) {
      LogInformational{} << "Global command " << i.second.name << " is set";
      bool is_ok{false};
      for (auto &j : g_global_commands) {
        if (i.second.name != j.first)
          continue;

        if (j.second.permissions != i.second.default_member_permissions)
          break;

        if (j.second.options.size() != i.second.options.size())
          break;

        std::vector<bool> option_found;
        option_found.resize(j.second.options.size(), false);

        for (size_t idx = 0; auto &k : i.second.options) {
          bool option_ok{false};
          for (auto &l : j.second.options) {
            if (k.name != l.name)
              continue;

            if (k.choices.size() != l.choices.size())
              break;

            std::vector<bool> choice_found;
            choice_found.resize(l.choices.size(), false);
            for (size_t choise_idx = 0; auto &m : l.choices) {
              bool choice_ok{false};
              for (auto &n : k.choices) {
                if (m.name != n.name)
                  continue;

                choice_ok = true;
                break;
              }

              if (!choice_ok)
                break;

              choice_found[choise_idx] = true;
              ++choise_idx;
            }

            if (std::ranges::find(choice_found, false) == end(choice_found))
              option_ok = true;
            break;
          }
          if (!option_ok) {
            break;
          }
          option_found[idx] = true;
          ++idx;
        }
        if (std::ranges::find(option_found, false) == end(option_found)) {
          j.second.registered = true;
          is_ok = true;
          LogInformational{} << "Keep";
          break;
        }
      }
      if (!is_ok) {
        LogInformational{} << "Delete";
        bot.global_command_delete(i.first);
      }
    }

    for (auto &i : g_global_commands) {
      if (!i.second.registered) {
        LogInformational{} << "Create Global command " << i.first;
        auto c = dpp::slashcommand{
            i.first, {i.second.help.begin(), i.second.help.end()}, bot.me.id};
        c.set_default_permissions(i.second.permissions);
        for (const auto &j : i.second.options) {
          c.add_option(j);
        }
        bot.global_command_create(c);
      }
    }
  });
}

static void global_test(dpp::cluster &bot, const dpp::slashcommand_t &event) {
  auto action = event.get_parameter("action");
  auto param = event.get_parameter("param");
  auto action_str = std::get_if<std::string>(&action);
  auto param_str = std::get_if<std::string>(&param);

  if (!action_str || !param_str)
    return event.reply("Même pas en rêve !");

  auto u = event.command.get_issuing_user();

  if (*action_str == "goodbye") {
    auto ev = dpp::guild_member_remove_t();
    ev.guild_id = event.command.guild_id;
    ev.removed.username = *param_str;
    send_goodbye(bot, ev);
  } else {
    LogError{} << "Action " << *action_str << " inconnue";
    return event.reply("Action inconnu");
  }

  event.reply("Effectué");
}

int main(int argc, char *const argv[]) {

  LogBase::setLevel(LogLevel::Debugging);

  if (argc > 1)
    g_config_file = argv[1];

  g_guild_configs = Configuration::from_file(g_config_file);

  dpp::cluster bot(BOT_TOKEN);

  bot.on_log([](const dpp::log_t &l) {
    switch (l.severity) {
    case dpp::ll_trace:
      [[fallthrough]];

    case dpp::ll_debug:
      LogDebugging{} << l.message;
      break;

    case dpp::ll_info:
      LogInformational{} << l.message;
      break;

    case dpp::ll_warning:
      LogWarning{} << l.message;
      break;

    case dpp::ll_error:
      LogError{} << l.message;
      break;

    case dpp::ll_critical:
      [[fallthrough]];
    default:
      LogCritical{} << l.message;
      break;
    }
  });

  bot.on_slashcommand([&bot](const dpp::slashcommand_t &event) {
    auto command = event.command.get_command_name();
    auto idx = g_global_commands.find(command);
    if (idx == std::end(g_global_commands))
      return;
    idx->second(bot, event);
  });

  bot.on_guild_member_remove([&bot](const dpp::guild_member_remove_t &event) {
    send_goodbye(bot, event);
  });

  bot.on_ready([&bot](const dpp::ready_t &) {
    if (dpp::run_once<struct register_bot_commands>()) {
      register_bot(bot);
    }
  });

  bot.on_message_reaction_add([&bot](const dpp::message_reaction_add_t &event) {
    if (!event.reacting_guild) {
      LogError{} << "Pas de guild";
      return;
    }
    auto [chan, mess] =
        g_guild_configs.get_guild_charte_message(event.reacting_guild->id);
    if (chan != event.channel_id.str() || mess != event.message_id.str()) {
      LogError{} << "Pas le bon message";
      return;
    }

    auto emoji = g_guild_configs.get_guild_charte_reaction_valider(
        event.reacting_guild->id);
    if (emoji != event.reacting_emoji.name) {
      LogError{} << "Pas le bon emoji: " << emoji << " <=> "
                 << event.reacting_emoji.name;
      return;
    }

    auto r = g_guild_configs.get_guild_charte_role(event.reacting_guild->id);

    bot.guild_member_add_role(
        event.reacting_guild->id, event.reacting_user.id, r,
        [event](const dpp::confirmation_callback_t &ccb) {
          if (ccb.is_error()) {
            return dpp::utility::log_error()(ccb);
          }
          LogError{} << "User accepté: " << event.reacting_user.username;
        });
  });

  bot.start(dpp::st_wait);
}
