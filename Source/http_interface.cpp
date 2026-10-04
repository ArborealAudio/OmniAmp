#pragma once

#include <JuceHeader.h>
#include "PluginEditor.h"

#if USE_HTTP_BACKEND
#include "http.h"
#include "http.c"
#endif

#define URL_BASE "https://arborealaudio.com/"
#define VERSIONS_URI "versions/index.json"
#define VERSIONS_DRAFT_URI "versions/draft/index.json"
#define LICENSE_URL_BASE "https://3pvj52nx17.execute-api.us-east-1.amazonaws.com/default/licenses/" 
#if PRODUCTION_BUILD
static http_String versions_url = STR_LIT(URL_BASE VERSIONS_URI);
#else
static http_String versions_url = STR_LIT(URL_BASE VERSIONS_DRAFT_URI);
#endif

#if JUCE_WINDOWS
#define OS_STRING "windows"
#define BIN_EXT ".exe"
#elif JUCE_MAC
#define OS_STRING "macos"
#define BIN_EXT ".dmg"
#elif JUCE_LINUX
#define OS_STRING "linux"
#define BIN_EXT ".tar.xz"
#endif

static const char *const plugin_name = ProjectInfo::projectName;
static const char *const plugin_version = ProjectInfo::versionString;

enum class UpdateCheckResult {
	NoUpdate,
	NewUpdate,
	ConnectionFailed,
};

struct UpdateCheck {
	UpdateCheckResult result;
	juce::String version;
	juce::String changes;
	juce::String bin_url;
	juce::String bin_checksum;
};

enum class LicenseCheckResult {
	None,
	CheckSucceeded,
	EmptyLicense,
	InvalidLicense,
	LicenseNotFound,
	ConnectionFailed,
};

enum class HttpThreadCmd {
	None,
	CheckUpdate,
	CheckLicense,
	DownloadUpdate,
};

struct HttpThread;

// Callback for when activation check has completed, whereby the parent UI
// can check the result & any accompanying information
typedef void(*ActivationCheckCb)(HttpThread *ctx, const juce::String &key, LicenseCheckResult result);
typedef void(*UpdateCheckCb)(HttpThread *ctx, UpdateCheck update);
typedef void(*DownloadProgressCb)(void *ctx, size_t bytes_read, size_t total_size);
typedef void(*DownloadFinishedCb)(HttpThread *ctx, size_t total_size, bool valid_file);

// The main functions which wrap specific HTTP client fuctionality (our in-house
// statically-linked Curl wrapper for Linux, or the default JUCE web connection elsewhere
static UpdateCheck check_for_update();
static LicenseCheckResult check_license(const juce::String &license);
static void download_update(HttpThread *ctx, const juce::String &url, const juce::String &file_path,
		DownloadProgressCb progress_cb);

struct HttpThread : juce::Thread {
	AudioProcessorEditor &editor;
	juce::CriticalSection lock;
	HttpThreadCmd cmd_buf[8];
	int cmd_head;
	int cmd_tail;
	juce::String license_key;
	LicenseCheckResult license_result;
	UpdateCheck update_check;
	ActivationCheckCb activation_cb;
	UpdateCheckCb update_cb;
	DownloadProgressCb dl_prog_cb;
	DownloadFinishedCb dl_finished_cb;

	HttpThread (AudioProcessorEditor &editor, ActivationCheckCb activation_cb, UpdateCheckCb update_cb,
			DownloadProgressCb dl_prog_cb, DownloadFinishedCb dl_finished_cb)
			: juce::Thread("HttpThread"), editor(editor), activation_cb(activation_cb),
			update_cb(update_cb), dl_prog_cb(dl_prog_cb), dl_finished_cb(dl_finished_cb)
	{
		cmd_head = cmd_tail = 0;
		memset(cmd_buf, 0, sizeof(cmd_buf));
		startThread();
	}

	~HttpThread() {
		stopThread(2000);
	}

	void push_cmd(HttpThreadCmd cmd) {
		const juce::ScopedLock sl(lock);
		if (cmd_head + 1 >= 8) {
			DBG("HTTP thread command buffer full");
			return;
		}
		cmd_buf[cmd_head] = cmd;
		cmd_head += 1;
	}

	// Call this from the message thread with the license key inputted by the user
	void set_license_string(const juce::String &license) {
		const juce::ScopedLock sl(lock);
		license_key = String(license);
	}

	void run() override {
		while(!threadShouldExit()) {
			if (cmd_head == 0) {
				wait(100);
				continue;
			}
			switch (cmd_buf[cmd_tail]) {
				case HttpThreadCmd::None:
					cmd_head = 0;
					continue;
				case HttpThreadCmd::CheckUpdate:
					{
						const juce::ScopedLock sl(lock);
						update_check = check_for_update();
					}
					juce::MessageManager::callAsync([this]{
						update_cb(this, update_check);
					});
					break;
				case HttpThreadCmd::CheckLicense:
					{
						const juce::ScopedLock sl(lock);
						license_result = check_license(license_key); 
					}
					juce::MessageManager::callAsync([this]{
						activation_cb(this, license_key, license_result);
					});
					break;
				case HttpThreadCmd::DownloadUpdate: {
					juce::String file_path = File::getSpecialLocation(File::userHomeDirectory).getFullPathName() + "/Downloads/" +
						ProjectInfo::projectName + "-" OS_STRING BIN_EXT;
					download_update(this, update_check.bin_url, file_path, dl_prog_cb);
					juce::File bin_file = File(file_path);
					// validate checksum
					juce::SHA256 file_hash = juce::SHA256(bin_file);
					const String &expect_hash = update_check.bin_checksum;
					bool valid = file_hash.toHexString() == expect_hash;
					size_t file_size = (size_t)bin_file.getSize();
					if (!valid) {
						if (!bin_file.deleteRecursively()) {
							DBG(__func__ << ": Failed to delete downloaded bin...");
						}
					}

					juce::MessageManager::callAsync([this, file_size, valid]{
						dl_finished_cb(this, file_size, valid);
					});

				} break;
			}
			cmd_tail += 1;
			if (cmd_tail == cmd_head) {
				cmd_head = cmd_tail = 0;
			}
		}
	}
};

static int parse_version_string(const juce::String &str) {
	juce::String numbers = str.removeCharacters(".");
	int version = 0;
	int len = numbers.length();
	int n = len - 1;
	for (int i = 0; i < len; ++i, --n) {
		int v = String(numbers[i]).getIntValue() - '0';
		version |= v << (n * 8);
	}
	return version;
}

static bool char_is_alphanumeric(char c) {
	return ((c >= '0' && c <= '9') && (c >= 'A' && c <= 'Z') && (c >= 'a' && c <= 'z'));
}

static bool validate_license_string(const juce::String &license) {
	bool result = true;
	for (auto c : license) {
		if (!juce::CharacterFunctions::isLetterOrDigit(c) && c != '-') {
			result = false;
			printf("Invalid license char: %c\n", c);
			break;
		}
	}
	return result;
}


static void parse_update_json(juce::var json, UpdateCheck *check) {
	juce::var plugin, latest, changes;
	plugin = json[plugin_name];
	check->version = plugin["version"];
	changes = plugin["changes"];
	check->bin_url = plugin["bin"][OS_STRING]["url"];
	check->bin_checksum = plugin["bin"][OS_STRING]["checksum"];
	if (changes.isArray()) {
		// create & populate a StringArray
	} else {
		check->changes = changes;
	}

	int latest_version_int = parse_version_string(latest);
	bool new_update = latest_version_int > ProjectInfo::versionNumber;
	DBG("Needs update: " << (int)new_update);
	if (new_update)
		check->result = UpdateCheckResult::NewUpdate;
}

#if USE_HTTP_BACKEND
static UpdateCheck check_for_update() {
	Http ctx = {};
	HttpOpt http_opt = {};
	http_opt.url = versions_url;

	UpdateCheck check = {};
	http_String response;

	printf("Checking for update at URL: %.*s\n", versions_url.len, versions_url.data);
	http_init(&ctx, &http_opt);
	http_send_request(&ctx);

	// parse response body
	http_ResponseCode res_code = ctx.response.code;
	if (res_code != 200) {
		check.result = UpdateCheckResult::ConnectionFailed;
		goto cleanup;
	}
	response = string_from_sb(&ctx.response.body);
	parse_update_json(juce::JSON::parse(juce::String(response.data, response.len)), &check);

#if !defined(NDEBUG)
	check.result = UpdateCheckResult::NewUpdate;
#endif

cleanup:
	http_deinit(&ctx);
	return check;
}


static LicenseCheckResult check_license(const juce::String &license) {
	LicenseCheckResult result = LicenseCheckResult::None;
	Http ctx = {};
	HttpOpt http_opt = {};
	juce::String license_url = juce::String(LICENSE_URL_BASE) + license;
	http_opt.url = (http_String){.data = license_url.toRawUTF8(), .len = license_url.length()};
	juce::var json, success;

	if (license.isEmpty()) {
		return LicenseCheckResult::EmptyLicense;
	}

	if (!validate_license_string(license)) {
		result = LicenseCheckResult::InvalidLicense;
		goto cleanup;
	}

	http_debug("Checking license %.*s at URL: %s\n", license.length(), license.toRawUTF8(), LICENSE_URL_BASE);

	{

		http_init(&ctx, &http_opt);
		http_add_header(&ctx, STR_LIT("x-api-key: " AWS_API_KEY));
		http_send_request(&ctx);
		
		// parse response body
		http_ResponseCode res_code = ctx.response.code;
		if (res_code != 200) {
			result = LicenseCheckResult::ConnectionFailed;
			goto cleanup;
		}
		{
			http_String body = string_from_sb(&ctx.response.body);
			json = JSON::parse(String(body.data, body.len));
			success = json["success"];
			if (success)
				result = LicenseCheckResult::CheckSucceeded;
			else
				result = LicenseCheckResult::LicenseNotFound;
		}
	}

cleanup:
	http_deinit(&ctx);
	return result;
}

static void download_update(HttpThread *http_thread, const juce::String &url,
		const juce::String &file_path, DownloadProgressCb progress_cb) {
	DBG("Saving update to: " << file_path);
	Http http = {};
	HttpOpt http_opt = {};
	http_opt.url = (http_String){.data = url.toRawUTF8(), .len = url.length()};
	http_opt.should_save_file = true;
	http_opt.save_file_path = (http_String){.data = file_path.toRawUTF8(), .len = file_path.length()};
	http_opt.dl_progress = progress_cb;
	http_opt.user = (void*)http_thread;
	http_init(&http, &http_opt);
	DBG("Downloading update from: " << url);
	http_send_request(&http);
	http_deinit(&http);
}
#endif // USE_HTTP_BACKEND


struct ActivationComponent : Component {
	HttpThread *http_thread;
	TextEditor text_edit;
	String key;
	String message = "Enter your license:";
	Colour message_color = Colours::white;
	String trial_message;
	TextButton submit{"Submit"}, close{"Close"}, buy{"Buy"};
	int64 trial_remaining;
	float color;

	ActivationComponent(HttpThread *_http_thread, int64 _trialRemaining)
		: trial_remaining(_trialRemaining), http_thread(_http_thread)
	{
		addAndMakeVisible(text_edit);
		text_edit.setFont(Font(18.f));
		text_edit.onReturnKey = [&] { request_check(); };
		text_edit.setTextToShowWhenEmpty("License", Colours::lightgrey);

		if (trial_remaining > 0)
			trial_message = RelativeTime::milliseconds(trial_remaining).getDescription() + " remaining in free trial";
		else
			trial_message = "Trial expired";

		addAndMakeVisible(submit);
		submit.onClick = [&] { request_check(); };

		addAndMakeVisible(close);
		close.onClick = [&] { setVisible(false); };

		addAndMakeVisible(buy);
		buy.onClick = [&] {
            URL("https://arborealaudio.com/plugins/omniamp")
                .launchInDefaultBrowser();
		};

	}

	// Called whenever license is inputted
	void request_check() {
		key = text_edit.getText();
		http_thread->set_license_string(key);
		http_thread->push_cmd(HttpThreadCmd::CheckLicense);
	}

	void paint(Graphics &g) override {
		g.setColour(Colours::black);
		g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.f);

		g.setFont(18.f);
		g.setColour(message_color);
		g.drawFittedText(message + "\n" + trial_message,
				getLocalBounds().removeFromTop(getHeight() * 0.3f),
				Justification::centred, 3);
	}

	void resized() override {
        auto b = getLocalBounds();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto editorPadding = BorderSize<int>(h * 0.4, 20, h * 0.4, 20);
        text_edit.setBoundsInset(editorPadding);

        auto buttons = b.removeFromBottom(h * 0.3f);
        submit.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
        close.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
        buy.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
	}
};

struct DownloadComponent : Component {
	HttpThread *http_thread;
	UpdateCheck check;
	TextButton download{"Download"}, close{"Close"};
	size_t bytes_read = 0, total_size = 0;

	enum class State {
		None,
		UpdateAvailable,
		Downloading,
		BadChecksum,
		Finished,
	};

	State state = State::None;

	const String update_check_messages[3] = {
		[(int)UpdateCheckResult::NoUpdate] = String("No Update"),
		[(int)UpdateCheckResult::NewUpdate] = String("New Update"),
		[(int)UpdateCheckResult::ConnectionFailed] = String("Connection Failed"),
	};

	DownloadComponent(HttpThread *_http_thread) : http_thread(_http_thread) {
		addChildComponent(download);
		download.onClick = [&] {
			http_thread->push_cmd(HttpThreadCmd::DownloadUpdate);
		};

		addAndMakeVisible(close);
		close.onClick = [&] {
			setVisible(false);
		};
	}

	void set_update_check_info(const UpdateCheck *check) {
		this->check = *check;
		bool update_available = this->check.result == UpdateCheckResult::NewUpdate;
		download.setVisible(update_available);
		if (update_available) {
			state = State::UpdateAvailable;
		}
	}

	void paint(Graphics &g) override
	{
		juce::String display;
		switch (state) {
		case State::UpdateAvailable:
			display = update_check_messages[(int)check.result] + "\n" + check.version + "\n" + check.changes;
			break;
		case State::Downloading: {
			double pct = 100.0 * ((double)bytes_read / (double)total_size);
			display = "Downloading...\n" + String(pct, 2) + "% || " + String(total_size / 1024) + " KB";
		} break;
		case State::BadChecksum:
			display = "Bad checksum. Download is corrupted. Please contact us at contact@arborealaudio.com.";
			break;
		case State::Finished:
			display = "Download complete.\nThe installer is in your Downloads folder. You must close your DAW to run the installation.";
			break;
		default: display = String(); break;
		}
		g.setColour(Colours::black);
		g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.f);

		g.setFont(18.f);
		g.setColour(Colours::white);
		g.drawFittedText(display, getLocalBounds().removeFromTop(getHeight() * 0.5f), Justification::centred, 5);
	}

	void resized() override
	{
		auto bot_third = getLocalBounds().removeFromBottom((float)getHeight() * 0.33f);
		download.setBounds(bot_third.removeFromLeft(getWidth() / 2).reduced(10));
		close.setBounds(bot_third.reduced(10));
	}
};

