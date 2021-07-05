#include "input.h"
#include "render.h"

bool allow_part = false;
bool allow_join = false;
bool has_quit = false;

int ms_input_delay = 250;
std::string input;
std::string prompt; // mostly for length to erase/restore properly
int max_input = 220;
int input_scroll = 0;
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

  if (input_scroll == 0)
    erase(d, input.size());
  else
    erase(d, input.size() - input_scroll + 3);
  erase(d, prompt.size() + 1);
}

void restore_input(door::Door &d) {
  if (prompt.empty())
    return;

  d << prompt_color << prompt << input_color << " ";
  if (input_scroll == 0)
    d << input;
  else
    d << "..." << input.substr(input_scroll);
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

    if (cmd[0] == "/names") {
      std::string talk = irc.talkto();
      if (talk[0] == '#') {
        // or we could pull this from /info & talk.
        irc.write("NAMES " + talk);
      } // handle this input error
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

    // TODO: feed this and /me to render so DRY/one place to render
    if (cmd[0] == "/msg") {
      std::string tmp = "PRIVMSG " + cmd[1] + " :" + cmd[2];
      irc.write(tmp);
      // build msg for render
      tmp = ":" + irc.nick + "!" + " " + tmp;
      message_stamp msg;
      msg.buffer = irc_split(tmp);
      render(msg, door, irc);
      /*
      stamp(now_t, door);
      if (cmd[1][0] == '#') {
        door << irc.nick << "/" << cmd[1] << " " << cmd[2] << door::nl;
      } else {
        door << cmd[1] << " " << cmd[2] << door::nl;
      }
      */
    }

    if (cmd[0] == "/notice") {
      std::string tmp = "NOTICE " + cmd[1] + " :" + cmd[2];
      irc.write(tmp);
      // build msg for render
      tmp = ":" + irc.nick + "!" + " " + tmp;
      message_stamp msg;
      msg.buffer = irc_split(tmp);
      render(msg, door, irc);
    }

    if (cmd[0] == "/me") {
      cmd = split_limit(input, 2);
      std::string tmp =
          "PRIVMSG " + irc.talkto() + " :\x01" + "ACTION " + cmd[1] + "\x01";
      irc.write(tmp);
      // build msg for render
      tmp = ":" + irc.nick + "!" + " ACTION " + irc.talkto() + " :" + cmd[1];
      message_stamp msg;
      msg.buffer = irc_split(tmp);
      render(msg, door, irc);
      /*
      stamp(now_t, door);
      door << "* " << irc.nick << " " << cmd[1] << door::nl;
      */
    }

    if (cmd[0] == "/nick") {
      std::string tmp = "NICK " + cmd[1];
      irc.write(tmp);
    }

    if (cmd[0] == "/help") {
      door << "IRC Commands :" << door::nl;
      door << "/help /motd /quit /nick" << door::nl;
      door << "/me ACTION" << door::nl;
      door << "/msg TARGET Message" << door::nl;

      if (allow_part) {
        door << "/join #CHANNEL" << door::nl;
        door << "/part #CHANNEL" << door::nl;
      }
      door << "[ESC] aborts input" << door::nl;
    }

    if (cmd[0] == "/flood") {
      std::string target = irc.talkto();
      std::string bugz = "bugz";
      for (int x = 0; x < 20; ++x) {
        std::string message = "PRIVMSG " + target +
                              " : CHANNEL FLOOD TESTING THIS IS MESSAGE " +
                              std::to_string(x + 1) + " TEST TEST TEST";
        irc.write_queue(target, message);
        message = "PRIVMSG " + bugz + " : USER FLOOD TESTING THIS IS MESSAGE " +
                  std::to_string(x + 1) + " TEST TEST TEST";
        irc.write_queue(bugz, message);
        message = "PRIVMSG apollo : USER FLOOD TESTING THIS IS MESSAGE " +
                  std::to_string(x + 1) + " TEST TEST TEST";
        irc.write_queue("apollo", message);
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

    irc.write(output);

    // build message for render
    message_stamp msg;
    output = ":" + irc.nick + "!" + " " + output;
    msg.buffer = irc_split(output);
    render(msg, door, irc);
    /*
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
    */
  }

  input.clear();
  input_scroll = 0;
}

// can't do /motd it matches /me /msg
// nick matches names.

const char *hot_keys[] = {"/join #", "/part #", "/talkto ", "/help", "/quit "};

bool check_for_input(door::Door &door, ircClient &irc) {
  int c;
  int width = door.width;
  int third = width / 3;

  // return true when we have input and is "valid" // ready
  if (prompt.empty()) {
    // ok, nothing has been displayed at this time.
    if (door.haskey()) {
      // something to do.
      c = door.sleep_key(1);
      if (c < 0) {
        // handle timeout/hangup/out of time
        if (c < -1) {
          if (!has_quit) {
            irc.write("QUIT");
            has_quit = true;
          }
        }
        return false;
      }
      if (c > 0x1000)
        return false;

      // How to handle "early" typing, we we're still connecting...
      // FAIL-WHALE (what if we part all channels?)
      if (irc.registered)
        // don't take any imput unless our talkto has been set.
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
    if (c < -1) {
      if (!has_quit) {
        irc.write("QUIT");
        has_quit = true;
      }
    } else if (c != -1) {
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

        if ((int)input.size() == max_input) {
          door << (char)7;
          return false;
        }

        door << (char)c;
        input.append(1, c);

        int pos = input.size() - input_scroll;
        if (input_scroll != 0)
          pos += 3;

        // need something better then magic number here.
        // 3 is our extra padding here.
        int prompt_size = prompt.size() + 1; // prompt + space
        if (pos + prompt_size + 3 == width) {
          // Ok, scroll!
          clear_input(door);
          input_scroll = input.size() - third;
          restore_input(door);
        }
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

      if (c == 0x1b) {
        // escape key
        clear_input(door);
        input.clear();
        prompt.clear();
        input_scroll = 0;
        return false;
      }

      if ((c == 0x08) or (c == 0x7f)) {
        // hot-keys
        if (input[0] == '/') {
          for (auto hk : hot_keys) {
            if (input == hk) {
              clear_input(door);

              input.clear();
              prompt.clear();
              input_scroll = 0;
              return false;
            }
          }
        }
        if (input.size() > 1) {
          erase(door, 1);
          door << input_color;
          input.erase(input.length() - 1);
          if (input_scroll != 0) {
            // are we getting close?
            if ((int)input.size() - third < input_scroll) {
              // scroll the other way
              clear_input(door);
              input_scroll = (input.size() - 2 * third);
              if (input_scroll < 0)
                input_scroll = 0;
              restore_input(door);
            }
          }
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
        input_scroll = 0;
        return true;
      }
    }
    return false;
  }
}
