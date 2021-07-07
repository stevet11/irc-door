
#include <chrono>
#include <iostream>
#include <string>
#include <thread> // sleep_for

#include "door.h"
#include "input.h"
#include "irc.h"
#include "render.h"
#include "yaml-cpp/yaml.h"

#include <boost/asio.hpp>
// #include <boost/thread.hpp>

YAML::Node config;
std::function<std::ofstream &(void)> get_logger;

bool file_exists(const std::string name) {
  std::ifstream f(name.c_str());
  return f.good();
}

int main(int argc, char *argv[]) {
  using namespace std::chrono_literals;

  boost::asio::io_context io_context;
  ircClient irc(io_context);

  door::Door door("irc-door", argc, argv);
  get_logger = [&door]() -> ofstream & { return door.log(); };

  if (file_exists("irc-door.yaml")) {
    config = YAML::LoadFile("irc-door.yaml");
  }

  bool update_config = false;

  if (!config["hostname"]) {
    config["hostname"] = "127.0.0.1";
    update_config = true;
  }

  if (!config["port"]) {
    config["port"] = "6697";
    update_config = true;
  }

  if (!config["allow_join"]) {
    config["allow_join"] = "0";
    update_config = true;
  }

  if (!config["autojoin"]) {
    config["autojoin"] = "#bugz";
    update_config = true;
  }

  if (!config["realname"]) {
    config["realname"] = "A poor soul on BZBZ BBS...";
    update_config = true;
  }

  if (!config["username"]) {
    config["username"] = "bzbz";
    update_config = true;
  }

  if (!config["input_delay"]) {
    config["input_delay"] = "500";
    update_config = true;
  }

  if (!config["timestamp_format"]) {
    config["timestamp_format"] = "%T";
    update_config = true;
  }

  if (update_config) {
    std::ofstream fout("irc-door.yaml");
    fout << "# IRC Chat Door configuration" << std::endl;
    fout << "# to add comments (that don't get destroyed)" << std::endl;
    fout << "# Add comments as key: values, like:" << std::endl;
    fout << "# _comment: This will survive the test of time." << std::endl;
    fout << "# %r AM/PM, %T 24 hour time" << std::endl;
    fout << std::endl;
    fout << config << std::endl;
    fout << "# end yaml config" << std::endl;
  }

  // configure
  irc.nick = door.handle;
  irc.realname = config["realname"].as<std::string>();
  irc.hostname = config["hostname"].as<std::string>();
  irc.port = config["port"].as<std::string>();

  if (config["server_password"]) {
    irc.server_password = config["server_password"].as<std::string>();
  }

  if (config["sasl_password"]) {
    irc.sasl_plain_password = config["sasl_password"].as<std::string>();
  }

  irc.username = config["username"].as<std::string>();
  irc.autojoin = config["autojoin"].as<std::string>();
  irc.version = "Bugz IRC Door 0.1 (C) 2021 Red-Green Software";

  // set the delay between irc updates
  ms_input_delay = config["input_delay"].as<int>();
  timestamp_format = config["timestamp_format"].as<std::string>();

  if (config["log"]) {
    irc.debug_output = config["log"].as<std::string>();
    door << "irc debug logfile = " << config["log"].as<std::string>()
         << door::nl;
  }

  if (config["allow_join"].as<int>() == 1) {
    allow_part = true;
    allow_join = true;
  }

  irc.begin(); // start the initial request so io_context has work to do
  // boost::thread thread(boost::bind(&boost::asio::io_service::run,
  // &io_context));
  // thread Thread(boost::bind(&boost::asio::io_service::run, &io_context));
  thread Thread([&io_context]() -> void { io_context.run(); });

  door << "Welcome to the IRC chat door." << door::nl;

  bool in_door = true;

  while (in_door) {
    // the main loop
    // custom input routine goes here

    check_for_input(door, irc);

    boost::optional<message_stamp> msg;
    bool input_cleared = false;

    if (irc.channels_updated)
      do {
        msg = irc.message_pop();

        if (msg) {
          if (!input_cleared) {
            input_cleared = true;
            clear_input(door);
          }

          render(*msg, door, irc);
        }
      } while (msg);

    if (input_cleared)
      restore_input(door);

    // sleep is done in the check_for_input
    // std::this_thread::sleep_for(200ms);
    if (irc.shutdown)
      in_door = false;
  }

  // Store error messages into door log!
  while (!irc.errors.empty()) {
    door.log() << "ERROR: " << irc.errors.front() << std::endl;
    irc.errors.erase(irc.errors.begin());
  }

  io_context.stop();
  Thread.join();

  // disable the global logging std::function
  get_logger = nullptr;

  door << "Returning to the BBS..." << door::nl;

  return 0;
}
