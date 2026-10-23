#pragma once

#include "view.h"

#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace cactus {

class IWriter {
public:
    virtual ~IWriter() = default;
    virtual void Write(ConstView buf) = 0;
};

class IFlusher {
public:
    virtual ~IFlusher() = default;
    virtual void Flush() = 0;
};

class ICloser {
public:
    virtual ~ICloser() = default;
    virtual void Close() = 0;
};

class IBufferedWriter : public virtual IWriter, public virtual IFlusher {
public:
    virtual MutableView WriteNext() = 0;
    virtual void WriteBackUp(size_t size) = 0;

    // Default implementation.
    virtual void Write(ConstView buf) override;

    void Append(char c);

    void AppendC(const char* str) {
        Write(View(str, std::strlen(str)));
    }
};

class StringWriter : public IBufferedWriter {
public:
    virtual MutableView WriteNext() {
        constexpr size_t kMinSize = 256;
        if (str_.empty()) {
            str_.resize(kMinSize);
        }

        if (size_ == str_.size()) {
            str_.resize(2 * str_.size());
        }

        auto chunk = cactus::View(str_.data() + size_, str_.size() - size_);
        size_ = str_.size();
        return chunk;
    }

    virtual void WriteBackUp(size_t size) {
        size_ -= size;
    }

    virtual void Flush() {
    }

    std::string_view View() const {
        return {str_.data(), size_};
    }

private:
    std::string str_;
    size_t size_ = 0;
};

class IWriteCloser : public virtual IWriter, public virtual ICloser {};

class IBufferedWriteCloser : public virtual IBufferedWriter, public virtual ICloser {};

class BufferedWriter : public virtual IBufferedWriter {
public:
    explicit BufferedWriter(IWriter* underlying, size_t buffer_size = 65536);

    MutableView WriteNext() override;
    void WriteBackUp(size_t size) override;

    void Flush() override;
    void Write(ConstView buf) override;

private:
    std::unique_ptr<char[]> buffer_;
    size_t used_ = 0;
    size_t buffer_size_ = 0;

    IWriter* underlying_;
};

class BufferedWriteCloser : public virtual IBufferedWriteCloser, public virtual BufferedWriter {
public:
    explicit BufferedWriteCloser(IWriteCloser* underlying, size_t buffer_size = 65536);

    virtual void Close() override;

private:
    IWriteCloser* underlying_;
};

}  // namespace cactus
