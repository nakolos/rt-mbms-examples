# rt-mbms-examples - FLUTE FFMPEG TX

This is a modification of FLUTE FFMPEG intended to be used with the eNodeB and MBMS GW available [here](https://github.com/nakolos/srsRAN/tree/qrd-tx) for transmitting DASH or HLS streams to Qualcomm QRD handsets.

It reads video segments from configurable watch folders and sends them to the MBMS gateway for 5G-BC transmission. All files are sent over one PMCH, as the gw/enb do not support configuring multiple channels at the moment.

## Installation 

### Install dependencies

Dependencies are the same as for FLUTE-FFMPEG. Please follow the instructions [in its README](https://github.com/5G-MAG/rt-mbms-examples/tree/development/flute-ffmpeg).

### Build setup

````
cd rt-mbms-examples/flute-ffmpeg-tx
mkdir build && cd build
cmake -GNinja ..
````

### Building

````
ninja
````

This will output the application binary in your build folder.


## Configuration

Most of the parameters can directly be changed in the configuration file located at `config/config.cfg`. An example
configuration looks the following

````
multicast_ip = "238.1.1.95";
multicast_port = 40085;
mtu = 1500;
rate_limit = 1200000;
watchfolder_path = "/tmp/5gbc-tx/"; // always add trailing slash
streams = ( { id = "1";
              subfolder = "ch1"; },
            { id = "2";
              subfolder = "ch2"; } );
                 
service_announcement: {
  location = "../files/bootstrap.multipart.hls";
  send_interval = 5; //secs
};

api: {
  enabled = true;
  key = "3c3c817fbc53b9a917380ebe3b61cc2c";
  endpoint = "https://ladon.nakolos.com";
}
````

Adapt your service announcement, watchfolder and subfolders paths according to your needs.

The API subsection is needed when using NAKOLOS-controlled broadcast on demand. Enter the API key from [ida.nakolos.com](https://ida.nakolos.com) here if using this, otherwise set enabled to false.


## Running

### 1. Start mbms-gw and eNode 

By following the instructions in the qrd-tx repo (https://github.com/nakolos/srsRAN/tree/qrd-tx). Leave out the ffmpeg command in step 4.

### 2. Start flute-ffmpeg-tx
````
cd build
./flute-ffmpeg-tx
````

### 3. Start the video stream(s)

You can use the scripts provided in FLUTE-FFMPEG as a baseline, see [this section](https://github.com/5G-MAG/rt-mbms-examples/tree/development/flute-ffmpeg#configure-the-ffmpeg-command)

Adapt them to write to the subfolders you have configured, und run the script(s).
