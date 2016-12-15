// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Utils.hh"
#include "GossamerException.hh"

#ifdef __GNUC__
#include <dirent.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <atomic>


namespace Gossamer {

#if 0
    const uint8_t X = 0xFF;

    const uint8_t sPrecomputedPopcnt4[16] = {
        0, 1, 1, 2, 1, 2, 1, 3, 1, 2, 2, 3, 2, 3, 3, 4
    };

    const uint8_t sPrecomputedSelect4[4][16] = {
        { X, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 },
        { X, X, X, 1, X, 2, 2, 1, X, 3, 3, 1, 3, 2, 2, 1 },
        { X, X, X, X, X, X, X, 2, X, X, X, 3, X, 3, 3, 2 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 3 }
    };

    const uint8_t sPrecomputedPopcnt8[256] = {
        0, 1, 1, 2, 1, 2, 1, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 2, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 2, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 3, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 2, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 3, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 2, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        3, 4, 4, 5, 4, 5, 4, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 2, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 3, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 3, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 4, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 3, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 4, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 4, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 5, 7, 5, 6, 6, 7, 6, 7, 7, 8
    };

    const uint8_t sPrecomputedSelect8[8][256] = {
        { X, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
          4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 },
        { X, X, X, 1, X, 2, 2, 1, X, 3, 3, 1, 3, 2, 2, 1,
          X, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          X, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1,
          5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          X, 6, 6, 1, 6, 2, 2, 1, 6, 3, 3, 1, 3, 2, 2, 1,
          6, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          6, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1,
          5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          X, 7, 7, 1, 7, 2, 2, 1, 7, 3, 3, 1, 3, 2, 2, 1,
          7, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          7, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1,
          5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          7, 6, 6, 1, 6, 2, 2, 1, 6, 3, 3, 1, 3, 2, 2, 1,
          6, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
          6, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1,
          5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1 },
        { X, X, X, X, X, X, X, 2, X, X, X, 3, X, 3, 3, 2,
          X, X, X, 4, X, 4, 4, 2, X, 4, 4, 3, 4, 3, 3, 2,
          X, X, X, 5, X, 5, 5, 2, X, 5, 5, 3, 5, 3, 3, 2,
          X, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2,
          X, X, X, 6, X, 6, 6, 2, X, 6, 6, 3, 6, 3, 3, 2,
          X, 6, 6, 4, 6, 4, 4, 2, 6, 4, 4, 3, 4, 3, 3, 2,
          X, 6, 6, 5, 6, 5, 5, 2, 6, 5, 5, 3, 5, 3, 3, 2,
          6, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2,
          X, X, X, 7, X, 7, 7, 2, X, 7, 7, 3, 7, 3, 3, 2,
          X, 7, 7, 4, 7, 4, 4, 2, 7, 4, 4, 3, 4, 3, 3, 2,
          X, 7, 7, 5, 7, 5, 5, 2, 7, 5, 5, 3, 5, 3, 3, 2,
          7, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2,
          X, 7, 7, 6, 7, 6, 6, 2, 7, 6, 6, 3, 6, 3, 3, 2,
          7, 6, 6, 4, 6, 4, 4, 2, 6, 4, 4, 3, 4, 3, 3, 2,
          7, 6, 6, 5, 6, 5, 5, 2, 6, 5, 5, 3, 5, 3, 3, 2,
          6, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 3,
          X, X, X, X, X, X, X, 4, X, X, X, 4, X, 4, 4, 3,
          X, X, X, X, X, X, X, 5, X, X, X, 5, X, 5, 5, 3,
          X, X, X, 5, X, 5, 5, 4, X, 5, 5, 4, 5, 4, 4, 3,
          X, X, X, X, X, X, X, 6, X, X, X, 6, X, 6, 6, 3,
          X, X, X, 6, X, 6, 6, 4, X, 6, 6, 4, 6, 4, 4, 3,
          X, X, X, 6, X, 6, 6, 5, X, 6, 6, 5, 6, 5, 5, 3,
          X, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 3,
          X, X, X, 7, X, 7, 7, 4, X, 7, 7, 4, 7, 4, 4, 3,
          X, X, X, 7, X, 7, 7, 5, X, 7, 7, 5, 7, 5, 5, 3,
          X, 7, 7, 5, 7, 5, 5, 4, 7, 5, 5, 4, 5, 4, 4, 3,
          X, X, X, 7, X, 7, 7, 6, X, 7, 7, 6, 7, 6, 6, 3,
          X, 7, 7, 6, 7, 6, 6, 4, 7, 6, 6, 4, 6, 4, 4, 3,
          X, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 3,
          7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 4,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 5,
          X, X, X, X, X, X, X, 5, X, X, X, 5, X, 5, 5, 4,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 6,
          X, X, X, X, X, X, X, 6, X, X, X, 6, X, 6, 6, 4,
          X, X, X, X, X, X, X, 6, X, X, X, 6, X, 6, 6, 5,
          X, X, X, 6, X, 6, 6, 5, X, 6, 6, 5, 6, 5, 5, 4,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 4,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 5,
          X, X, X, 7, X, 7, 7, 5, X, 7, 7, 5, 7, 5, 5, 4,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 6,
          X, X, X, 7, X, 7, 7, 6, X, 7, 7, 6, 7, 6, 6, 4,
          X, X, X, 7, X, 7, 7, 6, X, 7, 7, 6, 7, 6, 6, 5,
          X, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 5,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 6,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 6,
          X, X, X, X, X, X, X, 6, X, X, X, 6, X, 6, 6, 5,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 5,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 6,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 6,
          X, X, X, 7, X, 7, 7, 6, X, 7, 7, 6, 7, 6, 6, 5 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 6,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7,
          X, X, X, X, X, X, X, 7, X, X, X, 7, X, 7, 7, 6 },
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
          X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 7 }
    };
#endif

}

namespace {

    bool sInitialised = false;

    // Suppose that you want to see whether or not a particular
    // CPU instruction works. There are two things you need to
    // ensure:
    //
    //     1. The instruction isn't compiled away.
    //
    //     2. The instruction isn't reordered with respect to
    //        whatever steps you take to detect a faulting
    //        instruction.
    //
    // This is especially important when the instruction isn't
    // necessarily a memory operation, because C++ semantics only
    // guarantees memory consistency.
    //
    // The solution is to make any instruction under test data-
    // dependent on sequentially-ordered memory operations, and
    // also to make a sequentially-ordered memory operation
    // which is data-dependent on the instruction. This is the
    // reason for all the shenanigans with std::atomic.
    //
    // Welcome to the future, where it's harder to prevent a
    // compiler optimisation than it is to ensure one.

    enum testing_what_t {
        TESTING_NOTHING = 0,
        TESTING_POPCNT
    };

    std::atomic<testing_what_t> sWhichTest;

    void
    sigill_handler(int pSigNum)
    {
        switch (sWhichTest.load(std::memory_order_seq_cst))
        {
            case TESTING_POPCNT:
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info(
                        "POPCNT instruction not supported on this platform"));
            }

            default:
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info(
                        "Unexpected illegal instruction exception"));
            }
        }
    }

#if defined(GOSS_WINDOWS_X64)
    typedef void (*sig_t)(int);
#endif

    struct TemporarySignalHandler
    {
        TemporarySignalHandler(int pSigNum, sig_t pSigHandler)
            : mSigNum(pSigNum), mOldSigHandler(0)
        {
            mOldSigHandler = signal(pSigNum, pSigHandler);
        }

        ~TemporarySignalHandler()
        {
            if (mOldSigHandler)
            {
                signal(mSigNum, mOldSigHandler);
            }
        }

        int mSigNum;
        sig_t mOldSigHandler;
    };


    std::atomic<uint64_t> sPopcntTest(0xDEADBEEFCAFEBABEull);
    std::atomic<uint64_t> sPopcntTestResult(0);

    void testInstructions()
    {
        // Check to make sure that popcnt works.
        try
        {
            sWhichTest.store(TESTING_NOTHING, std::memory_order_seq_cst);

            TemporarySignalHandler sigHandler(SIGILL, &sigill_handler);

            sWhichTest.store(TESTING_POPCNT, std::memory_order_seq_cst);

            {
                uint64_t v = sPopcntTest.load(std::memory_order_seq_cst);
                sPopcntTestResult.store(Gossamer::popcnt(v), std::memory_order_seq_cst);
            }

            sWhichTest.store(TESTING_NOTHING, std::memory_order_seq_cst);
        }
        catch (Gossamer::error& err)
        {
            sWhichTest.store(TESTING_NOTHING, std::memory_order_seq_cst);
            throw;
        }
    }
} // namespace


void
MachineAutoSetup::setup()
{
    if (sInitialised)
    {
        return;
    }
    setupMachineSpecific();
    testInstructions();
    sInitialised = true;
}


