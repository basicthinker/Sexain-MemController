/*
 * Copyright (c) 2010-2012 ARM Limited
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
 *          Jinglei Ren <jinglei@ren.systems>
 */

#include <sys/mman.h>
#include "arch/registers.hh"
#include "config/the_isa.hh"
#include "debug/LLSC.hh"
#include "debug/MemoryAccess.hh"
#include "mem/abstract_mem.hh"
#include "mem/packet_access.hh"
#include "sim/system.hh"
#include "base/intmath.hh"

using namespace std;

AbstractMemory::AbstractMemory(const Params *p) :
    MemObject(p), range(params()->range),
    profBase(p->block_bits, p->page_bits),
    addrController(range.size(), p->dram_size,
            p->att_length, p->block_bits, p->page_bits, this),
    pmemAddr(NULL), confTableReported(p->conf_table_reported),
    inAddrMap(p->in_addr_map), _system(NULL)
{
    if (range.size() % TheISA::PageBytes != 0)
        panic("Memory Size not divisible by page size\n");
    ckBusUtil = 0;
    ckDRAMWriteHits = 0;
    regCaches = 0;
#ifdef MEMCK
    ckmem = (uint8_t*) mmap(NULL, hostSize(), PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_PRIVATE, -1, 0);
    inform("THNVM runs with memory check.\n");
#endif
}

void
AbstractMemory::setBackingStore(uint8_t* pmem_addr)
{
    pmemAddr = pmem_addr;
}

void
AbstractMemory::regStats()
{
    using namespace Stats;

    assert(system());

    bytesRead
        .init(system()->maxMasters())
        .name(name() + ".bytes_read")
        .desc("Number of bytes read from this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bytesRead.subname(i, system()->getMasterName(i));
    }
    bytesInstRead
        .init(system()->maxMasters())
        .name(name() + ".bytes_inst_read")
        .desc("Number of instructions bytes read from this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bytesInstRead.subname(i, system()->getMasterName(i));
    }
    bytesWritten
        .init(system()->maxMasters())
        .name(name() + ".bytes_written")
        .desc("Number of bytes written to this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bytesWritten.subname(i, system()->getMasterName(i));
    }
    numReads
        .init(system()->maxMasters())
        .name(name() + ".num_reads")
        .desc("Number of read requests responded to by this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        numReads.subname(i, system()->getMasterName(i));
    }
    numWrites
        .init(system()->maxMasters())
        .name(name() + ".num_writes")
        .desc("Number of write requests responded to by this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        numWrites.subname(i, system()->getMasterName(i));
    }
    numOther
        .init(system()->maxMasters())
        .name(name() + ".num_other")
        .desc("Number of other requests responded to by this memory")
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        numOther.subname(i, system()->getMasterName(i));
    }
    bwRead
        .name(name() + ".bw_read")
        .desc("Total read bandwidth from this memory (bytes/s)")
        .precision(0)
        .prereq(bytesRead)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bwRead.subname(i, system()->getMasterName(i));
    }

    bwInstRead
        .name(name() + ".bw_inst_read")
        .desc("Instruction read bandwidth from this memory (bytes/s)")
        .precision(0)
        .prereq(bytesInstRead)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bwInstRead.subname(i, system()->getMasterName(i));
    }
    bwWrite
        .name(name() + ".bw_write")
        .desc("Write bandwidth from this memory (bytes/s)")
        .precision(0)
        .prereq(bytesWritten)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bwWrite.subname(i, system()->getMasterName(i));
    }
    bwTotal
        .name(name() + ".bw_total")
        .desc("Total bandwidth to/from this memory (bytes/s)")
        .precision(0)
        .prereq(bwTotal)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < system()->maxMasters(); i++) {
        bwTotal.subname(i, system()->getMasterName(i));
    }

    numEpochs
        .name(name() + ".num_epochs")
        .desc("Total number of epochs");
    numATTWriteHits
        .name(name() + ".att_write_hits")
        .desc("Total number of write hits on ATT");
    numATTWriteMisses
        .name(name() + ".att_write_misses")
        .desc("Total number of write misses on ATT");

    numNVMWrites
        .name(name() + ".num_nvm_writes")
        .desc("Total number of writes on NVM pages");
    numDRAMWrites
        .name(name() + ".num_dram_writes")
        .desc("Total number of writes on DRAM pages");
    numDirtyNVMBlocks
        .name(name() + ".dirty_nvm_blocks")
        .desc("Total number of dirty NVM blocks");
    numDirtyNVMPages
        .name(name() + ".dirty_nvm_pages")
        .desc("Total number of dirty NVM pages");
    numDirtyDRAMPages
        .name(name() + ".dirty_dram_pages")
        .desc("Total number of dirty DRAM pages");

    bytesChannel
        .name(name() + ".bytes_channel")
        .desc("Data transfer through channel");
    bytesInterChannel
        .name(name() + ".bytes_inter_channel")
        .desc("Data transfer through channel excluding intra");

    avgNVMDirtyRatio
        .name(name() + ".avg_nvm_dirty_ratio")
        .desc("Average dirty ratio of NVM pages")
        .prereq(numDirtyNVMBlocks);
    avgDRAMWriteRatio
        .name(name() + ".avg_dram_write_ratio")
        .desc("Average write ratio of DRAM pages")
        .prereq(numDirtyDRAMPages);

    numPagesToDRAM
        .name(name() + ".num_pages_to_dram")
        .desc("Total number of pages ever migrated from NVM to DRAM");
    numPagesToNVM
        .name(name() + ".num_pages_to_nvm")
        .desc("Total number of pages ever migrated from DRAM to NVM");
    avgPagesToDRAM
        .name(name() + ".avg_pages_to_dram")
        .desc("Number of pages migrated to DRAM per epoch")
        .prereq(numPagesToDRAM);
    avgPagesToNVM
        .name(name() + ".avg_pages_to_nvm")
        .desc("Number of pages migrated to NVM per epoch")
        .prereq(numPagesToNVM);

    numRegCaches
        .name(name() + ".num_reg_caches")
        .desc("Number of caches registered to the THNVM cache controller");
    numRegCaches = constant(regCaches);

    bwRead = bytesRead / simSeconds;
    bwInstRead = bytesInstRead / simSeconds;
    bwWrite = bytesWritten / simSeconds;
    bwTotal = (bytesRead + bytesWritten) / simSeconds;

    avgNVMDirtyRatio = numDirtyNVMBlocks / numDirtyNVMPages /
        constant(addrController.migrator().page_blocks());
    avgDRAMWriteRatio = numDRAMWrites / numDirtyDRAMPages /
        constant(addrController.migrator().page_blocks());
    avgPagesToDRAM = numPagesToDRAM / numEpochs;
    avgPagesToNVM = numPagesToNVM / numEpochs;
}

AddrRange
AbstractMemory::getAddrRange() const
{
    return range;
}

// Add load-locked to tracking list.  Should only be called if the
// operation is a load and the LLSC flag is set.
void
AbstractMemory::trackLoadLocked(PacketPtr pkt)
{
    Request *req = pkt->req;
    Addr paddr = LockedAddr::mask(req->getPaddr());

    // first we check if we already have a locked addr for this
    // xc.  Since each xc only gets one, we just update the
    // existing record with the new address.
    list<LockedAddr>::iterator i;

    for (i = lockedAddrList.begin(); i != lockedAddrList.end(); ++i) {
        if (i->matchesContext(req)) {
            DPRINTF(LLSC, "Modifying lock record: context %d addr %#x\n",
                    req->contextId(), paddr);
            i->addr = paddr;
            return;
        }
    }

    // no record for this xc: need to allocate a new one
    DPRINTF(LLSC, "Adding lock record: context %d addr %#x\n",
            req->contextId(), paddr);
    lockedAddrList.push_front(LockedAddr(req));
}


// Called on *writes* only... both regular stores and
// store-conditional operations.  Check for conventional stores which
// conflict with locked addresses, and for success/failure of store
// conditionals.
bool
AbstractMemory::checkLockedAddrList(PacketPtr pkt)
{
    Request *req = pkt->req;
    Addr paddr = LockedAddr::mask(req->getPaddr());
    bool isLLSC = pkt->isLLSC();

    // Initialize return value.  Non-conditional stores always
    // succeed.  Assume conditional stores will fail until proven
    // otherwise.
    bool allowStore = !isLLSC;

    // Iterate over list.  Note that there could be multiple matching records,
    // as more than one context could have done a load locked to this location.
    // Only remove records when we succeed in finding a record for (xc, addr);
    // then, remove all records with this address.  Failed store-conditionals do
    // not blow unrelated reservations.
    list<LockedAddr>::iterator i = lockedAddrList.begin();

    if (isLLSC) {
        while (i != lockedAddrList.end()) {
            if (i->addr == paddr && i->matchesContext(req)) {
                // it's a store conditional, and as far as the memory system can
                // tell, the requesting context's lock is still valid.
                DPRINTF(LLSC, "StCond success: context %d addr %#x\n",
                        req->contextId(), paddr);
                allowStore = true;
                break;
            }
            // If we didn't find a match, keep searching!  Someone else may well
            // have a reservation on this line here but we may find ours in just
            // a little while.
            i++;
        }
        req->setExtraData(allowStore ? 1 : 0);
    }
    // LLSCs that succeeded AND non-LLSC stores both fall into here:
    if (allowStore) {
        // We write address paddr.  However, there may be several entries with a
        // reservation on this address (for other contextIds) and they must all
        // be removed.
        i = lockedAddrList.begin();
        while (i != lockedAddrList.end()) {
            if (i->addr == paddr) {
                DPRINTF(LLSC, "Erasing lock record: context %d addr %#x\n",
                        i->contextId, paddr);
                i = lockedAddrList.erase(i);
            } else {
                i++;
            }
        }
    }

    return allowStore;
}


#if TRACING_ON

#define CASE(A, T)                                                        \
  case sizeof(T):                                                         \
    DPRINTF(MemoryAccess,"%s from %s of size %i on address 0x%x data " \
            "0x%x %c\n", A, system()->getMasterName(pkt->req->masterId()),\
            pkt->getSize(), pkt->getAddr(), pkt->get<T>(),                \
            pkt->req->isUncacheable() ? 'U' : 'C');                       \
  break


#define TRACE_PACKET(A)                                                 \
    do {                                                                \
        switch (pkt->getSize()) {                                       \
          CASE(A, uint64_t);                                            \
          CASE(A, uint32_t);                                            \
          CASE(A, uint16_t);                                            \
          CASE(A, uint8_t);                                             \
          default:                                                      \
            DPRINTF(MemoryAccess, "%s from %s of size %i on address 0x%x %c\n",\
                    A, system()->getMasterName(pkt->req->masterId()),          \
                    pkt->getSize(), pkt->getAddr(),                            \
                    pkt->req->isUncacheable() ? 'U' : 'C');                    \
            DDUMP(MemoryAccess, pkt->getPtr<uint8_t>(), pkt->getSize());       \
        }                                                                      \
    } while (0)

#else

#define TRACE_PACKET(A)

#endif

#ifdef MEMCK

#define MEMCK_BEFORE_READ(HA, PKT)                                             \
    do {                                                                       \
        uint8_t* shadow_addr = ckmem + localAddr(PKT);                         \
        if (std::memcmp(HA, shadow_addr, (PKT)->getSize()) != 0) {             \
            warn("File %s, line %d: Memory read of %d bytes corrupted @ %lx\n",\
                 __FILE__, __LINE__, (PKT)->getSize(), localAddr(PKT));        \
            pair<AddrInfo, AddrInfo> info =                                    \
                    addrController.GetAddrInfo(localAddr(PKT));                \
            if (info.first.state) {                                            \
                warn("\tATT: %lx => %lx in %s\n", info.first.phy_base,         \
                     info.first.mach_base, info.first.state);                  \
            } else warn("\tATT: none");                                        \
            if (info.second.state) {                                           \
                warn("\tPTT: %lx => %lx in %s\n", info.second.phy_base,        \
                     info.second.mach_base, info.second.state);                \
            } else warn("\tPTT: none");                                        \
        }                                                                      \
        HA = shadow_addr;                                                      \
    } while (0)

#define MEMCK_AFTER_WRITE(LA, PKT)                                             \
    do {                                                                       \
        Addr post_addr = addrController.LoadAddr(localAddr(PKT), Profiler::Null);\
    	if (post_addr != (LA)) {                                               \
    	    warn("File %s, line %d: Memory write meets corrupted address: "    \
    	         "%lx => %lx for physical %lx\n",                              \
    	         __FILE__, __LINE__, (LA), post_addr, localAddr(PKT));         \
        }                                                                      \
        uint8_t* shadow_addr = ckmem + localAddr(PKT);                         \
        memcpy(shadow_addr, hostAddr(LA), (PKT)->getSize());                   \
    } while (0)

#else

#define MEMCK_BEFORE_READ(host_addr, pkt)
#define MEMCK_AFTER_WRITE(local_addr, pkt)

#endif

void
AbstractMemory::access(PacketPtr pkt, Profiler& pf)
{
    assert(AddrRange(pkt->getAddr(),
                     pkt->getAddr() + pkt->getSize() - 1).isSubset(range));

    if (pkt->memInhibitAsserted()) {
        DPRINTF(MemoryAccess, "mem inhibited on 0x%x: not responding\n",
                pkt->getAddr());
        return;
    }

    if (pkt->cmd == MemCmd::SwapReq) {
        TheISA::IntReg overwrite_val;
        bool overwrite_mem;
        uint64_t condition_val64;
        uint32_t condition_val32;

        if (!pmemAddr)
            panic("Swap only works if there is real memory (i.e. null=False)");
        assert(sizeof(TheISA::IntReg) >= pkt->getSize());

        overwrite_mem = true;
        // keep a copy of our possible write value, and copy what is at the
        // memory address into the packet
        memcpy(&overwrite_val, pkt->getPtr<uint8_t>(), pkt->getSize());
        uint8_t* host_addr = hostAddr(
            addrController.LoadAddr(localAddr(pkt), pf));
        MEMCK_BEFORE_READ(host_addr, pkt);
        memcpy(pkt->getPtr<uint8_t>(), host_addr, pkt->getSize());

        if (pkt->req->isCondSwap()) {
            if (pkt->getSize() == sizeof(uint64_t)) {
                condition_val64 = pkt->req->getExtraData();
                overwrite_mem = !std::memcmp(&condition_val64, host_addr,
                                             sizeof(uint64_t));
            } else if (pkt->getSize() == sizeof(uint32_t)) {
                condition_val32 = (uint32_t)pkt->req->getExtraData();
                overwrite_mem = !std::memcmp(&condition_val32, host_addr,
                                             sizeof(uint32_t));
            } else
                panic("Invalid size for conditional read/write\n");
        }

        if (overwrite_mem) {
            Addr local_addr = addrController.LoadAddr(localAddr(pkt), pf);
            memcpy(hostAddr(local_addr), &overwrite_val, pkt->getSize());
            MEMCK_AFTER_WRITE(local_addr, pkt);
        }

        assert(!pkt->req->isInstFetch());
        TRACE_PACKET("Read/Write");
        numOther[pkt->req->masterId()]++;
    } else if (pkt->isRead()) {
        assert(!pkt->isWrite());
        if (pkt->isLLSC()) {
            trackLoadLocked(pkt);
        }
        if (pmemAddr) {
            assert(pkt->getSize() == addrController.block_size());
            uint8_t* host_addr =
                    hostAddr(addrController.LoadAddr(localAddr(pkt), pf));
            MEMCK_BEFORE_READ(host_addr, pkt);
            memcpy(pkt->getPtr<uint8_t>(), host_addr, pkt->getSize());
        }
        TRACE_PACKET(pkt->req->isInstFetch() ? "IFetch" : "Read");
        numReads[pkt->req->masterId()]++;
        bytesRead[pkt->req->masterId()] += pkt->getSize();
        if (pkt->req->isInstFetch())
            bytesInstRead[pkt->req->masterId()] += pkt->getSize();
    } else if (pkt->isWrite()) {
        if (writeOK(pkt)) {
            if (pmemAddr) {
                assert(pkt->getSize() == addrController.block_size());
                Addr local_addr = addrController.StoreAddr(
                        localAddr(pkt), pkt->getSize(), pf);
                memcpy(hostAddr(local_addr), pkt->getPtr<uint8_t>(),
                        pkt->getSize());
                MEMCK_AFTER_WRITE(local_addr, pkt);
                pf.AddBlockMoveInter();
                ckBusUtilAdd(addrController.block_size());
                DPRINTF(MemoryAccess, "%s wrote %x bytes to address %x\n",
                        __func__, pkt->getSize(), pkt->getAddr());
            }
            assert(!pkt->req->isInstFetch());
            TRACE_PACKET("Write");
            numWrites[pkt->req->masterId()]++;
            bytesWritten[pkt->req->masterId()] += pkt->getSize();
        }
    } else if (pkt->isInvalidate()) {
        // no need to do anything
    } else {
        panic("unimplemented");
    }

    if (pkt->needsResponse()) {
        pkt->makeResponse();
    }
}

void
AbstractMemory::functionalAccess(PacketPtr pkt)
{
    assert(AddrRange(pkt->getAddr(),
                     pkt->getAddr() + pkt->getSize() - 1).isSubset(range));
    Addr local_addr = addrController.LoadAddr(localAddr(pkt), Profiler::Null);
    uint8_t *host_addr = hostAddr(local_addr);

    if (pkt->isRead()) {
        if (pmemAddr) {
            MEMCK_BEFORE_READ(host_addr, pkt);
            memcpy(pkt->getPtr<uint8_t>(), host_addr, pkt->getSize());
        }
        TRACE_PACKET("Read");
        pkt->makeResponse();
    } else if (pkt->isWrite()) {
        if (pmemAddr) {
            memcpy(host_addr, pkt->getPtr<uint8_t>(), pkt->getSize());
            MEMCK_AFTER_WRITE(local_addr, pkt);
        }
        TRACE_PACKET("Write");
        pkt->makeResponse();
    } else if (pkt->isPrint()) {
        Packet::PrintReqState *prs =
            dynamic_cast<Packet::PrintReqState*>(pkt->senderState);
        assert(prs);
        // Need to call printLabels() explicitly since we're not going
        // through printObj().
        prs->printLabels();
        // Right now we just print the single byte at the specified address.
        ccprintf(prs->os, "%s%#x\n", prs->curPrefix(), *host_addr);
    } else {
        panic("AbstractMemory: unimplemented functional command %s",
              pkt->cmdString());
    }
}

void
AbstractMemory::MemCopy(uint64_t direct_addr, uint64_t mach_addr, int size)
{
    assert(direct_addr != mach_addr);
    memcpy(hostAddr(direct_addr), hostAddr(mach_addr), size);
    ckBusUtilAdd(size);
}

void
AbstractMemory::MemSwap(uint64_t direct_addr, uint64_t mach_addr, int size)
{
    assert(direct_addr != mach_addr);
    uint8_t data[size];
    memcpy(data, hostAddr(direct_addr), size);
    memcpy(hostAddr(direct_addr), hostAddr(mach_addr), size);
    memcpy(hostAddr(mach_addr), data, size);
    ckBusUtilAdd(size * 3);
}

