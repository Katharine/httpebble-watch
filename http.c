/*
 * httpebble stuff
 * Copyright (C) 2013 Katharine Berry
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pebble_os.h"
#include "http.h"

#define HTTP_URL_KEY 0xFFFF
#define HTTP_STATUS_KEY 0xFFFE
#define HTTP_COOKIE_KEY 0xFFFC
#define HTTP_CONNECT_KEY 0xFFFB
#define HTTP_USE_GET_KEY 0xFFFA
    
#define HTTP_APP_ID_KEY 0xFFF2
#define HTTP_COOKIE_STORE_KEY 0xFFF0
#define HTTP_COOKIE_LOAD_KEY 0xFFF1
#define HTTP_COOKIE_FSYNC_KEY 0xFFF3
#define HTTP_COOKIE_DELETE_KEY 0xFFF4
    
#define HTTP_TIME_KEY 0xFFF5
#define HTTP_UTC_OFFSET_KEY 0xFFF6
#define HTTP_IS_DST_KEY 0xFFF7
#define HTTP_TZ_NAME_KEY 0xFFF8

#define HTTP_LOCATION_KEY 0xFFE0
#define HTTP_LATITUDE_KEY 0xFFE1
#define HTTP_LONGITUDE_KEY 0xFFE2
#define HTTP_ALTITUDE_KEY 0xFFE3

static bool callbacks_registered;
static AppMessageCallbacksNode app_callbacks;
static HTTPCallbacks http_callbacks;
static int32_t our_app_id;

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
    dict_result = dict_write_int32(*iter_out, HTTP_COOKIE_KEY, cookie);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    dict_result = dict_write_int32(*iter_out, HTTP_APP_ID_KEY, our_app_id);
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

static void app_received_http_response(DictionaryIterator* received, bool success, void* context) {
    Tuple* status_tuple = dict_find(received, HTTP_STATUS_KEY);
    Tuple* cookie_tuple = dict_find(received, HTTP_COOKIE_KEY);
    if(status_tuple == NULL || cookie_tuple == NULL) {
        if(http_callbacks.failure) {
            http_callbacks.failure(0, 1000 + HTTP_INVALID_BRIDGE_RESPONSE, context);
        }
        return;
    }
    uint16_t status = status_tuple->value->int16;
    int32_t cookie = cookie_tuple->value->int32;
    if(!success) {
        if(http_callbacks.failure) {
            http_callbacks.failure(cookie, status, context);
        }
        return;
    }
    if(http_callbacks.success) {
        http_callbacks.success(cookie, status, received, context);
    }
}

static void app_received_cookie_set_response(int32_t request_id, void* context) {
    if(http_callbacks.cookie_set) {
        http_callbacks.cookie_set(request_id, true, context);
    }
}

static void app_received_cookie_get_response(int32_t request_id, DictionaryIterator* iter, void* context) {
    if(http_callbacks.cookie_batch_get) {
        http_callbacks.cookie_batch_get(request_id, iter, context);
    }
    if(http_callbacks.cookie_get) {
        Tuple* tuple = dict_read_first(iter);
        if(!tuple) return;
        do {
            // Don't pass along reserved values.
            if(tuple->key >= 0xF000 && tuple->key <= 0xFFFF) continue;
            http_callbacks.cookie_get(request_id, tuple, context);
        } while((tuple = dict_read_next(iter)));
    }
}

static void app_received_cookie_fsync_response(bool successful, void* context) {
    if(http_callbacks.cookie_fsync) {
        http_callbacks.cookie_fsync(successful, context);
    }
}
static void app_received_cookie_delete_response(int32_t request_id, void* context) {
    if(http_callbacks.cookie_delete) {
        http_callbacks.cookie_delete(request_id, true, context);
    }
}

static void app_received_time(uint32_t unixtime, DictionaryIterator *iter, void* context) {
    if(!http_callbacks.time) return;
    int32_t utc_offset;
    bool is_dst;
    const char* tz_name;
    Tuple* tuple = dict_find(iter, HTTP_UTC_OFFSET_KEY);
    if(!tuple) return;
    utc_offset = tuple->value->int32;
    tuple = dict_find(iter, HTTP_IS_DST_KEY);
    if(!tuple) return;
    is_dst = tuple->value->uint8;
    tuple = dict_find(iter, HTTP_TZ_NAME_KEY);
    if(!tuple) return;
    tz_name = tuple->value->cstring;
    http_callbacks.time(utc_offset, is_dst, unixtime, tz_name, context);
}

// Handy helper for getting floats out of ints.
struct alias_float {
    float f;
} __attribute__((__may_alias__));

float floatFromUint32(uint32_t value) {
    return ((struct alias_float*)&value)->f;
}

static void app_received_location(uint32_t accuracy_int, DictionaryIterator *iter, void* context) {
    if(!http_callbacks.location) return;
    float accuracy = floatFromUint32(accuracy_int);
    float latitude = 0.f;
    float longitude = 0.f;
    float altitude = 0.f;   

    Tuple* tuple = dict_read_first(iter);
    if(!tuple) return;
    do {
        switch(tuple->key) {
        case HTTP_LATITUDE_KEY:
            latitude = floatFromUint32(tuple->value->uint32);
            break;
        case HTTP_LONGITUDE_KEY:
            longitude = floatFromUint32(tuple->value->uint32);
            break;
        case HTTP_ALTITUDE_KEY:
            altitude = floatFromUint32(tuple->value->uint32);
            break;
        default:
            break;
        }
    } while((tuple = dict_read_next(iter)));
    http_callbacks.location(latitude, longitude, altitude, accuracy, context);  
}

static void app_received(DictionaryIterator* received, void* context) {
    // Reconnect message (special: no app id)
    Tuple* tuple = dict_find(received, HTTP_CONNECT_KEY);
    if(tuple && tuple->value->uint8) {
        if(http_callbacks.reconnect) {
            http_callbacks.reconnect(context);
        }
        return;
    }
    // Time response (special: no app id)
    tuple = dict_find(received, HTTP_TIME_KEY);
    if(tuple) {
        app_received_time(tuple->value->uint32, received, context);
        return;
    }
    // Location response (special: no app id)
    tuple = dict_find(received, HTTP_LOCATION_KEY);
    if(tuple) {
        app_received_location(tuple->value->uint32, received, context);
        return;
    }
    // Check for the app id
    tuple = dict_find(received, HTTP_APP_ID_KEY);
    if(!tuple) {
        return;
    }
    // Ignore it if it isn't ours.
    if(tuple->value->int32 != our_app_id) return;

    // HTTP responses
    tuple = dict_find(received, HTTP_URL_KEY);
    if(tuple) {
        app_received_http_response(received, tuple->value->uint8, context);
        return;
    }

    // Cookie set confirmation
    tuple = dict_find(received, HTTP_COOKIE_STORE_KEY);
    if(tuple) {
        app_received_cookie_set_response(tuple->value->int32, context);
        return;
    }

    // Cookie get response
    tuple = dict_find(received, HTTP_COOKIE_LOAD_KEY);
    if(tuple) {
        app_received_cookie_get_response(tuple->value->int32, received, context);
        return;
    }

    // Save response
    tuple = dict_find(received, HTTP_COOKIE_FSYNC_KEY);
    if(tuple) {
        app_received_cookie_fsync_response(tuple->value->uint8, context);
        return;
    }
    
    // Delete response
    tuple = dict_find(received, HTTP_COOKIE_DELETE_KEY);
    if(tuple) {
        app_received_cookie_delete_response(tuple->value->int32, context);
        return;
    }
}

static void app_dropped(void* context, AppMessageResult reason) {
    if(!http_callbacks.failure) return;
    http_callbacks.failure(0, 1000 + reason, context);
}

// Time stuff
HTTPResult http_time_request() {
    DictionaryIterator *iter;
    AppMessageResult app_result = app_message_out_get(&iter);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_uint8(iter, HTTP_TIME_KEY, 1);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    app_result = app_message_out_send();
    app_message_out_release();
    return app_result;
}

// Location stuff
HTTPResult http_location_request() {
    DictionaryIterator *iter;
    AppMessageResult app_result = app_message_out_get(&iter);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_uint8(iter, HTTP_LOCATION_KEY, 1);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    app_result = app_message_out_send();
    app_message_out_release();
    return app_result;
}

// Cookie stuff
void http_set_app_id(int32_t new_app_id) {
    our_app_id = new_app_id;
}

HTTPResult http_cookie_set_start(int32_t request_id, DictionaryIterator **iter_out) {
    AppMessageResult app_result = app_message_out_get(iter_out);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_int32(*iter_out, HTTP_COOKIE_STORE_KEY, request_id);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    dict_result = dict_write_int32(*iter_out, HTTP_APP_ID_KEY, our_app_id);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    return HTTP_OK;
}

HTTPResult http_cookie_set_end() {
    AppMessageResult result = app_message_out_send();
    app_message_out_release(); // We don't care if it's already released.
    return result;
}

HTTPResult http_cookie_get_multiple(int32_t request_id, uint32_t* keys, int32_t length) {
    // Basic setup
    DictionaryIterator *iter;
    AppMessageResult app_result = app_message_out_get(&iter);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_int32(iter, HTTP_COOKIE_LOAD_KEY, request_id);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    dict_result = dict_write_int32(iter, HTTP_APP_ID_KEY, our_app_id);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    // Add the keys
    for(int i = 0; i < length; ++i) {
        dict_result = dict_write_uint8(iter, keys[i], 1);
        if(dict_result != DICT_OK) {
            app_message_out_release();
            return dict_result << 12;
        }
    }
    // Send it.
    app_result = app_message_out_send();
    app_message_out_release();
    return app_result;
}

HTTPResult http_cookie_delete_multiple(int32_t request_id, uint32_t* keys, int32_t length) {
    // Basic setup
    DictionaryIterator *iter;
    AppMessageResult app_result = app_message_out_get(&iter);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_int32(iter, HTTP_COOKIE_DELETE_KEY, request_id);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    dict_result = dict_write_int32(iter, HTTP_APP_ID_KEY, our_app_id);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    // Add the keys
    for(int i = 0; i < length; ++i) {
        dict_result = dict_write_uint8(iter, keys[i], 1);
        if(dict_result != DICT_OK) {
            app_message_out_release();
            return dict_result << 12;
        }
    }
    // Send it.
    app_result = app_message_out_send();
    app_message_out_release();
    return app_result;
}

HTTPResult http_cookie_fsync() {
    DictionaryIterator *iter;
    AppMessageResult app_result = app_message_out_get(&iter);
    if(app_result != APP_MSG_OK) {
        return app_result;
    }
    DictionaryResult dict_result = dict_write_int32(iter, HTTP_APP_ID_KEY, our_app_id);
    if(dict_result != DICT_OK) {
        return dict_result << 12;
    }
    app_result = app_message_out_send();
    app_message_out_release();
    return app_result;
}

HTTPResult http_cookie_set_int(uint32_t request_id, uint32_t key, const void* integer, uint8_t width_bytes, bool is_signed) {
    DictionaryIterator *iter;
    HTTPResult http_result = http_cookie_set_start(request_id, &iter);
    if(http_result != HTTP_OK) {
        return http_result;
    }
    DictionaryResult dict_result = dict_write_int(iter, key, integer, width_bytes, is_signed);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    return http_cookie_set_end();
}

HTTPResult http_cookie_set_cstring(uint32_t request_id, uint32_t key, const char* value) {
    DictionaryIterator *iter;
    HTTPResult http_result = http_cookie_set_start(request_id, &iter);
    if(http_result != HTTP_OK) {
        return http_result;
    }
    DictionaryResult dict_result = dict_write_cstring(iter, key, value);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    return http_cookie_set_end();
}

HTTPResult http_cookie_set_data(uint32_t request_id, uint32_t key, const uint8_t* const value, int length) {
    DictionaryIterator *iter;
    HTTPResult http_result = http_cookie_set_start(request_id, &iter);
    if(http_result != HTTP_OK) {
        return http_result;
    }
    DictionaryResult dict_result = dict_write_data(iter, key, value, length);
    if(dict_result != DICT_OK) {
        app_message_out_release();
        return dict_result << 12;
    }
    return http_cookie_set_end();
}

HTTPResult http_cookie_get(uint32_t request_id, uint32_t key) {
    return http_cookie_get_multiple(request_id, &key, 1);
}

HTTPResult http_cookie_delete(uint32_t request_id, uint32_t key) {
    return http_cookie_delete_multiple(request_id, &key, 1);
}

HTTPResult http_cookie_set_int32(uint32_t request_id, uint32_t key, int32_t value) {
    return http_cookie_set_int(request_id, key, &value, 4, true);
}
HTTPResult http_cookie_set_uint32(uint32_t request_id, uint32_t key, uint32_t value) {
    return http_cookie_set_int(request_id, key, &value, 4, false);
}
HTTPResult http_cookie_set_int16(uint32_t request_id, uint32_t key, int16_t value) {
    return http_cookie_set_int(request_id, key, &value, 2, true);
}
HTTPResult http_cookie_set_uint16(uint32_t request_id, uint32_t key, uint16_t value) {
    return http_cookie_set_int(request_id, key, &value, 2, false);
}
HTTPResult http_cookie_set_int8(uint32_t request_id, uint32_t key, int8_t value) {
    return http_cookie_set_int(request_id, key, &value, 1, true);
}
HTTPResult http_cookie_set_uint8(uint32_t request_id, uint32_t key, uint8_t value) {
    return http_cookie_set_int(request_id, key, &value, 1, false);
}
