#include "logging/fwd.hpp"

#include <iosfwd>

namespace logging {

class log_destination {
public:
    log_destination() = default;

    log_destination(const log_destination&) = default;
    log_destination& operator=(const log_destination&) = default;
    log_destination(log_destination&&) = default;
    log_destination& operator=(log_destination&&) = default;

    virtual ~log_destination();
    virtual void emit(const log_record& record) = 0;
};

class log_destination_ostream final : public log_destination {
public:
    explicit log_destination_ostream(std::ostream& stream);

    log_destination_ostream(const log_destination_ostream&) = delete;
    log_destination_ostream& operator=(const log_destination_ostream&) = delete;
    log_destination_ostream(log_destination_ostream&&) = delete;
    log_destination_ostream& operator=(log_destination_ostream&&) = delete;
    ~log_destination_ostream() override = default;

    void emit(const log_record& record) override;
private:
    std::ostream& stream_;
};

}
