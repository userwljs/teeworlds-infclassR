#ifndef INFCLASS_PATH_FINDER_H
#define INFCLASS_PATH_FINDER_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>
#include <base/vmath.h>

#include <game/server/entities/character.h>

#include "motion_planner.h"

struct CNetObj_PlayerInput;

using CMotionPlanner = MotionPlanning::CMotionPlanner;

struct CTask;

class CChannel
{
public:
    CChannel(const CChannel &) = delete;
    CChannel &operator=(const CChannel &) = delete;
    CChannel() = default;

    std::shared_ptr<CTask> Consume();
    void Push(std::shared_ptr<CTask> Item);
    void RemoveFirst(const std::shared_ptr<CTask> &Item);
    void RemoveAll();
    void Shutdown();

private:
    std::mutex m_Mutex;
    std::deque<std::shared_ptr<CTask>> m_Deque;
    std::condition_variable m_Cv;
    bool m_Shutdown = false;
};

struct CTask
{
    enum class EState
    {
        READY,
        FINISHED,
        CANCELED,
    };

    // Synchronizes visibility of m_Planner writes to the main thread.
    // Worker writes m_Planner, then stores FINISHED with release ordering.
    // Main thread loads with acquire; once it observes a terminal state
    // (FINISHED/CANCELED), all prior m_Planner writes are visible.
    std::atomic<EState> m_State{EState::READY};

    const std::shared_ptr<const CCollision> m_pCollision;

    int m_Iterations = 0; // only accessed by the executor thread
    CMotionPlanner m_Planner; // main thread should only access this when m_State == FINISHED
    const int m_MaxIterations;
};

class CPathfinder // should live shorter than CGameContext
{
public:
    explicit CPathfinder(const CCollision *pCollision);
    ~CPathfinder();

    void SubmitTask(int SlotId, const CTuningParams *pTuningParams, const CCharacter *pCharacter, vec2 Goal);
    void CancelTask(int SlotId);
    void CancelAll();
    bool IsTaskFinished(int SlotId) const;
    void SetCollision(const CCollision *pCollision);
    std::optional<std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>>> GetTaskResult(int SlotId) const;

private:
    static constexpr int Quantum = 10;
    static constexpr unsigned MaxWorkerThreads = 32;
    std::vector<std::thread> m_WorkerThreads;
    CChannel m_ReadyQueue;
    std::array<std::shared_ptr<CTask>, MAX_CLIENTS> m_Tasks;
    std::shared_ptr<const CCollision> m_pCollision;
    void WorkerThread();
};

#endif //INFCLASS_PATH_FINDER_H
