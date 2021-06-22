#include "irc.h"

#include <boost/algorithm/string.hpp>
#include <iostream>

void string_toupper(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

/**
 * @brief remove channel modes (op,voice,hop,...)
 *
 * @param nick
 */
void remove_channel_modes(std::string &nick) {
  // ~&@%+
  std::string remove("~&@%+");
  std::string::size_type pos;
  do {
    pos = remove.find(nick[0]);
    if (pos != std::string::npos)
      nick.erase(0, 1);
  } while (pos != std::string::npos);
}

/**
 * @brief split on spaces, with limit
 *
 * max is the maximum number of splits we will do.
 * default -1 is split all.
 *
 * "this is a test", 3  => [this][is][a test]
 *
 * @param text
 * @param max
 * @return std::vector<std::string>
 */
std::vector<std::string> split_limit(std::string &text, int max) {
  std::vector<std::string> ret;
  int t = 0;
  boost::split(ret, text, [&t, max](char c) {
    if (c == ' ') {
      ++t;
      return ((max == -1) or (t < max));
    };
    return false;
  });
  return ret;
}

/**
 * @brief irc split
 *
 * If it doesn't start with a ':', split into two parts.
 * Otherwise 4 parts
 * [from] [command] [to] [message]
 *
 * @param text
 * @return std::vector<std::string>
 */
std::vector<std::string> irc_split(std::string &text) {
  if (text[0] != ':')
    return split_limit(text, 2);
  return split_limit(text, 4);
}

/**
 * @brief parse_nick
 *
 * Parse out the nick from nick!username@host
 *
 * @param name
 * @return std::string
 */
std::string parse_nick(std::string &name) {
  std::string to = name;
  if (to[0] == ':')
    to.erase(0, 1);
  size_t pos = to.find('!');
  if (pos != std::string::npos) {
    to.erase(pos);
  }
  return to;
}

// namespace io = boost::asio;
// namespace ip = io::ip;
// using tcp = boost::asio::ip; // ip::tcp;

using error_code = boost::system::error_code;
using namespace std::placeholders;

// #define DEBUG_OUTPUT

typedef std::function<void(std::string &)> receiveFunction;

ircClient::ircClient(boost::asio::io_context &io_context)
    : resolver{io_context}, ssl_context{boost::asio::ssl::context::tls},
      socket{io_context, ssl_context}, context{io_context} {
  registered = false;
  nick_retry = 1;
  shutdown = false;
  logging = false;
}

std::ofstream &ircClient::log(void) {
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  debug_file << std::put_time(&tm, "%c ");
  return debug_file;
}

void ircClient::begin(void) {
  original_nick = nick;
  resolver.async_resolve(hostname, port,
                         std::bind(&ircClient::on_resolve, this, _1, _2));
  if (!debug_output.empty()) {
    debug_file.open(debug_output.c_str(),
                    std::ofstream::out | std::ofstream::app);
    logging = true;
  }
}

void ircClient::write(std::string output) {
  if (logging) {
    log() << "<< " << output << std::endl;
  }
  error_code error;
  socket.write_some(boost::asio::buffer(output + "\r\n"), error);
  if (error) {
    if (logging) {
      log() << "Write: " << error.message() << std::endl;
    }
  }
}

void ircClient::buffer_append(std::vector<std::string> &data) {
  lock.lock();
  buffer.push_back(data);
  lock.unlock();
}

int ircClient::buffer_size(void) {
  lock.lock();
  int size = buffer.size();
  lock.unlock();
  return size;
}

std::vector<std::string> ircClient::buffer_pop(void) {
  lock.lock();
  std::vector<std::string> ret = buffer.front();
  buffer.erase(buffer.begin());
  lock.unlock();
  return ret;
}

boost::optional<std::vector<std::string>> ircClient::buffer_maybe_pop(void) {
  lock.lock();
  if (buffer.empty()) {
    lock.unlock();
    return boost::optional<std::vector<std::string>>{};
  }
  std::vector<std::string> ret = buffer.front();
  buffer.erase(buffer.begin());
  lock.unlock();
  return ret;
}

void ircClient::on_resolve(
    error_code error, boost::asio::ip::tcp::resolver::results_type results) {
  if (logging) {
    log() << "Resolve: " << error.message() << std::endl;
  }
  if (error) {
    std::string output = "Unable to resolve (DNS Issue?): " + error.message();
    message(output);
    socket.async_shutdown(std::bind(&ircClient::on_shutdown, this, _1));
  }
  boost::asio::async_connect(socket.next_layer(), results,
                             std::bind(&ircClient::on_connect, this, _1, _2));
}

void ircClient::on_connect(error_code error,
                           boost::asio::ip::tcp::endpoint const &endpoint) {
  if (logging) {
    log() << "Connect: " << error.message() << ", endpoint: " << endpoint
          << std::endl;
  }
  if (error) {
    std::string output = "Unable to connect: " + error.message();
    message(output);
    socket.async_shutdown(std::bind(&ircClient::on_shutdown, this, _1));
  }

  socket.async_handshake(boost::asio::ssl::stream_base::client,
                         std::bind(&ircClient::on_handshake, this, _1));
}

void ircClient::on_handshake(error_code error) {
  if (logging) {
    log() << "Handshake: " << error.message() << std::endl;
  }
  if (error) {
    std::string output = "Handshake: " + error.message();
    message(output);
    socket.async_shutdown(std::bind(&ircClient::on_shutdown, this, _1));
  }

  std::string request = registration();
  boost::asio::async_write(socket, boost::asio::buffer(request),
                           std::bind(&ircClient::on_write, this, _1, _2));
  // socket.async_shutdown(std::bind(&ircClient::on_shutdown, this, _1));
}

void ircClient::on_write(error_code error, std::size_t bytes_transferred) {
  if ((error) and (logging)) {
    log() << "Write: " << error.message() << std::endl;
  }

  // << ", bytes transferred: " << bytes_transferred << "\n";
  boost::asio::async_read_until(
      socket, response, '\n', std::bind(&ircClient::read_until, this, _1, _2));
}

void ircClient::on_shutdown(error_code error) {
  if (logging) {
    log() << "SHUTDOWN: " << error.message() << std::endl;
  }
  shutdown = true;
  context.stop();
}

void ircClient::read_until(error_code error, std::size_t bytes) {
  // std::cout << "Read: " << bytes << ", " << error << "\n";
  // auto data = response.data();
  if (bytes == 0) {
    if (logging) {
      log() << "Read 0 bytes, shutdown..." << std::endl;
    }
    socket.async_shutdown(std::bind(&ircClient::on_shutdown, this, _1));
    return;
  };

  // Only try to get the data -- if we're read some bytes.
  auto data = response.data();
  response.consume(bytes);
  std::string text{(const char *)data.data(), bytes};

  while ((text[text.size() - 1] == '\r') or (text[text.size() - 1] == '\n'))
    text.erase(text.size() - 1);

  receive(text);

  // repeat until closed

  boost::asio::async_read_until(
      socket, response, '\n', std::bind(&ircClient::read_until, this, _1, _2));
}

void ircClient::message(std::string msg) {
  std::vector<std::string> vs;
  vs.push_back(msg);
  buffer_append(vs);
}

void ircClient::receive(std::string &text) {
  std::vector<std::string> parts = irc_split(text);

  if (logging) {
    // this also shows our parser working
    std::ofstream &l = log();
    l << ">> ";
    for (auto &s : parts) {
      l << "[" << s << "] ";
    }
    l << std::endl;
  }

  // INTERNAL IRC PARSING/TRACKING

  if (parts.size() == 2) {
    // hide PING / PONG messages
    if (parts[0] == "PING") {
      std::string output = "PONG " + parts[1];
      write(output);
      return;
    }
  }

  if (parts.size() >= 3) {
    std::string source = parse_nick(parts[0]);
    std::string cmd = parts[1];
    std::string msg_to = parts[2];

    std::string msg;
    if (parts.size() == 4) {
      msg = parts[3];
    }

    if (cmd == "JOIN") {
      msg_to.erase(0, 1); // channel

      channels_lock.lock();
      if (nick == source) {
        // yes, we are joining
        std::string output =
            "You have joined " + msg_to + " [talkto = " + msg_to + "]";
        message(output);
        talkto(msg_to);
        // insert empty set here.
        std::set<std::string> empty;
        channels[msg_to] = empty;
      } else {
        // Someone else is joining
        std::string output = source + " has joined " += msg_to;
        message(output);
        channels[msg_to].insert(source);
      }
      channels_lock.unlock();
    }

    if (cmd == "PART") {
      channels_lock.lock();
      if (nick == source) {
        std::string output = "You left " + msg_to;

        if (logging) {
          for (auto c : channels) {
            log() << c.first << " ";
            for (auto s : c.second) {
              log() << s << " ";
            }
            log() << std::endl;
          }
        }

        {
          auto ch = channels.find(msg_to);
          if (ch != channels.end()) {
            channels.erase(ch);
            log() << "erase ! " << msg_to << std::endl;

          } else {
            log() << "failed to find " << msg_to << std::endl;
          }
        }

        if (logging) {
          for (auto c : channels) {
            log() << c.first << " ";
            for (auto s : c.second) {
              log() << s << " ";
            }
            log() << std::endl;
          }
        }

        if (!channels.empty()) {
          talkto(channels.begin()->first);
          output += " [talkto = " + talkto() + "]";
        } else {
          talkto("");
        }
        message(output);

      } else {
        std::string output = source + " has left " + msg_to;
        if (!msg.empty()) {
          output += " " + msg;
        }
        message(output);
        channels[msg_to].erase(source);
      }
      channels_lock.unlock();
    }

    if (cmd == "KICK") {
      std::string wholeft = split_limit(parts[3], 2)[0];
      std::string output =
          source + " has kicked " + wholeft + " from " + msg_to;

      channels_lock.lock();
      if (wholeft == nick) {
        channels.erase(msg_to);
        if (!channels.empty()) {
          talkto(channels.begin()->first);
          output += " [talkto = " + talkto() + "]";
        } else {
          talkto("");
        }
      } else {
        channels[msg_to].erase(wholeft);
      }
      channels_lock.unlock();
      message(output);
    }

    if (cmd == "QUIT") {
      std::string output = "* " + source + " has quit ";
      message(output);

      channels_lock.lock();
      if (source == nick) {
        // We've quit?
        channels.erase(channels.begin(), channels.end());
      } else {
        for (auto c : channels) {
          c.second.erase(source);
          // would it be possible that channel is empty now?
        }
      }
      channels_lock.unlock();
    }

    if (cmd == "353") {
      // NAMES list for channel
      std::vector<std::string> names_list = split_limit(msg);
      names_list.erase(names_list.begin());
      std::string channel = names_list.front();
      names_list.erase(names_list.begin());

      if ((names_list.size() > 0) and (names_list[0][0] == ':')) {
        names_list[0].erase(0, 1);
      }

      channels_lock.lock();
      if (channels.find(channel) == channels.end()) {
        // does not exist
        channels.insert({channel, std::set<std::string>{}});
      }

      for (auto name : names_list) {
        remove_channel_modes(name);
        channels[channel].insert(name);
      }
      channels_lock.unlock();
    }

    if (cmd == "PRIVMSG") {
      // Possibly a CTCP request.  Let's see
      std::string message = msg;
      if ((message[0] == ':') and (message[1] == '\x01') and
          (message[message.size() - 1] == '\x01')) {
        // CTCP MESSAGE FOUND  strip \x01's
        message.erase(0, 2);
        message.erase(message.size() - 1);

        std::vector<std::string> ctcp_cmd = split_limit(message, 2);

        if (ctcp_cmd[0] != "ACTION") {
          std::string msg =
              "Received CTCP " + ctcp_cmd[0] + " from " + parse_nick(parts[0]);
          this->message(msg);
          if (logging) {
            log() << "CTCP : [" << message << "] from " + parse_nick(parts[0])
                  << std::endl;
          }
        }

        if (message == "VERSION") {
          std::string reply_to = parse_nick(parts[0]);
          boost::format fmt =
              boost::format("NOTICE %1% :\x01VERSION Bugz IRC thing V0.1\x01") %
              reply_to;
          std::string response = fmt.str();
          write(response);
          return;
        }

        if (message.substr(0, 5) == "PING ") {
          message.erase(0, 5);
          boost::format fmt = boost::format("NOTICE %1% :\x01PING %2%\x01") %
                              parse_nick(parts[0]) % message;
          std::string response = fmt.str();
          write(response);
          return;
        }

        if (message == "TIME") {
          auto now = std::chrono::system_clock::now();
          auto in_time_t = std::chrono::system_clock::to_time_t(now);
          std::string datetime = boost::lexical_cast<std::string>(
              std::put_time(std::localtime(&in_time_t), "%c"));

          boost::format fmt = boost::format("NOTICE %1% :\x01TIME %2%\x01") %
                              parse_nick(parts[0]) % datetime;
          std::string response = fmt.str();
          write(response);
          return;
        }

        if (message.substr(0, 7) == "ACTION ") {
          message.erase(0, 7);
          parts[1] = "ACTION"; // change PRIVMSG to ACTION
          parts[3] = message;
        }

        // I have this parsed this far, now what can I do with it?!
      }
    }
  }

  // CTCP handler
  // NOTE:  When sent to a channel, the response is sent to the sender.

  if (!registered) {
    // We're not registered yet
    if (parts[1] == "433") {
      // nick collision!  Nick already in use
      if (nick == original_nick) {
        // try something basic
        nick += "_";
        std::string output = "NICK " + nick;
        write(output);
        return;
      } else {
        // Ok, go advanced
        nick = original_nick + "_" + std::to_string(nick_retry);
        ++nick_retry;
        std::string output = "NICK " + nick;
        write(output);
        return;
      }
    }

    if ((parts[1] == "376") or (parts[1] == "422")) {
      // END MOTD, or MOTD MISSING
      registered = true;
      if (!autojoin.empty()) {
        std::string msg = "JOIN " + autojoin;
        write(msg);
      }
    }
  }

  if (parts[0] == "ERROR") {
    // we're outta here.  :O
    // std::cout << "BANG!" << std::endl;
  }

  buffer_append(parts);

  // :FROM command TO :rest and ':' is optional

  // std::cout << text << "\n";
}

std::string ircClient::registration(void) {
  std::string text;
  text = "NICK " + nick + "\r\n" + "USER " + username + " 0 * :" + realname +
         "\r\n";
  return text;
}
