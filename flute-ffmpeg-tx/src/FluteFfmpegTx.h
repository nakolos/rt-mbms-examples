// 5G-MAG Reference Tools
// Flute ffmpeg TX
// Klaus Kuehnhammer
// based on flute-ffmpeg by Daniel Silhavy
//
// Licensed under the License terms and conditions for use, reproduction, and
// distribution of 5G-MAG software (the “License”).  You may not use this file
// except in compliance with the License.  You may obtain a copy of the License at
// https://www.5g-mag.com/reference-tools.  Unless required by applicable law or
// agreed to in writing, software distributed under the License is distributed on
// an “AS IS” BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.
//
// See the License for the specific language governing permissions and limitations
// under the License.
//
//
#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <libconfig.h++>
#include "Poco/DirectoryWatcher.h"
#include "Transmitter.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

class FluteFfmpegTx {
public:

  FluteFfmpegTx(const libconfig::Config &cfg, boost::asio::io_service& io_service);

  void run();
  void stop() { _running = false; _io_service.stop();}
  void register_directory_watcher();
private:

  void flute_transmission_completed(uint32_t toi);

  struct Stream {
    std::string id = {};
    std::string directory = {};
    unsigned tsi = {};
    uint32_t rate_limit = 50000;
    std::unique_ptr<Poco::DirectoryWatcher> watcher = {};
    std::unique_ptr<LibFlute::Transmitter> transmitter = {};
    void on_file_renamed(const Poco::DirectoryWatcher::DirectoryEvent &changeEvent);
    std::map<uint32_t, void*> file_buffers = {};
    bool enabled = true;
  };
  std::vector<std::shared_ptr<Stream>> _streams = {};

  boost::asio::io_service& _io_service;
  const libconfig::Config &_cfg;
  unsigned _mtu = 1500;
  uint32_t _rate_limit = 50000;
  std::string _watchfolder_path = "/var/www/watchfolder_out";
  std::string _service_announcement = "../files/bootstrap.multipart.hls";
  std::chrono::time_point<std::chrono::high_resolution_clock> _last_sa_send_time = {};
  unsigned _sa_resend_interval = 5;

  void send_service_announcement();

  bool _api_enabled = false;
  unsigned _api_poll_interval = 1;
  std::string _api_ept = {};
  std::string _api_token = {};
  std::unique_ptr<httplib::Client> _api_client;
  boost::asio::deadline_timer _api_poll_timer;
  void api_poll_tick();

  bool _running = true;

};

