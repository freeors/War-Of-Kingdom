#ifndef SLEEP_CAF_HPP_INCLUDED
#define SLEEP_CAF_HPP_INCLUDED

#include "sdl_utils.hpp"
#include "tstring.hpp"
#include "posix.h"
#include "area_anim.hpp"

#define MDATA_THRESHOLD		4096
#define MAX_USED_UNITS		32
#define MAX_TUNE_TIME		1800 // 30 minite

#define MIN_BATTERY_LEVEL	20

namespace anim2 {
enum tapp_type {SCORE = MIN_APP_ANIM, RIGHT_DRAG};
}

struct tfopen_lock
{
	tfopen_lock(const std::string& file, bool read_only)
		: fp(INVALID_FILE)
		, data(NULL)
		, data_size(0)
	{
		if (read_only) {
			posix_fopen(file.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
		} else {
			posix_fopen(file.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
		}
	}

	~tfopen_lock()
	{
		close();
	}

	bool valid() const { return fp != INVALID_FILE; }

	void resize_data(int size, int vsize = 0)
	{
		if (size > data_size) {
			char* tmp = (char*)malloc(size);
			if (data) {
				if (vsize) {
					memcpy(tmp, data, vsize);
				}
				free(data);
			}
			data = tmp;
			data_size = size;
		}
	}

	void close()
	{
		if (data) {
			free(data);
			data = NULL;
			data_size = 0;
		}
		if (fp != INVALID_FILE) {
			posix_fclose(fp);
			fp = INVALID_FILE;
		}
	}

public:
	posix_file_t fp;
	char* data;
	int data_size;
};

struct CAFFileHeader {
	uint32_t mFileType;
	uint16_t mFileVersion;
	uint16_t mFileFlags;
};

#pragma pack(4)
struct CAFChunkHeader {
	uint32_t mChunkType;
	uint64_t mChunkSize;
};

struct CAFAudioFormat { 
	double mSampleRate;
	uint32_t mFormatID;
	uint32_t mFormatFlags;
	uint32_t mBytesPerPacket;
	uint32_t mFramesPerPacket;
	uint32_t mChannelsPerFrame;
	uint32_t mBitsPerChannel;
};
#pragma pack()

#pragma pack(4)
struct tmdat_header {
	uint64_t start;
	uint32_t duration;
	uint32_t deep_sleep;
	uint32_t snore;
	uint32_t audio_data_size;
	uint32_t reserve0;
	uint32_t reserve1;
};
#pragma pack()

#define posix_swap64(ll) (((ll) >> 56) |	\
	(((ll) & 0x00ff000000000000) >> 40) |	\
	(((ll) & 0x0000ff0000000000) >> 24) |	\
	(((ll) & 0x000000ff00000000) >> 8)    |	\
	(((ll) & 0x00000000ff000000) << 8)    |	\
	(((ll) & 0x0000000000ff0000) << 24) |	\
	(((ll) & 0x000000000000ff00) << 40) |	\
	(((ll) << 56)))

void swap_caf_audio_format(CAFAudioFormat& format);
bool valid_caf_file(tfopen_lock& lock, int64_t* data_chunk_offset, int64_t* data_chunk_size);

struct tchunk_2
{
	uint32_t magic;
	uint32_t length;
};

bool valid_mda_file(tfopen_lock& lock, int64_t* data_offset, int64_t* data_size);
std::pair<std::string, std::string> valid_files(const std::string& tag);
void generate_data_file(const std::string& utf8_file);
std::string form_file_stem(const time_t t);
std::string form_full_file_name(const time_t t, bool sound);

namespace preferences {

void wakeup_musics(std::vector<std::string>& musics);
std::string wakeup_music();
void set_wakeup_music(const std::string& music);

bool enable_snore();
void set_enable_snore(bool val);

int delay_sleep();
void set_delay_sleep(int minute);

std::string version();
void set_version(const std::string& str);

}

struct tscore_analysis
{
	static const int min_shallow_samples;

	struct talert {
		talert(int samples, int value)
			: samples(samples)
			, value(value)
		{}

		int samples;
		int value;
	};
	tscore_analysis()
		: mda_data_size(0)
		, caf_data_size2(0)
		, max_audio_value(0)
		, max_motion_value(0)
		, deep_sleep(0)
		, shallow_sleep(0)
	{
		memset(&header, 0, sizeof(header));
	}

	void from(tfopen_lock& lock);
	bool valid() const { return deep_sleep + shallow_sleep != 0; }

	int calculate_audio_score() const;
	int calculate_motion_score() const;
	int calculate_score() const { return calculate_audio_score() + calculate_motion_score(); }

	int calculate_max_value(tfopen_lock& lock, bool caf_session, int64_t data_offset, int64_t data_size);
	void calculate_alert(tfopen_lock& lock, bool caf_session, int64_t data_offset, int64_t data_size, const int max_value, const double ms_per_sample, std::vector<talert>& alerts);
	void analysis_motion(int64_t data_size, double* shallow, double* deep) const;

	bool in_deep_sleep(const int samples) const;

	std::vector<talert> audios;
	std::vector<talert> motions;
	tmdat_header header;
	int64_t mda_data_size;
	int64_t caf_data_size2;
	int max_audio_value;
	int max_motion_value;
	double deep_sleep;
	double shallow_sleep;
};

extern "C" void health_register();
extern "C" void health_write_sleep(time_t start, unsigned char* data, int len, int interval);
extern "C" void health_write_temperature(time_t start, unsigned char* data, int len, int interval);

extern time_t current_chart_time;


//
// ble
//
typedef struct {
	char* uuid;
	void* characteristic;
} SDL_BleCharacteristic;

typedef struct {
	char* uuid;
	SDL_BleCharacteristic* characteristics;
	int valid_characteristics;
} SDL_BleService;

typedef struct {
    char* name;
    char* uuid;
    SDL_BleService* services;
    int valid_services;
        
    int connected;
    unsigned int last_active;
        
    void* cookie;
} SDL_BlePeripheral;

typedef struct {
    void (*discover_peripheral)(SDL_BlePeripheral* peripheral);
    void (*release_peripheral)(const void* cookie);
    void (*connect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
    void (*disconnect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
    void (*discover_services)(const void* cookie, SDL_BlePeripheral* peripheral);
    void (*discover_characteristics)(const void* cookie, SDL_BleService* service, SDL_BlePeripheral* peripheral);
    void (*read_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, unsigned char* data, int len);
    void (*discover_descriptors)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
} SDL_BleCallbacks;

#endif
