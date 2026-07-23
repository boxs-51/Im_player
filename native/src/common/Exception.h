// common/Exception.h
#pragma once

#include <stdexcept>
#include <string>

class AppException : public std::runtime_error {
private:
    std::string m_file;
    int m_line;
    std::string m_func;
    std::string m_formattedMessage;

public:
    AppException(const std::string& message, const char* file, int line, const char* func)
        : std::runtime_error(message),
          m_file(file),
          m_line(line),
          m_func(func) {
        m_formattedMessage = "Error: " + std::string(what()) +
                             "\n  at " + m_file + ":" + std::to_string(m_line) +
                             " in " + m_func;
    }

    const char* what() const noexcept override {
        return m_formattedMessage.c_str();
    }
};

// Macro to easily throw the exception with file/line info
#define THROW_APP_EXCEPTION(msg) throw AppException(msg, __FILE__, __LINE__, __func__)