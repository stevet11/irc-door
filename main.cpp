
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

const int ms_input_delay = 50;
std::string input;
std::string prompt; // mostly for length to erase/restore properly
int max_input = 100;
door::ANSIColor prompt_color{door::COLOR::YELLOW, door::COLOR::BLUE,
                             door::ATTR::BOLD};
door::ANSIColor input_color{door::COLOR::WHITE}; // , door::COLOR::BLUE};

void erase(door::Door &d, int count) {
  d << door::reset;
  for (int x = 0; x < count; ++x) {
    d << "\x08 \x08";
  }
}

void clear_input(door::Door &d) {
  if (prompt.empty())
    return;
  erase(d, input.size());
  erase(d, prompt.size() + 1);
}

void restore_input(door::Door &d) {
  if (prompt.empty())
    return;
  d << prompt_color << prompt << input_color << " " << input;
}

/*
commands:

/h /help /?
/t /talk /talkto [TARGET]
/msg [TO] [message]
/me [message]
/quit [message, maybe]
/join [TARGET]
/part [TARGET]

future:
/list    ?
/version ?
*/

bool check_for_input(door::Door &d, ircClient &irc) {
  int c;

  // return true when we have input and is "valid" // ready
  if (prompt.empty()) {
    // ok, nothing has been displayed at this time.
    if (d.haskey()) {
      // something to do.
      prompt = "[" + irc.talkto() + "]";
      d << prompt_color << prompt << input_color << " ";
      c = d.sleep_key(1);
      if (c < 0) {
        // handle timeout/hangup/out of time
        return false;
      }
      if (c > 0x1000)
        return false;
      if (isprint(c)) {
        d << (char)c;
        input.append(1, c);
      }
    }
    return false;
  } else {
    // continue on with what we have displayed.
    c = d.sleep_ms_key(ms_input_delay);
    if (c != -1) {
      /*
      c = d.sleep_key(1);
      if (c < 0) {
        // handle error
        return false;
      }
      */
      if (c > 0x1000)
        return false;
      if (isprint(c)) {
        d << (char)c;
        input.append(1, c);
        // hot-keys
        if (input[0] == '/') {
          if (input.size() == 2) {
            switch (input[1]) {
            case 'j':
            case 'J':
              erase(d, input.size());
              input = "/join ";
              d << input;
              break;
            case 'p':
            case 'P':
              erase(d, input.size());
              input = "/part ";
              d << input;
              break;
            case 't':
            case 'T':
              erase(d, input.size());
              input = "/talkto ";
              d << input;
              break;
            case 'h':
            case 'H':
            case '?':
              erase(d, input.size());
              input = "/help";
              d << input;
              break;
            case 'q':
            case 'Q':
              erase(d, input.size());
              input = "/quit";
              d << input;
            }
          }
        }
      }
      if ((c == 0x08) or (c == 0x7f)) {
        // hot-keys
        if (input[0] == '/') {
          if ((input == "/help") or (input == "/talkto ") or
              (input == "/join ") or (input == "/part") or (input == "/quit")) {
            erase(d, input.size());
            erase(d, prompt.size());
            input.clear();
            prompt.clear();
            return false;
          }
        }
        if (input.size() > 1) {
          erase(d, 1);
          d << input_color;
          input.erase(input.length() - 1);
        } else {
          // erasing the last character
          erase(d, 1);
          input.clear();
          erase(d, prompt.size() + 1);
          prompt.clear();
          return false;
        }
      }
      if (c == 0x0d) {
        clear_input(d);
        prompt.clear();
        return true;
      }
    }
    return false;
  }
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
    // the main loop
    // custom input routine goes here

    if (check_for_input(door, irc)) {
      // yes, we have something
      if (input[0] == '/') {
        // command given
        std::vector<std::string> cmd = split_limit(input, 3);

        if (cmd[0] == "/quit") {
          irc.write("QUIT");
        }
        if (cmd[0] == "/talkto") {
          irc.talkto(cmd[1]);
          door << "[talkto = " << cmd[1] << "]" << door::nl;
        }

        if (cmd[0] == "/join") {
          std::string tmp = "JOIN " + cmd[1];
          irc.write(tmp);
        }

        if (cmd[0] == "/part") {
          std::string tmp = "PART " + cmd[1];
          irc.write(tmp);
        }

        if (cmd[0] == "/me") {
          cmd = split_limit(input, 2);
          std::string tmp = "PRIVMSG " + irc.talkto() + " :\x01" + "ACTION " +
                            cmd[1] + "\x01";
          irc.write(tmp);
          door << "* " << irc.nick << " " << cmd[1] << door::nl;
        }

        if (cmd[0] == "/info") {
          irc.channels_lock.lock();
          for (auto c : irc.channels) {
            door << "CH " << c.first << " ";
            for (auto s : c.second) {
              door << s << " ";
            }
            door << door::nl;
          }
          irc.channels_lock.unlock();
        }
        /*
        if (std::toupper(input[1]) == 'Q') {
          irc.write("QUIT");
        } else {
          // for now, just output whatever they gave us.
          input.erase(0, 1);
          irc.write(input);
        }
        */
      } else {
        std::string target = irc.talkto();
        std::string output = "PRIVMSG " + target + " :" + input;
        // I need to display something here to show we've said something (and
        // where we've said it)
        door::ANSIColor nick_color{door::COLOR::WHITE, door::COLOR::BLUE};

        if (target[0] == '#') {
          door::ANSIColor channel_color = door::ANSIColor{
              door::COLOR::YELLOW, door::COLOR::BLUE, door::ATTR::BOLD};

          door << nick_color << irc.nick << door::ANSIColor(door::COLOR::CYAN)
               << "/" << channel_color << target << door::reset << " " << input
               << door::nl;

        } else {
          door << nick_color << irc.nick << "/" << target << door::reset << " "
               << input << door::nl;
        }
        irc.write(output);
      };
      input.clear();
    }

    /*
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
    */

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

        if (cmd == "372") {
          // MOTD
          std::string temp = m[3];
          temp.erase(0, 1);
          door << "* " << temp << door::nl;
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
          door::ANSIColor nick_color{door::COLOR::WHITE, door::COLOR::BLUE};

          if (m[2][0] == '#') {
            std::string tmp = m[3];
            tmp.erase(0, 1);
            door::ANSIColor channel_color{door::COLOR::WHITE,
                                          door::COLOR::BLUE};
            if (m[2] == irc.talkto()) {
              channel_color = door::ANSIColor{
                  door::COLOR::YELLOW, door::COLOR::BLUE, door::ATTR::BOLD};
            }
            door << nick_color << parse_nick(m[0])
                 << door::ANSIColor(door::COLOR::CYAN) << "/" << channel_color
                 << m[2] << door::reset << " " << tmp << door::nl;
          } else {
            std::string tmp = m[3];
            tmp.erase(0, 1);

            door << nick_color << parse_nick(m[0]) << door::reset << " " << tmp
                 << door::nl;
          }
        }
      }
    } while (msg);

    if (input_cleared)
      restore_input(door);

    // std::this_thread::sleep_for(200ms);
    if (irc.shutdown)
      in_door = false;
  }

  io_context.stop();
  Thread.join();

  door << "Returning to the BBS..." << door::nl;

  // std::this_thread::sleep_for(2s);
  return 0;
}
