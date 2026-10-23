#include "writer.h"

#include <glog/logging.h>

namespace cactus {

void IBufferedWriter::Write(ConstView buf) {
    while (buf.size() != 0) {
        auto chunk = WriteNext();
        size_t copy_size = Copy(chunk, buf);

        buf = buf.subspan(copy_size);
        chunk = chunk.subspan(copy_size);

        if (buf.size() == 0) {
            if (chunk.size() != 0) {
                WriteBackUp(chunk.size());
            }
            return;
        }
    }
}

BufferedWriter::BufferedWriter(IWriter* underlying, size_t buffer_size)
    : buffer_(new char[buffer_size]), buffer_size_(buffer_size), underlying_(underlying) {
}

MutableView BufferedWriter::WriteNext() {
    if (used_ == buffer_size_) {
        Flush();
    }

    auto chunk = View(buffer_.get() + used_, buffer_size_ - used_);
    used_ = buffer_size_;
    return chunk;
}

void BufferedWriter::WriteBackUp(size_t count) {
    DCHECK(count <= used_);
    used_ -= count;
}

void BufferedWriter::Flush() {
    underlying_->Write(View(buffer_.get(), used_));
    used_ = 0;
}

void BufferedWriter::Write(ConstView buf) {
    if (buf.size() < 2 * buffer_size_) {
        IBufferedWriter::Write(buf);
    } else {
        Flush();
        Write(buf);
    }
}

void IBufferedWriter::Append(char c) {
    auto chunk = WriteNext();
    Copy(chunk, View(&c, 1));
    if (chunk.size() != 1) {
        WriteBackUp(chunk.size() - 1);
    }
}

BufferedWriteCloser::BufferedWriteCloser(IWriteCloser* underlying, size_t buffer_size)
    : BufferedWriter(underlying, buffer_size), underlying_(underlying) {
}

void BufferedWriteCloser::Close() {
    underlying_->Close();
}

}  // namespace cactus
