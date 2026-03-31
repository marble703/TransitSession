#pragma once

#include <boost/asio.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <utility>

namespace session {
template<typename Derived, typename ReadValue, typename WriteValue = ReadValue>
class AsyncTransportSession {
public:
    using ReadHandler = std::function<void(boost::system::error_code, ReadValue)>;

    explicit AsyncTransportSession(boost::asio::io_context& io_context):
        strand_(boost::asio::make_strand(io_context)) {}

    void set_read_handler(ReadHandler read_handler) {
        boost::asio::post(
            strand_,
            [this,
             self         = derived_shared_from_this(),
             read_handler = std::move(read_handler)]() mutable {
                read_handler_ = std::move(read_handler);
            }
        );
    }

    void start_reading() {
        boost::asio::post(strand_, [this, self = derived_shared_from_this()]() {
            read_enabled_ = true;
            schedule_operations();
        });
    }

    void stop_reading() {
        boost::asio::post(strand_, [this, self = derived_shared_from_this()]() {
            read_enabled_ = false;
        });
    }

    void send(WriteValue message) {
        boost::asio::post(
            strand_,
            [this, self = derived_shared_from_this(), message = std::move(message)]() mutable {
                write_queue_.push_back(std::make_shared<WriteValue>(std::move(message)));
                schedule_operations();
            }
        );
    }

    void close() {
        boost::asio::post(strand_, [this, self = derived_shared_from_this()]() {
            if (closed_) {
                return;
            }

            read_enabled_ = false;
            write_queue_.clear();
            closed_ = true;
            static_cast<Derived*>(this)->close_transport();
        });
    }

protected:
    boost::asio::strand<boost::asio::io_context::executor_type>& strand() {
        return strand_;
    }

    const boost::asio::strand<boost::asio::io_context::executor_type>& strand() const {
        return strand_;
    }

    bool is_read_enabled() const {
        return read_enabled_;
    }

    bool is_read_in_progress() const {
        return read_in_progress_;
    }

    bool is_write_in_progress() const {
        return write_in_progress_;
    }

    bool is_closed() const {
        return closed_;
    }

    bool has_pending_writes() const {
        return !write_queue_.empty();
    }

    std::shared_ptr<WriteValue> current_write() {
        if (write_queue_.empty()) {
            return {};
        }

        return write_queue_.front();
    }

    void consume_write() {
        if (!write_queue_.empty()) {
            write_queue_.pop_front();
        }
    }

    void mark_read_started() {
        read_in_progress_ = true;
    }

    void mark_read_finished() {
        read_in_progress_ = false;
    }

    void mark_write_started() {
        write_in_progress_ = true;
    }

    void mark_write_finished() {
        write_in_progress_ = false;
    }

    void notify_read(boost::system::error_code ec, ReadValue value) {
        if (read_handler_) {
            read_handler_(ec, std::move(value));
        }
    }

    void schedule_operations() {
        auto* derived = static_cast<Derived*>(this);

        if (derived->can_start_write()) {
            derived->do_write();
        }

        if (derived->can_start_read()) {
            derived->do_read();
        }
    }

private:
    auto derived_shared_from_this() {
        return static_cast<Derived*>(this)->shared_from_this();
    }

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    ReadHandler read_handler_;
    std::deque<std::shared_ptr<WriteValue>> write_queue_;

    bool read_enabled_      = false;
    bool read_in_progress_  = false;
    bool write_in_progress_ = false;
    bool closed_            = false;
};
} // namespace session
