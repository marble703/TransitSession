#include <boost/asio.hpp>

#include <deque>

template<typename SocketT>
class AsyncSession: public std::enable_shared_from_this<AsyncSession<SocketT>> {
public:
    const static std::size_t default_read_buffer_size = 1024;

    enum class DuplexMode {
        full_duplex,
        half_duplex,
    };

    using ReadHandler = std::function<void(boost::system::error_code, std::vector<char>)>;

    explicit AsyncSession(boost::asio::io_context& io_context):
        strand_(boost::asio::make_strand(io_context)),
        socket_(io_context) {
    }

    SocketT& socket() {
        return socket_;
    }

    template<typename... Args>
    boost::system::error_code open(Args&&... args) {
        boost::system::error_code ec;
        socket_.open(std::forward<Args>(args)..., ec);
        return ec;
    }
    void set_duplex_mode(DuplexMode duplex_mode) {
        boost::asio::post(strand_, [this, self = this->shared_from_this(), duplex_mode]() {
            duplex_mode_ = duplex_mode;
            schedule_operations();
        });
    }

    void set_read_handler(ReadHandler read_handler) {
        boost::asio::post(
            strand_,
            [this,
             self         = this->shared_from_this(),
             read_handler = std::move(read_handler)]() mutable {
                read_handler_ = std::move(read_handler);
            }
        );
    }

    void set_read_buffer_size(std::size_t read_buffer_size) {
        boost::asio::post(strand_, [this, self = this->shared_from_this(), read_buffer_size]() {
            if (read_in_progress_ || read_buffer_size == 0U) {
                return;
            }

            read_buffer_.resize(read_buffer_size);
        });
    }

    void start_reading() {
        boost::asio::post(strand_, [this, self = this->shared_from_this()]() {
            read_enabled_ = true;
            schedule_operations();
        });
    }

    void stop_reading() {
        boost::asio::post(strand_, [this, self = this->shared_from_this()]() {
            read_enabled_ = false;
        });
    }

    template<typename BufferType>
    void send(BufferType&& msg) {
        auto message    = boost::asio::buffer(std::forward<BufferType>(msg));
        auto buffer_ptr = std::make_shared<std::vector<char>>(
            boost::asio::buffer_cast<const char*>(message),
            boost::asio::buffer_cast<const char*>(message) + boost::asio::buffer_size(message)
        );

        boost::asio::post(
            strand_,
            [this, self = this->shared_from_this(), buffer_ptr = std::move(buffer_ptr)]() {
                write_queue_.push_back(buffer_ptr);
                schedule_operations();
            }
        );
    }

    void close() {
        boost::asio::post(strand_, [this, self = this->shared_from_this()]() {
            read_enabled_ = false;
            write_queue_.clear();
            close_socket();
        });
    }

private:
    bool can_start_read() const {
        if (!read_enabled_ || read_in_progress_ || closed_) {
            return false;
        }

        if (duplex_mode_ == DuplexMode::half_duplex) {
            return !write_in_progress_ && write_queue_.empty();
        }

        return true;
    }

    bool can_start_write() const {
        if (write_in_progress_ || write_queue_.empty() || closed_) {
            return false;
        }

        if (duplex_mode_ == DuplexMode::half_duplex) {
            return !read_in_progress_;
        }

        return true;
    }

    void schedule_operations() {
        if (can_start_write()) {
            do_write();
        }

        if (can_start_read()) {
            do_read();
        }
    }

    void do_read() {
        read_in_progress_ = true;

        socket_.async_read_some(
            boost::asio::buffer(read_buffer_),
            boost::asio::bind_executor(
                strand_,
                [this, self = this->shared_from_this()](
                    boost::system::error_code ec,
                    std::size_t bytes_transferred
                ) {
                    read_in_progress_ = false;

                    if (ec) {
                        notify_read(ec, {});
                        close_socket();
                        return;
                    }

                    std::vector<char> data(
                        read_buffer_.begin(),
                        read_buffer_.begin() + bytes_transferred
                    );
                    notify_read({}, std::move(data));
                    schedule_operations();
                }
            )
        );
    }

    void do_write() {
        write_in_progress_ = true;

        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*write_queue_.front()),
            boost::asio::bind_executor(
                strand_,
                [this, self = this->shared_from_this()](
                    boost::system::error_code ec,
                    std::size_t /*bytes_transferred*/
                ) {
                    write_in_progress_ = false;

                    if (ec) {
                        close_socket();
                        return;
                    }

                    write_queue_.pop_front();
                    schedule_operations();
                }
            )
        );
    }

    void notify_read(boost::system::error_code ec, std::vector<char> data) {
        if (read_handler_) {
            read_handler_(ec, std::move(data));
        }
    }

    void close_socket() {
        if (closed_) {
            return;
        }

        closed_ = true;

        boost::system::error_code ec;
        socket_.close(ec);
    }

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    SocketT socket_;
    DuplexMode duplex_mode_ = DuplexMode::full_duplex;
    ReadHandler read_handler_;
    std::vector<char> read_buffer_ = std::vector<char>(default_read_buffer_size);

    std::deque<std::shared_ptr<std::vector<char>>> write_queue_;

    bool read_enabled_      = false;
    bool read_in_progress_  = false;
    bool write_in_progress_ = false;
    bool closed_            = false;
};
