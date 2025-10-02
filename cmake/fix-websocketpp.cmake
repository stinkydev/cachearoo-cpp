# Fix websocketpp C++20 compatibility issue in logger/basic.hpp
file(READ "${SOURCE_DIR}/websocketpp/logger/basic.hpp" LOGGER_CONTENTS)

# Fix constructor declarations - remove template parameters from constructor names
string(REPLACE 
    "basic<concurrency,names>(channel_type_hint::value h ="
    "basic(channel_type_hint::value h ="
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(std::ostream * out)"
    "basic(std::ostream * out)"
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(level c, channel_type_hint::value h ="
    "basic(level c, channel_type_hint::value h ="
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

string(REPLACE 
    "basic<concurrency,names>(level c, std::ostream * out)"
    "basic(level c, std::ostream * out)"
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

# Fix copy constructor
string(REPLACE 
    "basic<concurrency,names>(basic<concurrency,names> const & other)"
    "basic(basic const & other)"
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

# Fix move constructor
string(REPLACE 
    "basic<concurrency,names>(basic<concurrency,names> && other)"
    "basic(basic && other)"
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

# Fix destructor declaration
string(REPLACE 
    "~basic<concurrency,names>()"
    "~basic()"
    LOGGER_CONTENTS "${LOGGER_CONTENTS}")

file(WRITE "${SOURCE_DIR}/websocketpp/logger/basic.hpp" "${LOGGER_CONTENTS}")

# Fix websocketpp C++20 compatibility issue in endpoint.hpp
file(READ "${SOURCE_DIR}/websocketpp/endpoint.hpp" ENDPOINT_CONTENTS)

# Fix endpoint destructor
string(REPLACE 
    "~endpoint<connection,config>()"
    "~endpoint()"
    ENDPOINT_CONTENTS "${ENDPOINT_CONTENTS}")

file(WRITE "${SOURCE_DIR}/websocketpp/endpoint.hpp" "${ENDPOINT_CONTENTS}")

message(STATUS "Applied C++20 compatibility fix to websocketpp/logger/basic.hpp")
message(STATUS "Applied C++20 compatibility fix to websocketpp/endpoint.hpp")
