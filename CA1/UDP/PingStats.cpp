#include "PingStats.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

PingStats::PingStats(int total_expected) 
    : total_expected(total_expected), sent_count(0), received_count(0) {}

void PingStats::recordRtt(double rtt_ms) {
    sent_count++;
    received_count++;
    rtt_history.push_back(rtt_ms);
}

void PingStats::recordTimeout() {
    sent_count++;
}

void PingStats::printFinalReport() const {
    double loss_percentage = 0.0;
    if (sent_count > 0) {
        loss_percentage = (static_cast<double>(sent_count - received_count) / sent_count) * 100.0;
    }

    std::cout << "--- ping statistics ---\n";
    std::cout << "sent=" << sent_count 
              << " received=" << received_count 
              << " loss=" << std::fixed << std::setprecision(1) << loss_percentage << "%\n";

    if (received_count == 0) {
        std::cout << "rtt min/avg/max = -/-/- ms\n";
    } else {
        double min_rtt = *std::min_element(rtt_history.begin(), rtt_history.end());
        double max_rtt = *std::max_element(rtt_history.begin(), rtt_history.end());
        
        double sum_rtt = 0;
        for (double rtt : rtt_history) {
            sum_rtt += rtt;
        }
        double avg_rtt = sum_rtt / received_count;

        std::cout << "rtt min/avg/max = " 
                  << std::fixed << std::setprecision(3) << min_rtt << "/" 
                  << avg_rtt << "/" << max_rtt << " ms\n";
    }
}