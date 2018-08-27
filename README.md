# WILTON_SMARTEC_READER Module

Wilton standalone module to catch input from smartec RFID readers.

Functions:

Function | Description 
---------| -------------
read_smartec_input(**tiomeout**) | Reurns digital input from SMARTEC RFID readers. Function blocked until reader send RETURN key* pressed or timeout expires. Returns result as json string. 

* - reader supposed to be configured to send RETURN key at the end of RFID number.


Readed data result example

```js
{ 
	"data": "12341", 
	"is_expired": false
}
```

in case of error return error result as json string:

```js
{
	"error": "Can't init X display 0:2"
}
```


### Using in code example:
```js
define([
    "wilton/dyload",
    "wilton/wiltoncall"
], function(dyload, wiltoncall) {
    "use strict";

    // load shared lib on init
    dyload({
        name: "wilton_smartec_reader"
    });
    
    return {
        main: function() {
            print("Calling native module ...");
            var resp = wiltoncall("read_smartec_input", 2000);
            print("Call response: [" + resp + "]");
        }
    };
});
```