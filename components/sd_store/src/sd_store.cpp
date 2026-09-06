#include "energy/sd_store.hpp"
#include "energy/time_service.hpp"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace em { namespace {
constexpr char kTag[]="sd_store"; constexpr char kPath[]="/sdcard/energy_history.csv";
constexpr char kSeedMarker[]="/sdcard/.initial_history_v1";
constexpr char kYearHistoryPath[]="/sdcard/qemu_hourly_year_v3.bin";
constexpr char kYearHistoryMarker[]="/sdcard/.qemu_hourly_year_v3";
constexpr std::int64_t kYearHistoryStart=1756850400;constexpr std::int64_t kYearHistoryEnd=1788386400;
struct SimHistoryRecord{std::int64_t epoch_s;float energy_kwh[3];float active_power_w[3];float frequency_hz[3];float voltage_rms_v[3];float current_rms_a[3];};
extern const unsigned char fixture_start[] asm("_binary_initial_history_csv_start");
extern const unsigned char fixture_end[] asm("_binary_initial_history_csv_end");
constexpr std::size_t kDepth=8; StaticQueue_t queue_state; std::uint8_t queue_bytes[kDepth*sizeof(MeasurementSnapshot)]; QueueHandle_t queue_handle{};
StaticSemaphore_t file_mutex_buffer;SemaphoreHandle_t file_mutex{};
portMUX_TYPE stats_lock=portMUX_INITIALIZER_UNLOCKED; SdStoreStats stats{};
void install_initial_history(){
#if CONFIG_EM_SAMPLE_SOURCE_SIM
    // Remove the obsolete separate test-history file used by an earlier build.
    std::remove("/sdcard/qemu_history_fixture.csv");
    FILE*marker=std::fopen(kSeedMarker,"rb");if(marker){std::fclose(marker);return;}
    FILE*out=std::fopen(kPath,"a+");if(!out){ESP_LOGE(kTag,"Cannot append initial SD history");return;}
    std::fseek(out,0,SEEK_END);if(std::ftell(out)==0)std::fputs("timestamp_us,sequence,L1_V,L1_A,L1_W,L1_VA,L1_var,L1_PF,L1_Hz,L1_import_Wh,L1_export_Wh,L1_flags,L2_V,L2_A,L2_W,L2_VA,L2_var,L2_PF,L2_Hz,L2_import_Wh,L2_export_Wh,L2_flags,L3_V,L3_A,L3_W,L3_VA,L3_var,L3_PF,L3_Hz,L3_import_Wh,L3_export_Wh,L3_flags,missing_us,utc_epoch_s,time_quality\n",out);
    const char*cursor=reinterpret_cast<const char*>(fixture_start);const char*end=reinterpret_cast<const char*>(fixture_end);while(cursor<end&&*cursor!='\n')cursor++;if(cursor<end)cursor++;std::uint64_t sequence=100000;
    while(cursor<end){long long epoch=0;double e1=0,e2=0,e3=0;if(std::sscanf(cursor,"%lld,%lf,%lf,%lf",&epoch,&e1,&e2,&e3)==4){std::fprintf(out,"%llu,%llu,230,1,200,230,0,0.87,50,%.3f,0,0,230,1,160,230,0,0.70,50,%.3f,0,0,230,1,120,230,0,0.52,50,%.3f,0,0,0,%lld,2\n",(unsigned long long)(sequence*1000000ULL),(unsigned long long)sequence,e1,e2,e3,epoch);sequence++;}while(cursor<end&&*cursor!='\n')cursor++;if(cursor<end)cursor++;}
    std::fclose(out);marker=std::fopen(kSeedMarker,"wb");if(marker){std::fputs("loaded\n",marker);std::fclose(marker);}ESP_LOGI(kTag,"Initial history appended to %s",kPath);
#endif
}
#if CONFIG_EM_SAMPLE_SOURCE_SIM
void install_year_history_task(void*){
    FILE*marker=std::fopen(kYearHistoryMarker,"rb");if(marker){std::fclose(marker);vTaskDelete(nullptr);return;}
    FILE*out=std::fopen(kYearHistoryPath,"wb");if(!out){ESP_LOGE(kTag,"Cannot create QEMU yearly history");vTaskDelete(nullptr);return;}std::setvbuf(out,nullptr,_IOFBF,16*1024);std::uint32_t row=0;
    for(std::int64_t epoch=kYearHistoryStart;epoch<kYearHistoryEnd;epoch+=3600,++row){const int hour=static_cast<int>((epoch/3600+2)%24),day=static_cast<int>(row/24),weekday=day%7;double base=hour<5?.10:hour<7?.34:hour<9?.92:hour<16?.43:hour<22?1.18:.28;const double season=.88+.22*std::abs(day-182)/182.0,weekend=weekday>=5?.86:1.0;double l1=base*season*weekend*(.92+.08*((day%11)/10.0)),l2=base*season*weekend*(.70+.16*((day%7)/6.0)),l3=base*season*weekend*(.48+.18*((day%13)/12.0));if((day%97==0&&hour>=2&&hour<5)||(day%61==0&&hour==13))l1=l2=l3=0;const double hz=50.0+(static_cast<int>(day%17)-8)*.003+(hour-12)*.0005,v1=230.0+(day%9-4)*.35,v2=229.2+(day%7-3)*.42,v3=231.0+(day%11-5)*.28;SimHistoryRecord record{epoch,{static_cast<float>(l1),static_cast<float>(l2),static_cast<float>(l3)},{static_cast<float>(l1*1000.0),static_cast<float>(l2*1000.0),static_cast<float>(l3*1000.0)},{static_cast<float>(hz),static_cast<float>(hz+.002),static_cast<float>(hz-.002)},{static_cast<float>(v1),static_cast<float>(v2),static_cast<float>(v3)},{static_cast<float>(l1*1000.0/(v1*.92)),static_cast<float>(l2*1000.0/(v2*.88)),static_cast<float>(l3*1000.0/(v3*.84))}};if(std::fwrite(&record,sizeof(record),1,out)!=1){ESP_LOGE(kTag,"Cannot write QEMU yearly history");std::fclose(out);vTaskDelete(nullptr);return;}if((row&127U)==0U)taskYIELD();}
    if(std::fclose(out)==0){marker=std::fopen(kYearHistoryMarker,"wb");if(marker){std::fputs("ready\n",marker);std::fclose(marker);}ESP_LOGI(kTag,"QEMU hourly history ready: 365 days");}else ESP_LOGE(kTag,"Cannot finalize QEMU yearly history");vTaskDelete(nullptr);
}
#endif
void storage_task(void*) {
    MeasurementSnapshot s{};std::uint64_t last_us=0;
    for(;;){if(xQueueReceive(queue_handle,&s,pdMS_TO_TICKS(2000))!=pdPASS)continue;if(last_us&&s.ended_at_us<last_us+1000000)continue;last_us=s.ended_at_us;
        if(xSemaphoreTake(file_mutex,pdMS_TO_TICKS(2000))!=pdPASS){portENTER_CRITICAL(&stats_lock);stats.write_errors++;portEXIT_CRITICAL(&stats_lock);continue;}FILE*f=std::fopen(kPath,"a+");int ok=-1;
        if(f){std::fseek(f,0,SEEK_END);if(std::ftell(f)==0)std::fputs("timestamp_us,sequence,L1_V,L1_A,L1_W,L1_VA,L1_var,L1_PF,L1_Hz,L1_import_Wh,L1_export_Wh,L1_flags,L2_V,L2_A,L2_W,L2_VA,L2_var,L2_PF,L2_Hz,L2_import_Wh,L2_export_Wh,L2_flags,L3_V,L3_A,L3_W,L3_VA,L3_var,L3_PF,L3_Hz,L3_import_Wh,L3_export_Wh,L3_flags,missing_us,utc_epoch_s,time_quality\n",f);
        const auto&a=s.phase[0];const auto&b=s.phase[1];const auto&c=s.phase[2];
        const auto wt=wall_time_now();ok=std::fprintf(f,"%llu,%llu,%.3f,%.5f,%.3f,%.3f,%.3f,%.5f,%.3f,%.8f,%.8f,%lu,%.3f,%.5f,%.3f,%.3f,%.3f,%.5f,%.3f,%.8f,%.8f,%lu,%.3f,%.5f,%.3f,%.3f,%.3f,%.5f,%.3f,%.8f,%.8f,%lu,%llu,%lld,%u\n",(unsigned long long)s.ended_at_us,(unsigned long long)s.sequence,a.voltage_rms_v,a.current_rms_a,a.active_power_w,a.apparent_power_va,a.reactive_power_var,a.power_factor,a.frequency_hz,a.imported_energy_wh,a.exported_energy_wh,(unsigned long)a.quality_flags,b.voltage_rms_v,b.current_rms_a,b.active_power_w,b.apparent_power_va,b.reactive_power_var,b.power_factor,b.frequency_hz,b.imported_energy_wh,b.exported_energy_wh,(unsigned long)b.quality_flags,c.voltage_rms_v,c.current_rms_a,c.active_power_w,c.apparent_power_va,c.reactive_power_var,c.power_factor,c.frequency_hz,c.imported_energy_wh,c.exported_energy_wh,(unsigned long)c.quality_flags,(unsigned long long)s.missing_time_us,(long long)wt.epoch_s,(unsigned)wt.quality);if(std::fclose(f)!=0)ok=-1;}xSemaphoreGive(file_mutex);
        portENTER_CRITICAL(&stats_lock);if(ok>0)stats.written++;else stats.write_errors++;portEXIT_CRITICAL(&stats_lock);
    }
}
}
esp_err_t start_sd_store(){esp_vfs_fat_sdmmc_mount_config_t mount={.format_if_mount_failed=true,.max_files=6,.allocation_unit_size=16*1024,.disk_status_check_enable=false,.use_one_fat=false};sdmmc_host_t host=SDMMC_HOST_DEFAULT();sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();slot.width=1;sdmmc_card_t*card=nullptr;esp_err_t err=esp_vfs_fat_sdmmc_mount("/sdcard",&host,&slot,&mount,&card);if(err!=ESP_OK){ESP_LOGE(kTag,"SD mount failed: %s",esp_err_to_name(err));return err;}install_initial_history();queue_handle=xQueueCreateStatic(kDepth,sizeof(MeasurementSnapshot),queue_bytes,&queue_state);file_mutex=xSemaphoreCreateMutexStatic(&file_mutex_buffer);if(!queue_handle||!file_mutex)return ESP_ERR_NO_MEM;portENTER_CRITICAL(&stats_lock);stats.mounted=true;portEXIT_CRITICAL(&stats_lock);if(xTaskCreate(storage_task,"sd_writer",6144,nullptr,3,nullptr)!=pdPASS)return ESP_ERR_NO_MEM;
#if CONFIG_EM_SAMPLE_SOURCE_SIM
if(xTaskCreate(install_year_history_task,"sd_fixture",4096,nullptr,1,nullptr)!=pdPASS)ESP_LOGW(kTag,"Cannot start QEMU history fixture task");
#endif
ESP_LOGI(kTag,"Persistent SD ready: %s",kPath);return ESP_OK;}
bool sd_store_enqueue(const MeasurementSnapshot&s){if(!queue_handle)return false;if(xQueueSend(queue_handle,&s,0)==pdPASS)return true;portENTER_CRITICAL(&stats_lock);stats.dropped++;portEXIT_CRITICAL(&stats_lock);return false;}
SdStoreStats sd_store_stats(){portENTER_CRITICAL(&stats_lock);auto copy=stats;portEXIT_CRITICAL(&stats_lock);return copy;}
const char* sd_store_csv_path(){return kPath;}
esp_err_t sd_store_aggregate(std::uint64_t range_us,std::size_t bucket_count,SdEnergyAggregate& out){
    if(!stats.mounted||bucket_count==0||bucket_count>SdEnergyAggregate::kMaxBuckets)return ESP_ERR_INVALID_STATE;
    FILE*f=std::fopen(kPath,"r");if(!f)return ESP_FAIL;char line[768];std::uint64_t previous=0,latest=0;long session_offset=0;
    std::fgets(line,sizeof(line),f);
    while(true){long pos=std::ftell(f);if(!std::fgets(line,sizeof(line),f))break;char*end=nullptr;auto t=std::strtoull(line,&end,10);if(end==line)continue;if(previous&&t<previous)session_offset=pos;previous=t;latest=t;}
    out={};out.count=bucket_count;out.range_us=range_us;if(!latest){std::fclose(f);return ESP_OK;}const std::uint64_t start=latest>range_us?latest-range_us:0,bucket_us=range_us/bucket_count;
    std::fseek(f,session_offset,SEEK_SET);double prev_power[3]{};std::uint64_t prev_t=0;
    while(std::fgets(line,sizeof(line),f)){char*ctx=nullptr;char*token=strtok_r(line,",",&ctx);int col=0;std::uint64_t t=0;std::int64_t epoch=0;double power[3]{};while(token){if(col==0)t=std::strtoull(token,nullptr,10);else if(col==4)power[0]=std::strtod(token,nullptr);else if(col==14)power[1]=std::strtod(token,nullptr);else if(col==24)power[2]=std::strtod(token,nullptr);else if(col==33)epoch=std::strtoll(token,nullptr,10);token=strtok_r(nullptr,",",&ctx);++col;}if(t<start||t>latest){prev_t=t;for(int n=0;n<3;n++)prev_power[n]=power[n];continue;}std::size_t index=std::min<std::size_t>(bucket_count-1,(t-start)/std::max<std::uint64_t>(1,bucket_us));auto&b=out.buckets[index];b.present=true;b.start_us=start+index*bucket_us;if(epoch>0)b.start_epoch_s=epoch-static_cast<std::int64_t>((t-b.start_us)/1000000ULL);if(prev_t&&t>=prev_t){double hours=double(t-prev_t)/3600000000.0;for(int n=0;n<3;n++)b.energy_kwh[n]+=std::max(0.0,prev_power[n])*hours/1000.0;}prev_t=t;for(int n=0;n<3;n++)prev_power[n]=power[n];}
    std::fclose(f);return ESP_OK;
}
esp_err_t sd_store_aggregate_utc(std::int64_t from,std::int64_t to,std::size_t count,SdAggregationResolution resolution,SdEnergyAggregate& out){
    if(!stats.mounted||from<=0||to<=from||count==0||count>SdEnergyAggregate::kMaxBuckets)return ESP_ERR_INVALID_ARG;
    FILE*f=std::fopen(kPath,"r");if(!f)return ESP_FAIL;std::setvbuf(f,nullptr,_IOFBF,8*1024);out={};out.count=count;out.range_us=static_cast<std::uint64_t>(to-from)*1000000ULL;
    std::array<std::int64_t,SdEnergyAggregate::kMaxBuckets+1> boundaries{};std::time_t origin=static_cast<std::time_t>(from);std::tm local{};localtime_r(&origin,&local);
    for(std::size_t i=0;i<=count;i++){if(resolution==SdAggregationResolution::five_minute)boundaries[i]=from+static_cast<std::int64_t>(i)*300;else if(resolution==SdAggregationResolution::hour)boundaries[i]=from+static_cast<std::int64_t>(i)*3600;else if(resolution==SdAggregationResolution::three_hour)boundaries[i]=from+static_cast<std::int64_t>(i)*10800;else{std::tm edge=local;if(resolution==SdAggregationResolution::day)edge.tm_mday+=static_cast<int>(i);else edge.tm_mon+=static_cast<int>(i);edge.tm_isdst=-1;boundaries[i]=static_cast<std::int64_t>(std::mktime(&edge));}if(i<count)out.buckets[i].start_epoch_s=boundaries[i];}
    const auto bucket_index=[&](std::int64_t epoch){if(resolution==SdAggregationResolution::five_minute)return std::min<std::size_t>(count-1,static_cast<std::size_t>((epoch-from)/300));if(resolution==SdAggregationResolution::hour)return std::min<std::size_t>(count-1,static_cast<std::size_t>((epoch-from)/3600));if(resolution==SdAggregationResolution::three_hour)return std::min<std::size_t>(count-1,static_cast<std::size_t>((epoch-from)/10800));std::size_t index=0;while(index+1<count&&epoch>=boundaries[index+1])++index;return index;};
#if CONFIG_EM_SAMPLE_SOURCE_SIM
    FILE*marker=std::fopen(kYearHistoryMarker,"rb");if(marker){std::fclose(marker);FILE*history=std::fopen(kYearHistoryPath,"rb");if(history){std::setvbuf(history,nullptr,_IOFBF,8*1024);const std::int64_t first=std::max(from,kYearHistoryStart),offset=first<=kYearHistoryStart?0:(first-kYearHistoryStart+3599)/3600;if(std::fseek(history,static_cast<long>(offset*static_cast<std::int64_t>(sizeof(SimHistoryRecord))),SEEK_SET)==0){SimHistoryRecord record{};while(std::fread(&record,sizeof(record),1,history)==1&&record.epoch_s<to){if(record.epoch_s<from)continue;auto&bucket=out.buckets[bucket_index(record.epoch_s)];for(int n=0;n<3;n++){bucket.energy_kwh[n]+=record.energy_kwh[n];bucket.active_power_w[n]+=record.active_power_w[n];bucket.frequency_hz[n]+=record.frequency_hz[n];bucket.voltage_rms_v[n]+=record.voltage_rms_v[n];bucket.current_rms_a[n]+=record.current_rms_a[n];}bucket.measurement_count++;bucket.present=true;}}std::fclose(history);}}
#endif
    char line[768];std::fgets(line,sizeof(line),f);double previous_import[3]{};std::uint64_t previous_sequence=0;std::int64_t previous_epoch=0;
    while(std::fgets(line,sizeof(line),f)){
        char*ctx=nullptr;char*token=strtok_r(line,",",&ctx);int col=0;std::uint64_t sequence=0;std::int64_t epoch=0;double imported[3]{},power[3]{},frequency[3]{},voltage[3]{},current[3]{};
        while(token){if(col==1)sequence=std::strtoull(token,nullptr,10);else if(col==2)voltage[0]=std::strtod(token,nullptr);else if(col==3)current[0]=std::strtod(token,nullptr);else if(col==4)power[0]=std::strtod(token,nullptr);else if(col==8)frequency[0]=std::strtod(token,nullptr);else if(col==9)imported[0]=std::strtod(token,nullptr);else if(col==12)voltage[1]=std::strtod(token,nullptr);else if(col==13)current[1]=std::strtod(token,nullptr);else if(col==14)power[1]=std::strtod(token,nullptr);else if(col==18)frequency[1]=std::strtod(token,nullptr);else if(col==19)imported[1]=std::strtod(token,nullptr);else if(col==22)voltage[2]=std::strtod(token,nullptr);else if(col==23)current[2]=std::strtod(token,nullptr);else if(col==24)power[2]=std::strtod(token,nullptr);else if(col==28)frequency[2]=std::strtod(token,nullptr);else if(col==29)imported[2]=std::strtod(token,nullptr);else if(col==33)epoch=std::strtoll(token,nullptr,10);token=strtok_r(nullptr,",",&ctx);++col;}
        if(epoch<=0)continue;
        const bool continuous=previous_epoch>0&&epoch>=previous_epoch&&sequence>previous_sequence;
        if(epoch>=from&&epoch<to){auto&b=out.buckets[bucket_index(epoch)];for(int n=0;n<3;n++){b.active_power_w[n]+=power[n];b.frequency_hz[n]+=frequency[n];b.voltage_rms_v[n]+=voltage[n];b.current_rms_a[n]+=current[n];if(continuous){const double delta=imported[n]-previous_import[n];if(delta>=0.0)b.energy_kwh[n]+=delta/1000.0;}}b.measurement_count++;b.present=true;}
        previous_epoch=epoch;previous_sequence=sequence;for(int n=0;n<3;n++)previous_import[n]=imported[n];
    }
    std::fclose(f);
    for(std::size_t i=0;i<count;i++){auto&b=out.buckets[i];if(!b.measurement_count)continue;for(int n=0;n<3;n++){b.active_power_w[n]/=b.measurement_count;b.frequency_hz[n]/=b.measurement_count;b.voltage_rms_v[n]/=b.measurement_count;b.current_rms_a[n]/=b.measurement_count;}}
    return ESP_OK;
}
}
