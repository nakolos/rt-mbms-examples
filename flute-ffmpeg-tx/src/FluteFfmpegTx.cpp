// 5G-MAG Reference Tools
// Flute ffmpeg
// Daniel Silhavy
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

#include "FluteFfmpegTx.h"
#include "spdlog/spdlog.h"
#include "Poco/Delegate.h"
#include <fstream>
#include <libconfig.h++>

using libconfig::Setting;


FluteFfmpegTx::FluteFfmpegTx(const libconfig::Config &cfg, boost::asio::io_service& io_service)
    : _cfg(cfg)
    , _io_service(io_service)
    , _api_poll_timer(io_service)
{
  _cfg.lookupValue("multicast_ip", _transmitter_multicast_ip);
  _cfg.lookupValue("multicast_port", _transmitter_multicast_port);
  _cfg.lookupValue("mtu", _mtu);
  _cfg.lookupValue("rate_limit", _rate_limit);
  _cfg.lookupValue("watchfolder_path", _watchfolder_path);

  const Setting& root = _cfg.getRoot();
  const Setting& streams = root["streams"];
  for (const auto& stream_cfg : streams) {
    auto& stream = _streams.emplace_back();
    stream.id = (std::string)stream_cfg.lookup("id");
    stream.directory = _watchfolder_path + (std::string)stream_cfg.lookup("subfolder");
    stream.watcher = std::make_unique<Poco::DirectoryWatcher>(stream.directory);
    stream.watcher->itemMovedTo += Poco::delegate(this, &FluteFfmpegTx::on_file_renamed);
    spdlog::info("Registered stream {} at {}", stream.id, stream.directory);
  }

  _cfg.lookupValue("api.enabled", _api_enabled);
  if (_api_enabled) {
    _cfg.lookupValue("api.endpoint", _api_ept);
    _cfg.lookupValue("api.key", _api_token);
    spdlog::info("Enabling API client, endpoint is {}", _api_ept);
    _api_client = std::make_unique<httplib::Client>(_api_ept);
    _api_poll_timer.expires_from_now(boost::posix_time::seconds(_api_poll_interval));
    _api_poll_timer.async_wait( boost::bind(&FluteFfmpegTx::api_poll_tick, this));
  }

  _transmitter = std::make_unique<LibFlute::Transmitter>(_transmitter_multicast_ip, _transmitter_multicast_port, 0,
                                                         _mtu, _rate_limit, _io_service);
  _transmitter->register_completion_callback(
        [&](uint32_t toi) {
          spdlog::debug("File with TOI {} has been sent. Freeing buffer.", toi);
          free(_file_buffers[toi]);
          _file_buffers.erase(toi);
        });
}
auto FluteFfmpegTx::api_poll_tick() -> void
{
  httplib::Headers headers = {
    { "Authorization", "Bearer " + _api_token }
  };
  if (_api_enabled) {
    for (auto& stream : _streams) {
      if (auto res = _api_client->Get("/capi/stream/"+stream.id+"/enabled", headers)) {
        if (res->status == 200) {
          bool enabled = (res->body == "TRUE");
          if (stream.enabled != enabled) {
            spdlog::info("{}abling stream {}", enabled ? "En" : "Dis", stream.id);
            stream.enabled = enabled;
          }
        }
      } else {
        auto err = res.error();
        spdlog::warn("API error: {}", httplib::to_string(err));
      }
    }
  }
  _api_poll_timer.expires_from_now(boost::posix_time::seconds(_api_poll_interval));
  _api_poll_timer.async_wait( boost::bind(&FluteFfmpegTx::api_poll_tick, this));
}

void FluteFfmpegTx::send_by_flute(const std::string &path, std::string content_type = "video/mp4") {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  char *buffer = (char *) malloc(size);
  file.read(buffer, size);

  if (path.find(".mpd") != std::string::npos) {
    content_type = DASH_CONTENT_TYPE;
  } else if (path.find(".m3u8") != std::string::npos) {
    content_type = HLS_CONTENT_TYPE;
  }

  std::string stripped_path = path;
  size_t pos = stripped_path.find(_watchfolder_path);
  if (pos != std::string::npos)
  {
    stripped_path.erase(pos, _watchfolder_path.length());
  }

  uint32_t toi = _transmitter->send(stripped_path,
                                    content_type,
                                    _transmitter->seconds_since_epoch() + 60, // 1 minute from now
                                    buffer,
                                    (size_t) size
  );
  _file_buffers[toi] = (void*)buffer;

  spdlog::info("Queued {} ({}b) for FLUTE transmission with MIME type {}, TOI is {}", path, size, content_type, toi);
}

void FluteFfmpegTx::on_file_renamed(const Poco::DirectoryWatcher::DirectoryEvent &changeEvent) {
  std::string path = changeEvent.item.path();
  spdlog::info("File renamed: Name: {} ", path);

  for (const auto& stream : _streams) {
    if (path.rfind(stream.directory, 0) == 0) { // file is part of this stream
      if (stream.enabled) {
        send_by_flute(path);
      }
    }
  }
}

