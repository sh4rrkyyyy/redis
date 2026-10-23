#include "reader.h"
#include "writer.h"

#include <glog/logging.h>

namespace cactus {

void IReader::ReadFull(MutableView buf) {
    if (buf.size() == 0) {
        return;
    }

    size_t len = 0;
    while (buf.size() != 0) {
        size_t n = Read(buf);
        if (n == 0) {
            break;
        }

        buf = buf.subspan(n);
        len += n;
    }

    if (buf.size() != 0) {
        throw EOFException{};
    }
}

std::string IReader::ReadAllToString() {
    std::string out(1024, '\0');
    size_t pos = 0;

    while (true) {
        if (pos == out.size()) {
            out.resize(out.size() * 2);
        }

        auto buf = View(out).subspan(pos);
        auto len = Read(buf);
        if (len == 0) {
            break;
        }

        pos += len;
    }

    out.resize(pos);
    return out;
}

size_t IBufferedReader::Read(MutableView buf) {
    size_t bytes_read = 0;

    auto chunk = ReadNext();
    if (chunk.size() == 0) {
        return bytes_read;
    }

    size_t copy_size = Copy(buf, chunk);
    buf = buf.subspan(copy_size);
    chunk = chunk.subspan(copy_size);
    bytes_read += copy_size;

    if (chunk.size() != 0) {
        ReadBackUp(chunk.size());
    }

    return bytes_read;
}

std::string IBufferedReader::ReadString(char delim, size_t limit) {
    std::string result;

    for (;;) {
        auto buf = ReadNext();
        if (buf.size() == 0) {
            return result;
        }

        for (size_t i = buf.size(); buf.size() != 0; --i) {
            if (limit == 0) {
                ReadBackUp(i);
                return result;
            }

            if (limit != std::string::npos) {
                limit--;
            }

            auto symbol = *reinterpret_cast<const char *>(buf.data());
            result.push_back(symbol);
            buf = buf.subspan(1);

            if (symbol == delim) {
                ReadBackUp(i - 1);
                return result;
            }
        }
    }
}

size_t IBufferedReader::WriteTo(IWriter *writer) {
    size_t total = 0;
    for (;;) {
        auto buf = ReadNext();
        if (buf.size() == 0) {
            return total;
        }

        total += buf.size();
        writer->Write(buf);
    }
}

BufferedReader::BufferedReader(IReader *reader, size_t buffer_size)
    : underlying_(reader), buffer_(new char[buffer_size]), cap_(buffer_size) {
    CHECK_NOTNULL(underlying_);
}

ConstView BufferedReader::ReadNext() {
    if (pos_ == end_) {
        pos_ = end_ = 0;
    }

    if (end_ == 0) {
        end_ = underlying_->Read(View(buffer_.get(), cap_));
    }

    auto view = View(buffer_.get() + pos_, end_ - pos_);
    pos_ = end_;
    return view;
}

void BufferedReader::ReadBackUp(size_t size) {
    CHECK(size <= pos_);
    pos_ -= size;
}

ViewReader::ViewReader(ConstView buf) : buf_(buf) {
}

ConstView ViewReader::ReadNext() {
    auto result = buf_;
    result = result.subspan(pos_);
    pos_ = buf_.size();
    return result;
}

void ViewReader::ReadBackUp(size_t size) {
    CHECK(size <= pos_);
    pos_ -= size;
}

LoopReader::LoopReader(ConstView view) : view_{view}, pos_{0} {
    if (view_.empty()) {
        throw std::runtime_error{"Empty view"};
    }
}

size_t LoopReader::Read(MutableView buf) {
    if (buf.empty()) {
        return 0;
    }
    buf[0] = view_[pos_++];
    pos_ %= view_.size();
    return 1;
}

}  // namespace cactus
