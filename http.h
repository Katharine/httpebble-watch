/*
 * httpebble header
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

#ifndef HTTP_H
#define HTTP_H

#define HTTP_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }

// Shared values.
typedef enum {
    HTTP_OK                             = 0,
    HTTP_SEND_TIMEOUT                   = APP_MSG_SEND_TIMEOUT,
    HTTP_SEND_REJECTED                  = APP_MSG_SEND_REJECTED,
    HTTP_NOT_CONNECTED                  = APP_MSG_NOT_CONNECTED,
    HTTP_BRIDGE_NOT_RUNNING             = APP_MSG_APP_NOT_RUNNING,
    HTTP_INVALID_ARGS                   = APP_MSG_INVALID_ARGS,
    HTTP_BUSY                           = APP_MSG_BUSY,
    HTTP_BUFFER_OVERFLOW                = APP_MSG_BUFFER_OVERFLOW,
    HTTP_ALREADY_RELEASED               = APP_MSG_ALREADY_RELEASED,
    HTTP_CALLBACK_ALREADY_REGISTERED    = APP_MSG_CALLBACK_ALREADY_REGISTERED,
    HTTP_CALLBACK_NOT_REGISTERED        = APP_MSG_CALLBACK_NOT_REGISTERED,
    HTTP_NOT_ENOUGH_STORAGE             = DICT_NOT_ENOUGH_STORAGE << 12,
    HTTP_INVALID_DICT_ARGS              = DICT_INVALID_ARGS << 12,
    HTTP_INTERNAL_INCONSISTENCY         = DICT_INTERNAL_INCONSISTENCY << 12,
    HTTP_INVALID_BRIDGE_RESPONSE        = 1 << 17
} HTTPResult;

// HTTP Request callbacks
typedef void(*HTTPRequestFailedHandler)(int32_t request_id, int http_status, void* context);
typedef void(*HTTPRequestSucceededHandler)(int32_t request_id, int http_status, DictionaryIterator* sent, void* context);
typedef void(*HTTPReconnectedHandler)(void* context);
// Local cookie callbacks
typedef void(*HTTPPhoneCookieBatchGetHandler)(int32_t request_id, DictionaryIterator* result, void* context);
typedef void(*HTTPPhoneCookieGetHandler)(int32_t request_id, Tuple* result, void* context);
typedef void(*HTTPPhoneCookieSetHandler)(int32_t request_id, bool successful, void* context);
typedef void(*HTTPPhoneCookieFsyncHandler)(bool successful, void* context);
typedef void(*HTTPPhoneCookieDeleteHandler)(int32_t request_id, bool success, void* context);
// Time callback
typedef void(*HTTPTimeHandler)(int32_t utc_offset_seconds, bool is_dst, uint32_t unixtime, const char* tz_name, void* context);
// Location callback
typedef void(*HTTPLocationHandler)(float latitude, float longitude, float altitude, float accuracy, void* context);

// HTTP stuff
typedef struct {
    HTTPRequestFailedHandler failure;
    HTTPRequestSucceededHandler success;
    HTTPReconnectedHandler reconnect;
    HTTPPhoneCookieGetHandler cookie_get;
    HTTPPhoneCookieBatchGetHandler cookie_batch_get;
    HTTPPhoneCookieSetHandler cookie_set;
    HTTPPhoneCookieFsyncHandler cookie_fsync;
    HTTPPhoneCookieDeleteHandler cookie_delete;
    HTTPTimeHandler time;
    HTTPLocationHandler location;
} HTTPCallbacks;

// HTTP requests
HTTPResult http_out_get(const char* url, int32_t request_id, DictionaryIterator **iter_out);
HTTPResult http_out_send();
bool http_register_callbacks(HTTPCallbacks callbacks, void* context);

// Time information
HTTPResult http_time_request();

// Location information
HTTPResult http_location_request();

// Local cookies
// Basic API
void http_set_app_id(int32_t id);
HTTPResult http_cookie_set_start(int32_t request_id, DictionaryIterator **iter_out);
HTTPResult http_cookie_set_end();
HTTPResult http_cookie_get_multiple(int32_t request_id, uint32_t* keys, int32_t length);
HTTPResult http_cookie_delete_multiple(int32_t request_id, uint32_t* keys, int32_t length);
HTTPResult http_cookie_fsync();
// Convenience methods
HTTPResult http_cookie_set_int(uint32_t request_id, uint32_t key, const void* integer, uint8_t width_bytes, bool is_signed);
HTTPResult http_cookie_set_cstring(uint32_t request_id, uint32_t key, const char* value);
HTTPResult http_cookie_set_data(uint32_t request_id, uint32_t key, const uint8_t* const value, int length);
HTTPResult http_cookie_get(uint32_t request_id, uint32_t key);
HTTPResult http_cookie_delete(uint32_t request_id, uint32_t key);
// Convenience convenience methods
HTTPResult http_cookie_set_int32(uint32_t request_id, uint32_t key, int32_t value);
HTTPResult http_cookie_set_uint32(uint32_t request_id, uint32_t key, uint32_t value);
HTTPResult http_cookie_set_int16(uint32_t request_id, uint32_t key, int16_t value);
HTTPResult http_cookie_set_uint16(uint32_t request_id, uint32_t key, uint16_t value);
HTTPResult http_cookie_set_int8(uint32_t request_id, uint32_t key, int8_t value);
HTTPResult http_cookie_set_uint8(uint32_t request_id, uint32_t key, uint8_t value);

#endif
