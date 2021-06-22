
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

  if (update_config) {
    std::ofstream fout("irc-door.yaml");
    fout << config << std::endl;
  }

  // configure
  irc.nick = door.handle;
  irc.realname = config["realname"].as<std::string>();
  irc.hostname = config["hostname"].as<std::string>();
  irc.port = config["port"].as<std::string>();
  irc.username = config["username"].as<std::string>();
  irc.autojoin = config["autojoin"].as<std::string>();

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

  // main "loop" -- not the proper way to do it.
  bool in_door = true;

  while (in_door) {
    // the main loop
    // custom input routine goes here

    if (check_for_input(door, irc)) {
    }

    boost::optional<std::vector<std::string>> msg;

    // hold list of users -- until end names received.
    // std::vector<std::string> names;

    bool input_cleared = false;

    do {
      msg = irc.buffer_maybe_pop();

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

    // std::this_thread::sleep_for(200ms);
    if (irc.shutdown)
      in_door = false;
  }

  // We miss the ERROR / connection closed message

  io_context.stop();
  Thread.join();

  door << "Returning to the BBS..." << door::nl;

  // std::this_thread::sleep_for(2s);
  return 0;
}
