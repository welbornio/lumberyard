/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYSYSTEM_JOBMANAGER_BLOCKINGBACKEND_BLOCKINGBACKEND_H
#define CRYINCLUDE_CRYSYSTEM_JOBMANAGER_BLOCKINGBACKEND_BLOCKINGBACKEND_H
#pragma once


#include <IJobManager.h>
#include "../JobStructs.h"

namespace JobManager
{
    class CJobManager;
    class CWorkerBackEndProfiler;
}

namespace JobManager {
    namespace BlockingBackEnd {
        namespace detail {
            // stack size for each worker thread of the blocking backend
            enum
            {
                eStackSize = 64 * 1024
            };
        } // namespace detail

        // forward declarations
        class CBlockingBackEnd;

        // class to represent a worker thread for the PC backend
        class CBlockingBackEndWorkerThread
            : public CrySimpleThread<>
        {
        public:
            CBlockingBackEndWorkerThread(CBlockingBackEnd* pBlockingBackend, CryFastSemaphore& rSemaphore, JobManager::SJobQueue_BlockingBackEnd& rJobQueue, JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads, uint32 nID);
            ~CBlockingBackEndWorkerThread();

            virtual void Run();
            virtual void Stop();

        private:
            void DoWork();
            void DoWorkProducerConsumerQueue(SInfoBlock& rInfoBlock);

            string                                                                  m_sThreadName;          // name of the worker thread
            uint32                                                                  m_nId;                          // id of the worker thread
            volatile bool                                                       m_bStop;
            CryFastSemaphore&                                               m_rSemaphore;
            JobManager::SJobQueue_BlockingBackEnd&  m_rJobQueue;
            CBlockingBackEnd*                                             m_pBlockingBackend;

            // members used for special blocking backend fallback handling
            JobManager::SInfoBlock**        m_pRegularWorkerFallbacks;
            uint32                                          m_nRegularWorkerThreads;
        };

        // the implementation of the PC backend
        // has n-worker threads which use atomic operations to pull from the job queue
        // and uses a semaphore to signal the workers if there is work requiered
        class CBlockingBackEnd
            : public IBackend
        {
        public:

            inline void* operator new(std::size_t)
            {
                size_t allocated = 0;
                return CryMalloc(sizeof(CBlockingBackEnd), allocated, 128);
            }
            inline void operator delete(void* p)
            {
                CryFree(p, 128);
            }

            CBlockingBackEnd(JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads);
            virtual ~CBlockingBackEnd();

            bool Init(uint32 nSysMaxWorker);
            bool ShutDown();
            void Update(){}

            virtual void AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);

            virtual uint32 GetNumWorkerThreads() const { return m_nNumWorker; }

            void AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, uint32 nWorkerThreadID);

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
            JobManager::IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return m_pBackEndWorkerProfiler; }
#endif

        private:
            friend class JobManager::CJobManager;

            JobManager::SJobQueue_BlockingBackEnd               m_JobQueue;                 // job queue node where jobs are pushed into and from
            CryFastSemaphore                                                        m_Semaphore;                // semaphore to count available jobs, to allow the workers to go sleeping instead of spinning when no work is requiered
            CBlockingBackEndWorkerThread**                          m_pWorkerThreads;       // worker threads for blocking backend
            uint8                                                                             m_nNumWorker;             // number of allocated worker threads

            // members used for special blocking backend fallback handling
            JobManager::SInfoBlock**                                    m_pRegularWorkerFallbacks;
            uint32                                                                      m_nRegularWorkerThreads;

            // members required for profiling jobs in the frame profiler
#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
            JobManager::IWorkerBackEndProfiler*                         m_pBackEndWorkerProfiler;
#endif
        };
    } // namespace BlockingBackEnd
} // namespace JobManager

#endif // CRYINCLUDE_CRYSYSTEM_JOBMANAGER_BLOCKINGBACKEND_BLOCKINGBACKEND_H
