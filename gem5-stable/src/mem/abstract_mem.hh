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
 *          Jinglei Ren <jinglei@ren.systems>
 */

/**
 * @file
 * AbstractMemory declaration
 */

#ifndef __ABSTRACT_MEMORY_HH__
#define __ABSTRACT_MEMORY_HH__

#include "mem/mem_object.hh"
#include "params/AbstractMemory.hh"
#include "sim/stats.hh"

#include "mem/addr_trans_controller.h"

class System;

/**
 * Locked address class that represents a physical address and a
 * context id.
 */
class LockedAddr {

  private:

    // on alpha, minimum LL/SC granularity is 16 bytes, so lower
    // bits need to masked off.
    static const Addr Addr_Mask = 0xf;

  public:

    // locked address
    Addr addr;

    // locking hw context
    const int contextId;

    static Addr mask(Addr paddr) { return (paddr & ~Addr_Mask); }

    // check for matching execution context
    bool matchesContext(Request *req) const
    {
        return (contextId == req->contextId());
    }

    LockedAddr(Request *req) : addr(mask(req->getPaddr())),
                               contextId(req->contextId())
    {}

    // constructor for unserialization use
    LockedAddr(Addr _addr, int _cid) : addr(_addr), contextId(_cid)
    {}
};

/**
 * An abstract memory represents a contiguous block of physical
 * memory, with an associated address range, and also provides basic
 * functionality for reading and writing this memory without any
 * timing information. It is a MemObject since any subclass must have
 * at least one slave port.
 */
class AbstractMemory : public MemObject, public MemStore
{
  protected:
    // Physical ddress range of this memory
    AddrRange range;

    // Base profiler for copy construction
    Profiler profBase;

    // Controller for addr translation
    AddrTransController addrController;

    // Pointer to host memory used to implement this memory
    uint8_t* pmemAddr;

    // Enable specific memories to be reported to the configuration table
    bool confTableReported;

    // Should the memory appear in the global address map
    bool inAddrMap;

    std::list<LockedAddr> lockedAddrList;

    // helper function for checkLockedAddrs(): we really want to
    // inline a quick check for an empty locked addr list (hopefully
    // the common case), and do the full list search (if necessary) in
    // this out-of-line function
    bool checkLockedAddrList(PacketPtr pkt);

    // Record the address of a load-locked operation so that we can
    // clear the execution context's lock flag if a matching store is
    // performed
    void trackLoadLocked(PacketPtr pkt);

    // Compare a store address with any locked addresses so we can
    // clear the lock flag appropriately.  Return value set to 'false'
    // if store operation should be suppressed (because it was a
    // conditional store and the address was no longer locked by the
    // requesting execution context), 'true' otherwise.  Note that
    // this method must be called on *all* stores since even
    // non-conditional stores must clear any matching lock addresses.
    bool writeOK(PacketPtr pkt) {
        Request *req = pkt->req;
        if (lockedAddrList.empty()) {
            // no locked addrs: nothing to check, store_conditional fails
            bool isLLSC = pkt->isLLSC();
            if (isLLSC) {
                req->setExtraData(0);
            }
            return !isLLSC; // only do write if not an sc
        } else {
            // iterate over list...
            return checkLockedAddrList(pkt);
        }
    }

    // Convert physical address to local address
    Addr localAddr(PacketPtr pkt) const
    { return pkt->getAddr() - range.start(); }

    // Convert local address to host memory address
    uint8_t* hostAddr(Addr local_addr) const
    {
        assert(local_addr < hostSize());
        return pmemAddr + local_addr;
    }

    /** Number of total bytes read from this memory */
    Stats::Vector bytesRead;
    /** Number of instruction bytes read from this memory */
    Stats::Vector bytesInstRead;
    /** Number of bytes written to this memory */
    Stats::Vector bytesWritten;
    /** Number of read requests */
    Stats::Vector numReads;
    /** Number of write requests */
    Stats::Vector numWrites;
    /** Number of other requests */
    Stats::Vector numOther;
    /** Read bandwidth from this memory */
    Stats::Formula bwRead;
    /** Read bandwidth from this memory */
    Stats::Formula bwInstRead;
    /** Write bandwidth from this memory */
    Stats::Formula bwWrite;
    /** Total bandwidth from this memory */
    Stats::Formula bwTotal;

    /** Number of epochs */
    Stats::Scalar numEpochs;
    /** Number of free-writes */
    Stats::Scalar numFreeWrites;
    /** Number of shrink-writes */
    Stats::Scalar numShrinkWrites;
    /** Number of replace-writes */
    Stats::Scalar numReplWrites;

    /** Number of writes on NVM pages */
    Stats::Scalar numNVMWrites;
    /** Number of writes on DRAM pages */
    Stats::Scalar numDRAMWrites;
    /** Number of dirty NVM blocks */
    Stats::Scalar numDirtyNVMBlocks;
    /** Number of dirty NVM pages */
    Stats::Scalar numDirtyNVMPages;
    /** Number of dirty DRAM pages */
    Stats::Scalar numDirtyDRAMPages;

    /** Data transfer through channel in bytes */
    Stats::Scalar bytesChannel;
    /** Data transfer excluding intra-channel in bytes */
    Stats::Scalar bytesInterChannel;

    /** Average dirty ratio of NVM pages */
    Stats::Formula avgNVMDirtyRatio;
    /** Average write ratio of DRAM pages */
    Stats::Formula avgDRAMWriteRatio;

    /** Total number of pages migrated from NVM to DRAM */
    Stats::Scalar numPagesToDRAM;
    /** Total number of pages migrated from DRAM to NVM */
    Stats::Scalar numPagesToNVM;
    /** Average number of pages migrated to DRAM per epoch */
    Stats::Formula avgPagesToDRAM;
    /** Average number of pages migrated to NVM per epoch */
    Stats::Formula avgPagesToNVM;

    int regCaches;
    Stats::Formula numRegCaches;

    /** Pointor to the System object.
     * This is used for getting the number of masters in the system which is
     * needed when registering stats
     */
    System *_system;

    uint64_t ckBusUtil;

  private:

    // Prevent copying
    AbstractMemory(const AbstractMemory&);

    // Prevent assignment
    AbstractMemory& operator=(const AbstractMemory&);

#ifdef MEMCK
    uint8_t* ckmem;
#endif

  public:

    typedef AbstractMemoryParams Params;

    AbstractMemory(const Params* p);
    virtual ~AbstractMemory() {}

    /**
     * See if this is a null memory that should never store data and
     * always return zero.
     *
     * @return true if null
     */
    bool isNull() const { return params()->null; }

    /**
     * Set the host memory backing store to be used by this memory
     * controller.
     *
     * @param pmem_addr Pointer to a segment of host memory
     */
    void setBackingStore(uint8_t* pmem_addr);

    /**
     * Get the length of backing host memory
     */
    uint64_t hostSize() const
    { return addrController.Size(); };

    /**
     * Get the list of locked addresses to allow checkpointing.
     */
    const std::list<LockedAddr>& getLockedAddrList() const
    { return lockedAddrList; }

    /**
     * Add a locked address to allow for checkpointing.
     */
    void addLockedAddr(LockedAddr addr) { lockedAddrList.push_back(addr); }

    /** read the system pointer
     * Implemented for completeness with the setter
     * @return pointer to the system object */
    System* system() const { return _system; }

    /** Set the system pointer on this memory
     * This can't be done via a python parameter because the system needs
     * pointers to all the memories and the reverse would create a cycle in the
     * object graph. An init() this is set.
     * @param sys system pointer to set
     */
    void system(System *sys) { _system = sys; }

    const Params *
    params() const
    {
        return dynamic_cast<const Params *>(_params);
    }

    /**
     * Get the physical address range
     *
     * @return a single contiguous address range
     */
    AddrRange getAddrRange() const;

    /**
     * Get the physical memory size.
     *
     * @return the physical size of the memory
     */
    uint64_t size() const { return range.size(); }

    /**
     * Get the start address.
     *
     * @return the start address of the memory
     */
    Addr start() const { return range.start(); }

    /**
     *  Should this memory be passed to the kernel and part of the OS
     *  physical memory layout.
     *
     * @return if this memory is reported
     */
    bool isConfReported() const { return confTableReported; }

    /**
     * Some memories are used as shadow memories or should for other
     * reasons not be part of the global address map.
     *
     * @return if this memory is part of the address map
     */
    bool isInAddrMap() const { return inAddrMap; }

    /**
     * Perform an untimed memory access and update all the state
     * (e.g. locked addresses) and statistics accordingly. The packet
     * is turned into a response if required.
     *
     * @param pkt Packet performing the access
     * @param pf Profiler counting internal behaviors
     */
    void access(PacketPtr pkt, Profiler& pf = Profiler::Null);

    /**
     * Perform an untimed memory read or write without changing
     * anything but the memory itself. No stats are affected by this
     * access. In addition to normal accesses this also facilitates
     * print requests.
     *
     * @param pkt Packet performing the access
     */
    void functionalAccess(PacketPtr pkt);

    /**
     * Register Statistics
     */
    virtual void regStats();

    virtual bool isDRAM(Addr phy_addr)
    {
        return addrController.IsDRAM(phy_addr, Profiler::Null);
    }

    virtual void MemCopy(uint64_t direct_addr, uint64_t mach_addr, int size);
    virtual void MemSwap(uint64_t direct_addr, uint64_t mach_addr, int size);

    virtual void OnWaiting()
    {
        panic("AbstractMemory should never have write waiting\n");
    }

    virtual void OnEpochEnd()
    {
        ++numEpochs;
    }

    virtual void OnATTFreeSetup(uint64_t phy_addr, int state)
    {
        ++numFreeWrites;
    }

    virtual void OnATTHideClean(uint64_t phy_addr, bool move_data)
    {
        ++numShrinkWrites;
    }

    virtual void OnATTResetClean(uint64_t phy_addr, bool move_data)
    {
        ++numShrinkWrites;
    }

    virtual void OnATTFreeClean(uint64_t phy_addr, bool move_data)
    {
        ++numReplWrites;
    }

    virtual void OnATTFreeLoan(uint64_t phy_addr, bool move_data)
    {
        ++numReplWrites;
    }

    virtual void OnCacheRegister()
    {
        ++regCaches;
    }

    virtual void ckBusUtilAdd(uint64_t bytes)
    {
        ckBusUtil += bytes;
    }

    virtual void ckNVMWrite() {
        ++numNVMWrites;
    }

    virtual void ckDRAMWrite() {
        ++numDRAMWrites;
    }
};

#endif //__ABSTRACT_MEMORY_HH__
