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
#include "Poco/Path.h"
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

  const Setting& root = _cfg.getRoot();
  const Setting& streams = root["streams"];
  for (const auto& stream_cfg : streams) {
    auto stream = std::make_shared<Stream>();
    stream->id = (std::string)stream_cfg.lookup("id");
    stream->directory = (std::string)stream_cfg.lookup("watchfolder");
    stream->tsi = (unsigned)stream_cfg.lookup("tsi");
    stream->rate_limit = (uint32_t)stream_cfg.lookup("rate_limit");
    stream->watcher = std::make_unique<Poco::DirectoryWatcher>(stream->directory);
    stream->watcher->itemMovedTo += Poco::delegate(stream.get(), &FluteFfmpegTx::Stream::on_file_renamed);

    stream->transmitter = std::make_unique<LibFlute::Transmitter>(_transmitter_multicast_ip, _transmitter_multicast_port, stream->tsi,
        _mtu, stream->rate_limit, _io_service);
    stream->transmitter->register_completion_callback(
        [stream](uint32_t toi) {
        spdlog::debug("File with TOI {} has been sent. Freeing buffer.", toi);
        free(stream->file_buffers[toi]);
        stream->file_buffers.erase(toi);
        });
    spdlog::info("Registered stream {} at {}", stream->id, stream->directory);
    _streams.push_back(stream);
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
}
auto FluteFfmpegTx::api_poll_tick() -> void
{
  httplib::Headers headers = {
    { "Authorization", "Bearer " + _api_token }
  };
  if (_api_enabled) {
    for (auto& stream : _streams) {
      if (auto res = _api_client->Get("/capi/stream/"+stream->id+"/enabled", headers)) {
        if (res->status == 200) {
          bool enabled = (res->body == "TRUE");
          if (stream->enabled != enabled) {
            spdlog::info("{}abling stream {}", enabled ? "En" : "Dis", stream->id);
            stream->enabled = enabled;
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

void FluteFfmpegTx::Stream::on_file_renamed(const Poco::DirectoryWatcher::DirectoryEvent &changeEvent) {
 // std::string path = changeEvent.item.path();
  Poco::Path path(changeEvent.item.path()); 
  spdlog::info("File renamed: Name: {} ", path.getFileName());
  std::ifstream file(changeEvent.item.path(), std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  char *buffer = (char *) malloc(size);
  file.read(buffer, size);

  std::string content_type;
  if (path.getExtension() == "mpd"){
    content_type = DASH_CONTENT_TYPE;
  } else if (path.getExtension() == "m3u8"){
    content_type = HLS_CONTENT_TYPE;
  }

  uint32_t toi = transmitter->send(path.getFileName(),
                                    content_type,
                                    transmitter->seconds_since_epoch() + 60, // 1 minute from now
                                    buffer,
                                    (size_t) size
  );
  file_buffers[toi] = (void*)buffer;

  spdlog::info("Queued {} ({}b) for FLUTE transmission with MIME type {}, TOI {}", path.getFileName(), size, content_type, toi);

}

