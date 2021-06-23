#include "render.h"

#include <iomanip>

void stamp(message_stamp &msg_stamp, door::Door &door) {
  door << std::put_time(std::localtime(&msg_stamp.stamp), "%T ");
}

void render(message_stamp &msg_stamp, door::Door &door, ircClient &irc) {
  // std::vector<std::string> irc_msg = *msg;
  std::vector<std::string> &irc_msg = msg_stamp.buffer;

  if (irc_msg.size() == 1) {
    // system message
    stamp(msg_stamp, door);
    door << "(" << irc_msg[0] << ")" << door::nl;
    return;
  }

  std::string cmd = irc_msg[1];

  if (irc_msg[0] == "ERROR") {
    std::string tmp = irc_msg[1];
    if (tmp[0] == ':')
      tmp.erase(0, 1);
    stamp(msg_stamp, door);
    door << "* ERROR: " << tmp << door::nl;
  }

  if (cmd == "332") {
    // joined channel with topic
    std::vector<std::string> channel_topic = split_limit(irc_msg[3], 2);
    channel_topic[1].erase(0, 1);
    std::string output =
        "Topic for " + channel_topic[0] + " is: " + channel_topic[1];
    stamp(msg_stamp, door);
    door << output << door::nl;
  }

  if (cmd == "366") {
    // end of names, output and clear
    std::string channel = split_limit(irc_msg[3], 2)[0];

    irc.channels_lock.lock();
    stamp(msg_stamp, door);
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
    std::string temp = irc_msg[3];
    temp.erase(0, 1);
    stamp(msg_stamp, door);
    door << "* " << temp << door::nl;
  }

  // 400 and 500 are errors?  should show those.
  if ((cmd[0] == '4') or (cmd[0] == '5')) {
    std::string tmp = irc_msg[3];
    if (tmp[0] == ':')
      tmp.erase(0, 1);
    stamp(msg_stamp, door);
    door << "* " << tmp << door::nl;
  }

  if (cmd == "NOTICE") {
    std::string tmp = irc_msg[3];
    tmp.erase(0, 1);
    stamp(msg_stamp, door);
    door << parse_nick(irc_msg[0]) << " NOTICE " << tmp << door::nl;
  }

  if (cmd == "ACTION") {
    if (irc_msg[2][0] == '#') {
      stamp(msg_stamp, door);
      door << "* " << irc_msg[2] << "/" << parse_nick(irc_msg[0]) << " "
           << irc_msg[3] << door::nl;
    } else {
      stamp(msg_stamp, door);
      door << "* " << parse_nick(irc_msg[0]) << " " << irc_msg[3] << door::nl;
    }
  }

  if (cmd == "TOPIC") {
    std::string tmp = irc_msg[3];
    tmp.erase(0, 1);
    stamp(msg_stamp, door);
    door << parse_nick(irc_msg[0]) << " set topic of " << irc_msg[2] << " to "
         << tmp << door::nl;
  }

  if (cmd == "PRIVMSG") {
    door::ANSIColor nick_color{door::COLOR::WHITE, door::COLOR::BLUE};

    if (irc_msg[2][0] == '#') {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);
      door::ANSIColor channel_color{door::COLOR::WHITE, door::COLOR::BLUE};
      if (irc_msg[2] == irc.talkto()) {
        channel_color = door::ANSIColor{door::COLOR::YELLOW, door::COLOR::BLUE,
                                        door::ATTR::BOLD};
      }
      stamp(msg_stamp, door);
      door << channel_color << irc_msg[2] << "/" << nick_color
           << parse_nick(irc_msg[0]) << door::reset << " " << tmp << door::nl;
    } else {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);
      stamp(msg_stamp, door);
      door << nick_color << parse_nick(irc_msg[0]) << door::reset << " " << tmp
           << door::nl;
    }
  }

  if (cmd == "NICK") {
    std::string tmp = irc_msg[2];
    tmp.erase(0, 1);
    door << "* " << parse_nick(irc_msg[0]) << " is now known as " << tmp
         << door::nl;
  }
}
