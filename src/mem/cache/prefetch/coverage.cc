// Implementation of a Coverage Guided Tournament Prefetcher heavily based on the CoverageTournamentPrefetcher

#include "mem/cache/prefetch/coverage.hh"
#include "mem/cache/base.hh"
#include "sim/cur_tick.hh"
#include <unistd.h>


#include "params/CoverageTournamentPrefetcher.hh"


namespace gem5
{

namespace prefetch
{

CoverageTournament::CoverageTournament(const CoverageTournamentPrefetcherParams &p)
  : Base(p),
    prefetchers(p.prefetchers.begin(), p.prefetchers.end()),
    currChosenPf(0),
    currWindowCacheAccess(0),
    // runningCoverage(p.prefetchers.size(), 0),
    currWindowUsefulPrefetches(p.prefetchers.size(), 0),
    prefetchTargetsBuffer(p.prefetchers.size())
{
}

void
CoverageTournament::setParentInfo(System *sys, ProbeManager *pm, unsigned blk_size)
{
    for (auto pf : prefetchers)
        pf->setParentInfo(sys, pm, blk_size);
}


Tick
CoverageTournament::nextPrefetchReadyTime() const
{
    Tick next_ready = MaxTick;

    for (auto pf : prefetchers)
        next_ready = std::min(next_ready, pf->nextPrefetchReadyTime());

    return next_ready;
}

PacketPtr
CoverageTournament::getPacket()
{

    // DPRINTFTR(CoverageDebug, "Before tick %d\n", curTick());

    if (pendingSwitch)
    {
        // Push ready prefetch targets into buffer
        for (int pf = 0 ;  pf < prefetchers.size(); pf++)
        {
            if (pf == currChosenPf)
            {
                if (prefetchers[pf]->nextPrefetchReadyTime() <= curTick())
                {
                    PacketPtr pkt = prefetchers[pf]->recordPacket(0);
                    panic_if(!pkt, "Prefetcher is ready but didn't return a packet.");
                    prefetchTargetsBuffer[pf].push_back(pkt->getAddr());
                }
            }
            else
            {
                if (prefetchers[pf]->nextPrefetchReadyTime() <= curTick())
                {
                    PacketPtr pkt = prefetchers[pf]->getPacket();
                    panic_if(!pkt, "Prefetcher is ready but didn't return a packet.");
                    prefetchTargetsBuffer[pf].push_back(pkt->getAddr());
                }
            }
        }
    }

    // DPRINTFTR(CoverageDebug, "After tick %d\n", curTick());


    // Get prefetch from current chosen prefetcher
    if (prefetchers[currChosenPf]->nextPrefetchReadyTime() <= curTick())
    {
        PacketPtr pkt = prefetchers[currChosenPf]->getPacket();
        panic_if(!pkt, "Prefetcher is ready but didn't return a packet.");
        prefetchStats.pfIssued++;
        issuedPrefetches++;
        return pkt;
    }

    return nullptr;

    // Multi Implementation vvv

    // lastChosenPf = (lastChosenPf + 1) % prefetchers.size();
    // uint8_t pf_turn = lastChosenPf;

    // for (int pf = 0 ;  pf < prefetchers.size(); pf++) {
    //     if (prefetchers[pf_turn]->nextPrefetchReadyTime() <= curTick()) {
    //         PacketPtr pkt = prefetchers[pf_turn]->getPacket();
    //         panic_if(!pkt, "Prefetcher is ready but didn't return a packet.");
    //         prefetchStats.pfIssued++;
    //         // Ignoring Multi prefetcher stats for now
    //         // DPRINTF(PrefetchAddr, "Prefetching addr %#x.\n", pkt->getAddr());
    //         issuedPrefetches++;
    //         return pkt;
    //     }
    //     pf_turn = (pf_turn + 1) % prefetchers.size();
    // }

    // return nullptr;
}

void
CoverageTournament::recordCacheAccess(const PacketPtr &pkt, bool miss)
{
    currWindowCacheAccess++;

    if (pendingSwitch)
    {
        Addr addr = pkt->getAddr();

        // Iterate through prefetchTargetsBuffer
        for (int pf = 0; pf < prefetchers.size(); pf++)
        {
            for (int i = 0; i < prefetchTargetsBuffer[pf].size(); i++)
            {
                // If the address is found in the cache line of the prefetchTargetsBuffer
                if (addr >= prefetchTargetsBuffer[pf][i]
                    && addr < prefetchTargetsBuffer[pf][i] + 8) // Hardcoding cache line size for now
                {
                    // Increment the useful prefetches for the prefetcher
                    currWindowUsefulPrefetches[pf]++;
                    // prefetchTargetsBuffer[pf].erase(prefetchTargetsBuffer[pf].begin() + i);
                    // usleep(10);
                }
            }
        }
    }

    // If the current window is over
    if (currWindowCacheAccess >= currWindowSize)
    {
        // DPRINTFTR(CoverageDebug, "Window cleared\n");

        // Reset the current window
        pendingSwitch = true;
        currWindowCacheAccess = 0;
        // currWindowUsefulPrefetches = std::vector<uint64_t>(prefetchers.size(), 0);
        // Introduce residue to stabliize prefetcher selection
        for (int pf = 0; pf < prefetchers.size(); pf++)
        {
            currWindowUsefulPrefetches[pf] = currWindowUsefulPrefetches[pf] / 10;
        }
        prefetchTargetsBuffer = std::vector<std::vector<Addr>>(prefetchers.size());
    }

    if (currWindowCacheAccess >= windowSwitchDelay && pendingSwitch)
    {

        // DPRINTF(CoverageDebug, "%d\n", currWindowCacheAccess);

        // Iterate through the prefetchers
        // for (int pf = 0; pf < prefetchers.size(); pf++)
        // {
        //     // Calculate the coverage for the prefetcher
        //     runningCoverage[pf] = (double) currWindowUsefulPrefetches[pf] / currWindowCacheAccess;
        // }

        // Find the prefetcher with the highest coverage
        double maxCoverage = 0;
        uint8_t prevChosenPf = currChosenPf;

        for (int pf = 0; pf < prefetchers.size(); pf++)
        {
            if (currWindowUsefulPrefetches[pf] > maxCoverage)
            {
                maxCoverage = currWindowUsefulPrefetches[pf];
                currChosenPf = pf;
            }
        }

        if (currChosenPf != prevChosenPf)
        {
            // DPRINTFTR(CoverageDebug, "Switching Prefetcher from %d to %d\n", prevChosenPf, currChosenPf);
        }

        // Print coverage for each prefetcher
        for (int pf = 0; pf < prefetchers.size(); pf++)
        {
            // DPRINTFR(CoverageDebug, "%d: %d ; ", pf, currWindowUsefulPrefetches[pf]);
        }
        // DPRINTFR(CoverageDebug, "\n");

        pendingSwitch = false;
    }

}


} // namespace prefetch
} // namespace gem5
