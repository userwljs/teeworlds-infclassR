#include "pathfinder.h"

#include <algorithm>

#include <base/system.h>
#include <base/tl/ic_array.h>
#include <engine/shared/config.h>
#include <game/collision.h>
#include <game/infclass/ic_classes.h>
#include <game/server/infclass/entities/ic_entity.h>

std::shared_ptr<CTask> CChannel::Consume()
{
    std::unique_lock Lock(m_Mutex);
    m_Cv.wait(Lock, [this] { return !m_Deque.empty() || m_Shutdown; });
    if(m_Shutdown)
    {
        return nullptr;
    }
    auto Item = std::move(m_Deque.front());
    m_Deque.pop_front();
    return Item;
}

void CChannel::Push(std::shared_ptr<CTask> Item)
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Deque.push_back(std::move(Item));
    }
    m_Cv.notify_one();
}

void CChannel::RemoveFirst(const std::shared_ptr<CTask> &Item)
{
    std::lock_guard Lock(m_Mutex);
    for(auto it = m_Deque.begin(); it != m_Deque.end(); ++it)
    {
        if(*it == Item)
        {
            m_Deque.erase(it);
            return;
        }
    }
}

void CChannel::RemoveAll()
{
    std::lock_guard Lock(m_Mutex);
    m_Deque.clear();
}

void CChannel::Shutdown()
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Shutdown = true;
    }
    m_Cv.notify_all();
}

CPathfinder::CPathfinder(const CCollision *pCollision)
{
    m_pCollision = std::make_shared<const CCollision>(*pCollision);
    const unsigned Hw = std::thread::hardware_concurrency();
    const unsigned Threads = std::clamp(Hw, 1u, MaxWorkerThreads);
    m_WorkerThreads.reserve(Threads);
    for(unsigned i = 0; i < Threads; i++)
    {
        m_WorkerThreads.emplace_back([this] { WorkerThread(); });
    }
    dbg_msg("pathfinding", "Worker threads started: %d", Threads);
}

CPathfinder::~CPathfinder()
{
    m_ReadyQueue.Shutdown();
    // Mark any in-flight task as canceled so workers observe it quickly.
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(!m_Tasks[i])
        {
            continue;
        }
        m_Tasks[i]->m_State.store(CTask::EState::CANCELED, std::memory_order_release);
    }
    for(auto &Thread : m_WorkerThreads)
    {
        Thread.join();
    }
    m_WorkerThreads.clear();
}

void CPathfinder::SubmitTask(int SlotId, const CTuningParams *pTuningParams, const CCharacterCore &CharacterCore, vec2 Goal,
                             int MaxIters,
                             std::function<bool(const CCollision *, const MotionPlanning::CMotionState &)> fnIsStateValid)
{
    std::function<bool(const MotionPlanning::CMotionState &)> Fn;
    if(fnIsStateValid)
    {
        Fn = [pRawColl = m_pCollision.get(), fnIsStateValid](const MotionPlanning::CMotionState &State) -> bool
        {
            return fnIsStateValid(pRawColl, State);
        };
    }
    else
    {
        Fn = nullptr;
    }
    CancelTask(SlotId);
    m_Tasks[SlotId] = std::make_shared<CTask>(CTask::EState::READY, m_pCollision, 0,
                                              CMotionPlanner(CMotionPlanner::Params{
                                                                 g_Config.m_InfPathfindingSimulateStep,
                                                                 2 * pi / g_Config.m_InfPathfindingHookDirections, 32.f,
                                                                 0.3, g_Config.m_InfPathfindingGoalEpsilon * TileSizeF,
                                                                 1.5, 0.15
                                                             }, m_pCollision.get(), pTuningParams,
                                                             CharacterCore, Goal, Fn),
                                              MaxIters);
    m_ReadyQueue.Push(m_Tasks[SlotId]);
}

void CPathfinder::CancelTask(int SlotId)
{
    if(!m_Tasks[SlotId])
    {
        return;
    }
    // Marking first guarantees the worker (if it currently holds the task)
    // observes cancel immediately; RemoveFirst then drains any queued copy.
    m_Tasks[SlotId]->m_State.store(CTask::EState::CANCELED, std::memory_order_release);
    m_ReadyQueue.RemoveFirst(m_Tasks[SlotId]);
}

void CPathfinder::CancelAll()
{
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(m_Tasks[i])
        {
            m_Tasks[i]->m_State.store(CTask::EState::CANCELED, std::memory_order_release);
        }
    }
    m_ReadyQueue.RemoveAll();
}

bool CPathfinder::IsTaskFinished(int SlotId) const
{
    if(!m_Tasks[SlotId])
    {
        return true;
    }
    // acquire synchronizes visibility of m_Planner for a subsequent GetTaskResult.
    const auto State = m_Tasks[SlotId]->m_State.load(std::memory_order_acquire);
    return State == CTask::EState::FINISHED || State == CTask::EState::CANCELED;
}

void CPathfinder::SetCollision(const CCollision *pCollision)
{
    m_pCollision = std::make_shared<const CCollision>(*pCollision);
    CancelAll();
}

std::optional<std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>>> CPathfinder::GetTaskResult(int SlotId) const
{
    if(!m_Tasks[SlotId])
    {
        return std::nullopt;
    }

    // IsTaskFinished (acquire) establishes visibility of m_Planner writes for
    // both FINISHED and CANCELED states; a path found before cancellation is
    // still returned.
    if(IsTaskFinished(SlotId) && m_Tasks[SlotId]->m_Planner.HasPath())
    {
        return m_Tasks[SlotId]->m_Planner.GetPath();
    }
    return std::nullopt;
}

void CPathfinder::WorkerThread()
{
    while(true)
    {
        auto pTask = m_ReadyQueue.Consume();
        if(!pTask)
        {
            return;
        }
        for(int i = 0; i < Quantum; i++)
        {
            if(pTask->m_State.load(std::memory_order_acquire) == CTask::EState::CANCELED)
            {
                break;
            }
            pTask->m_Iterations++;
            pTask->m_Planner.Step();
            const bool ReachedGoal = pTask->m_Planner.HasPath();
            const bool Exhausted = pTask->m_Iterations >= pTask->m_MaxIterations;
            if(ReachedGoal || Exhausted)
            {
                auto Expected = CTask::EState::READY;
                if(pTask->m_State.compare_exchange_strong(
                    Expected, CTask::EState::FINISHED,
                    std::memory_order_release,
                    std::memory_order_relaxed))
                {
                    break;
                }
                // Lost the race to CancelTask: CANCELED wins, do not publish.
                break;
            }
        }
        // Requeue only if still runnable; decision is pure read, no lock held during Push.
        if(pTask->m_State.load(std::memory_order_acquire) == CTask::EState::READY)
        {
            m_ReadyQueue.Push(std::move(pTask));
        }
    }
}

std::function<bool(const CCollision *, const MotionPlanning::CMotionState &)> CPathfinder::GetFnIsStateValid(
    EPlayerClass Class, int ZoneHandleIcDamage, bool VanillaMapLoaded)
{
    // Does not support animated death zones
    const auto fnGetDamageZoneValueAt = [ZoneHandleIcDamage, VanillaMapLoaded](const CCollision *pCollision, vec2 Pos)
    {
        int DamageIndex = pCollision->GetZoneValueAt(ZoneHandleIcDamage, Pos);
        if(!DamageIndex && VanillaMapLoaded)
        {
            if(pCollision->GetCollisionAt(Pos.x, Pos.y) == TILE_DEATH)
            {
                DamageIndex = ZONE_DAMAGE_DEATH;
            }
        }
        return DamageIndex;
    };
    const auto fnGetDamageZoneValuesAt = [fnGetDamageZoneValueAt](const CCollision *pCollision, vec2 Pos) -> icArray<int, 4>
    {
        icArray<int, 4> Result;
        constexpr float ProximityRadius = CCharacterCore::PhysicalSize();
        Result.Add(fnGetDamageZoneValueAt(
            pCollision, vec2(Pos.x + ProximityRadius / 3.f, Pos.y - ProximityRadius / 3.f)));
        Result.Add(fnGetDamageZoneValueAt(
            pCollision, vec2(Pos.x + ProximityRadius / 3.f, Pos.y + ProximityRadius / 3.f)));
        Result.Add(fnGetDamageZoneValueAt(
            pCollision, vec2(Pos.x - ProximityRadius / 3.f, Pos.y - ProximityRadius / 3.f)));
        Result.Add(fnGetDamageZoneValueAt(
            pCollision, vec2(Pos.x - ProximityRadius / 3.f, Pos.y + ProximityRadius / 3.f)));
        return Result;
    };
    return [fnGetDamageZoneValuesAt, Class](const CCollision *pCollision,
                                            const MotionPlanning::CMotionState &State) -> bool
    {
        const auto Indices = fnGetDamageZoneValuesAt(pCollision, State.m_Core.m_Pos);
        if(Indices.Contains(ZONE_DAMAGE_DEATH))
        {
            return false;
        }
        if(Class != EPlayerClass::Undead && Indices.Contains(ZONE_DAMAGE_DEATH_NOUNDEAD))
        {
            return false;
        }
        if(IsInfectedClass(Class) && Indices.Contains(ZONE_DAMAGE_DEATH_INFECTED))
        {
            return false;
        }
        if(IsHumanClass(Class) && Indices.Contains(ZONE_DAMAGE_INFECTION))
        {
            return false;
        }
        return true;
    };
}
