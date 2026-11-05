#include "PingStats.hpp"

#include <iomanip>
#include <iostream>
#include <limits>

PingStats::PingStats()
    : sent_count(0), received_count(0),
      min_rtt(std::numeric_limits<double>::max()),
      max_rtt(0.0), total_rtt(0.0) {}

void PingStats::recordRtt(double rtt_ms) {
    ++sent_count;
    ++received_count;
    total_rtt += rtt_ms;
    if (rtt_ms < min_rtt) min_rtt = rtt_ms;
    if (rtt_ms > max_rtt) max_rtt = rtt_ms;
}

void PingStats::recordTimeout() {
    ++sent_count;
}

void PingStats::printFinalReport() const {
    const double loss = sent_count == 0 ? 0.0 :
        (static_cast<double>(sent_count - received_count) / sent_count) * 100.0;

    std::cout << "--- ping statistics ---\n";
    std::cout << "sent=" << sent_count
              << " received=" << received_count
              << " loss=" << std::fixed << std::setprecision(1) << loss << "%\n";

    if (received_count == 0) {
        std::cout << "rtt min/avg/max = -/-/- ms\n";
        return;
    }

    const double average = total_rtt / received_count;
    std::cout << "rtt min/avg/max = "
              << std::fixed << std::setprecision(3)
              << min_rtt << "/" << average << "/" << max_rtt << " ms\n";
}
