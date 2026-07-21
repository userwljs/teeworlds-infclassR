#include "motion_planner.h"

#include <game/collision.h>
#include <game/server/entities/character.h>
#include <game/server/infclass/entities/ic_entity.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace MotionPlanning
{
    CAabb::CAabb(vec2 Min, vec2 Max) :
        m_Min(Min), m_Max(Max)
    {
    }

    CAabb CAabb::FromSegment(vec2 Start, vec2 End)
    {
        return CAabb(
            vec2(std::min(Start.x, End.x), std::min(Start.y, End.y)),
            vec2(std::max(Start.x, End.x), std::max(Start.y, End.y)));
    }

    CAabb CAabb::FromSegments(const std::vector<SSegment> &Segments, const std::vector<int> &Indices)
    {
        vec2 Min = Segments[Indices[0]].m_pParent->m_State.m_Core.m_Pos;
        vec2 Max = Segments[Indices[0]].m_State.m_Core.m_Pos;
        for(size_t i = 1; i < Indices.size(); i++)
        {
            const vec2 &From = Segments[Indices[i]].m_pParent->m_State.m_Core.m_Pos;
            const vec2 &To = Segments[Indices[i]].m_State.m_Core.m_Pos;
            Min.x = std::min({Min.x, From.x, To.x});
            Min.y = std::min({Min.y, From.y, To.y});
            Max.x = std::max({Max.x, From.x, To.x});
            Max.y = std::max({Max.y, From.y, To.y});
        }
        return CAabb(Min, Max);
    }

    CAabb CAabb::FromSegmentsRange(const std::vector<SSegment> &Segments, const std::vector<int> &Sorted, int Begin,
                                   int End)
    {
        vec2 Min = Segments[Sorted[Begin]].m_pParent->m_State.m_Core.m_Pos;
        vec2 Max = Segments[Sorted[Begin]].m_State.m_Core.m_Pos;
        for(int i = Begin + 1; i < End; i++)
        {
            const vec2 &From = Segments[Sorted[i]].m_pParent->m_State.m_Core.m_Pos;
            const vec2 &To = Segments[Sorted[i]].m_State.m_Core.m_Pos;
            Min.x = std::min({Min.x, From.x, To.x});
            Min.y = std::min({Min.y, From.y, To.y});
            Max.x = std::max({Max.x, From.x, To.x});
            Max.y = std::max({Max.y, From.y, To.y});
        }
        return CAabb(Min, Max);
    }

    void CAabb::Expand(const CAabb &Other)
    {
        m_Min.x = std::min(m_Min.x, Other.m_Min.x);
        m_Min.y = std::min(m_Min.y, Other.m_Min.y);
        m_Max.x = std::max(m_Max.x, Other.m_Max.x);
        m_Max.y = std::max(m_Max.y, Other.m_Max.y);
    }

    float CAabb::DistanceToPointSq(vec2 Point) const
    {
        float dx = std::max(0.0f, std::max(m_Min.x - Point.x, Point.x - m_Max.x));
        float dy = std::max(0.0f, std::max(m_Min.y - Point.y, Point.y - m_Max.y));
        return dx * dx + dy * dy;
    }

    float CAabb::SurfaceArea() const
    {
        float dx = m_Max.x - m_Min.x;
        float dy = m_Max.y - m_Min.y;
        if(dx <= 0.0f || dy <= 0.0f)
            return 0.0f;
        return dx * dy;
    }

    class CSegmentTree::CSpatialHash
    {
    public:
        explicit CSpatialHash(float CellSize) :
            m_CellSize(std::max(CellSize, 1.0f))
        {
        }

        void Insert(int Index, vec2 Pos)
        {
            m_Cells[CellKey(Pos)].emplace_back(Index, Pos);
        }

        void Remove(int Index, vec2 Pos)
        {
            int64_t Key = CellKey(Pos);
            auto It = m_Cells.find(Key);
            if(It == m_Cells.end())
                return;
            auto &List = It->second;
            std::erase_if(List,
                          [Index](const std::pair<int, vec2> &P) { return P.first == Index; });
        }

        int Query(vec2 Pos, float Radius) const
        {
            auto [Cx, Cy] = CellCoord(Pos);
            int BestIndex = -1;
            float BestDist = Radius;
            for(int dy = -1; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx++)
                {
                    int64_t Key = PackKey(Cx + dx, Cy + dy);
                    auto It = m_Cells.find(Key);
                    if(It == m_Cells.end())
                        continue;
                    for(const auto &[Idx, Ep] : It->second)
                    {
                        float d = distance(Pos, Ep);
                        if(d < BestDist)
                        {
                            BestDist = d;
                            BestIndex = Idx;
                        }
                    }
                }
            }
            return BestIndex;
        }

    private:
        static int64_t PackKey(int32_t Cx, int32_t Cy)
        {
            return (static_cast<int64_t>(Cx) << 32) | static_cast<int64_t>(static_cast<uint32_t>(Cy));
        }

        std::pair<int32_t, int32_t> CellCoord(vec2 Pos) const
        {
            return {
                static_cast<int32_t>(std::floor(Pos.x / m_CellSize)),
                static_cast<int32_t>(std::floor(Pos.y / m_CellSize))
            };
        }

        int64_t CellKey(vec2 Pos) const
        {
            auto [Cx, Cy] = CellCoord(Pos);
            return PackKey(Cx, Cy);
        }

        float m_CellSize;
        std::unordered_map<int64_t, std::vector<std::pair<int, vec2>>> m_Cells;
    };

    CSegmentTree::CSegmentTree()
    {
        m_Segments.reserve(4096);
        m_Active.reserve(1024);
    }

    CSegmentTree::~CSegmentTree() = default;

    CSegmentTree::CSegmentTree(CSegmentTree &&) noexcept = default;
    CSegmentTree &CSegmentTree::operator=(CSegmentTree &&) noexcept = default;

    int CSegmentTree::Add(const SSegment &Seg)
    {
        int Idx;
        if(!m_FreeSlots.empty())
        {
            Idx = m_FreeSlots.back();
            m_FreeSlots.pop_back();
            m_Segments[Idx] = Seg;
        }
        else
        {
            Idx = static_cast<int>(m_Segments.size());
            m_Segments.push_back(Seg);
        }
        m_Active.insert(Idx);
        if(m_pSpatial)
            m_pSpatial->Insert(Idx, Seg.m_State.m_Core.m_Pos);
        m_BVHDirtyCount++;
        return Idx;
    }

    void CSegmentTree::Remove(int Index)
    {
        m_Active.erase(Index);
        if(m_pSpatial && Index < static_cast<int>(m_Segments.size()))
            m_pSpatial->Remove(Index, m_Segments[Index].m_State.m_Core.m_Pos);
        m_FreeSlots.push_back(Index);
        m_BVHDirtyCount++;
    }

    const SSegment &CSegmentTree::GetSegment(int Index) const
    {
        return m_Segments[Index];
    }

    void CSegmentTree::InitSpatial(float CellSize)
    {
        m_pSpatial = std::make_unique<CSpatialHash>(CellSize);
        for(const int Idx : m_Active)
            m_pSpatial->Insert(Idx, m_Segments[Idx].m_State.m_Core.m_Pos);
    }

    int CSegmentTree::DedupQuery(vec2 Pos, float Radius) const
    {
        if(!m_pSpatial)
            return -1;
        return m_pSpatial->Query(Pos, Radius);
    }

    void CSegmentTree::MarkBVHDirty()
    {
        m_BVHDirtyCount++;
    }

    void CSegmentTree::RebuildBVH()
    {
        m_BVHNodes.clear();
        m_BVHRoot = -1;
        m_BVHNodeCount = 0;
        m_BVHMaxDepth = 0;

        std::vector<int> ActiveIndices(m_Active.begin(), m_Active.end());
        if(ActiveIndices.empty())
            return;

        BuildBVH(m_BVHNodes, m_Segments, ActiveIndices, m_BVHRoot, m_BVHNodeCount, m_BVHMaxDepth);
        m_BVHDirtyCount = 0;
    }

    void CSegmentTree::EnsureBVH()
    {
        float Ratio = m_Active.empty()
                          ? 0.0f
                          : static_cast<float>(m_BVHDirtyCount) / static_cast<float>(m_Active.size());
        if(Ratio <= DirtyRatioThreshold && m_BVHRoot >= 0)
            return;

        RebuildBVH();
    }

    int CSegmentTree::QueryNearest(vec2 Point)
    {
        SQueryResult Result;
        if(!QueryNearestWithScore(Point, Result))
            return -1;
        return Result.m_Index;
    }

    int CSegmentTree::QueryNearestWithScore(vec2 Point, SQueryResult &OutResult)
    {
        EnsureBVH();
        if(m_BVHRoot < 0)
            return 0;

        OutResult = {-1, std::numeric_limits<float>::infinity()};
        QueryBVH(m_BVHNodes, m_Segments, m_BVHRoot, Point, OutResult.m_Index, OutResult.m_Score);
        return 1;
    }

    float CSegmentTree::PointToSegmentDistance(vec2 Point, vec2 A, vec2 B)
    {
        vec2 AB = B - A;
        float Len2 = AB.x * AB.x + AB.y * AB.y;
        if(Len2 < 1e-12f)
            return distance(Point, A);
        float t = ((Point.x - A.x) * AB.x + (Point.y - A.y) * AB.y) / Len2;
        t = std::clamp(t, 0.0f, 1.0f);
        vec2 Closest(A.x + t * AB.x, A.y + t * AB.y);
        return distance(Point, Closest);
    }

    bool CSegmentTree::IsBehindSegment(vec2 Point, vec2 Start, vec2 Direction)
    {
        return (Point.x - Start.x) * Direction.x + (Point.y - Start.y) * Direction.y <= 0.0f;
    }

    float CSegmentTree::SegmentScore(const SSegment &Seg, vec2 Point)
    {
        if(IsBehindSegment(Point, Seg.m_pParent->m_State.m_Core.m_Pos, Seg.m_Direction))
            return std::numeric_limits<float>::infinity();
        return distance(Point, Seg.m_State.m_Core.m_Pos) +
            PointToSegmentDistance(Point, Seg.m_pParent->m_State.m_Core.m_Pos, Seg.m_State.m_Core.m_Pos);
    }

    void CSegmentTree::BuildBVH(std::vector<SBvhNode> &Nodes, const std::vector<SSegment> &Segments,
                                const std::vector<int> &Indices,
                                int &OutRoot, int &OutNodeCount, int &OutMaxDepth)
    {
        struct SBuildJob
        {
            std::vector<int> Indices;
            int ParentIdx;
            bool IsLeftChild;
            int Depth;
        };

        std::vector<SBuildJob> Stack;
        Stack.push_back({Indices, -1, false, 0});
        Nodes.clear();
        OutNodeCount = 0;
        OutMaxDepth = 0;
        OutRoot = -1;

        while(!Stack.empty())
        {
            SBuildJob Job = std::move(Stack.back());
            Stack.pop_back();

            OutMaxDepth = std::max(OutMaxDepth, Job.Depth);

            if(static_cast<int>(Job.Indices.size()) <= MaxLeaf)
            {
                CAabb Aabb = CAabb::FromSegments(Segments, Job.Indices);
                int Idx = static_cast<int>(Nodes.size());
                Nodes.push_back({SBvhNode::Leaf, Aabb, Job.Indices, -1, -1});

                if(Job.ParentIdx >= 0)
                {
                    if(Job.IsLeftChild)
                        Nodes[Job.ParentIdx].m_Left = Idx;
                    else
                        Nodes[Job.ParentIdx].m_Right = Idx;
                }
                else
                {
                    OutRoot = Idx;
                }
                OutNodeCount++;
            }
            else
            {
                CAabb Aabb = CAabb::FromSegments(Segments, Job.Indices);
                bool SplitAlongX = (Aabb.m_Max.x - Aabb.m_Min.x) >= (Aabb.m_Max.y - Aabb.m_Min.y);

                std::vector<int> Sorted = Job.Indices;
                auto CenterOf = [SplitAlongX](const CMotionState &State) -> float
                {
                    return SplitAlongX
                               ? (State.m_Core.m_Pos.x + State.m_Core.m_Pos.x) * 0.5f
                               : (State.m_Core.m_Pos.y + State.m_Core.m_Pos.y) * 0.5f;
                };
                std::ranges::sort(Sorted, [&Segments, &CenterOf](int a, int b)
                {
                    float ma = CenterOf(Segments[a].m_pParent->m_State) + CenterOf(Segments[a].m_State);
                    float mb = CenterOf(Segments[b].m_pParent->m_State) + CenterOf(Segments[b].m_State);
                    return ma < mb;
                });

                int n = static_cast<int>(Sorted.size());
                int BucketSize = std::max(1, (n + SAHBuckets - 1) / SAHBuckets);

                float InvTotalSA = 1.0f / std::max(Aabb.SurfaceArea(), 1e-12f);
                float BestCost = std::numeric_limits<float>::infinity();
                int BestSplit = n / 2;

                for(int b = 1; b < SAHBuckets; b++)
                {
                    int Split = std::min(b * BucketSize, n - 1);
                    if(Split <= 0 || Split >= n)
                        continue;

                    CAabb LeftAabb = CAabb::FromSegmentsRange(Segments, Sorted, 0, Split);
                    CAabb RightAabb = CAabb::FromSegmentsRange(Segments, Sorted, Split, n);

                    float Cost = LeftAabb.SurfaceArea() * InvTotalSA * static_cast<float>(Split) +
                        RightAabb.SurfaceArea() * InvTotalSA * static_cast<float>(n - Split);

                    if(Cost < BestCost)
                    {
                        BestCost = Cost;
                        BestSplit = Split;
                    }
                }

                int Idx = static_cast<int>(Nodes.size());
                Nodes.push_back({SBvhNode::Internal, Aabb, {}, -1, -1});

                if(Job.ParentIdx >= 0)
                {
                    if(Job.IsLeftChild)
                        Nodes[Job.ParentIdx].m_Left = Idx;
                    else
                        Nodes[Job.ParentIdx].m_Right = Idx;
                }
                else
                {
                    OutRoot = Idx;
                }
                OutNodeCount++;

                std::vector<int> RightIndices(Sorted.begin() + BestSplit, Sorted.end());
                std::vector<int> LeftIndices(Sorted.begin(), Sorted.begin() + BestSplit);
                Stack.push_back({std::move(RightIndices), Idx, false, Job.Depth + 1});
                Stack.push_back({std::move(LeftIndices), Idx, true, Job.Depth + 1});
            }
        }
    }

    void CSegmentTree::QueryBVH(const std::vector<SBvhNode> &Nodes, const std::vector<SSegment> &Segments,
                                int Root, vec2 Point, int &BestIndex, float &BestScore)
    {
        std::vector<int> Stack;
        Stack.push_back(Root);

        while(!Stack.empty())
        {
            int NodeIdx = Stack.back();
            Stack.pop_back();

            const SBvhNode &Node = Nodes[NodeIdx];
            float BestScoreSq = BestScore * BestScore;
            if(Node.m_Aabb.DistanceToPointSq(Point) >= BestScoreSq)
                continue;

            if(Node.m_Type == SBvhNode::Leaf)
            {
                for(int SegIdx : Node.m_SegIndices)
                {
                    float Score = SegmentScore(Segments[SegIdx], Point);
                    if(Score < BestScore)
                    {
                        BestScore = Score;
                        BestIndex = SegIdx;
                    }
                }
            }
            else
            {
                int Left = Node.m_Left;
                int Right = Node.m_Right;
                float dl = Nodes[Left].m_Aabb.DistanceToPointSq(Point);
                float dr = Nodes[Right].m_Aabb.DistanceToPointSq(Point);

                if(dl <= dr)
                {
                    Stack.push_back(Right);
                    Stack.push_back(Left);
                }
                else
                {
                    Stack.push_back(Left);
                    Stack.push_back(Right);
                }
            }
        }
    }

    CMotionPlanner::CMotionPlanner(const Params &Params, const CCollision *pCollision,
                                   const CTuningParams *TuningParams,
                                   const CCharacterCore &Core, vec2 Goal,
                                   const std::function<bool(const CMotionState &)> &fnIsStateValid) :
        m_pCollision(pCollision),
        m_Params(Params),
        m_TuningParams(*TuningParams),
        m_ExploredMin(Core.m_Pos - vec2(10 * TileSize, 10 * TileSize)),
        m_ExploredMax(Core.m_Pos + vec2(10 * TileSize, 10 * TileSize)),
        m_Goal(Goal),
        m_fnIsStateValid(fnIsStateValid)
    {
        m_SegmentTree.InitSpatial(Params.DedupRadius);
        const auto InitNode = std::make_shared<SNode>(nullptr, CMotionState(Core, pCollision), 0, 0, SInput{});
        ExpandNode(InitNode);
    }

    bool CMotionPlanner::ExpandNode(std::shared_ptr<SNode> pNode)
    {
        const auto Inputs = GenerateInputs(pNode->m_State);
        std::vector<SSegment> vCandidates;
        for(const auto &Input : Inputs)
        {
            auto ModInput = Input;
            auto State = pNode->m_State;
            bool Accept = true;
            for(int i = 0; i < m_Params.SimulateStep; i++)
            {
                State.Tick(ModInput, TuningParams());
                if(distance(m_Goal, State.m_Core.m_Pos) < m_Params.GoalEpsilon)
                {
                    std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>> Path;
                    auto pCur = pNode;
                    while(pCur->m_pParent)
                    {
                        Path.emplace_back(pCur->m_Tick, pCur->m_State.m_Core.m_Pos, pCur->m_Input);
                        pCur = pCur->m_pParent;
                    }
                    std::ranges::reverse(Path);
                    Path.emplace_back(pNode->m_Tick + i + 1, State.m_Core.m_Pos, Input);
                    m_Path = std::move(Path);
                    return true;
                }
                if(ModInput.m_Jump == 1)
                {
                    ModInput.m_Jump = false;
                }
                if(m_fnIsStateValid && !m_fnIsStateValid(State))
                {
                    Accept = false;
                    break;
                }
            }
            if(Accept)
            {
                vCandidates.emplace_back(
                    normalize(State.m_Core.m_Pos - pNode->m_State.m_Core.m_Pos),
                    distance(State.m_Core.m_Pos, pNode->m_State.m_Core.m_Pos), pNode,
                    std::move(State), Input
                );
            }
        }

        // Prune
        std::ranges::sort(vCandidates, [](const SSegment &Val1, const SSegment &Val2) -> bool
        {
            return Val1.m_Displacement > Val2.m_Displacement;
        }); // desc order
        vCandidates.erase(vCandidates.end() - vCandidates.size() * m_Params.PruneRatio, vCandidates.end());

        for(const auto &Segment : vCandidates)
        {
            int DupIndex = m_SegmentTree.DedupQuery(Segment.m_State.m_Core.m_Pos, m_Params.DedupRadius);
            if(DupIndex >= 0)
            {
                if(m_SegmentTree.GetSegment(DupIndex).m_pParent->m_Tick > pNode->m_Tick)
                {
                    m_SegmentTree.Remove(DupIndex);
                    m_SegmentTree.Add(Segment);
                    ExpandExploredBox(Segment.m_State.m_Core.m_Pos);
                }
            }
            else
            {
                m_SegmentTree.Add(Segment);
                ExpandExploredBox(Segment.m_State.m_Core.m_Pos);
            }
        }
        return false;
    }

    void CMotionPlanner::ExpandExploredBox(const vec2 Pos)
    {
        m_ExploredMin.x = minimum(m_ExploredMin.x, Pos.x);
        m_ExploredMin.y = minimum(m_ExploredMin.y, Pos.y);
        m_ExploredMax.x = maximum(m_ExploredMax.x, Pos.x);
        m_ExploredMax.y = maximum(m_ExploredMax.y, Pos.y);
    }

    bool CMotionPlanner::Step()
    {
        const vec2 Sample = DoSample();
        const int NearestIndex = m_SegmentTree.QueryNearest(Sample);
        if(NearestIndex < 0)
            return false;

        // Take a full copy of the segment, then remove it from the tree
        // to avoid dangling references and argument-evaluation-order UB.
        SSegment Seg = m_SegmentTree.GetSegment(NearestIndex);
        m_SegmentTree.Remove(NearestIndex);

        auto pParent = Seg.m_pParent;
        int NewTick = pParent->m_Tick + m_Params.SimulateStep;
        int NewDepth = pParent->m_Depth + 1;
        const auto pNode = std::make_shared<SNode>(
            pParent,
            std::move(Seg.m_State),
            NewTick,
            NewDepth,
            std::move(Seg.m_Input)
        );
        if(distance(m_Goal, pNode->m_State.m_Core.m_Pos) < m_Params.GoalEpsilon)
        {
            std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>> Path;
            auto pCur = pNode;
            while(pCur->m_pParent)
            {
                Path.emplace_back(pCur->m_Tick, pCur->m_State.m_Core.m_Pos, pCur->m_Input);
                pCur = pCur->m_pParent;
            }
            std::ranges::reverse(Path);
            m_Path = std::move(Path);
            return true;
        }
        return ExpandNode(pNode);
    }

    void CMotionPlanner::SetGoal(vec2 Goal)
    {
        m_Path = std::nullopt;
        m_Goal = Goal;
    }

    /**
     * Get the path. The array index doesn't correspond to the tick. The start point is not included.
     * @return A vector of (tick, position, input leading to this waypoint) tuples.
     */
    const std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>> &CMotionPlanner::GetPath() const
    {
        return m_Path.value();
    }

    std::vector<SInput> CMotionPlanner::GenerateInputs(const CMotionState &MotionState) const
    {
        // This function is executed before tick, so MotionState.m_Core.m_Input is the previous input
        struct HookInput
        {
            int m_Hook;
            vec2 m_Direction;
        };
        std::vector<HookInput> HookInputs;
        HookInputs.emplace_back(MotionState.m_Core.m_Input.m_Hook, vec2(0, -1)); // keep the previous hook input
        if(MotionState.m_Core.m_HookState == HOOK_GRABBED || MotionState.m_Core.m_HookState == HOOK_RETRACTED)
        {
            // MotionState.m_Core.m_Input.m_Hook is always 1 in this branch
            HookInputs.emplace_back(0, vec2(0, -1)); // release hook
        }
        if(MotionState.m_Core.m_HookState == HOOK_IDLE)
        {
            // MotionState.m_Core.m_Input.m_Hook is always 0 in this branch
            HookInputs.reserve(HookInputs.size() + static_cast<size_t>(2 * pi / m_Params.HookDirectionStep));
            for(float Radian = 0; Radian < 2 * pi; Radian += m_Params.HookDirectionStep)
            {
                // not precise (hook flying needs time) but maybe ok
                const vec2 End = vec2(cos(Radian), sin(Radian)) * TuningParams()->m_HookLength + MotionState.m_Core.
                    m_Pos;
                vec2 CollPos;
                const int CollType = Collision()->IntersectLineHook(MotionState.m_Core.m_Pos, End, &CollPos, nullptr);
                if(CollType == TILE_SOLID)
                {
                    HookInputs.emplace_back(1, normalize(CollPos - MotionState.m_Core.m_Pos));
                }
            }
        }

        int CanJump = 0;
        if(!(MotionState.m_Core.m_Jumped & 1))
        {
            if(MotionState.m_Core.IsGrounded() && (!(MotionState.m_Core.m_Jumped & 2) || MotionState.m_Core.m_Jumps !=
                0))
            {
                CanJump = 1;
            }
            else if(!(MotionState.m_Core.m_Jumped & 2))
            {
                CanJump = 1;
            }
        }

        std::vector<SInput> Result;
        Result.reserve(3 * (CanJump + 1) * HookInputs.size());
        for(int Direction = -1; Direction <= 1; Direction++)
        {
            for(int Jump = 0; Jump <= CanJump; Jump++)
            {
                for(auto &[Hook, AimDirection] : HookInputs)
                {
                    Result.emplace_back(Direction, AimDirection.x * 5 * TileSize, AimDirection.y * 5 * TileSize, Jump,
                                        Hook);
                }
            }
        }
        return Result;
    }

    vec2 CMotionPlanner::DoSample() const
    {
        if(random_prob(m_Params.GoalBias))
        {
            return m_Goal;
        }
        const vec2 Center = (m_ExploredMin + m_ExploredMax) * 0.5f;
        const vec2 HalfExtents = (m_ExploredMax - m_ExploredMin) * 0.5f * m_Params.SampleExpandFactor;

        const float MinX = Center.x - HalfExtents.x;
        const float MaxX = Center.x + HalfExtents.x;
        const float MinY = Center.y - HalfExtents.y;
        const float MaxY = Center.y + HalfExtents.y;

        while(true)
        {
            const vec2 Pos(random_float(MinX, MaxX), random_float(MinY, MaxY));
            if(!Collision()->IsSolid(Pos.x, Pos.y))
            {
                return Pos;
            }
        }
    }

    CMotionState::CMotionState(const CCharacterCore &Core, const CCollision *pCollision)
    {
        m_Core = Core;
        m_Core.m_pWorld = nullptr;
        m_Core.m_pCollision = pCollision;
        m_pCollision = pCollision;
    }

    SInput::operator CNetObj_PlayerInput() const
    {
        return CNetObj_PlayerInput{
            m_Direction, m_TargetX, m_TargetY, m_Jump, 0, m_Hook, 0, 0, 0, 0
        };
    }

    void CMotionState::Tick(const CNetObj_PlayerInput &PlayerInput, const CTuningParams *TuningParams)
    {
        const CCharacterCore::CParams CoreTickParams(TuningParams);
        m_Core.m_Input = PlayerInput;
        m_Core.Tick(true, &CoreTickParams);

        CCharacter::HandleCoreJump(&m_Core);

        if(const int CurrentIndex = Collision()->GetMapIndex(m_Core.m_Pos); CurrentIndex >= 0)
        {
            CCharacter::HandleSkippableTiles(CurrentIndex, Collision(), &m_Core,
                                             Collision()->GetMoveRestrictions(m_Core.m_Pos));

            const CTeleTile *pTeleLayer = Collision()->TeleLayer();
            const int TeleNumber = pTeleLayer[CurrentIndex].m_Number;
            const int TeleType = pTeleLayer[CurrentIndex].m_Type;
            if((TeleNumber > 0) && (TeleType != TILE_TELEOUT) && (TeleType != TILE_TELECHECKOUT))
            {
                CCharacter::TeleportToTeleIdImpl(TeleNumber, TeleType, Collision(), nullptr, &m_Core, nullptr, false);
            }
        }

        m_Core.Move(&CoreTickParams);
        m_Core.Quantize();
    }
} // namespace MotionPlanning
