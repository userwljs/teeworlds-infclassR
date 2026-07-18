#ifndef INFCLASS_MOTION_PLANNER_H
#define INFCLASS_MOTION_PLANNER_H

#include <game/gamecore.h>

#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace MotionPlanning
{
    class CAabb
    {
    public:
        CAabb() = default;
        CAabb(vec2 Min, vec2 Max);

        static CAabb FromSegment(vec2 Start, vec2 End);
        static CAabb FromSegments(const std::vector<struct SSegment> &Segments, const std::vector<int> &Indices);
        static CAabb FromSegmentsRange(const std::vector<struct SSegment> &Segments, const std::vector<int> &Sorted,
                                       int Begin, int End);

        void Expand(const CAabb &Other);
        float DistanceToPointSq(vec2 Point) const;
        float SurfaceArea() const;

        vec2 m_Min;
        vec2 m_Max;
    };

    class CMotionState
    {
    public:
        explicit CMotionState(const CCharacterCore &Core, const CCollision *pCollision);
        void Tick(const CNetObj_PlayerInput &PlayerInput, const CTuningParams *TuningParams);

        CCharacterCore m_Core;

    private:
        const CCollision *m_pCollision;
        const CCollision *Collision() const { return m_pCollision; }
    };

    struct SInput
    {
        int8_t m_Direction;
        int m_TargetX;
        int m_TargetY;
        bool m_Jump;
        bool m_Hook;

        operator CNetObj_PlayerInput() const;
    };

    struct SNode
    {
        std::shared_ptr<SNode> m_pParent;
        CMotionState m_State;
        int m_Tick;
        int m_Depth;
        SInput m_Input; // transition to this state, must be dealt specially when m_Tick == 0
    };

    struct SSegment
    {
        vec2 m_Direction;
        float m_Displacement;
        std::shared_ptr<SNode> m_pParent;
        CMotionState m_State;
        SInput m_Input;
    };

    class CSegmentTree
    {
    public:
        CSegmentTree();
        ~CSegmentTree();

        CSegmentTree(const CSegmentTree &) = delete;
        CSegmentTree &operator=(const CSegmentTree &) = delete;
        CSegmentTree(CSegmentTree &&) noexcept;
        CSegmentTree &operator=(CSegmentTree &&) noexcept;

        void InitSpatial(float CellSize);

        int Add(const SSegment &Seg);
        void Remove(int Index);
        const SSegment &GetSegment(int Index) const;

        int DedupQuery(vec2 Pos, float Radius) const;
        int QueryNearest(vec2 Point);

    private:
        static constexpr float DirtyRatioThreshold = 0.25f;
        static constexpr int MaxLeaf = 8;
        static constexpr int SAHBuckets = 8;

        struct SBvhNode
        {
            enum EType : uint8_t { Leaf, Internal } m_Type;

            CAabb m_Aabb;
            std::vector<int> m_SegIndices;
            int m_Left = -1;
            int m_Right = -1;
        };

        struct SQueryResult
        {
            int m_Index;
            float m_Score;
        };

        class CSpatialHash;

        void EnsureBVH();
        void RebuildBVH();
        void MarkBVHDirty();

        int QueryNearestWithScore(vec2 Point, SQueryResult &OutResult);

        static void BuildBVH(std::vector<SBvhNode> &Nodes, const std::vector<SSegment> &Segments,
                             const std::vector<int> &Indices,
                             int &OutRoot, int &OutNodeCount, int &OutMaxDepth);
        static void QueryBVH(const std::vector<SBvhNode> &Nodes, const std::vector<SSegment> &Segments,
                             int Root, vec2 Point, int &BestIndex, float &BestScore);
        static float PointToSegmentDistance(vec2 Point, vec2 A, vec2 B);
        static bool IsBehindSegment(vec2 Point, vec2 Start, vec2 Direction);
        static float SegmentScore(const SSegment &Seg, vec2 Point);

        std::vector<SSegment> m_Segments;
        std::vector<int> m_FreeSlots;
        std::unordered_set<int> m_Active;
        std::unique_ptr<CSpatialHash> m_pSpatial;

        std::vector<SBvhNode> m_BVHNodes;
        int m_BVHRoot = -1;
        int m_BVHDirtyCount = 0;
        int m_BVHNodeCount = 0;
        int m_BVHMaxDepth = 0;
    };

    class CMotionPlanner
    {
    public:
        struct Params
        {
            int SimulateStep;
            float HookDirectionStep; // radian
            float DedupRadius;
            float PruneRatio;
            float GoalEpsilon;
            float SampleExpandFactor;
            float GoalBias;
        };

        explicit CMotionPlanner(const Params &Params, const CCollision *pCollision,
                                const CTuningParams *TuningParams,
                                const CCharacterCore &Core, vec2 Goal,
                                const std::function<bool(const CMotionState &)> &fnIsStateValid);
        CMotionPlanner(const CMotionPlanner &) = delete;
        CMotionPlanner &operator=(const CMotionPlanner &) = delete;
        CMotionPlanner(CMotionPlanner &&) noexcept = default;
        CMotionPlanner &operator=(CMotionPlanner &&) noexcept = default;

        const CTuningParams *TuningParams() const { return &m_TuningParams; }
        const CCollision *Collision() const { return m_pCollision; }
        bool Step();
        void SetGoal(vec2 Goal);

        bool HasPath() const { return m_Path.has_value(); }
        const std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>> &GetPath() const;

    private:
        const CCollision *m_pCollision;
        Params m_Params;
        CTuningParams m_TuningParams;
        vec2 m_ExploredMin;
        vec2 m_ExploredMax;
        CSegmentTree m_SegmentTree;
        vec2 m_Goal;
        std::function<bool(const CMotionState &)> m_fnIsStateValid;

        std::optional<std::vector<std::tuple<int, vec2, CNetObj_PlayerInput>>> m_Path;

        std::vector<SInput> GenerateInputs(const CMotionState &MotionState) const;
        vec2 DoSample() const;
        bool ExpandNode(std::shared_ptr<SNode> pNode);
        void ExpandExploredBox(const vec2 Pos);
    };
} // namespace MotionPlanning

#endif // INFCLASS_MOTION_PLANNER_H
