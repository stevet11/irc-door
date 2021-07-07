#ifndef IRC_H
#define IRC_H
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/signals2/mutex.hpp>
#include <chrono>
#include <iomanip> // put_time
#include <string>
// #include <vector>
#include <algorithm>
#include <ctime> // time_t
#include <fstream>
#include <set>

#include <boost/asio/io_context.hpp>

#define SENDQ

std::string base64encode(const std::string &str);
void string_toupper(std::string &str);

std::vector<std::string> split_limit(std::string &text, int max = -1);
std::vector<std::string> irc_split(std::string &text);
std::string parse_nick(std::string &name);
void remove_channel_modes(std::string &nick);

class message_stamp {
public:
  message_stamp() { time(&stamp); }
  std::time_t stamp;
  std::vector<std::string> buffer;
};

// using error_code = boost::system::error_code;

class ircClient {
  using error_code = boost::system::error_code;

public:
  ircClient(boost::asio::io_context &io_context);

  // async startup
  void begin(void);

  // thread-safe write to IRC
  void write(std::string output);
#ifdef SENDQ
  // queued writer
  void write_queue(std::string target, std::string output);
  void on_sendq(error_code error);
#endif

  // configuration
  std::string hostname;
  std::string port;
  std::string server_password;
  std::string nick;
  std::string sasl_plain_password;
  std::string username = "bzbz";
  std::string realname;
  std::string autojoin;
  std::string version;

  // filename to use for logfile
  std::string debug_output;
  std::ofstream debug_file;

protected:
  boost::signals2::mutex talkto_lock;
  std::string _talkto;

public:
  std::string talkto(void) {
    talkto_lock.lock();
    std::string ret{_talkto};
    talkto_lock.unlock();
    return ret;
  };

  void talkto(std::string talkvalue) {
    talkto_lock.lock();
    _talkto = talkvalue;
    talkto_lock.unlock();
  };

  // channels / users
  std::atomic<bool> channels_updated;
  boost::signals2::mutex channels_lock;
  std::map<std::string, std::set<std::string>> channels;
  std::atomic<int> max_nick_length;

  void message(std::string msg);
  std::atomic<bool> shutdown;

  // thread-safe messages access
  virtual void message_append(message_stamp &msg);
  boost::optional<message_stamp> message_pop(void);

  std::vector<std::string> errors;
  std::atomic<bool> registered;

private:
  void find_max_nick_length(void);
  boost::signals2::mutex lock;
  std::vector<message_stamp> messages;

  std::string original_nick;
  int nick_retry;

  bool logging;
  std::ofstream &log(void);

  // async callbacks
  void on_resolve(error_code error,
                  boost::asio::ip::tcp::resolver::results_type results);
  void on_connect(error_code error,
                  boost::asio::ip::tcp::endpoint const &endpoint);

  void on_handshake(error_code error);
  void on_write(error_code error, std::size_t bytes_transferred);
  void read_until(error_code error, std::size_t bytes);
  void on_shutdown(error_code error);
  // end async callback

  void receive(std::string &text);

  std::string registration(void);

  // initialization order matters for socket, ssl_context!
  //
  // socket up here causes segmentation fault!
  // boost::asio::ssl::stream<ip::tcp::socket> socket;

  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ssl::context ssl_context;
  boost::asio::streambuf response;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;

#ifdef SENDQ
  boost::asio::high_resolution_timer sendq_timer;
  std::map<std::string, std::vector<std::string>> sendq;
  std::vector<std::string> sendq_targets;
  int sendq_current;
  std::atomic<bool> sendq_active;
  int sendq_ms;
#endif

  boost::asio::io_context &context;
};

#endif