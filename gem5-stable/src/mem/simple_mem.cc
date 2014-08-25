/*
 * Copyright (c) 2010-2013 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ron Dreslinski
 *          Ali Saidi
 *          Andreas Hansson
 */

#include "base/random.hh"
#include "mem/simple_mem.hh"
#include "debug/RowBuffer.hh"

using namespace std;

SimpleMemory::SimpleMemory(const SimpleMemoryParams* p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p->latency),
    latency_miss(p->latency_miss), banks(uint64_t(1) << ceilLog2(hostSize())),
    tATTOp(p->lat_att_operate), tBufferOp(p->lat_buffer_operate),
    tNVMRead(p->lat_nvm_read), tNVMWrite(p->lat_nvm_write),
    latency_var(p->latency_var), bandwidth(p->bandwidth),
    isBusy(false), retryReq(false), retryResp(false),
    releaseEvent(this), freezeEvent(this), unfreezeEvent(this),
    dequeueEvent(this), drainManager(NULL)
{
    isTiming = !p->disable_timing;
    wbBandwidth = (double)latency / 64;
    waitStart = 0;
    profBase.set_op_latency(p->lat_att_operate);
}

void
SimpleMemory::init()
{
    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

void
SimpleMemory::regStats()
{
    using namespace Stats;
    AbstractMemory::regStats();

    extraRespLatency
        .name(name() + ".extra_resp_latency")
        .desc("Incremental latency of individual access");
    totalCkptTime
        .name(name() + ".total_ckpt_time")
        .desc("Total time in checkpointing frames");
    totalWaitTime
        .name(name() + ".total_wait_time")
        .desc("Total wait time in checkpointing frames");

    readRowHits
        .name(name() + ".readRowHits")
        .desc("Number of row buffer hits during reads");

    writeRowHits
        .name(name() + ".writeRowHits")
        .desc("Number of row buffer hits during writes");

    readRowMisses
        .name(name() + ".readRowMisses")
        .desc("Number of row buffer misses during reads");

    writeRowMisses
        .name(name() + ".writeRowMisses")
        .desc("Number of row buffer misses during writes");
}

Tick
SimpleMemory::recvAtomic(PacketPtr pkt)
{
    access(pkt, Profiler::Null);
    return pkt->memInhibitAsserted() ? 0 : getLatency();
}

void
SimpleMemory::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    functionalAccess(pkt);

    // potentially update the packets in our packet queue as well
    for (auto i = packetQueue.begin(); i != packetQueue.end(); ++i)
        pkt->checkFunctional(i->pkt);

    pkt->popLabel();
}

bool
SimpleMemory::isWait()
{
    return waitStart;
}

void
SimpleMemory::setWait()
{
    if (!waitStart) {
        waitStart = curTick();
    }
}

void
SimpleMemory::clearWait()
{
    assert(isWait());
    totalWaitTime += curTick() - waitStart;
    waitStart = 0;
}

bool
SimpleMemory::recvTimingReq(PacketPtr pkt)
{
    /// @todo temporary hack to deal with memory corruption issues until
    /// 4-phase transactions are complete
    for (int x = 0; x < pendingDelete.size(); x++)
        delete pendingDelete[x];
    pendingDelete.clear();

    if (pkt->memInhibitAsserted()) {
        // snooper will supply based on copy of packet
        // still target's responsibility to delete packet
        pendingDelete.push_back(pkt);
        return true;
    }

    // we should never get a new request after committing to retry the
    // current one, the bus violates the rule as it simply sends a
    // retry to the next one waiting on the retry list, so simply
    // ignore it
    if (retryReq)
        return false;

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        retryReq = true;
        return false;
    }

    if (pkt->cmd != MemCmd::SwapReq && pkt->isWrite()) {
        Control ctrl = addrController.Probe(pkt->getAddr());
        if (ctrl == NEW_EPOCH) {
            Profiler pf(profBase);
            addrController.MigratePages(pf);
            bytesChannel += pf.SumBusUtil();
            bytesInterChannel += pf.SumBusUtil(true);

            // ATT and PTT flushes
            uint64_t area = addrController.att_length() * 8;
            area += addrController.migrator().ptt_length() * 8;
            Tick duration = area * wbBandwidth;
            duration += pf.SumLatency();
            totalWaitTime += duration;
            schedule(freezeEvent, curTick() + duration);
            isBusy = true;
            retryReq = true;
            return false;
        } else if (ctrl == WAIT_CKPT) {
            setWait();
            return false;
        } else assert(ctrl == REG_WRITE);
    }

    // @todo someone should pay for this
    pkt->busFirstWordDelay = pkt->busLastWordDelay = 0;

    // update the release time according to the bandwidth limit, and
    // do so with respect to the time it takes to finish this request
    // rather than long term as it is the short term data rate that is
    // limited for any real memory

    // only look at reads and writes when determining if we are busy,
    // and for how long, as it is not clear what to regulate for the
    // other types of commands
    if (pkt->isRead() || pkt->isWrite()) {
        // calculate an appropriate tick to release to not exceed
        // the bandwidth limit
        Tick duration = pkt->getSize() * bandwidth;

        // only consider ourselves busy if there is any need to wait
        // to avoid extra events being scheduled for (infinitely) fast
        // memories
        if (duration != 0) {
            schedule(releaseEvent, curTick() + duration);
            isBusy = true;
        }
    }

    // go ahead and deal with the packet and put the response in the
    // queue if there is one
    bool needsResponse = pkt->needsResponse();

    Profiler pf(profBase);
    access(pkt, pf);
    bytesChannel += pf.SumBusUtil();
    bytesInterChannel += pf.SumBusUtil(true);

    // turn packet around to go back to requester if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());
        Tick lat = pf.SumLatency();
        extraRespLatency += (double)lat - getLatency();
        // to keep things simple (and in order), we put the packet at
        // the end even if the latency suggests it should be sent
        // before the packet(s) before it
        packetQueue.push_back(
                DeferredPacket(pkt, curTick() + lat));
        if (!retryResp && !dequeueEvent.scheduled())
            schedule(dequeueEvent, packetQueue.back().tick);
    } else {
        pendingDelete.push_back(pkt);
    }
    return true;
}

void
SimpleMemory::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetry();
    }
}

void
SimpleMemory::freeze()
{
    assert(isBusy);
    isBusy = false;
    assert(numNVMWrites.value() == addrController.migrator().total_nvm_writes());
    assert(numDRAMWrites.value() == addrController.migrator().total_dram_writes());
    assert(numNVMWrites.value() + numDRAMWrites.value() ==
        numATTWriteHits.value() + numATTWriteMisses.value() + ckDRAMWriteHits);
    numDirtyNVMBlocks = addrController.migrator().dirty_nvm_blocks();
    numDirtyNVMPages = addrController.migrator().dirty_nvm_pages();
    numDirtyDRAMPages = addrController.migrator().dirty_dram_pages();
    numPagesToDRAM = addrController.pages_to_dram();
    numPagesToNVM = addrController.pages_to_nvm();

    Profiler pf(profBase);
    addrController.BeginCheckpointing(pf);
    bytesChannel += pf.SumBusUtil();
    bytesInterChannel += pf.SumBusUtil(true);

    Addr block = addrController.NextCheckpointBlock();
    if (block != -EINVAL) {
        schedule(unfreezeEvent, curTick() + latency); //TODO
        assert(ckBusUtil == bytesChannel.value());
        totalCkptTime += latency;
    } else {
        addrController.FinishCheckpointing();
    }

    if (retryReq) {
        retryReq = false;
        port.sendRetry();
    }
}

void
SimpleMemory::unfreeze()
{
    Addr block = addrController.NextCheckpointBlock();
    if (block != -EINVAL) {
        schedule(unfreezeEvent, curTick() + latency); //GetWriteLatency(block, false));
        totalCkptTime += latency;
    } else {
        addrController.FinishCheckpointing();
        if (isWait()) {
            clearWait();
            port.sendRetry();
        }
    }
}

void
SimpleMemory::dequeue()
{
    assert(!packetQueue.empty());
    DeferredPacket deferred_pkt = packetQueue.front();

    retryResp = !port.sendTimingResp(deferred_pkt.pkt);

    if (!retryResp) {
        packetQueue.pop_front();

        // if the queue is not empty, schedule the next dequeue event,
        // otherwise signal that we are drained if we were asked to do so
        if (!packetQueue.empty()) {
            // if there were packets that got in-between then we
            // already have an event scheduled, so use re-schedule
            reschedule(dequeueEvent,
                       std::max(packetQueue.front().tick, curTick()), true);
        } else if (drainManager) {
            drainManager->signalDrainDone();
            drainManager = NULL;
        }
    }
}

Tick
SimpleMemory::getLatency()
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

uint64_t
SimpleMemory::GetReadLatency(Addr mach_addr, bool is_dram)
{
    bool hit = banks.access(mach_addr);
    DPRINTF(RowBuffer, "RowBuffer: Read addr=%lx %d\n", mach_addr, hit);
    if (hit) {
        ++readRowHits;
        return latency;
    } else {
        ++readRowMisses;
        return is_dram ? latency_miss : tNVMRead;
    }
}

uint64_t
SimpleMemory::GetWriteLatency(Addr mach_addr, bool is_dram)
{
    bool hit = banks.access(mach_addr);
    DPRINTF(RowBuffer, "RowBuffer: Write addr=%lx %d\n", mach_addr, hit);
    if (hit) {
        ++writeRowHits;
        return latency;
    } else {
        ++writeRowMisses;
        return is_dram ? latency_miss : tNVMWrite;
    }
}

void
SimpleMemory::recvRetry()
{
    assert(retryResp);

    dequeue();
}

BaseSlavePort &
SimpleMemory::getSlavePort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return MemObject::getSlavePort(if_name, idx);
    } else {
        return port;
    }
}

unsigned int
SimpleMemory::drain(DrainManager *dm)
{
    int count = 0;

    // also track our internal queue
    if (!packetQueue.empty()) {
        count += 1;
        drainManager = dm;
    }

    if (count)
        setDrainState(Drainable::Draining);
    else
        setDrainState(Drainable::Drained);
    return count;
}

SimpleMemory::MemoryPort::MemoryPort(const std::string& _name,
                                     SimpleMemory& _memory)
    : SlavePort(_name, &_memory), memory(_memory)
{ }

AddrRangeList
SimpleMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}

Tick
SimpleMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return memory.recvAtomic(pkt);
}

void
SimpleMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    memory.recvFunctional(pkt);
}

bool
SimpleMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return memory.recvTimingReq(pkt);
}

void
SimpleMemory::MemoryPort::recvRetry()
{
    memory.recvRetry();
}

SimpleMemory*
SimpleMemoryParams::create()
{
    return new SimpleMemory(this);
}
