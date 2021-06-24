#include "input.h"
#include "render.h"

bool allow_part = false;
bool allow_join = false;

int ms_input_delay = 250;
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

void parse_input(door::Door &door, ircClient &irc) {
  // yes, we have something
  std::time_t now_t;
  time(&now_t);

  if (input[0] == '/') {
    // command given
    std::vector<std::string> cmd = split_limit(input, 3);

    if (cmd[0] == "/motd") {
      irc.write("MOTD");
    }

    if (cmd[0] == "/quit") {
      irc.write("QUIT");
    }

    if (cmd[0] == "/talkto") {
      irc.talkto(cmd[1]);
      door << "[talkto = " << cmd[1] << "]" << door::nl;
    }

    if (cmd[0] == "/join") {
      if (allow_join) {
        std::string tmp = "JOIN " + cmd[1];
        irc.write(tmp);
      }
    }

    if (cmd[0] == "/part") {
      if (allow_part) {
        std::string tmp = "PART " + cmd[1];
        irc.write(tmp);
      }
    }

    if (cmd[0] == "/msg") {
      std::string tmp = "PRIVMSG " + cmd[1] + " :" + cmd[2];
      irc.write(tmp);
      stamp(now_t, door);
      if (cmd[1][0] == '#') {
        door << irc.nick << "/" << cmd[1] << " " << cmd[2] << door::nl;
      } else {
        door << cmd[1] << " " << cmd[2] << door::nl;
      }
    }

    if (cmd[0] == "/me") {
      cmd = split_limit(input, 2);
      std::string tmp =
          "PRIVMSG " + irc.talkto() + " :\x01" + "ACTION " + cmd[1] + "\x01";
      irc.write(tmp);
      stamp(now_t, door);
      door << "* " << irc.nick << " " << cmd[1] << door::nl;
    }

    if (cmd[0] == "/help") {
      door << "IRC Commands :" << door::nl;
      door << "/help /motd /quit" << door::nl;
      door << "/me ACTION" << door::nl;
      door << "/msg TARGET Message" << door::nl;
      if (allow_part) {
        door << "/join #CHANNEL" << door::nl;
        door << "/part #CHANNEL" << door::nl;
      }
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

  } else {
    std::string target = irc.talkto();
    std::string output = "PRIVMSG " + target + " :" + input;
    // I need to display something here to show we've said something (and
    // where we've said it)
    door::ANSIColor nick_color{door::COLOR::WHITE, door::COLOR::BLUE};

    stamp(now_t, door);
    if (target[0] == '#') {
      door::ANSIColor channel_color = door::ANSIColor{
          door::COLOR::YELLOW, door::COLOR::BLUE, door::ATTR::BOLD};

      door << channel_color << target << nick_color << "/" << nick_color
           << irc.nick << door::reset << " " << input << door::nl;

    } else {
      door << nick_color << irc.nick << " -> " << target << door::reset << " "
           << input << door::nl;
    }
    irc.write(output);
  }

  input.clear();
}

// can't do /motd it matches /me /msg

const char *hot_keys[] = {"/join #", "/part #", "/talkto ", "/help", "/quit "};

bool check_for_input(door::Door &door, ircClient &irc) {
  int c;

  // return true when we have input and is "valid" // ready
  if (prompt.empty()) {
    // ok, nothing has been displayed at this time.
    if (door.haskey()) {
      // something to do.
      c = door.sleep_key(1);
      if (c < 0) {
        // handle timeout/hangup/out of time
        return false;
      }
      if (c > 0x1000)
        return false;
      if (isprint(c)) {
        prompt = "[" + irc.talkto() + "]";
        door << prompt_color << prompt << input_color << " ";

        door << (char)c;
        input.append(1, c);
      }
    }
    return false;
  } else {
    // continue on with what we have displayed.
    c = door.sleep_ms_key(ms_input_delay);
    if (c != -1) {
      /*
      c = door.sleep_key(1);
      if (c < 0) {
        // handle error
        return false;
      }
      */
      if (c > 0x1000)
        return false;
      if (isprint(c)) {
        // string length check / scroll support?
        door << (char)c;
        input.append(1, c);
        // hot-keys
        if (input[0] == '/') {
          if (input.size() == 2) {
            char c = std::tolower(input[1]);

            if (!allow_part) {
              if (c == 'p')
                c = '!';
            }
            if (!allow_join) {
              if (c == 'j')
                c = '!';
            }

            for (auto hk : hot_keys) {
              if (c == hk[1]) {
                erase(door, input.size());
                input = hk;
                door << input;
                break;
              }
            }
          }
        }
      }
      if ((c == 0x08) or (c == 0x7f)) {
        // hot-keys
        if (input[0] == '/') {
          for (auto hk : hot_keys) {
            if (input == hk) {
              clear_input(door);

              input.clear();
              prompt.clear();
              return false;
            }
          }
        }
        if (input.size() > 1) {
          erase(door, 1);
          door << input_color;
          input.erase(input.length() - 1);
        } else {
          // erasing the last character
          erase(door, 1);
          input.clear();
          erase(door, prompt.size() + 1);
          prompt.clear();
          return false;
        }
      }
      if (c == 0x0d) {
        clear_input(door);
        prompt.clear();
        parse_input(door, irc);
        input.clear();
        return true;
      }
    }
    return false;
  }
}
