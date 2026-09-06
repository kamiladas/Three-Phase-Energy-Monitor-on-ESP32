#include "energy/web_ui.hpp"

#include "energy/snapshot_store.hpp"
#include "energy/history_store.hpp"
#include "energy/sd_store.hpp"

#include "esp_http_server.h"
#include "esp_check.h"
#include "esp_log.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace em {
namespace {

constexpr char kTag[] = "web_ui";

extern const unsigned char dashboard_html_start[] asm("_binary_dashboard_html_start");
extern const unsigned char dashboard_html_end[] asm("_binary_dashboard_html_end");
extern const unsigned char statistics_html_start[] asm("_binary_statistics_html_start");
extern const unsigned char statistics_html_end[] asm("_binary_statistics_html_end");
extern const unsigned char statistics_pro_html_start[] asm("_binary_statistics_pro_html_start");
extern const unsigned char statistics_pro_html_end[] asm("_binary_statistics_pro_html_end");

esp_err_t dashboard_handler(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(
        request,
        reinterpret_cast<const char*>(dashboard_html_start),
        dashboard_html_end - dashboard_html_start);
}

esp_err_t measurement_handler(httpd_req_t* request) {
    MeasurementSnapshot snapshot{};
    if (!snapshot_store().read(snapshot)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"ready\":false}");
    }

    std::array<char, 2048> json{};
    const auto& a = snapshot.phase[0];
    const auto& b = snapshot.phase[1];
    const auto& c = snapshot.phase[2];
    const double total_import_kwh =
        (a.imported_energy_wh + b.imported_energy_wh + c.imported_energy_wh) / 1000.0;
    const double total_power = a.active_power_w + b.active_power_w + c.active_power_w;
    const int length = std::snprintf(
        json.data(), json.size(),
        "{\"ready\":true,\"sequence\":%llu,\"sample_time_us\":%llu,\"missing_time_us\":%llu,"
        "\"total\":{\"active_power_w\":%.3f,\"imported_energy_kwh\":%.9f},"
        "\"phases\":["
        "{\"id\":\"L1\",\"voltage_v\":%.3f,\"current_a\":%.4f,\"active_power_w\":%.3f,\"pf\":%.4f,\"frequency_hz\":%.3f,\"flags\":%" PRIu32 "},"
        "{\"id\":\"L2\",\"voltage_v\":%.3f,\"current_a\":%.4f,\"active_power_w\":%.3f,\"pf\":%.4f,\"frequency_hz\":%.3f,\"flags\":%" PRIu32 "},"
        "{\"id\":\"L3\",\"voltage_v\":%.3f,\"current_a\":%.4f,\"active_power_w\":%.3f,\"pf\":%.4f,\"frequency_hz\":%.3f,\"flags\":%" PRIu32 "}]}",
        static_cast<unsigned long long>(snapshot.sequence),
        static_cast<unsigned long long>(snapshot.ended_at_us),
        static_cast<unsigned long long>(snapshot.missing_time_us),
        total_power, total_import_kwh,
        a.voltage_rms_v, a.current_rms_a, a.active_power_w, a.power_factor, a.frequency_hz, a.quality_flags,
        b.voltage_rms_v, b.current_rms_a, b.active_power_w, b.power_factor, b.frequency_hz, b.quality_flags,
        c.voltage_rms_v, c.current_rms_a, c.active_power_w, c.power_factor, c.frequency_hz, c.quality_flags);
    if (length < 0 || static_cast<std::size_t>(length) >= json.size()) {
        return httpd_resp_send_500(request);
    }

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json.data(), length);
}

esp_err_t statistics_handler(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, reinterpret_cast<const char*>(statistics_pro_html_start),
                           statistics_pro_html_end - statistics_pro_html_start);
}

esp_err_t history_handler(httpd_req_t* request) {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, "{\"interval_us\":1000000,\"points\":[", HTTPD_RESP_USE_STRLEN), kTag, "history header");
    const std::size_t count = history_store().size();
    std::array<char, 768> json{};
    HistoryPoint p{};
    for (std::size_t n = 0; n < count; ++n) {
        if (!history_store().get(n, p)) break;
        const int length = std::snprintf(json.data(), json.size(),
            "%s{\"timestamp_us\":%llu,\"missing_time_us\":%llu,"
            "\"voltage_v\":[%.2f,%.2f,%.2f],\"current_a\":[%.4f,%.4f,%.4f],"
            "\"active_power_w\":[%.2f,%.2f,%.2f],\"apparent_power_va\":[%.2f,%.2f,%.2f],"
            "\"reactive_power_var\":[%.2f,%.2f,%.2f],\"power_factor\":[%.4f,%.4f,%.4f],"
            "\"frequency_hz\":[%.3f,%.3f,%.3f],\"imported_energy_wh\":[%.5f,%.5f,%.5f],"
            "\"exported_energy_wh\":[%.5f,%.5f,%.5f],\"flags\":[%" PRIu32 ",%" PRIu32 ",%" PRIu32 "]}",
            n ? "," : "", static_cast<unsigned long long>(p.timestamp_us), static_cast<unsigned long long>(p.missing_time_us),
            p.voltage_v[0],p.voltage_v[1],p.voltage_v[2],p.current_a[0],p.current_a[1],p.current_a[2],
            p.active_power_w[0],p.active_power_w[1],p.active_power_w[2],p.apparent_power_va[0],p.apparent_power_va[1],p.apparent_power_va[2],
            p.reactive_power_var[0],p.reactive_power_var[1],p.reactive_power_var[2],p.power_factor[0],p.power_factor[1],p.power_factor[2],
            p.frequency_hz[0],p.frequency_hz[1],p.frequency_hz[2],p.imported_energy_wh[0],p.imported_energy_wh[1],p.imported_energy_wh[2],
            p.exported_energy_wh[0],p.exported_energy_wh[1],p.exported_energy_wh[2],p.flags[0],p.flags[1],p.flags[2]);
        if (length < 0 || static_cast<std::size_t>(length) >= json.size()) return ESP_FAIL;
        ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, json.data(), length), kTag, "history point");
    }
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, "]}", 2), kTag, "history footer");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t waveform_handler(httpd_req_t* request) {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, "{\"sample_rate_hz\":4000,\"samples\":[", HTTPD_RESP_USE_STRLEN), kTag, "wave header");
    std::array<RawFrame, WaveformStore::kCapacity> samples{};
    const std::size_t count = waveform_store().copy(samples);
    std::array<char, 192> json{};
    for (std::size_t n = 0; n < count; ++n) {
        const auto& frame = samples[n];
        const int length = std::snprintf(json.data(), json.size(),
            "%s{\"t_us\":%llu,\"voltage\":[%.2f,%.2f,%.2f],\"current\":[%.2f,%.2f,%.2f]}",
            n ? "," : "", static_cast<unsigned long long>(frame.timestamp_us),
            frame.voltage[0], frame.voltage[1], frame.voltage[2],
            frame.current[0], frame.current[1], frame.current[2]);
        if (length < 0 || static_cast<std::size_t>(length) >= json.size()) return ESP_FAIL;
        ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, json.data(), length), kTag, "wave sample");
    }
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(request, "]}", 2), kTag, "wave footer");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t storage_status_handler(httpd_req_t*req){const auto s=sd_store_stats();char j[192];int n=std::snprintf(j,sizeof(j),"{\"mounted\":%s,\"records_written\":%llu,\"dropped\":%llu,\"write_errors\":%llu}",s.mounted?"true":"false",(unsigned long long)s.written,(unsigned long long)s.dropped,(unsigned long long)s.write_errors);httpd_resp_set_type(req,"application/json");return httpd_resp_send(req,j,n);}

esp_err_t aggregate_handler(httpd_req_t*req){
    char query[192]={},from_text[24]={},to_text[24]={},resolution[12]="hour",count_text[8]="24";
    if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK||
       httpd_query_key_value(query,"from",from_text,sizeof(from_text))!=ESP_OK||
       httpd_query_key_value(query,"to",to_text,sizeof(to_text))!=ESP_OK){
        httpd_resp_set_status(req,"400 Bad Request");return httpd_resp_sendstr(req,"{\"ready\":false,\"error\":\"from_and_to_required\"}");
    }
    httpd_query_key_value(query,"resolution",resolution,sizeof(resolution));httpd_query_key_value(query,"buckets",count_text,sizeof(count_text));
    const std::int64_t from=std::strtoll(from_text,nullptr,10),to=std::strtoll(to_text,nullptr,10);const std::size_t count=static_cast<std::size_t>(std::strtoul(count_text,nullptr,10));SdEnergyAggregate a{};
    if((std::strcmp(resolution,"hour")&&std::strcmp(resolution,"day")&&std::strcmp(resolution,"month"))||sd_store_aggregate_utc(from,to,count,a)!=ESP_OK){httpd_resp_set_status(req,"503 Service Unavailable");return httpd_resp_sendstr(req,"{\"ready\":false,\"error\":\"storage_or_resolution_unavailable\"}");}
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    char header[144];const int header_size=std::snprintf(header,sizeof(header),"{\"ready\":true,\"source\":\"sd_history\",\"resolution\":\"%s\",\"buckets\":[",resolution);httpd_resp_send_chunk(req,header,header_size);
    char j[288];for(std::size_t i=0;i<a.count;i++){const auto&b=a.buckets[i];double total=b.energy_kwh[0]+b.energy_kwh[1]+b.energy_kwh[2];int n=std::snprintf(j,sizeof(j),"%s{\"start_epoch_s\":%lld,\"present\":%s,\"energy_kwh\":[%.9f,%.9f,%.9f],\"total_kwh\":%.9f}",i?",":"",(long long)b.start_epoch_s,b.present?"true":"false",b.energy_kwh[0],b.energy_kwh[1],b.energy_kwh[2],total);httpd_resp_send_chunk(req,j,n);}httpd_resp_send_chunk(req,"]}",2);return httpd_resp_send_chunk(req,nullptr,0);
}

esp_err_t energy_series_handler(httpd_req_t*req){
    char query[96]={},from_text[24]={},to_text[24]={};if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"from",from_text,sizeof(from_text))!=ESP_OK||httpd_query_key_value(query,"to",to_text,sizeof(to_text))!=ESP_OK){httpd_resp_set_status(req,"400 Bad Request");return httpd_resp_sendstr(req,"{\"ready\":false,\"error\":\"from_and_to_required\"}");}
    SdEnergySeries series{};const auto from=std::strtoll(from_text,nullptr,10),to=std::strtoll(to_text,nullptr,10);if(sd_store_energy_series(from,to,series)!=ESP_OK){httpd_resp_set_status(req,"503 Service Unavailable");return httpd_resp_sendstr(req,"{\"ready\":false,\"error\":\"storage_unavailable\"}");}
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_send_chunk(req,"{\"ready\":true,\"source\":\"sd_history\",\"samples\":[",HTTPD_RESP_USE_STRLEN);char json[192];for(std::size_t i=0;i<series.count;i++){const auto&s=series.samples[i];const double total=s.energy_kwh[0]+s.energy_kwh[1]+s.energy_kwh[2];const int n=std::snprintf(json,sizeof(json),"%s{\"epoch_s\":%lld,\"energy_kwh\":[%.9f,%.9f,%.9f],\"total_kwh\":%.9f}",i?",":"",(long long)s.epoch_s,s.energy_kwh[0],s.energy_kwh[1],s.energy_kwh[2],total);httpd_resp_send_chunk(req,json,n);}httpd_resp_send_chunk(req,"]}",2);return httpd_resp_send_chunk(req,nullptr,0);
}

esp_err_t export_handler(httpd_req_t*req){FILE*f=std::fopen(sd_store_csv_path(),"rb");if(!f)return httpd_resp_send_404(req);httpd_resp_set_type(req,"text/csv; charset=utf-8");httpd_resp_set_hdr(req,"Content-Disposition","attachment; filename=energy_history.csv");char block[2048];esp_err_t result=ESP_OK;while(true){size_t n=std::fread(block,1,sizeof(block),f);if(n&&httpd_resp_send_chunk(req,block,n)!=ESP_OK){result=ESP_FAIL;break;}if(n<sizeof(block))break;}std::fclose(f);if(result==ESP_OK)return httpd_resp_send_chunk(req,nullptr,0);return result;}

}  // namespace

esp_err_t start_web_ui() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Waveform responses take one coherent 160-frame copy on the HTTP task
    // stack so the producer cannot rotate the ring while it is serialized.
    config.stack_size = 12288;
    config.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), kTag, "HTTP server start failed");

    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = dashboard_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t measurements = {
        .uri = "/api/v1/measurements/latest",
        .method = HTTP_GET,
        .handler = measurement_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t statistics = {
        .uri = "/statistics",
        .method = HTTP_GET,
        .handler = statistics_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t history = {
        .uri = "/api/v1/history",
        .method = HTTP_GET,
        .handler = history_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t waveform = {
        .uri = "/api/v1/waveform",
        .method = HTTP_GET,
        .handler = waveform_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t storage_status={.uri="/api/v1/storage/status",.method=HTTP_GET,.handler=storage_status_handler,.user_ctx=nullptr};
    const httpd_uri_t aggregate={.uri="/api/v1/history/aggregate",.method=HTTP_GET,.handler=aggregate_handler,.user_ctx=nullptr};
    const httpd_uri_t energy_series={.uri="/api/v1/history/energy-series",.method=HTTP_GET,.handler=energy_series_handler,.user_ctx=nullptr};
    const httpd_uri_t export_csv={.uri="/api/v1/history/export",.method=HTTP_GET,.handler=export_handler,.user_ctx=nullptr};
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root), kTag, "Root registration failed");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &measurements), kTag, "API registration failed");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &statistics), kTag, "Statistics registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &history), kTag, "History registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &waveform), kTag, "Waveform registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server,&storage_status),kTag,"Storage status registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server,&aggregate),kTag,"Aggregate registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server,&energy_series),kTag,"Energy series registration failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server,&export_csv),kTag,"Export registration failed");
    ESP_LOGI(kTag, "Dashboard ready on port 80");
    return ESP_OK;
}

}  // namespace em
