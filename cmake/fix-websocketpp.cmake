# Fix websocketpp C++20 compatibility issue in logger/basic.hpp
file(READ "${SOURCE_DIR}/websocketpp/logger/basic.hpp" FILE_CONTENTS)

# Fix constructor declarations - remove template parameters from constructor names
string(REPLACE 
    "basic<concurrency,names>(channel_type_hint::value h ="
    "basic(channel_type_hint::value h ="
    FILE_CONTENTS "${FILE_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(std::ostream * out)"
    "basic(std::ostream * out)"
    FILE_CONTENTS "${FILE_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(level c, channel_type_hint::value h ="
    "basic(level c, channel_type_hint::value h ="
    FILE_CONTENTS "${FILE_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(level c, std::ostream * out)"
    "basic(level c, std::ostream * out)"
    FILE_CONTENTS "${FILE_CONTENTS}")

# Fix destructor declaration
string(REPLACE 
    "~basic<concurrency,names>()"
    "~basic()"
    FILE_CONTENTS "${FILE_CONTENTS}")

file(WRITE "${SOURCE_DIR}/websocketpp/logger/basic.hpp" "${FILE_CONTENTS}")

message(STATUS "Applied C++20 compatibility fix to websocketpp/logger/basic.hpp")
