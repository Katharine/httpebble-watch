#ifndef HTTP_H
#define HTTP_H

#define HTTP_URL_KEY 0xFFFF
#define HTTP_STATUS_KEY 0xFFFE
#define HTTP_SUCCESS_KEY 0xFFFD
#define HTTP_COOKIE_KEY 0xFFFC
#define HTTP_CONNECT_KEY 0xFFFB

typedef enum {
  HTTP_OK 							= 0,
	HTTP_SEND_TIMEOUT 					= APP_MSG_SEND_TIMEOUT,
	HTTP_SEND_REJECTED					= APP_MSG_SEND_REJECTED,
	HTTP_NOT_CONNECTED					= APP_MSG_NOT_CONNECTED,
	HTTP_BRIDGE_NOT_RUNNING				= APP_MSG_APP_NOT_RUNNING,
	HTTP_INVALID_ARGS					= APP_MSG_INVALID_ARGS,
	HTTP_BUSY							= APP_MSG_BUSY,
	HTTP_BUFFER_OVERFLOW				= APP_MSG_BUFFER_OVERFLOW,
	HTTP_ALREADY_RELEASED				= APP_MSG_ALREADY_RELEASED,
	HTTP_CALLBACK_ALREADY_REGISTERED	= APP_MSG_CALLBACK_ALREADY_REGISTERED,
	HTTP_CALLBACK_NOT_REGISTERED		= APP_MSG_CALLBACK_NOT_REGISTERED,
	HTTP_NOT_ENOUGH_STORAGE				= DICT_NOT_ENOUGH_STORAGE << 12,
	HTTP_INVALID_DICT_ARGS				= DICT_INVALID_ARGS << 12,
	HTTP_INTERNAL_INCONSISTENCY			= DICT_INTERNAL_INCONSISTENCY << 12,
	HTTP_INVALID_BRIDGE_RESPONSE		= 1 << 17
} HTTPResult;

typedef void(*HTTPRequestFailedHandler)(int32_t cookie, int http_status, void* context);
typedef void(*HTTPRequestSucceededHandler)(int32_t cookie, int http_status, DictionaryIterator* sent, void* context);
typedef void(*HTTPReconnectedHandler)(void* context);

typedef struct {
	HTTPRequestFailedHandler failure;
	HTTPRequestSucceededHandler success;
	HTTPReconnectedHandler reconnect;
} HTTPCallbacks;


HTTPResult http_out_get(const char* url, int32_t cookie, DictionaryIterator **iter_out);
HTTPResult http_out_send();
bool http_register_callbacks(HTTPCallbacks callbacks, void* context);

#endif
