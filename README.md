httpebble
=========

httpebble is a scheme for communicating with the internet from the Pebble, using a generic protocol
and without any application-specific code running on the phone.

Due to restrictions of the two-way communication SDK, only one application can communicate with the
watch simultaneously. That means that using this HTTP support will break RunKeeper or any other app
with a custom app. There is no workaround for this at this time. However, by using the same UUID for
all apps that want to use HTTP requests, any number of watch apps can use this functionality simultaneously.

Samples and Code
-----------------

The Pebble-side prototype can be found [here](https://github.com/Katharine/httpebble-watch). Just
drop the files in, `#include "http.h"` and call the functions as documented.

The source for the iPhone bridge app (which has no UI whatsoever) can be found [here](https://github.com/Katharine/httpebble-ios).

A [sample watch face](http://builds.cloudpebble.net/c/6/c643a1e4c43f4cb0b549b9f4183a0a36/watchface.pbw) has
been built using httpebble. It replicates one of the watch faces found on getpebble.com, but never made available:

![](http://i.imgur.com/SOujils.jpg)  
Watchface build status: ![](https://cloudpebble.net/ide/project/1806/status.png)

The source can be found [here](https://github.com/Katharine/WeatherWatch). Note that it won't build unless you provide your own copy of Futura. You will need
a free API key from [forecast.io](http://forecast.io) to use it.

Server Interface
----------

This section describes how httpebble interacts with remote servers and is of interest to application developers.

### Pebble to server

All requests made via httpebble are sent as HTTP POST requests with content type `application/json`.
The key/value pairs attached to the request are sent as a flat JSON dictionary. There cannot be any nesting.

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


Pebble API
----------

This section describes the prototype user-space Pebble API and is of interest to application developers.

Due to restrictions in the implementation of the Pebble SDK, your app must have the UUID `9141B628-BC89-498E-B147-049F49C099AD` for this to function.
In list form, that is `{ 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }`. If this causes duplicates
you may have issues using libpebble.

### Methods:

#### http_register_callbacks

`bool http_register_callbacks(HTTPCallbacks callbacks, void* context);`

Registers the HTTP callbacks with the AppMessages system, and registers the provided `HTTPCallbacks`
with the HTTP system. Calling this method repeatedly will overwrite any previous handlers.

`context` will be passed to any HTTP callbacks.

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

### Callbacks:

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

### Structs

#### HTTPCallbacks

    typedef struct {
        HTTPRequestFailedHandler failure;
        HTTPRequestSucceededHandler success;
        HTTPReconnectedHandler reconnect;
    } HTTPCallbacks;

This is passed to `http_register_callbacks` to register callbacks. `failure`, `success` and `reconnect are all optional, but omitting
`success` is probably not productive and omitting `failure` may leave you scratching your head when nothing happens.

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

Bridge Interface
----------------

This section describes the pebble/bridge communication protocol, and is probably not of interest to application developers. It is,
however, of interest to anyone who has reason to implement their own bridge.

**NB:** If implementing your own bridge, please use the UUID `9141B628-BC89-498E-B147-049F49C099AD` if your bridge is compliant with
this specification. This allows all applications to use your bridge, rather than breaking them or failing to provide a useful implementation.

Since there is no way for the watch app to initialiate a session, a session should be opened on bridge launch and not closed until bridge exit.

### Reserved dictionary keys

The following dictionary keys are used by the protocol:

- `0xFFFF` – `HTTP_URL_KEY`: a URL to request
- `0xFFFE` – `HTTP_STATUS_KEY`: HTTP status code
- `0xFFFD` – `HTTP_SUCCESS_KEY`: whether a request was successful
- `0xFFFC` – `HTTP_COOKIE_KEY`: user-defined cookie
- `0xFFFB` – `HTTP_CONNECT_KEY`: sent when a the app connects to the watch

Henceforth they will be referred to by name.

### Making requests

To make a request the Pebble uses AppMessages to send a dictionary to the bridge app.

The dictionary must contain the following keys at minimum:

    HTTP_URL_KEY: (char*)"http://example.com/foo" // Any URL of the user's choosing. Both HTTP and HTTPS should be supported.
    HTTP_COOKIE_KEY: (int32_t)42                  // An arbitrary 32-bit signed integer provided by the user, to be sent with the response.


Any additional keys included must be included in the JSON dictionary to be sent to the server, as specified in the Server Interface section.
The cookie should be stored to be returned in the associated response. It **must not** be returned with any other response.

### Returning responses

Upon receiving a response (or failing to receive a response), the bridge must communicate this back to the pebble. This
is again achieved by sending an AppMessage. The following keys are used:

    HTTP_STATUS_KEY: (uint16_t)200  // An signed 16-bit integer containing the returned HTTP status code.
    HTTP_SUCCESS_KEY: (uint8_t)1    // An unsigned 8-bit boolean (true/false) indicating whether the request was successful.
    HTTP_COOKIE_KEY: (int32_t)42    // An arbitrary 32-bit signed integer provided by the user in the request

In addition, any values provided by the server should be appended to the dictionary, as specified in the Server Interface section.

The current bridge implementation also gives `HTTP_STATUS_KEY` as `500` and `HTTP_STATUS_KEY` as `0` in the event of an invalid "successful"
response from the server.

### Indicating connection

When the phone app discovers the watch, it must send a message to it to activate the connection. That message is as follows:

    HTTP_CONNECT_KEY: (uint8_t)1

This triggers a callback on the watch and ensures the connection is active.
