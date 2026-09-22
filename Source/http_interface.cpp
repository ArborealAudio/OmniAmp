#pragma once

#include <JuceHeader.h>
#include "PluginEditor.h"

#include "http.h"
#include "http.c"

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

// Update check API

enum class UpdateCheckResult {
	NoUpdate,
	NewUpdate,
	ConnectionFailed,
};

struct UpdateCheck {
	UpdateCheckResult result;
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
			http_debug("Invalid license char: %c\n", c);
			break;
		}
	}
	return result;
}

static UpdateCheck check_for_update() {
	Http ctx = {};
	UpdateCheck check = {};
	http_String response;
	juce::var json, plugin, latest, changes;

	http_debug("Checking for update at URL: %.*s\n", versions_url.len, versions_url.data);
	http_init(&ctx, versions_url, false);
	http_send_request(&ctx);

	// parse response body
	http_ResponseCode res_code = ctx.response.code;
	if (res_code != 200) {
		check.result = UpdateCheckResult::ConnectionFailed;
		goto cleanup;
	}
	response = string_from_sb(&ctx.response.body);
	json = juce::JSON::parse(String(response.data, response.len));
	plugin = json.getProperty(plugin_name, var());
	latest = plugin["version"];
	changes = plugin["changes"];
	check.bin_url = plugin["bin"][OS_STRING]["url"];
	check.bin_checksum = plugin["bin"][OS_STRING]["checksum"];
	if (changes.isArray()) {
		// create & populate a StringArray
	} else {
		check.changes = changes;
	}

	{
		int latest_version_int = parse_version_string(latest);
		bool new_update = latest_version_int > ProjectInfo::versionNumber;
		http_debug("Needs update: %d\n", new_update);
		if (new_update)
			check.result = UpdateCheckResult::NewUpdate;
	}

cleanup:
	http_deinit(&ctx);
	return check;
}

static LicenseCheckResult check_license(const juce::String &license) {
	LicenseCheckResult result = LicenseCheckResult::None;
	Http ctx = {};
	http_String response;
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
		juce::String license_url = juce::String(LICENSE_URL_BASE) + license;

		http_init(&ctx, (http_String){.data = license_url.toRawUTF8(), .len = license_url.length()}, false);
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

struct ActivationComponent;
typedef void(*ActivationCheckCb)(void *editor, String key);

struct ActivationComponent : Component {
	void *editor;
	TextEditor text_edit;
	String message = "Enter your license:";
	Colour message_color = Colours::white;
	String trial_message;
	TextButton submit{"Submit"}, close{"Close"}, buy{"Buy"};
	int64 trial_remaining;
	float color;
	LicenseCheckResult check_result;

	ActivationCheckCb on_activation_check;

	ActivationComponent(void *editor, int64 _trialRemaining, ActivationCheckCb check_cb)
		: editor(editor), trial_remaining(_trialRemaining), on_activation_check(check_cb)
	{
		addAndMakeVisible(text_edit);
		text_edit.setFont(Font(18.f));
		text_edit.onReturnKey = [&] { refresh(); };
		text_edit.setTextToShowWhenEmpty("License", Colours::lightgrey);

		if (trial_remaining > 0)
			trial_message = RelativeTime::milliseconds(trial_remaining).getDescription() + " remaining in free trial";
		else
			trial_message = "Trial expired";

		addAndMakeVisible(submit);
		submit.onClick = [&] { refresh(); };

		addAndMakeVisible(close);
		close.onClick = [&] { setVisible(false); };

		addAndMakeVisible(buy);
		buy.onClick = [&] {
            URL("https://arborealaudio.com/plugins/omniamp")
                .launchInDefaultBrowser();
		};
	}

	// Called whenever license is inputted
	void refresh() {
		String key = text_edit.getText();
		check_result = check_license(key); 
		message_color = Colours::white;
		switch (check_result) {
		case LicenseCheckResult::None: 
			message = "Activation not run. Try again.";
			message_color = Colours::red;
			break;
		case LicenseCheckResult::CheckSucceeded:
			message = "License activated! Thank you!";
			trial_message = String();
			break;
		case LicenseCheckResult::EmptyLicense:
			message = "Actually enter a license...";
			message_color = Colour::fromHSL(color / 360.f, 1.f, 0.75f, 1.f);
			color += 45.f;
			color = fmodf(color, 360.f);
			break;
		case LicenseCheckResult::InvalidLicense:
			message = "Invalid/malformed license";
			message_color = Colours::red;
			break;
		case LicenseCheckResult::LicenseNotFound:
			message = "License not found";
			message_color = Colours::red;
			break;
		case LicenseCheckResult::ConnectionFailed:
			message = "Connection Failed";
			message_color = Colours::red;
			break;
		}

		bool success = check_result == LicenseCheckResult::CheckSucceeded;

		if (!success) {
			if (trial_remaining > 0) {
				trial_message = RelativeTime::milliseconds(trial_remaining).getDescription() + " remaining in free trial";
			} else if (trial_remaining <= 0) {
				trial_message = "Trial expired";
			}
			text_edit.clear();
		} else {
			text_edit.setVisible(false);
			submit.setVisible(false);
			buy.setVisible(false);
		}

		on_activation_check(this->editor, key);

		repaint();
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
