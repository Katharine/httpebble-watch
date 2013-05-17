httpebble
=========

httpebble is a scheme for communicating with the internet from the Pebble, using a generic protocol
and without any application-specific code running on the phone. It also provides a mechanism for
storing persistent data, reading timezone information, and getting the user's approximate location.

Due to restrictions of the two-way communication SDK, only one application can communicate with the
watch simultaneously. That means that using this HTTP support will break RunKeeper or any other app
with a custom app. There is no workaround for this at this time. However, by using the same UUID for
all apps that want to use HTTP requests, any number of watch apps can use this functionality simultaneously.

Therefore, your app must have the UUID `9141B628-BC89-498E-B147-049F49C099AD` for this to function.
In list form, that is `{ 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }`,
and it is defined as `HTTP_UUID` in the httpebble headers.
If this causes duplicates
you may have issues using libpebble.

Samples and Code
-----------------

The Pebble-side prototype can be found [here](https://github.com/Katharine/httpebble-watch). Just
drop the files in, `#include "http.h"` and call the functions as documented.

The source for the iPhone bridge app (which has no UI whatsoever) can be found [here](https://github.com/Katharine/httpebble-ios).

A [sample watch face](http://builds.cloudpebble.net/2/7/2702284fdcbf4212a1c0baa1cd8c182e/watchface.pbw) has
been built using httpebble, demonstrating HTTP requests and location determination.
It replicates one of the watch faces found on getpebble.com, but never made available:

![](http://i.imgur.com/SOujils.jpg)  

The source can be found [here](https://github.com/Katharine/WeatherWatch). It will automatically determine your location using your
phone's GPS.
The simple PHP file it interacts with can be viewed [here](./weather2.phps). You will need
a free API key from [forecast.io](http://forecast.io) to use it.

----

A sample app demonstrating the timezone information can be found [here](http://builds.cloudpebble.net/3/3/3381a161742b4e22a141098f8e0e2ced/watchface.pbw).
It simply displays your timezone's name and offset from GMT, whether DST is active, and the current unix timestamp:

![](http://i.imgur.com/zF1528w.jpg)

Source can be found [on GitHub](https://github.com/Katharine/httpebble-timezone-demo).

----

A sample app demonstrating persistent data storage can be found [here](http://builds.cloudpebble.net/5/f/5ff46d27741741dab5cc768fe68780fb/watchface.pbw).
You can increment the value by pressing (or holding) up, and decrement is by pressing (or holding) down. You can save the value by clicking select, and
delete it by holding down select. Try leaving and returning to the app.

Source can be found [on GitHub](https://github.com/Katharine/httpebble-counter-demo).

HTTP Server Interface
----------

This section describes how httpebble interacts with remote servers and is of interest to application developers.

### Pebble to server

All requests made via httpebble are sent as HTTP POST requests with content type `application/json`.
The key/value pairs attached to the request are sent as a flat JSON dictionary. There cannot be any nesting.
Additionally, the client will send the HTTP header `X-Pebble-ID`, containing the serial number of the Pebble.

Since JSON requires string keys, all dictionary keys will be the decimal string form of the integers provided.
All integers will be provided as JSON integers, and cstrings as JSON strings. For example, a request might look
like this:

    {"1": 42, "2": -71, "3": "uk"}

Note that floating point values cannot be sent due to limitations of the Pebble AppMessage protocol.

The keys `0xF000` through `0xFFFF` are reserved by the protocol; using them is undefined.

### Server to Pebble

The server should respond with a flat JSON dictionary. All keys must be the decimal string representation of
signed 32-bit integers.

The keys `0xF000` through `0xFFFF` are reserved by the protocol; using them is undefined.

Values in the dictionary may be specified as follows:

- strings will be passed to the Pebble as cstrings (and thus should not contain null bytes).
- integers can either be specified in two ways:
    - Specifying an integer directly will encode it as a 32-bit signed integer when passing it to the Pebble
    - Integer size can be specified by instead giving the value as a `[width, value]` tuple. Specifying a value wider than the given width is undefined.
      `value` should be passed as a JSON integer.
      Valid values for `width` are:
        - `'b'` – signed eight-bit integer
        - `'B'` – unsigned eight-bit integer
        - `'s'` – signed 16-bit integer
        - `'S'` – unsigned 16-bit integer
        - `'i'` – signed 32-bit integer
        - `'I'` – unsigned 32-bit integer

Here is a sample valid response message:

    {"1": ['b', 7], "2": ['s', 18], "3": "Some string", "4": 17}


Pebble API: General
-------------------------

This section describes methods relevant to all parts of httpebble.

### Methods

#### http_set_app_id(int32_t app_id)

`void http_set_app_id(int32_t id);`

It is *strongly* recommended that you call this method with some (really) random integer somewhere in the 32-bit
signed integer range. Setting it uniquely ensures that your app will not receive misdirected responses or mix its
dict storage keys with another app.

#### http_register_callbacks

`bool http_register_callbacks(HTTPCallbacks callbacks, void* context);`

Registers the HTTP callbacks with the AppMessages system, and registers the provided `HTTPCallbacks`
with the HTTP system. Calling this method repeatedly will overwrite any previous handlers.

`context` will be passed to any callbacks.

### Structs

#### HTTPCallbacks

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

This is passed to `http_register_callbacks` to register callbacks. All of the callbacks are optional, and you need
only register those pertaining to functionality you actually use.

### Error codes

- `HTTP_OK` – Everything is fine.
- `HTTP_SEND_TIMEOUT` – There was a timeout attempting to contact the bridge on the user's phone.
- `HTTP_NOT_CONNECTED` – The watch is not connected to a phone.
- `HTTP_BRIDGE_NOT_RUNNING` – The HTTP bridge is not running on the user's phone.
- `HTTP_INVALID_ARGS` – The function was called with invalid arguments.
- `HTTP_BUSY` – There are other pending inbound or outbound messages preventing communication.
  Notably, this will occur if you try calling `http_out_get` in the app's `init_handler`.
- `HTTP_BUFFER_OVERFLOW` – The buffer was too small to contain the incoming message.
- `HTTP_NOT_ENOUGH_STORAGE` – The backing store did not have enough space.
- `HTTP_INTERNAL_INCONSISTENCY` – Something is very broken.
- `HTTP_INVALID_BRIDGE_RESPONSE` – The bridge on the user's phone returned an invalid response.

Pebble API: HTTP Requests
-------------------------

This section describes the prototype user-space Pebble API and is of interest to application developers.

### Methods

#### http_out_get

`HTTPResult http_out_get(const char* url, int32_t cookie, DictionaryIterator **iter_out);`

Prepares an HTTP POST request to `url`. The provided `cookie` **shall** be provided to success callbacks, and
**may** be provided to failure callbacks.

`iter_out` will point to a `DictionaryIterator`. Any values set on this dictionary will be included the POST
data to the specified URL.

It is considered impolite to call this function unless you intend to call `http_out_send` immediately. **This function cannot be called during
your app's init_handler.**

May return `HTTP_OK`, `HTTP_BUSY`, `HTTP_INVALID_ARGS`, `HTTP_NOT_ENOUGH_STORAGE` or `HTTP_INTERNAL_INCONSISTENCY`.

#### http_out_send

`HTTPResult http_out_send();`

Sends the HTTP POST request previously prepared by `http_out_get`; fails if there hasn't been one since the
last call.

May return `HTTP_OK` or `HTTP_BUSY`.

### Callbacks

#### HTTPRequestSucceededHandler

`typedef void(*HTTPRequestSucceededHandler)(int32_t cookie, int http_status, DictionaryIterator* response, void* context);`

Called when an HTTP request succeeds.

- `cookie` is the cookie provided to `http_out_get`.
- `http_status` is the HTTP status code returned by the remote server (usually 200)
- `response` is a `DictionaryIterator` pointing at a dictionary containing the server's response to the request.
- `context` is the value provided in `http_register_callbacks`.

Implementation limitations mean that it is plausible you will receive a misdirected response, so specifying and checking
the cookie is recommended.

#### HTTPRequestFailedHandler

`typedef void(*HTTPRequestFailedHandler)(int32_t cookie, int http_status, void* context);`

Called when an HTTP request fails, on a best-effort basis. Requests may fail without any warning.

- `cookie` will be either the cookie provided to `http_out_get` (if it could be retrieved) or zero.
- `http_status` will be the HTTP status code returned by the server if the request was made at all, or 
  an `HTTPResult` code plus 1000. You can check which by comparing against 1000.
- `context` is the value provided in `http_register_callbacks`.

#### HTTPReconnectedHandler

`typedef void(*HTTPReconnectedHandler)(void* context);`

This is called when the watch connects or reconnects to the phone app.

- `context` is the value provided in `http_register_callbacks`.

Pebble API: Local Cookies
-------------------------

The "Local Cookie" system enables your watch apps to store local data in a key-value store. You should
understand that the data storage is best-effort, and may be wiped without warning (e.g. because the user)
deleted the phone app or similar. For critical information, consider using the HTTP API and X-Pebble-ID
header to store the data remotely. All local cookie functions take a leading `request_id` argument that will
be returned to the application in any callbacks; it can take any integer value.

### Methods

#### http_cookie_set_int

`HTTPResult http_cookie_set_int(uint32_t request_id, uint32_t key, const void* integer, uint8_t width_bytes, bool is_signed);`

This method sets the given key to an integer of width `1`, `2` or `4` bytes, signed or not as appropriate

- `request_id` is an application-defined request ID
- `key` is the key to set a value on
- `integer` is a pointer to the value to set
- `width_bytes` is the number of bytes in the integer:
    - `1` for an `int8` or `uint8`
    - `2` for an `int16` or `uint16`
    - `4` for an `int32` or `uint32`
    - No other value is acceptable
- `is_signed` indicates whether the value is signed or not.

#### http_cookie_set_int…

`HTTPResult http_cookie_set_int32(uint32_t request_id, uint32_t key, int32_t value);`  
`HTTPResult http_cookie_set_uint32(uint32_t request_id, uint32_t key, uint32_t value);`  
`HTTPResult http_cookie_set_int16(uint32_t request_id, uint32_t key, int16_t value);`  
`HTTPResult http_cookie_set_uint16(uint32_t request_id, uint32_t key, uint16_t value);`  
`HTTPResult http_cookie_set_int8(uint32_t request_id, uint32_t key, int8_t value);`  
`HTTPResult http_cookie_set_uint8(uint32_t request_id, uint32_t key, uint8_t value);`  

These are helper methods for the above `http_cookie_set`.

- `request_id` is an application-defined request ID
- `key` is the key to set
- `value` is the value to set.

#### http_cookie_set_cstring

`HTTPResult http_cookie_set_cstring(uint32_t request_id, uint32_t key, const char* value);`

Sets the key `key` to be the C string `value`.

#### http_cookie_set_data

`HTTPResult http_cookie_set_data(uint32_t request_id, uint32_t key, const uint8_t* const value, int length);`

Sets the key `key` to hold `length` bytes of the arbitrary buffer `value`.

#### http_cookie_get

`HTTPResult http_cookie_get(uint32_t request_id, uint32_t key);`

Requests that the given `key` be retrieved. A callback will be fired when it is available.

#### http_cookie_Delete

`HTTPResult http_cookie_delete(uint32_t request_id, uint32_t key);`

Deletes `key` from the data store.

#### http_cookie_set_start

`HTTPResult http_cookie_set_start(int32_t request_id, DictionaryIterator **iter_out);`

Starts a cookie setting session for setting multiple keys simultaneously.
`iter_out` will point to a dictionary that you can write keys into using the standard dictionary writing methods.

`http_cookie_set_end` should be called as soon as possible after this function to commit the results.

#### http_cookie_set_end

`HTTPResult http_cookie_set_end();`

Ends a cookie setting session and sets any keys on the associated dictionary.

#### http_cookie_get_multiple

`HTTPResult http_cookie_get_multiple(int32_t request_id, uint32_t* keys, int32_t length);`

Request multiple keys at once.

- `keys` is an array of keys to fetch
- `length` is the number of keys in `keys`.

#### http_cookie_delete_multiple

Delete multiple keys at once.

- `keys` is an array of keys to delete
- `length` is the number of keys in `keys`.

#### http_cookie_fsync

`HTTPResult http_cookie_fsync();`

This function can optionally be called to ensure that the values are committed to disk on the phone (or some sort
of error will be raised). It is generally unnecessary to call it, and the current iOS implementation syncs after
each write or delete command anyway.

### Callbacks

There are lots of them, some of which are redundant to each other, depending on your needs.

#### HTTPPhoneCookieBatchGetHandler

`typedef void(*HTTPPhoneCookieBatchGetHandler)(int32_t request_id, DictionaryIterator* result, void* context);`

Called when a request for values (either one or many) completes. Keys not found will be omitted from the result dictionary.

- `request_id` is the id passed in when requesting the value
- `result` is a dictionary containing all the values set, and possibly some control values – disregard any
   value between 0xF000 and 0xFFFF if iterating over the dictionary.
- `context` is the context value passed to `http_register_callbacks`

#### HTTPPhoneCookieGetHandler

`typedef void(*HTTPPhoneCookieGetHandler)(int32_t request_id, Tuple* result, void* context);`

Called when a request for values (either one or many) completes. Contains only a single value, and called for every
key in the returned set (but no system-reserved keys). Will not be called for keys not found.

- `request_id` is the id passed in when requesting the value.
- `result` is a Tuple containing a single key/value pair
- `context` is the context value passed to `http_register_callbacks`

#### HTTPPhoneCookieSetHandler

`typedef void(*HTTPPhoneCookieSetHandler)(int32_t request_id, bool successful, void* context);`

Called to indicate the success or otherwise of a request to set keys. If tracking failure is important to you,
you should also check for errors above 1000 in a `HTTPRequestFailedHandler` callback.

- `request_id` is the value passed in when setting the values
- `successful` indicates the success of storing the values
- `context` is the context value passed to `http_register_callbacks`

#### HTTPPhoneCookieDeleteHandler

`typedef void(*HTTPPhoneCookieDeleteHandler)(int32_t request_id, bool successful, void* context);`

Called to indicate the success or otherwise of a request to delete keys. If tracking failure is important to you,
you should also check for errors above 1000 in a `HTTPRequestFailedHandler` callback.

- `request_id` is the value passed in when setting the values
- `successful` indicates the success of storing the values
- `context` is the context value passed to `http_register_callbacks`

#### HTTPPhoneCookieFsyncHandler

`typedef void(*HTTPPhoneCookieFsyncHandler)(int32_t request_id, bool successful, void* context);`

Called to indicate the success or otherwise of an fsync request. If tracking failure is important to you,
you should also check for errors above 1000 in a `HTTPRequestFailedHandler` callback.

- `request_id` is the value passed in when setting the values
- `successful` indicates the success of storing the values
- `context` is the context value passed to `http_register_callbacks`

Pebble API: Timezones
---------------------

The timezone functionality enables your watch app to determine the user's timezone. The accuracy of these functions
depends on the accuracy of the user's phone's clock.

### Functions

#### http_time_request

`HTTPResult http_time_request();`

Requests the user's timezone information from the phone.

### Callbacks

#### http_time_handler

`typedef void(*HTTPTimeHandler)(int32_t utc_offset_seconds, bool is_dst, uint32_t unixtime, const char* tz_name);`

Called when timezone information is available.

- `utc_offset_seconds` is the user's offset from UTC in seconds (may not be a multiple of 3600; may be negative)
- `is_dst` indicates whether it is currently Daylight Saving Time in the user's timezone
- `unixtime` gives the current unix timestamp (seconds since 1st January 1970, UTX)
- `tz_name` gives the name of the user's timezone (e.g. `"America/New_York"`). This value is allocated on the
  stack and must be copied for use outside the callback. This value may not be available, or may only give UTC
  offset, depending on the user's time settings.

Note that the `unixtime` is accurate as of the phone receiving and processing the message; not as of the time this
callback occurs. That said, there is usually no discernable latency between the two events.

Pebble API: Location
-------------------

The location API enables your application to determine a once-off reasonably accurate location for the user. The
resulting values should be accurate to within about a kilometre.

### Functions

#### http_location_request

`HTTPResult http_location_request();`

Requests the user's approximate location from the phone, as determined via GPS or other means at the phone's
discretion.

### Callbacks

#### HTTPLocationHandler

`typedef void(*HTTPLocationHandler)(float latitude, float longitude, float altitude, float accuracy, void* context);`

Called when the user's location is available.

- `latitude` contains the user's latitude.
- `longitude` contains the user's longitude.
- `altitude` contains the user's altitude in metres.
- `accuracy` contains the uncertainty of the *two-dimensional* location given in metres.

In general, `altitude` will probably be less accurate than `latitude` and `longitude`.

Bridge Interface
----------------

This section describes the pebble/bridge communication protocol, and is probably not of interest to application developers. It is,
however, of interest to anyone who has reason to implement their own bridge.

**NB:** If implementing your own bridge, please use the UUID `9141B628-BC89-498E-B147-049F49C099AD` if your bridge is compliant with
this specification. This allows all applications to use your bridge, rather than breaking them or failing to provide a useful implementation.

Since there is no way for the watch app to initialiate a session, a session should be opened on bridge launch and not closed until bridge exit.

### Reserved dictionary keys

All keys between `0xF000` and `0xFFFF` are reserved for use from user applications. Additionally, all keys between `0xFF00` and `0xFFFF` are
reserved specifically to `httpebble`.

The following named dictionary keys are used by the protocol:

- `0xFFFF` – `HTTP_URL_KEY`: a URL to request
- `0xFFFE` – `HTTP_STATUS_KEY`: HTTP status code
- `0xFFFC` – `HTTP_COOKIE_KEY`: user-defined cookie
- `0xFFFB` – `HTTP_CONNECT_KEY`: sent when a the app connects to the watch

- `HTTP_URL_KEY` (`0xFFFF`): a URL to request
- `HTTP_STATUS_KEY` (`0xFFFE`): the HTTP status code
- `HTTP_REQUEST_ID_KEY` (`0xFFFC`): the request ID specified by the user to return (HTTP requests only)
- `HTTP_CONNECT_KEY` (`0xFFFB`): indicates that the watch has (re)connected to the phone app
- `HTTP_APP_ID_KEY` (`0xFFF2`): specifies the application's app ID
- `HTTP_COOKIE_STORE_KEY` (`0xFFF0`): Request storing key-value data
- `HTTP_COOKIE_LOAD_KEY` (`0xFFF1`): Request loading key-value data
- `HTTP_COOKIE_FSYNC_KEY` (`0xFFF3`): Request syncing key-value data
- `HTTP_COOKIE_DELETE_KEY` (`0xFFF4`): Request deleting key-value data
- `HTTP_TIME_KEY` (`0xFFF5`): Request timezone information
- `HTTP_UTC_OFFSET_KEY` (`0xFFF6`): User's UTC offset
- `HTTP_IS_DST_KEY` (`0xFFF7`): Whether DST is in effect
- `HTTP_TZ_NAME_KEY` (`0xFFF8`): User's timezone name
- `HTTP_LOCATION_KEY` (`0xFFE0`): Request location information
- `HTTP_LATITUDE_KEY` (`0xFFE1`): User's latitude
- `HTTP_LONGITUDE_KEY` (`0xFFE2`): User's longitude
- `HTTP_ALTITUDE_KEY` (`0xFFE3`): User's altitude

Henceforth they will be referred to by name.

### HTTP requests

#### Pebble to Phone

To make a request the Pebble uses AppMessages to send a dictionary to the bridge app.

The dictionary must contain the following keys at minimum:

    HTTP_URL_KEY: (char*)"http://example.com/foo" // Any URL of the user's choosing. Both HTTP and HTTPS should be supported.
    HTTP_COOKIE_KEY: (int32_t)42                  // An arbitrary 32-bit signed integer provided by the user, to be sent with the response.
    HTTP_APP_ID_KEY: (int32_t)84                  // An arbitrary 32-bit signed integer that uniquely identifies an individual application


Any additional keys included must be included in the JSON dictionary to be sent to the server, as specified in the Server Interface section.
The cookie should be stored to be returned in the associated response. It **must not** be returned with any other response. The same applies to
the app ID.

#### Phone to Pebble

Upon receiving a response (or failing to receive a response), the bridge must communicate this back to the pebble. This
is again achieved by sending an AppMessage. The following keys are used:

    HTTP_STATUS_KEY: (uint16_t)200  // An signed 16-bit integer containing the returned HTTP status code.
    HTTP_URL_KEY: (uint8_t)1        // An unsigned 8-bit boolean (true/false) indicating whether the request was successful.
    HTTP_COOKIE_KEY: (int32_t)42    // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)84    // An arbitrary 32-bit signed integer that uniquely identifies an individual application.

In addition, any values provided by the server should be appended to the dictionary, as specified in the Server Interface section.

The current bridge implementation also gives `HTTP_STATUS_KEY` as `500` and `HTTP_STATUS_KEY` as `0` in the event of an invalid "successful"
response from the server.

### Indicating connection

When the phone app discovers the watch, it must send a message to it to activate the connection. That message is as follows:

    HTTP_CONNECT_KEY: (uint8_t)1

This triggers a callback on the watch and also ensures the connection is active. This message is phone to pebble only.

### Timezone information

#### Pebble to Phone

To request timezone information, the Pebble sends the following dictionary via AppMessages:

    HTTP_TIME_KEY: (uint8_t)1

In response, the phone should send time information as below:

#### Phone to Pebble

The response is a dictionary containing the user's current offset from GMT, DST state, a unix timestamp, and the timezone name, a follows:

    HTTP_TIME_KEY: (uint32_t)136878176          // The current unix time (unsigned)
    HTTP_UTC_OFFSET_KEY: (uint32_t)-14400       // User's offset from UTC in seconds
    HTTP_IS_DST_KEY: (uint8_t)1                 // Whether DST is active for the user
    HTTP_TZ_NAME_KEY: (char*)"America/New_York" // The name of the user's timezone, if possible.

The timezone name should ideally be in the standard tzinfo format, but this isn't a hard requirement, and a string containing the UTC offset
would also be acceptable.

### Location information

#### Pebble to Phone

The Pebble sends the following dictionary via AppMessages to request the user's location:

    HTTP_LOCATION_KEY: (uint8_t)1

In response, the phone sends location information as below:

#### Phone to Pebble

This response calls for sending floats, which AppMessage does not permit. Instead, the float should be converted to its binary form,
then that binary form reinterpreted as a uint32. Then the resultant uint32 should be sent to the pebble, which will perform the inverse
conversion.

    HTTP_LOCATION_KEY: (float)accuracy
    HTTP_LATITUDE_KEY: (float)latitude
    HTTP_LONGITUDE_KEY: (float)longitude
    HTTP_ALTITUDE_KEY: (float)altitude

The location given should be reasonably accurate, but within a kilometre or so is acceptable.

### Setting entries in the key-value store

#### Pebble to Phone

The pebble sends a dictionary containing at least the following keys:

    HTTP_COOKIE_STORE_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

In addition, the dictionary may include an arbitrary number of additional key/value pairs. Each such pair should be inserted into persistent
storage, maintaining the original type of the value (including the integer width and signedness, if appropriate).

#### Phone to Pebble

The phone responds with the following dictionary to indicate success:

    HTTP_COOKIE_STORE_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_Id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

This is the same as the received dictionary, but without any user-defined keys.

### Retrieving entries from the key-value store

#### Pebble to Phone

The pebble sends a dictionary containing at least the following keys:

    HTTP_COOKIE_LOAD_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_Id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

In addition, the dictionary may include an arbitrary number of keys with the uint8 value `1`. The values of these keys should be ignored;
the keys are those that the user is requesting be retrieved.

#### Phone to Pebble

The phone should respond with a dictionary containing at least the following:

    HTTP_COOKIE_LOAD_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_Id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

Additionally, for each key requested by the user that was found in the store, that key and its associated value (with original type)
should be inserted into the dictionary. If a requested key could not be found in the dictionary, it should be silently dropped.

### Deleting entries from the key-value store

#### Pebble to Phone

The pebble sends a dictionary containing at least the following keys:

    HTTP_COOKIE_DELETE_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_Id           // An arbitrary 32-bit signed integer that uniquely identifies an individual application

In addition, the dictionary may include an arbitrary number of keys with the uint8 value `1`. The values of these keys should be ignored;
the keys are those that the user is requesting be deleted.

#### Phone to Pebble

The phone responds with the following dictionary to indicate success:

    HTTP_COOKIE_DELETE_KEY:(int32_t)requestId  // An arbitrary 32-bit signed integer provided by the user in the request
    HTTP_APP_ID_KEY: (int32_t)app_Id           // An arbitrary 32-bit signed integer that uniquely identifies an individual application

This is the same as the received dictionary, but without any user-defined keys.

### Fsync

This call is a request to ensure that the persistent data store is actually written out to disk. It should not be necessary.

#### Pebble to Phone

    HTTP_COOKIE_LOAD_KEY:(uint8_t)1           // true.
    HTTP_APP_ID_KEY: (int32_t)app_Id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

#### Phone to Pebbble

    HTTP_COOKIE_LOAD_KEY:(uint8_t)success     // true.
    HTTP_APP_ID_KEY: (int32_t)app_Id          // An arbitrary 32-bit signed integer that uniquely identifies an individual application

Success should be true if the sync was successful, false otherwise. This whole is probably optional, but if not implemented should always
claim to have succeeded.
