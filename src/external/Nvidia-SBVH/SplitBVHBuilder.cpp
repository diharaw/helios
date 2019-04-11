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

// for details of the algorithm, see the Nvidia paper "Spatial Splits in Bounding Volume Hierarchies" by Martin Stich, 2009
// project page: http://www.nvidia.com/object/nvidia_research_pub_012.html
// direct link: http://www.nvidia.com/docs/IO/77714/sbvh.pdf

#include "SplitBVHBuilder.h"
#include "Sort.h"

SplitBVHBuilder::SplitBVHBuilder(BVH& bvh, const BVH::BuildParams& params) :
    m_bvh(bvh),
    m_platform(bvh.getPlatform()),
    m_params(params),
    m_minOverlap(0.0f), /// overlap of AABBs
    m_sortDim(-1),
    m_numNodes(0)
{
}

//------------------------------------------------------------------------

SplitBVHBuilder::~SplitBVHBuilder(void)
{
}

//------------------------------------------------------------------------

BVHNode* SplitBVHBuilder::run(int& numNodes) /// returns the rootnode
{
    // See SBVH paper by Martin Stich for details

    // Initialize reference stack and determine root bounds.

    const glm::ivec4* tris  = m_bvh.getScene()->m_triangles.data(); // list of all triangles in scene
    const glm::vec3*  verts = m_bvh.getScene()->m_vtx_positions.data();   // list of all vertices in scene

    NodeSpec rootSpec;
    rootSpec.numRef = m_bvh.getScene()->num_triangles(); // number of triangles/references in entire scene (root)
    m_refStack.resize(rootSpec.numRef);

    // calculate the bounds of the rootnode by merging the AABBs of all the references
    for (int i = 0; i < rootSpec.numRef; i++)
    {
        // assign triangle to the std::vector of references
        m_refStack[i].triIdx = i;

        // grow the bounds of each reference AABB in all 3 dimensions by including the vertex
        for (int j = 0; j < 3; j++)
            m_refStack[i].bounds.grow(verts[tris[i][j]]);

        rootSpec.bounds.grow(m_refStack[i].bounds);
    }

    // Initialize rest of the members.

    m_minOverlap = rootSpec.bounds.area() * m_params.splitAlpha; /// split alpha (maximum allowable overlap) relative to size of rootnode
    m_rightBounds.resize(max1i(rootSpec.numRef, (int)NumSpatialBins) - 1);
    m_numDuplicates = 0;
    m_progressTimer.start();

    // Build recursively.
    BVHNode* root = buildNode(rootSpec, 0, 0.0f, 1.0f); /// actual building of splitBVH
    numNodes      = m_numNodes;
    //m_bvh.getTriIndices().clear(); // removes unused memoryspace from triIndices std::vector

    // Done.

    if (m_params.enablePrints)
        printf("SplitBVHBuilder: progress %.0f%%, duplicates %.0f%%\n",
               100.0f,
               (float)m_numDuplicates / (float)m_bvh.getScene()->num_triangles() * 100.0f);

    return root;
}

//------------------------------------------------------------------------

int SplitBVHBuilder::sortCompare(void* data, int idxA, int idxB)
{
    const SplitBVHBuilder* ptr = (const SplitBVHBuilder*)data;
    int                    dim = ptr->m_sortDim;
    const Reference&       ra  = ptr->m_refStack[idxA]; // ra is a reference (struct containing a triIdx and bounds)
    const Reference&       rb  = ptr->m_refStack[idxB]; //
    float                    ca  = ra.bounds.min()[dim] + ra.bounds.max()[dim];
    float                    cb  = rb.bounds.min()[dim] + rb.bounds.max()[dim];
    return (ca < cb) ? -1 : (ca > cb) ? 1 : (ra.triIdx < rb.triIdx) ? -1 : (ra.triIdx > rb.triIdx) ? 1 : 0;
}

//------------------------------------------------------------------------

void SplitBVHBuilder::sortSwap(void* data, int idxA, int idxB)
{
    SplitBVHBuilder* ptr = (SplitBVHBuilder*)data;
    swap(ptr->m_refStack[idxA], ptr->m_refStack[idxB]);
}

//------------------------------------------------------------------------

inline float min1f3(const float& a, const float& b, const float& c) { return glm::min(glm::min(a, b), c); }

BVHNode* SplitBVHBuilder::buildNode(const NodeSpec& spec, int level, float progressStart, float progressEnd)
{
    // Display progress.

    if (m_params.enablePrints && m_progressTimer.getElapsed() >= 1.0f)
    {
        printf("SplitBVHBuilder: progress %.0f%%, duplicates %.0f%%\r",
               progressStart * 100.0f,
               (float)m_numDuplicates / (float)m_bvh.getScene()->num_triangles() * 100.0f);
        m_progressTimer.start();
    }
    m_numNodes++;

    // Small enough or too deep => create leaf.

    if (spec.numRef <= m_platform.getMinLeafSize() || level >= MaxDepth)
    {
        return createLeaf(spec);
    }

    // Find split candidates.

    float         area    = spec.bounds.area();
    float         leafSAH = area * m_platform.getTriangleCost(spec.numRef);
    float         nodeSAH = area * m_platform.getNodeCost(2);
    ObjectSplit object  = findObjectSplit(spec, nodeSAH);

    SpatialSplit spatial;
    if (level < MaxSpatialDepth)
    {
        AABB overlap = object.leftBounds;
        overlap.intersect(object.rightBounds);
        if (overlap.area() >= m_minOverlap)
            spatial = findSpatialSplit(spec, nodeSAH);
    }

    // Leaf SAH is the lowest => create leaf.

    float minSAH = min1f3(leafSAH, object.sah, spatial.sah);
    if (minSAH == leafSAH && spec.numRef <= m_platform.getMaxLeafSize())
    {
        return createLeaf(spec);
    }

    // Leaf SAH is not the lowest => Perform spatial split.

    NodeSpec left, right;
    if (minSAH == spatial.sah)
    {
        performSpatialSplit(left, right, spec, spatial);
    }

    if (!left.numRef || !right.numRef)
    { /// if either child contains no triangles/references
        performObjectSplit(left, right, spec, object);
    }

    // Create inner node.

    m_numDuplicates += left.numRef + right.numRef - spec.numRef;
    float      progressMid = lerp(progressStart, progressEnd, (float)right.numRef / (float)(left.numRef + right.numRef));
    BVHNode* rightNode   = buildNode(right, level + 1, progressStart, progressMid);
    BVHNode* leftNode    = buildNode(left, level + 1, progressMid, progressEnd);
    return new InnerNode(spec.bounds, leftNode, rightNode);
}

//------------------------------------------------------------------------

BVHNode* SplitBVHBuilder::createLeaf(const NodeSpec& spec)
{
    std::vector<int32_t>& tris = m_bvh.getTriIndices();

	for (int i = 0; i < spec.numRef; i++)
	{
		auto last = m_refStack.back();
		m_refStack.pop_back();
		tris.push_back(last.triIdx); // take a triangle from the stack and add it to tris std::vector
	}
        
    return new LeafNode(spec.bounds, tris.size() - spec.numRef, tris.size());
}

//------------------------------------------------------------------------

SplitBVHBuilder::ObjectSplit SplitBVHBuilder::findObjectSplit(const NodeSpec& spec, float nodeSAH)
{
    ObjectSplit      split;
    const Reference* refPtr = &m_refStack[m_refStack.size() - spec.numRef];

    // Sort along each dimension.

    for (m_sortDim = 0; m_sortDim < 3; m_sortDim++)
    {
        Sort(m_refStack.size() - spec.numRef, m_refStack.size(), this, sortCompare, sortSwap);

        // Sweep right to left and determine bounds.

        AABB rightBounds;
        for (int i = spec.numRef - 1; i > 0; i--)
        {
            rightBounds.grow(refPtr[i].bounds);
            m_rightBounds[i - 1] = rightBounds;
        }

        // Sweep left to right and select lowest SAH.

        AABB leftBounds;
        for (int i = 1; i < spec.numRef; i++)
        {
            leftBounds.grow(refPtr[i - 1].bounds);
            float sah = nodeSAH + leftBounds.area() * m_platform.getTriangleCost(i) + m_rightBounds[i - 1].area() * m_platform.getTriangleCost(spec.numRef - i);
            if (sah < split.sah)
            {
                split.sah         = sah;
                split.sortDim     = m_sortDim;
                split.numLeft     = i;
                split.leftBounds  = leftBounds;
                split.rightBounds = m_rightBounds[i - 1];
            }
        }
    }
    return split;
}

//------------------------------------------------------------------------

void SplitBVHBuilder::performObjectSplit(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const ObjectSplit& split)
{
    m_sortDim = split.sortDim;
    Sort(m_refStack.size() - spec.numRef, m_refStack.size(), this, sortCompare, sortSwap);

    left.numRef  = split.numLeft;
    left.bounds  = split.leftBounds;
    right.numRef = spec.numRef - split.numLeft;
    right.bounds = split.rightBounds;
}

//------------------------------------------------------------------------

// clamping functions for int, float and glm::ivec3
inline int   clamp1i(const int v, const int lo, const int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline float clamp1f(const float v, const float lo, const float hi) { return v < lo ? lo : v > hi ? hi : v; }

inline glm::ivec3 clamp3i(const glm::ivec3& v, const glm::ivec3& lo, const glm::ivec3& hi)
{
    return glm::ivec3(clamp1i(v.x, lo.x, hi.x), clamp1i(v.y, lo.y, hi.y), clamp1i(v.z, lo.z, hi.z));
}

SplitBVHBuilder::SpatialSplit SplitBVHBuilder::findSpatialSplit(const NodeSpec& spec, float nodeSAH)
{
    // Initialize bins.

    glm::vec3 origin     = spec.bounds.min();
    glm::vec3 binSize    = (spec.bounds.max() - origin) * (1.0f / (float)NumSpatialBins);
    glm::vec3 invBinSize = glm::vec3(1.0f / binSize.x, 1.0f / binSize.y, 1.0f / binSize.z);

    for (int dim = 0; dim < 3; dim++)
    {
        for (int i = 0; i < NumSpatialBins; i++)
        {
            SpatialBin& bin = m_bins[dim][i];
            bin.bounds      = AABB();
            bin.enter       = 0;
            bin.exit        = 0;
        }
    }

    // Chop references into bins.

    for (int refIdx = m_refStack.size() - spec.numRef; refIdx < m_refStack.size(); refIdx++)
    {
        const Reference& ref = m_refStack[refIdx];

        glm::ivec3 firstBin = clamp3i(glm::ivec3((ref.bounds.min() - origin) * invBinSize), glm::ivec3(0, 0, 0), glm::ivec3(NumSpatialBins - 1, NumSpatialBins - 1, NumSpatialBins - 1));
        glm::ivec3 lastBin  = clamp3i(glm::ivec3((ref.bounds.max() - origin) * invBinSize), firstBin, glm::ivec3(NumSpatialBins - 1, NumSpatialBins - 1, NumSpatialBins - 1));

        for (int dim = 0; dim < 3; dim++)
        {
            Reference currRef = ref;
            for (int i = firstBin[dim]; i < lastBin[dim]; i++)
            {
                Reference leftRef, rightRef;
                splitReference(leftRef, rightRef, currRef, dim, origin[dim] + binSize[dim] * (float)(i + 1));
                m_bins[dim][i].bounds.grow(leftRef.bounds);
                currRef = rightRef;
            }
            m_bins[dim][lastBin[dim]].bounds.grow(currRef.bounds);
            m_bins[dim][firstBin[dim]].enter++;
            m_bins[dim][lastBin[dim]].exit++;
        }
    }

    // Select best split plane.

    SpatialSplit split;
    for (int dim = 0; dim < 3; dim++)
    {
        // Sweep right to left and determine bounds.

        AABB rightBounds;
        for (int i = NumSpatialBins - 1; i > 0; i--)
        {
            rightBounds.grow(m_bins[dim][i].bounds);
            m_rightBounds[i - 1] = rightBounds;
        }

        // Sweep left to right and select lowest SAH.

        AABB leftBounds;
        int  leftNum  = 0;
        int  rightNum = spec.numRef;

        for (int i = 1; i < NumSpatialBins; i++)
        {
            leftBounds.grow(m_bins[dim][i - 1].bounds);
            leftNum += m_bins[dim][i - 1].enter;
            rightNum -= m_bins[dim][i - 1].exit;

            float sah = nodeSAH + leftBounds.area() * m_platform.getTriangleCost(leftNum) + m_rightBounds[i - 1].area() * m_platform.getTriangleCost(rightNum);
            if (sah < split.sah)
            {
                split.sah = sah;
                split.dim = dim;
                split.pos = origin[dim] + binSize[dim] * (float)i;
            }
        }
    }
    return split;
}

//------------------------------------------------------------------------

void SplitBVHBuilder::performSpatialSplit(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const SpatialSplit& split)
{
    // Categorize references and compute bounds.
    //
    // Left-hand side:      [leftStart, leftEnd[
    // Uncategorized/split: [leftEnd, rightStart[
    // Right-hand side:     [rightStart, refs.getSize()[

    std::vector<Reference>& refs       = m_refStack;
    int               leftStart  = refs.size() - spec.numRef;
    int               leftEnd    = leftStart;
    int               rightStart = refs.size();
    left.bounds = right.bounds = AABB();

    for (int i = leftEnd; i < rightStart; i++)
    {
        // Entirely on the left-hand side?

        if (refs[i].bounds.max()[split.dim] <= split.pos)
        {
            left.bounds.grow(refs[i].bounds);
            swap(refs[i], refs[leftEnd++]);
        }

        // Entirely on the right-hand side?

        else if (refs[i].bounds.min()[split.dim] >= split.pos)
        {
            right.bounds.grow(refs[i].bounds);
            swap(refs[i--], refs[--rightStart]);
        }
    }

    // Duplicate or unsplit references intersecting both sides.

    while (leftEnd < rightStart)
    {
        // Split reference.

        Reference lref, rref;
        splitReference(lref, rref, refs[leftEnd], split.dim, split.pos);

        // Compute SAH for duplicate/unsplit candidates.

        AABB lub = left.bounds;  // Unsplit to left:     new left-hand bounds.
        AABB rub = right.bounds; // Unsplit to right:    new right-hand bounds.
        AABB ldb = left.bounds;  // Duplicate:           new left-hand bounds.
        AABB rdb = right.bounds; // Duplicate:           new right-hand bounds.
        lub.grow(refs[leftEnd].bounds);
        rub.grow(refs[leftEnd].bounds);
        ldb.grow(lref.bounds);
        rdb.grow(rref.bounds);

        float lac = m_platform.getTriangleCost(leftEnd - leftStart);
        float rac = m_platform.getTriangleCost(refs.size() - rightStart);
        float lbc = m_platform.getTriangleCost(leftEnd - leftStart + 1);
        float rbc = m_platform.getTriangleCost(refs.size() - rightStart + 1);

        float unsplitLeftSAH  = lub.area() * lbc + right.bounds.area() * rac;
        float unsplitRightSAH = left.bounds.area() * lac + rub.area() * rbc;
        float duplicateSAH    = ldb.area() * lbc + rdb.area() * rbc;
        float minSAH          = min1f3(unsplitLeftSAH, unsplitRightSAH, duplicateSAH);

        // Unsplit to left?

        if (minSAH == unsplitLeftSAH)
        {
            left.bounds = lub;
            leftEnd++;
        }

        // Unsplit to right?

        else if (minSAH == unsplitRightSAH)
        {
            right.bounds = rub;
            swap(refs[leftEnd], refs[--rightStart]);
        }

        // Duplicate?

        else
        {
            left.bounds     = ldb;
            right.bounds    = rdb;
            refs[leftEnd++] = lref;
            refs.push_back(rref);
        }
    }

    left.numRef  = leftEnd - leftStart;
    right.numRef = refs.size() - rightStart;
}

//------------------------------------------------------------------------

void SplitBVHBuilder::splitReference(Reference& left, Reference& right, const Reference& ref, int dim, float pos)
{
    // Initialize references.

    left.triIdx = right.triIdx = ref.triIdx;
    left.bounds = right.bounds = AABB();

    // Loop over vertices/edges.

    const glm::ivec4& inds  = m_bvh.getScene()->m_triangles[ref.triIdx];
    const glm::vec3* verts = m_bvh.getScene()->m_vtx_positions.data();
    const glm::vec3* v1    = &verts[inds.z];

    for (int i = 0; i < 3; i++)
    {
        const glm::vec3* v0 = v1;
        v1              = &verts[inds[i]];
        float v0p         = (*v0)[dim];
        float v1p         = (*v1)[dim];

        // Insert vertex to the boxes it belongs to.

        if (v0p <= pos)
            left.bounds.grow(*v0);
        if (v0p >= pos)
            right.bounds.grow(*v0);

        // Edge intersects the plane => insert intersection to both boxes.

        if ((v0p < pos && v1p > pos) || (v0p > pos && v1p < pos))
        {
            glm::vec3 t = lerp(*v0, *v1, clamp1f((pos - v0p) / (v1p - v0p), 0.0f, 1.0f));
            left.bounds.grow(t);
            right.bounds.grow(t);
        }
    }

    // Intersect with original bounds.

    left.bounds.max()[dim]  = pos;
    right.bounds.min()[dim] = pos;
    left.bounds.intersect(ref.bounds);
    right.bounds.intersect(ref.bounds);
}

//------------------------------------------------------------------------
