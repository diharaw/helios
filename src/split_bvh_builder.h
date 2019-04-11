/*
*  Copyright (c) 2009-2011, NVIDIA Corporation
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*      * Redistributions of source code must retain the above copyright
*        notice, this list of conditions and the following disclaimer.
*      * Redistributions in binary form must reproduce the above copyright
*        notice, this list of conditions and the following disclaimer in the
*        documentation and/or other materials provided with the distribution.
*      * Neither the name of NVIDIA Corporation nor the
*        names of its contributors may be used to endorse or promote products
*        derived from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
*  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
*  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
*  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "bvh.h"
#include "timer.h"

namespace lumen
{
class SplitBVHBuilder
{
private:
    enum
    {
        MaxDepth        = 64,
        MaxSpatialDepth = 48,
        NumSpatialBins  = 32,
    };

    struct Reference /// a AABB bounding box enclosing 1 triangle, a reference can be duplicated by a split to be contained in 2 AABB boxes
    {
        int32_t triIdx;
        AABB    bounds;

        Reference(void) :
            triIdx(-1) {} /// constructor
    };

    struct NodeSpec
    {
        int32_t numRef; // number of references contained by node
        AABB    bounds;

        NodeSpec(void) :
            numRef(0) {}
    };

    struct ObjectSplit
    {
        float   sah;     // cost
        int32_t sortDim; // axis along which triangles are sorted
        int32_t numLeft; // number of triangles (references) in left child
        AABB    leftBounds;
        AABB    rightBounds;

        ObjectSplit(void) :
            sah(FLT_MAX), sortDim(0), numLeft(0) {}
    };

    struct SpatialSplit
    {
        float   sah;
        int32_t dim; /// split axis
        float   pos; /// position of split along axis (dim)

        SpatialSplit(void) :
            sah(FLT_MAX), dim(0), pos(0.0f) {}
    };

    struct SpatialBin
    {
        AABB    bounds;
        int32_t enter;
        int32_t exit;
    };

public:
    SplitBVHBuilder(BVH& bvh, const BVH::BuildParams& params);
    ~SplitBVHBuilder(void);

    BVHNode* run(int& numNodes);

private:
    static int  sortCompare(void* data, int idxA, int idxB);
    static void sortSwap(void* data, int idxA, int idxB);

    BVHNode* buildNode(const NodeSpec& spec, int level, float progressStart, float progressEnd);
    BVHNode* createLeaf(const NodeSpec& spec);

    ObjectSplit findObjectSplit(const NodeSpec& spec, float nodeSAH);
    void        performObjectSplit(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const ObjectSplit& split);

    SpatialSplit findSpatialSplit(const NodeSpec& spec, float nodeSAH);
    void         performSpatialSplit(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const SpatialSplit& split);
    void         splitReference(Reference& left, Reference& right, const Reference& ref, int dim, float pos);

private:
    SplitBVHBuilder(const SplitBVHBuilder&);            // forbidden
    SplitBVHBuilder& operator=(const SplitBVHBuilder&); // forbidden

private:
    BVH&                    m_bvh;
    const Platform&         m_platform;
    const BVH::BuildParams& m_params;

    std::vector<Reference> m_refStack;
    float                  m_minOverlap;
    std::vector<AABB>      m_rightBounds;
    int32_t                m_sortDim;
    SpatialBin             m_bins[3][NumSpatialBins];

    Timer   m_progressTimer;
    int32_t m_numDuplicates;
    int     m_numNodes;
};
} // namespace lumen