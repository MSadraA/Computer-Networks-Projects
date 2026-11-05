#ifndef PINGSTATS_HPP
#define PINGSTATS_HPP

#include <cstddef>

class PingStats {
public:
    PingStats();

    void recordRtt(double rtt_ms);
    void recordTimeout();
    void printFinalReport() const;

private:
    std::size_t sent_count;
    std::size_t received_count;
    double min_rtt;
    double max_rtt;
    double total_rtt;
};

#endif
