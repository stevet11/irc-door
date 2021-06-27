#include "render.h"

#include <boost/lexical_cast.hpp>
#include <iomanip>

std::string timestamp_format = "%T";

void stamp(std::time_t &stamp, door::Door &door) {
  std::string output = boost::lexical_cast<std::string>(
      std::put_time(std::localtime(&stamp), timestamp_format.c_str()));
  if (output.find('A') != std::string::npos)
    door << door::ANSIColor(door::COLOR::YELLOW, door::ATTR::BOLD);
  else
    door << door::ANSIColor(door::COLOR::BROWN);

  door << output << door::reset << " ";
  // door << std::put_time(std::localtime(&stamp), timestamp_format.c_str())
}

void render(message_stamp &msg_stamp, door::Door &door, ircClient &irc) {
  // std::vector<std::string> irc_msg = *msg;
  std::vector<std::string> &irc_msg = msg_stamp.buffer;

  door::ANSIColor info{door::COLOR::CYAN};
  door::ANSIColor error{door::COLOR::RED, door::ATTR::BOLD};

  if (irc_msg.size() == 1) {
    // system message
    stamp(msg_stamp.stamp, door);
    door << info << "(" << irc_msg[0] << ")" << door::reset << door::nl;
    return;
  }

  door::ANSIColor nick_color{door::COLOR::CYAN, door::ATTR::BOLD};
  door::ANSIColor channel_color{door::COLOR::WHITE, door::COLOR::BLUE};
  door::ANSIColor active_channel_color =
      door::ANSIColor{door::COLOR::YELLOW, door::COLOR::BLUE, door::ATTR::BOLD};
  door::ANSIColor text_color{door::COLOR::WHITE};

  std::string cmd = irc_msg[1];

  if (irc_msg[0] == "ERROR") {
    std::string tmp = irc_msg[1];
    if (tmp[0] == ':')
      tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << error << "* ERROR: " << tmp << door::reset << door::nl;
  }

  if (cmd == "332") {
    // joined channel with topic
    std::vector<std::string> channel_topic = split_limit(irc_msg[3], 2);
    channel_topic[1].erase(0, 1);
    std::string output =
        "Topic for " + channel_topic[0] + " is: " + channel_topic[1];
    stamp(msg_stamp.stamp, door);
    door << info << output << door::reset << door::nl;
  }

  if (cmd == "366") {
    // end of names, output and clear
    std::string channel = split_limit(irc_msg[3], 2)[0];

    irc.channels_lock.lock();
    stamp(msg_stamp.stamp, door);
    door << info << "* users on " << channel << " : ";
    for (auto name : irc.channels[channel]) {
      door << name << " ";
    }
    irc.channels_lock.unlock();
    door << door::reset << door::nl;
    // names.clear();
  }

  if (cmd == "372") {
    // MOTD
    std::string temp = irc_msg[3];
    temp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << info << "* " << temp << door::reset << door::nl;
  }

  // 400 and 500 are errors?  should show those.
  if ((cmd[0] == '4') or (cmd[0] == '5')) {
    std::string tmp = irc_msg[3];
    if (tmp[0] == ':')
      tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << error << "* " << tmp << door::reset << door::nl;
  }

  if (cmd == "NOTICE") {
    std::string tmp = irc_msg[3];
    tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << nick_color << parse_nick(irc_msg[0]) << " NOTICE " << tmp
         << door::reset << door::nl;
  }

  if (cmd == "ACTION") {
    if (irc_msg[2][0] == '#') {
      stamp(msg_stamp.stamp, door);
      if (irc_msg[2] == irc.talkto())
        door << active_channel_color;
      else
        door << channel_color;
      door << irc_msg[2] << "/" << nick_color;
      std::string nick = parse_nick(irc_msg[0]);
      int len = irc.max_nick_length - nick.size();
      if (len > 0)
        door << std::string(len, ' ');
      door << "* " << nick << " " << irc_msg[3] << door::reset << door::nl;
      /*
      door << "* " << irc_msg[2] << "/" << parse_nick(irc_msg[0]) << " "
           << irc_msg[3] << door::nl;
           */
    } else {
      stamp(msg_stamp.stamp, door);
      door << nick_color << "* " << parse_nick(irc_msg[0]) << " " << irc_msg[3]
           << door::reset << door::nl;

      // door << "* " << parse_nick(irc_msg[0]) << " " << irc_msg[3] <<
      // door::nl;
    }
  }

  if (cmd == "TOPIC") {
    std::string tmp = irc_msg[3];
    tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << info << parse_nick(irc_msg[0]) << " set topic of " << irc_msg[2]
         << " to " << tmp << door::reset << door::nl;
  }

  if (cmd == "PRIVMSG") {

    if (irc_msg[2][0] == '#') {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);

      stamp(msg_stamp.stamp, door);
      if (irc_msg[2] == irc.talkto())
        door << active_channel_color;
      else
        door << channel_color;
      door << irc_msg[2];
      door << "/" << nick_color;

      std::string nick = parse_nick(irc_msg[0]);
      int len = irc.max_nick_length + 2 - nick.size();
      if (len > 0)
        door << std::string(len, ' ');
      door << nick << " " << text_color << tmp << door::reset << door::nl;
      /*
      door << channel_color << irc_msg[2] << "/" << nick_color
           << parse_nick(irc_msg[0]) << door::reset << " " << tmp << door::nl;
      */
    } else {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);
      stamp(msg_stamp.stamp, door);
      door << nick_color << parse_nick(irc_msg[0]) << door::reset << " " << tmp
           << door::nl;
    }
  }

  if (cmd == "NICK") {
    std::string tmp = irc_msg[2];
    tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    door << info << "* " << parse_nick(irc_msg[0]) << " is now known as " << tmp
         << door::reset << door::nl;
  }
}
