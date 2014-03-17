/*
 * Copyright (c) 2012 ARM Limited
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
 *          Andreas Hansson
 */

/**
 * @file
 * SimpleMemory declaration
 */

#ifndef __SIMPLE_MEMORY_HH__
#define __SIMPLE_MEMORY_HH__

#include "mem/abstract_mem.hh"
#include "mem/tport.hh"
#include "params/SimpleMemory.hh"

/**
 * The simple memory is a basic single-ported memory controller with
 * an configurable throughput and latency, potentially with a variance
 * added to the latter. It uses a QueueSlavePort to avoid dealing with
 * the flow control of sending responses.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
class SimpleMemory : public AbstractMemory
{

  private:

    class MemoryPort : public QueuedSlavePort
    {

      private:

        /// Queue holding the response packets
        SlavePacketQueue queueImpl;
        SimpleMemory& memory;

      public:

        MemoryPort(const std::string& _name, SimpleMemory& _memory);

      protected:

        Tick recvAtomic(PacketPtr pkt);

        void recvFunctional(PacketPtr pkt);

        bool recvTimingReq(PacketPtr pkt);

        AddrRangeList getAddrRanges() const;

    };

    MemoryPort port;

    const Tick lat;
    const Tick lat_var;

    const Tick latATTLookup;
    const Tick latATTUpdate;
    const Tick latBlkWriteback;
    const Tick latNVMRead;
    const Tick latNVMWrite;

    Tick latATT;

    /// Bandwidth in ticks per byte
    const double bandwidth;

    /**
     * Track the state of the memory as either idle or busy, no need
     * for an enum with only two states.
     */
    bool isBusy;

    /**
     * Remember if we have to retry an outstanding request that
     * arrived while we were busy.
     */
    bool retryReq;

    /**
     * Release the memory after being busy and send a retry if a
     * request was rejected in the meanwhile.
     */
    void release();

    EventWrapper<SimpleMemory, &SimpleMemory::release> releaseEvent;

    /** @todo this is a temporary workaround until the 4-phase code is
     * committed. upstream caches needs this packet until true is returned, so
     * hold onto it for deletion until a subsequent call
     */
    std::vector<PacketPtr> pendingDelete;

  public:

    SimpleMemory(const SimpleMemoryParams *p);
    virtual ~SimpleMemory() { }

    unsigned int drain(DrainManager *dm);

    virtual BaseSlavePort& getSlavePort(const std::string& if_name,
                                        PortID idx = InvalidPortID);
    virtual void init();

    virtual void OnDirectWrite(uint64_t phy_tag, uint64_t mach_tag, int bits);
    virtual void OnWriteBack(uint64_t phy_tag, uint64_t mach_tag, int bits);
    virtual void OnOverwrite(uint64_t phy_tag, uint64_t mach_tag, int bits);
    virtual void OnShrink(uint64_t phy_tag, uint64_t mach_tag, int bits);
    virtual void OnEpochEnd(int bits);
    virtual void OnNVMRead(uint64_t mach_addr);
    virtual void OnNVMWrite(uint64_t mach_addr);

  protected:

    Tick doAtomicAccess(PacketPtr pkt);
    void doFunctionalAccess(PacketPtr pkt);
    bool recvTimingReq(PacketPtr pkt);
    Tick calculateLatency(PacketPtr pkt);

};

#endif //__SIMPLE_MEMORY_HH__
