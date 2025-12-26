#include <boost/stacktrace.hpp>
#include <boost/stacktrace/this_thread.hpp>
#include <catch2/catch_all.hpp>

#include <exception>
#include <sstream>
#include <string>

namespace {

[[maybe_unused]] const bool stacktrace_capture_enabled = []() {
    boost::stacktrace::this_thread::set_capture_stacktraces_at_throw(true);
    return true;
}();

}  // namespace

CATCH_TRANSLATE_EXCEPTION(const std::exception& ex) {
    std::ostringstream oss;
    oss << ex.what();

    auto stack = boost::stacktrace::stacktrace::from_current_exception();
    if (stack.empty()) {
        stack = boost::stacktrace::stacktrace();
    }
    if (!stack.empty()) {
        oss << '\n' << stack;
    }
    return oss.str();
}
