//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_EXCEPTIONS_H
#define RTVI_EXCEPTIONS_H

#include <string>

namespace rtvi {

class RTVIException : public std::exception {
   public:
    RTVIException(const std::string& msg) : _message(msg) {}

    const char* what() const noexcept override { return _message.c_str(); }

   private:
    std::string _message;
};

}  // namespace rtvi

#endif
