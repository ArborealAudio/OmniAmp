#include "http.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#if NDEBUG
#define http_debug
#else
#define http_debug printf
#endif

#define internal static
#define global static

internal size_t temp_begin() {
	return _alloc.head;
}

internal void temp_end(size_t save) {
	_alloc.head = save;
}

internal void *http_alloc(size_t size) {
	if (_alloc.head + size >= HTTP_MEM_CAP) {
		return 0;
	}

#define HTTP_DEFAULT_ALIGN (sizeof(void*))

	void *ptr = (void*)&_alloc.mem[_alloc.head];
	uintptr_t aligned = (uintptr_t)ptr;
	uintptr_t mod = (uintptr_t)ptr & (HTTP_DEFAULT_ALIGN - 1);
	if (mod != 0) {
		aligned += HTTP_DEFAULT_ALIGN - mod;
	}
	uintptr_t delta = (uintptr_t)aligned - (uintptr_t)ptr;

	memset((void*)aligned, 0, size);

	_alloc.head += size + delta;

	return (void*)aligned;
}

internal void http_alloc_reset() {
	_alloc.head = 0;
}

const char *cstring_from_string(http_String str) {
	char *cstr = (char*)http_alloc(str.len + 1);
	memcpy(cstr, str.data, str.len);
	cstr[str.len] = 0;
	return cstr;
}

http_StringBuilder sb_init(int capacity) {
	char *data = (char*)http_alloc(capacity);
	return (http_StringBuilder){
		.data = data,
		.len = 0,
		.cap = capacity,
	};
}

int sb_append(http_StringBuilder *sb, const char *ptr, int len) {
	int wrote = len;
	if (sb->len + len >= sb->cap) {
		http_debug("http_StringBuilder at capacity\n");
		wrote = sb->cap - sb->len;
		sb->len = sb->cap;
	}
	memcpy(sb->data + sb->len, ptr, len);
	sb->len += len;
	return wrote;
}

http_String string_from_sb(http_StringBuilder *sb) {
	return (http_String){
		.data = sb->data,
		.len = sb->len,
	};
}

/* Callback used to write response to an internal buffer */
internal size_t get_cb(char *ptr, size_t item_size, size_t n, void *user) {
	Http *http = (Http*)user;
	return sb_append(&http->response.body, ptr, n);
}

/* Callback used to write response to a file */
internal size_t save_cb(char *ptr, size_t item_size, size_t n, void *user) {
	Http *http = (Http*)user;
	size_t write_size = item_size * n;

	http_debug("Downloading %zu bytes...\n", write_size);
	fwrite(ptr, 1, write_size, http->save_file);
	return write_size;
}

void http_init(Http *http, http_String url) {
	http->url = url;
	http->response.body = sb_init(4096);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	http->curl = curl_easy_init();
	size_t save = temp_begin();
	curl_easy_setopt(http->curl, CURLOPT_URL, cstring_from_string(url));
	temp_end(save);
	if (http->should_save_file) {
		curl_easy_setopt(http->curl, CURLOPT_WRITEFUNCTION, save_cb);
	} else {
		curl_easy_setopt(http->curl, CURLOPT_WRITEFUNCTION, get_cb);
	}
	curl_easy_setopt(http->curl, CURLOPT_WRITEDATA, http);
}

void http_deinit(Http *http) {
	if (http->headers)
		curl_slist_free_all(http->headers);
	if (http->save_file)
		fclose(http->save_file);
	curl_global_cleanup();
	http_alloc_reset();
}

void http_set_url(Http *http, http_String url) {
	size_t save = temp_begin();
	curl_easy_setopt(http->curl, CURLOPT_URL, cstring_from_string(url));
	temp_end(save);
}

int http_add_header(Http *http, http_String header) {
	size_t save = temp_begin();
	http->headers = curl_slist_append(http->headers, cstring_from_string(header));
	temp_end(save);
	if (!http->headers) {
		http_debug("Add header failed\n");
		return 0;
	}

	return 1;
}

void http_send_request(Http *http) {
	if (http->headers) {
		curl_easy_setopt(http->curl, CURLOPT_HTTPHEADER, http->headers);
	}
	if (http->should_save_file) {
		size_t save = temp_begin();
		http->save_file = fopen(cstring_from_string(http->save_file_path), "wb");
		if (!http->save_file) {
			http_debug("Failed to open save file %.*s\n", http->save_file_path.len,
					http->save_file_path.data);
			return;
		}
		temp_end(save);
	}
	CURLcode res = curl_easy_perform(http->curl);
	if (res != CURLE_OK) {
		http_debug("libcurl request failed: %s\n", curl_easy_strerror(res));
		return;
	}

	{
		long response;
		res = curl_easy_getinfo(http->curl, CURLINFO_RESPONSE_CODE, &response);
		http->response.code = (http_ResponseCode)response;
	}
	{
		curl_off_t dl_len;
		res = curl_easy_getinfo(http->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dl_len);
		http_debug("DL SIZE: %" CURL_FORMAT_CURL_OFF_T "\n", dl_len);
		http->response.content_length = (size_t)dl_len;
	}
}

#if 0
int main() {
	{
		Http http = {0};
		http_init(&http, STR_LIT("https://3pvj52nx17.execute-api.us-east-1.amazonaws.com/default/licenses/********************"));

		http_String api_key = STR_LIT("x-api-key: ****************************************");

		http_add_header(&http, api_key);
		http_send_request(&http);
		http_String body = string_from_sb(&http.response.body);
		printf("Response: %d\n%.*s\n", http.response.code, body.len, body.data);
		http_deinit(&http);
	}

	{
		Http http = {
			.should_save_file = true,
			.save_file_path = STR_LIT("OmniAmp-linux.tar.xz"),
		};
		http_String dl_link = STR_LIT("https://arborealaudioinstallers.s3.amazonaws.com/omniamp/OmniAmp-linux.tar.xz");
		http_init(&http, dl_link);
		http_send_request(&http);
		http_debug("Response: %d\n", http.response.code);
		http_deinit(&http);
	}


	return 0;
}
#endif
