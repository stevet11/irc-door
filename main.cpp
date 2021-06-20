
#include <chrono>
#include <iostream>
#include <string>
#include <thread> // sleep_for

#include "door.h"
#include "irc.h"
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

  irc.debug_output = "irc.log";

  irc.begin(); // start the initial request so io_context has work to do
  // boost::thread thread(boost::bind(&boost::asio::io_service::run,
  // &io_context));
  // thread Thread(boost::bind(&boost::asio::io_service::run, &io_context));
  thread Thread([&io_context]() -> void { io_context.run(); });

  door << "Welcome to the IRC chat door." << door::nl;

  // main "loop" -- not the proper way to do it.
  bool in_door = true;
  while (in_door) {
    if (door.haskey()) {
      door << ">> ";
      std::string input = door.input_string(100);
      door << door::nl;
      if (!input.empty()) {
        if (input == "/q") {
          irc.write("QUIT :What other doors on BZ&BZ BBS work...");
          in_door = false;
        }
        if (input[0] == '/') {
          input.erase(0, 1);
          irc.write(input);
        } else {
          if (irc.talkto.empty()) {
            door << " talkto is empty?  Whaat?" << door::nl;
          } else {
            std::string output = "PRIVMSG " + irc.talkto + " :" + input;
            irc.write(output);
          }
        }
      }
    }

    boost::optional<std::vector<std::string>> msg;

    // hold list of users -- until end names received.
    // std::vector<std::string> names;

    do {
      msg = irc.buffer_maybe_pop();
      if (msg) {
        std::vector<std::string> m = *msg;

        if (m.size() == 1) {
          // system message
          door << "(" << m[0] << ")" << door::nl;
          continue;
        }

        std::string cmd = m[1];

        if (cmd == "366") {
          // end of names, output and clear
          std::string channel = split_limit(m[3], 2)[0];

          irc.channels_lock.lock();
          door << "* users on " << channel << " : ";
          for (auto name : irc.channels[channel]) {
            door << name << " ";
          }
          irc.channels_lock.unlock();
          door << door::nl;
          // names.clear();
        }

        // 400 and 500 are errors?  should show those.
        if ((cmd[0] == '4') or (cmd[0] == '5')) {
          std::string tmp = m[3];
          tmp.erase(0, 1);

          door << "* " << tmp << door::nl;
        }

        if (cmd == "NOTICE") {
          std::string tmp = m[3];
          tmp.erase(0, 1);

          door << parse_nick(m[0]) << " NOTICE " << tmp << door::nl;
        }

        if (cmd == "ACTION") {
          if (m[2][0] == '#') {
            door << "* " << parse_nick(m[0]) << "/" << m[2] << " " << m[3]
                 << door::nl;
          } else {
            door << "* " << parse_nick(m[0]) << " " << m[3] << door::nl;
          }
        }

        if (cmd == "TOPIC") {
          std::string tmp = m[3];
          tmp.erase(0, 1);

          door << parse_nick(m[0]) << " set topic of " << m[2] << " to " << tmp
               << door::nl;
        }

        if (cmd == "PRIVMSG") {
          if (m[2][0] == '#') {
            std::string tmp = m[3];
            tmp.erase(0, 1);

            door << parse_nick(m[0]) << "/" << m[2] << " " << tmp << door::nl;
          } else {
            std::string tmp = m[3];
            tmp.erase(0, 1);

            door << parse_nick(m[0]) << " " << tmp << door::nl;
          }
        }
      }
    } while (msg);

    std::this_thread::sleep_for(200ms);
    if (irc.shutdown)
      in_door = false;
  }

  io_context.stop();
  Thread.join();

  door << "Returning to the BBS..." << door::nl;

  // std::this_thread::sleep_for(2s);
  return 0;
}
