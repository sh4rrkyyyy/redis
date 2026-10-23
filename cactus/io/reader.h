#pragma once

#include "view.h"

#include <exception>
#include <memory>
#include <string>

namespace cactus {

class IWriter;

class EOFException : public std::exception {
public:
    EOFException() = default;

    const char *what() const noexcept override {
        return "error: EOF";
    }
};

class IReader {
public:
    virtual ~IReader() = default;

    virtual size_t Read(MutableView buf) = 0;

    void ReadFull(MutableView buf);

    std::string ReadAllToString();
};

class LoopReader : public IReader {
public:
    explicit LoopReader(ConstView view);

    size_t Read(MutableView buf) override;

private:
    ConstView view_;
    size_t pos_;
};

class IBufferedReader : public IReader {
public:
    virtual ConstView ReadNext() = 0;

    virtual void ReadBackUp(size_t size) = 0;

    // Default implementation.
    virtual size_t Read(MutableView buf) override;

    // ReadString читает символы из потока, пока не встретит символ delim,
    // или не встретит конец потока или не прочитает limit символов.
    //
    // Функция возвращает строку, содержащую все считаные символы, включая последний delim.
    std::string ReadString(char delim, size_t limit = std::string::npos);

    virtual size_t WriteTo(IWriter *writer);
};

class BufferedReader : public IBufferedReader {
public:
    explicit BufferedReader(IReader *reader, size_t buffer_size = 65536);

    virtual ConstView ReadNext() override;
    virtual void ReadBackUp(size_t size) override;

private:
    IReader *underlying_ = nullptr;
    std::unique_ptr<char[]> buffer_;
    size_t pos_ = 0, end_ = 0, cap_;
};

class ViewReader : public IBufferedReader {
public:
    explicit ViewReader(ConstView buf);

    virtual ConstView ReadNext() override;
    virtual void ReadBackUp(size_t size) override;

private:
    ConstView buf_;
    size_t pos_ = 0;
};

}  // namespace cactus
