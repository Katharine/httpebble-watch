#include "pebble_os.h"
#include "http.h"

static bool callbacks_registered;
static AppMessageCallbacksNode app_callbacks;
static HTTPCallbacks http_callbacks;

static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context);
static void app_received(DictionaryIterator* received, void* context);
static void app_dropped(void* context, AppMessageResult reason);

HTTPResult http_out_get(const char* url, int32_t cookie, DictionaryIterator **iter_out) {
  AppMessageResult app_result = app_message_out_get(iter_out);
	if(app_result != APP_MSG_OK) {
		return app_result;
	}
	DictionaryResult dict_result = dict_write_cstring(*iter_out, HTTP_URL_KEY, url);
	if(dict_result != DICT_OK) {
		return dict_result << 12;
	}
	dict_result = dict_write_int(*iter_out, HTTP_COOKIE_KEY, &cookie, 4, true);
	if(dict_result != DICT_OK) {
		return dict_result << 12;
	}
	return HTTP_OK;
}


HTTPResult http_out_send() {
	AppMessageResult result = app_message_out_send();
	app_message_out_release(); // We don't care if it's already released.
	return result;
}

bool http_register_callbacks(HTTPCallbacks callbacks, void* context) {
	http_callbacks = callbacks;
	if(callbacks_registered) {
		if(app_message_deregister_callbacks(&app_callbacks) == APP_MSG_OK)
			callbacks_registered = false;
	}
	if(!callbacks_registered) {
		app_callbacks = (AppMessageCallbacksNode){
			.callbacks = {
				.out_failed = app_send_failed,
				.in_received = app_received,
				.in_dropped = app_dropped,
			},
			.context = context
		};
		if(app_message_register_callbacks(&app_callbacks) == APP_MSG_OK)
			callbacks_registered = true;
	}
	return callbacks_registered;
}

static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context) {
	if(!http_callbacks.failure) return;
	http_callbacks.failure(0, 1000 + reason, context);
}

static void app_received(DictionaryIterator* received, void* context) {
	Tuple* connect_tuple = dict_find(received, HTTP_CONNECT_KEY);
	if(connect_tuple && connect_tuple->value->uint8) {
		if(http_callbacks.reconnect) {
			http_callbacks.reconnect(context);
		}
		return;
	}
	Tuple* success_tuple = dict_find(received, HTTP_SUCCESS_KEY);
	Tuple* status_tuple = dict_find(received, HTTP_STATUS_KEY);
	Tuple* cookie_tuple = dict_find(received, HTTP_COOKIE_KEY);
	if(success_tuple == NULL || status_tuple == NULL || cookie_tuple == NULL) {
		if(http_callbacks.failure) {
			http_callbacks.failure(0, 1000 + HTTP_INVALID_BRIDGE_RESPONSE, context);
		}
		return;
	}
	bool success = success_tuple->value->int8;
	uint16_t status = status_tuple->value->int16;
	if(!success) {
		if(http_callbacks.failure) {
			http_callbacks.failure(cookie_tuple->value->int32, status, context);
		}
		return;
	}
	if(http_callbacks.success) {
		http_callbacks.success(cookie_tuple->value->int32, status, received, context);
	}
}

static void app_dropped(void* context, AppMessageResult reason) {
	if(!http_callbacks.failure) return;
	http_callbacks.failure(0, 1000 + reason, context);
}
