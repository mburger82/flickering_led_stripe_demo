# ESP32 RMT Driver for WS2812 - Flickering Problem

The RMT-Module of the ESP32 seams to be the perfect Module to drive WS2812 (or LEDs like that)
But there is a issue when not only the rmt-module is running.
For Showcase, the WIFI-Module is initialized. 
As soon as the Wifi-Module is active and successfully connected, occasional flickering is appearing. 
It seams this Problem comes from Interrupts, preventing the rmt-interrupt to reload Data fast enough.

The Example supports preprocessor-switches to enable/disable wifi and other stuff.

Another Problem of the rmt-ledstripe driver seams to be, that only 6 out of 8 Channels of the rmt-module work properly.
Channel 6 and 7 keep crashing.

## How to Use Example

Output-Channels:
* RMT-Channel-0-GPIO: 13
* RMT-Channel-1-GPIO: 15
* RMT-Channel-2-GPIO: 17
* RMT-Channel-3-GPIO: 16
* RMT-Channel-4-GPIO: 04
* RMT-Channel-5-GPIO: 02
* RMT-Channel-6-GPIO: 19
* RMT-Channel-7-GPIO: 18

### Feature Switches:
Multiple compiler switches enable/disable features.

```
#define WIFI_ACTIVE  //Activate WIFI, should lead to occasional flickering
#define MORELEDSTRIPES_ACTIVE  //Activate 5 more Stripes (for testing)
#define MAXLEDSTRIPE_ACTIVE  //Activate RMTModule 6-7, so all RMT Channels are Running. Does not work. crash.
```

### Additional defines nescessary...
```
#define CONFIG_EXAMPLE_STRIP_LED_NUMBER 300  
#define EXAMPLE_ESP_WIFI_SSID      "yourssid"
#define EXAMPLE_ESP_WIFI_PASS      "yourpassword"
```

