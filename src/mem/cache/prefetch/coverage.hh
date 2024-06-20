// Implementation of a Coverage Guided Tournament Prefetcher heavily based on the MultiPrefetcher

#ifndef __MEM_CACHE_PREFETCH_COVERAGE_TOURNAMENT_HH__
#define __MEM_CACHE_PREFETCH_COVERAGE_TOURNAMENT_HH__

#include <vector>

#include "mem/cache/prefetch/base.hh"

namespace gem5
{

struct CoverageTournamentPrefetcherParams;

namespace prefetch
{

class CoverageTournament : public Base
{
  public: // SimObject
    CoverageTournament(const CoverageTournamentPrefetcherParams &p);

  public:
    void setParentInfo(System *sys, ProbeManager *pm, unsigned blk_size) override;
    PacketPtr getPacket() override;
    Tick nextPrefetchReadyTime() const override;

    // void processCoverage() const;
    PacketPtr recordPacket(uint64_t index) override {return nullptr;};
    void recordCacheAccess(const PacketPtr &pkt, bool miss) override;

    void
    notify(const CacheAccessProbeArg &arg, const PrefetchInfo &pfi) override
    {};

    void notifyFill(const CacheAccessProbeArg &arg) override {};

  protected:

    // List of prefetchers
    std::vector<Base*> prefetchers;

    // Whether a prefetcher switch is pending
    bool pendingSwitch = false;

    // Current chosen prefetcher
    uint8_t currChosenPf;

    const uint64_t currWindowSize = 100000; // Hardcoded for now

    // Delay between prefetcher switches to allow the coverage to stablise
    const uint64_t windowSwitchDelay = 40000;

    // Current cache access of this current window
    uint64_t currWindowCacheAccess;

    // List of useful prefetches for each prefetcher
    std::vector<uint64_t> currWindowUsefulPrefetches;

    // List of the prefetch targets of the prefetchers
    std::vector<std::vector<Addr>> prefetchTargetsBuffer;

};

} // namespace prefetch
} // namespace gem5

#endif//__MEM_CACHE_PREFETCH_COVERAGE_TOURNAMENT_HH_
