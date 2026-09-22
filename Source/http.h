#pragma once

#include <curl/curl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if NDEBUG
#define http_debug
#else
#define http_debug printf
#endif

#define HTTP_MEM_CAP (1<<20)
static struct {
	char mem[HTTP_MEM_CAP];
	int head;
} _alloc;

typedef struct {
	const char *data;
	int len;
} http_String;

#define STR_LIT(cstr) (http_String){.data = cstr, .len = sizeof(cstr)}

const char *cstring_from_string(http_String str);
http_String string_from_cstring(const char *cstring);

typedef struct {
	char *data;
	int len;
	int cap;
} http_StringBuilder;

/* Create a StringBuilder with a given capacity. Capacity will never be
 * exceeded*/
http_StringBuilder sb_init(int capacity);
/* Append `len` characters in `ptr` to the StringBuilder */
int sb_append(http_StringBuilder *sb, const char *ptr, int len);
/* Obtain a String which acts as a "view" into the StringBuilder's content.
 * The returned String is a reference to the same data as the StringBuilder
 * and must not be freed by the caller. */
http_String string_from_sb(http_StringBuilder *sb);

// TODO Message buffer

typedef enum {
	UninitializedCode = 0,
	Ok = 200,
	MovedPermanently = 301,
	Found = 302,
	TempRedirect = 307,
	PermanentRedirect = 308,
	BadRequest = 400,
	Unauthorized = 401,
	Forbidden = 403,
	NotFound = 404,
	RequestTimeout = 408,
	InternalError = 500,
	NotImplemented = 501,
	BadGateway = 502,
} http_ResponseCode;

typedef struct {
	http_ResponseCode code;
	http_StringBuilder body;
	size_t content_length;
} http_Response;

typedef struct {
	CURL *curl;
	struct curl_slist *headers;
	http_String url;
	http_Response response;
	bool should_save_file;
	http_String save_file_path;
	FILE *save_file;
} Http;

/* Initialize an Http context. You should stack-allocate an Http context yourself
 * and supply it to this function.
 * `url` is the target URL the request is pointed to.*/
void http_init(Http *http, http_String url, bool should_save_file);
/* Deinit Http context */
void http_deinit(Http *http);
/* Set the target URL. This might be used to provide a new URL for the same Http context */
void http_set_url(Http *http, http_String url);
int http_add_header(Http *http, http_String header);
/* Will execute a GET request on the URL supplied to the Http context */
void http_send_request(Http *http);

#ifdef __cplusplus
}
#endif
