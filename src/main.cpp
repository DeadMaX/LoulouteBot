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
              new_guild_config.set("goodby_channel", i.first.str());
              Configuration::to_file(guilds_config, g_config_file);
              return callback(guild_id, i.first);
            }
          }
        });
  }

  void clear_guild_goodbye_channel(dpp::snowflake guild_id) {
    guilds_config.set(guild_id.str(), "goodbye_channel", "0");
  }
};

struct GlobalCommand {
  std::string_view help;
  void (*handle)(dpp::cluster &, const dpp::slashcommand_t &);
  std::vector<dpp::command_option> options;

  bool registered{false};

  void operator()(dpp::cluster &b, const dpp::slashcommand_t &e) {
    handle(b, e);
  }

  template <size_t N>
  GlobalCommand(const char (&str)[N],
                void (*h)(dpp::cluster &, const dpp::slashcommand_t &))
      : help{str, N - 1}, handle{h} {}

  template <size_t N>
  GlobalCommand(const char (&str)[N],
                void (*h)(dpp::cluster &, const dpp::slashcommand_t &),
                std::initializer_list<dpp::command_option> l)
      : help{str, N - 1}, handle{h}, options{l} {}
};

static void global_help(dpp::cluster &, const dpp::slashcommand_t &event);
static void global_setup(dpp::cluster &, const dpp::slashcommand_t &event);
static void global_test(dpp::cluster &, const dpp::slashcommand_t &event);
static std::unordered_map<std::string, GlobalCommand> g_global_commands{
    {"help", {"Au secours!", &global_help}},
    {"test",
     {"Test une action",
      &global_test,
      {{dpp::co_string, "action", "l'action a tester", true}}}},
    {"setup", {"Configuration (Admin)", &global_setup}},
};

static GuildConfig g_guild_configs;

static void global_help(dpp::cluster &, const dpp::slashcommand_t &event) {
  std::ostringstream oss;
  oss << R"string(Ne te noie pas !
Voici la liste des commandes disponibles:)string";
  for (auto &i : g_global_commands) {
    oss << "\n- /" << i.first << ": " << i.second.help;
  }
  event.reply(oss.str());
}

static void global_test(dpp::cluster &, const dpp::slashcommand_t &event) {
  auto v = event.get_parameter("action");
  auto u = event.command.get_issuing_user();
  LogInformational{} << "Action " << std::get<std::string>(v)
                     << " a tester par " << u.username;
  std::ostringstream oss;
  oss << "C'est dl'a balle, n'est ce pas @" << u.username;
  event.reply(oss.str());
}

static void global_setup(dpp::cluster &, const dpp::slashcommand_t &event) {
  event.reply("Pas encore prêt");
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
        if (i.second.name == j.first) {
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
        for (const auto &j : i.second.options) {
          c.add_option(j);
        }
        bot.global_command_create(c);
      }
    }
  });
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

  bot.start(dpp::st_wait);
}