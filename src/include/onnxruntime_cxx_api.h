#pragma once

#include <string>
#include <memory>

#define ORT_LOGGING_LEVEL_WARNING 2

namespace Ort {

class Env {
public:
    Env(int logging_level, const char* logid) {}
    ~Env() = default;
};

class Session {
public:
    Session(Env& env, const char* model_path, void* session_options) {}
    ~Session() = default;
};

} // namespace Ort
