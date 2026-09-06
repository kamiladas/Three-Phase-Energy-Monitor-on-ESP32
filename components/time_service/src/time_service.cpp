#include "energy/time_service.hpp"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "nvs.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <ctime>
namespace em { namespace {
constexpr char kTag[]="time_service"; char server[64]="pool.ntp.org"; char zone[64]="CET-1CEST,M3.5.0,M10.5.0/3"; std::atomic_bool synced{};
void load_config(){nvs_handle_t h;if(nvs_open("time",NVS_READONLY,&h)!=ESP_OK)return;size_t n=sizeof(server);nvs_get_str(h,"server",server,&n);n=sizeof(zone);nvs_get_str(h,"tz",zone,&n);nvs_close(h);}
void on_sync(struct timeval*){synced.store(true);ESP_LOGI(kTag,"UTC synchronized using %s",server);}
}
esp_err_t start_time_service(){load_config();setenv("TZ",zone,1);tzset();esp_sntp_config_t cfg=ESP_NETIF_SNTP_DEFAULT_CONFIG(server);cfg.sync_cb=on_sync;const esp_err_t e=esp_netif_sntp_init(&cfg);if(e==ESP_OK)ESP_LOGI(kTag,"SNTP started: %s",server);return e;}
WallTime wall_time_now(){std::time_t now=std::time(nullptr);if(now<1704067200)return {0,TimeQuality::unknown};return {static_cast<std::int64_t>(now),synced.load()?TimeQuality::synced:TimeQuality::estimated};}
const char* time_server(){return server;} const char* timezone_rule(){return zone;}
}
