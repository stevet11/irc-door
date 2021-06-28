#include "render.h"

#include <boost/lexical_cast.hpp>
#include <iomanip>

std::string timestamp_format = "%T";

/**
 * @brief length of the timestamp string.
 *
 */
static int stamp_length;

void stamp(std::time_t &stamp, door::Door &door) {
  std::string output = boost::lexical_cast<std::string>(
      std::put_time(std::localtime(&stamp), timestamp_format.c_str()));
  if (output.find('A') != std::string::npos)
    door << door::ANSIColor(door::COLOR::YELLOW, door::ATTR::BOLD);
  else
    door << door::ANSIColor(door::COLOR::BROWN);

  door << output << door::reset << " ";
  stamp_length = (int)output.size() + 1;
  // door << std::put_time(std::localtime(&stamp), timestamp_format.c_str())
}

void word_wrap(int left_side, door::Door &door, std::string text) {
  int workarea = door.width - left_side;
  bool first_line = true;
  door::ANSIColor color = door.previous;

  /*
    door.log() << "word_wrap " << left_side << " area " << workarea << " ["
               << text << "]" << std::endl;
  */

  while (text.size() > 0) {
    if (!first_line) {
      // This isn't the first line, so move over to align the text.
      door << std::string(left_side, ' ') << color;
    }
    first_line = false;
    if ((int)text.size() > workarea) {
      // do magic
      int breaker = workarea;
      while ((breaker > workarea / 2) and (text[breaker] != ' '))
        --breaker;

      if (breaker > workarea / 2) {
        // Ok, we found a logical breaking point.
        door << text.substr(0, breaker) << door::reset << door::nl;
        text.erase(0, breaker + 1);
      } else {
        // We did not find a good breaking point.
        door << text.substr(0, workarea) << door::reset << door::nl;
        text.erase(0, workarea);
      }
    } else {
      // finish it up
      door << text << door::reset << door::nl;
      text.clear();
    }
  }
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
    int left = stamp_length;
    door << info;
    word_wrap(left, door, output);
    // << output << door::reset << door::nl;
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
      int left = stamp_length;
      if (irc_msg[2] == irc.talkto())
        door << active_channel_color;
      else
        door << channel_color;
      door << irc_msg[2] << "/" << nick_color;
      left += irc_msg[2].size() + 1;

      std::string nick = parse_nick(irc_msg[0]);
      left += nick.size();
      int len = irc.max_nick_length - nick.size();
      if (len > 0) {
        door << std::string(len, ' ');
        left += len;
      }
      left += 3;
      door << "* " << nick << " ";
      word_wrap(left, door, irc_msg[3]);
      // << irc_msg[3] << door::reset << door::nl;
      /*
      door << "* " << irc_msg[2] << "/" << parse_nick(irc_msg[0]) << " "
           << irc_msg[3] << door::nl;
           */
    } else {
      stamp(msg_stamp.stamp, door);
      int left = stamp_length;
      std::string nick = parse_nick(irc_msg[0]);
      door << nick_color << "* " << nick << " ";
      left += 3 + nick.size();
      word_wrap(left, door, irc_msg[3]);
      // << irc_msg[3] << door::reset << door::nl;

      // door << "* " << parse_nick(irc_msg[0]) << " " << irc_msg[3] <<
      // door::nl;
    }
  }

  if (cmd == "TOPIC") {
    std::string tmp = irc_msg[3];
    tmp.erase(0, 1);
    stamp(msg_stamp.stamp, door);
    int left = stamp_length;
    door << info;
    std::string text =
        parse_nick(irc_msg[0]) + " set topic of " + irc_msg[2] + " to " + tmp;
    word_wrap(left, door, text);
    // door << info << parse_nick(irc_msg[0]) << " set topic of " << irc_msg[2]
    //     << " to " << tmp << door::reset << door::nl;
  }

  if (cmd == "PRIVMSG") {
    if (irc_msg[2][0] == '#') {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);

      stamp(msg_stamp.stamp, door);
      int left = stamp_length;

      if (irc_msg[2] == irc.talkto())
        door << active_channel_color;
      else
        door << channel_color;
      door << irc_msg[2];
      door << "/" << nick_color;
      left += irc_msg[2].size() + 1;
      std::string nick = parse_nick(irc_msg[0]);
      left += nick.size();
      int len = irc.max_nick_length + 2 - nick.size();
      if (len > 0) {
        door << std::string(len, ' ');
        left += len;
      }
      door << nick << " " << text_color;
      left++;
      word_wrap(left, door, tmp);
      // << tmp << door::reset << door::nl;
      /*
      door << channel_color << irc_msg[2] << "/" << nick_color
           << parse_nick(irc_msg[0]) << door::reset << " " << tmp << door::nl;
      */
    } else {
      std::string tmp = irc_msg[3];
      tmp.erase(0, 1);
      stamp(msg_stamp.stamp, door);
      int left = stamp_length;
      std::string nick = parse_nick(irc_msg[0]);
      door << nick_color << nick << door::reset << " ";
      left += nick.size() + 1;
      word_wrap(left, door, tmp);
      // << tmp << door::nl;
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
