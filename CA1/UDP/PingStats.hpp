#include <vector>

class PingStats {
public:
    PingStats(int total_expected);

    void recordRtt(double rtt_ms); 
    void recordTimeout();        

    void printFinalReport() const;

private:
    int total_expected;
    int sent_count;
    int received_count;
    std::vector<double> rtt_history;
};