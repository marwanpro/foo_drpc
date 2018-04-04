#include <cmath>
#include <chrono>
#include "Plugin.h"


DECLARE_COMPONENT_VERSION(
"foo_drpc",
"0.4.2.0",
"Foobar2000 music status for Discord Rich Presence! (c) 2018 - ultrasn0w / Automne von Einzbern");

static initquit_factory_t<foo_drpc> foo_interface;
static std::chrono::time_point<std::chrono::high_resolution_clock> lastT;
static std::chrono::time_point<std::chrono::high_resolution_clock> req;
static bool errored; // Still kind of unused
static bool connected;
static bool first;

foo_drpc::foo_drpc()
{
	errored = false;
	connected = true;
	first = true;
}

foo_drpc::~foo_drpc() = default;

void foo_drpc::on_init()
{
	static_api_ptr_t<play_callback_manager> pcm;
	pcm->register_callback(
		this,
		play_callback::flag_on_playback_starting |
		play_callback::flag_on_playback_stop |
		play_callback::flag_on_playback_pause |
		play_callback::flag_on_playback_new_track |
		play_callback::flag_on_playback_edited |
		play_callback::flag_on_playback_dynamic_info_track,
		false);
	discordInit();
	initDiscordPresence();
}

void foo_drpc::on_quit()
{
	Discord_ClearPresence();
	Discord_Shutdown();
	static_api_ptr_t<play_callback_manager>()->unregister_callback(this);
}

void foo_drpc::on_playback_starting(playback_control::t_track_command command, bool pause)
{
	if (!connected) return;

	if (pause)
	{
		discordPresence.smallImageKey = "pause";
	}
	else
	{
		switch (command)
		{
		case playback_control::track_command_play:
		case playback_control::track_command_next:
		case playback_control::track_command_prev:
		case playback_control::track_command_resume:
		case playback_control::track_command_rand:
		case playback_control::track_command_settrack:
			discordPresence.smallImageKey = "play";
			break;
		}
	}

	
	metadb_handle_ptr track;
	static_api_ptr_t<playback_control> pbc;
	if (pbc->get_now_playing(track)) on_playback_new_track(track);
	updateDiscordPresence();
}

void foo_drpc::on_playback_stop(playback_control::t_stop_reason reason)
{
	if (!connected) return;

	switch (reason)
	{
	case playback_control::stop_reason_user:
	case playback_control::stop_reason_eof:
	case playback_control::stop_reason_shutting_down:
		discordPresence.state = "Stopped";
		discordPresence.smallImageKey = "stop";
		break;
	}
	updateDiscordPresence();
}

void foo_drpc::on_playback_pause(bool pause)
{
	if (!connected) return;

	discordPresence.smallImageKey = (pause ? "pause" : "play");
	updateDiscordPresence();
}

void foo_drpc::on_playback_new_track(metadb_handle_ptr track)
{
	if (!connected) return;

	service_ptr_t<titleformat_object> script1;
	service_ptr_t<titleformat_object> script2;
	pfc::string8 detailLine = "%artist% - %album%";
	pfc::string8 stateLine = "%title%";

	if (!static_api_ptr_t<titleformat_compiler>()->compile(script1, detailLine)) return;
	if (!static_api_ptr_t<titleformat_compiler>()->compile(script2, stateLine)) return;
		
	static_api_ptr_t<playback_control> pbc;

	pbc->playback_format_title_ex(
		track,
		nullptr,
		detailLine,
		script1,
		nullptr,
		playback_control::display_level_titles);

	pbc->playback_format_title_ex(
		track,
		nullptr,
		stateLine,
		script2,
		nullptr,
		playback_control::display_level_titles);


	if (detailLine.get_length() + 1 > 128) return;
	if (stateLine.get_length() + 1 > 128) return;
	
	static char details[128];
	const size_t destination_size1 = sizeof(details);
	strncpy_s(details, detailLine.get_ptr(), destination_size1);
	details[destination_size1 - 1] = '\0';


	static char state[128];
	const size_t destination_size2 = sizeof(state);
	strncpy_s(state, stateLine.get_ptr(), destination_size2);
	state[destination_size2 - 1] = '\0';
						
	discordPresence.smallImageKey = "play";
	discordPresence.details = details;
	discordPresence.state = state;
	updateDiscordPresence();
}

void foo_drpc::on_playback_dynamic_info_track(const file_info& info)
{
	metadb_handle_ptr track;
	static_api_ptr_t<playback_control> pbc;
	if (pbc->get_now_playing(track))
	{
		on_playback_new_track(track);
	}
}

void foo_drpc::initDiscordPresence()
{
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "nee Darlin' \"Aishiteru\"";
	discordPresence.details = "Automne / ultrasn0w";
	discordPresence.largeImageKey = "logo";
	discordPresence.smallImageKey = "stop";
	// discordPresence.partyId = "party1234";
	// discordPresence.partySize = 1;
	// discordPresence.partyMax = 6;

	updateDiscordPresence();
}

void foo_drpc::updateDiscordPresence()
{
	/*
	if (first) {
		lastT = std::chrono::high_resolution_clock::now();
		first = false;
		Discord_UpdatePresence(&discordPresence);
	}
	else {
		req = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = req - lastT;
		// spam protection
		if (elapsed.count() > 0.42) {
			Discord_UpdatePresence(&discordPresence);
			lastT = std::chrono::high_resolution_clock::now();
		}
	}
	*/
	Discord_UpdatePresence(&discordPresence);
#ifdef DISCORD_DISABLE_IO_THREAD
	Discord_UpdateConnection();
#endif
	Discord_RunCallbacks();
}

void connectedF()
{
	connected = true;
}

void disconnectedF(int errorCode, const char* message)
{
	connected = false;
}

void erroredF(int errorCode, const char* message)
{
	errored = true;
}

void foo_drpc::discordInit()
{
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = connectedF;
	handlers.disconnected = disconnectedF;
	handlers.errored = erroredF;
	// handlers.joinGame = [](const char* joinSecret) {};
	// handlers.spectateGame = [](const char* spectateSecret) {};
	// handlers.joinRequest = [](const DiscordJoinRequest* request) {};
	Discord_Initialize(APPLICATION_ID, &handlers, 1, nullptr);
}

// thx SuperKoko (unused)
LPSTR foo_drpc::UnicodeToAnsi(LPCWSTR s)
{
	if (s == nullptr) return nullptr;
	const int cw = lstrlenW(s);
	if (cw == 0) { CHAR *psz = new CHAR[1]; *psz = '\0'; return psz; }
	int cc = WideCharToMultiByte(CP_UTF8, 0, s, cw, nullptr, 0, nullptr, nullptr);
	if (cc == 0) return nullptr;
	CHAR *psz = new CHAR[cc + 1];
	cc = WideCharToMultiByte(CP_UTF8, 0, s, cw, psz, cc, nullptr, nullptr);
	if (cc == 0) { delete[] psz; return nullptr; }
	psz[cc] = '\0';
	return psz;
}
