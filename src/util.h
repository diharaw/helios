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

#include <string>
#include <ctime>

namespace lumen
{
class Platform
{
public:
    Platform()
    {
        m_name            = std::string("Default");
        m_SAHNodeCost     = 1.f;
        m_SAHTriangleCost = 1.f;
        m_nodeBatchSize   = 1;
        m_triBatchSize    = 1;
        m_minLeafSize     = 1;
        m_maxLeafSize     = 0x7FFFFFF;
    } /// leafsize = aantal tris
    Platform(const std::string& name, float nodeCost = 1.f, float triCost = 1.f, int32_t nodeBatchSize = 1, int32_t triBatchSize = 1)
    {
        m_name            = name;
        m_SAHNodeCost     = nodeCost;
        m_SAHTriangleCost = triCost;
        m_nodeBatchSize   = nodeBatchSize;
        m_triBatchSize    = triBatchSize;
        m_minLeafSize     = 1;
        m_maxLeafSize     = 0x7FFFFFF;
    }

    const std::string& getName() const { return m_name; }

    // SAH weights
    float getSAHTriangleCost() const { return m_SAHTriangleCost; }
    float getSAHNodeCost() const { return m_SAHNodeCost; }

    // SAH costs, raw and batched
    float getCost(int numChildNodes, int numTris) const { return getNodeCost(numChildNodes) + getTriangleCost(numTris); }
    float getTriangleCost(int32_t n) const { return roundToTriangleBatchSize(n) * m_SAHTriangleCost; }
    float getNodeCost(int32_t n) const { return roundToNodeBatchSize(n) * m_SAHNodeCost; }

    // batch processing (how many ops at the price of one)
    int32_t  getTriangleBatchSize() const { return m_triBatchSize; }
    int32_t  getNodeBatchSize() const { return m_nodeBatchSize; }
    void setTriangleBatchSize(int32_t triBatchSize) { m_triBatchSize = triBatchSize; }
    void setNodeBatchSize(int32_t nodeBatchSize) { m_nodeBatchSize = nodeBatchSize; }
    int32_t  roundToTriangleBatchSize(int32_t n) const { return ((n + m_triBatchSize - 1) / m_triBatchSize) * m_triBatchSize; }
    int32_t  roundToNodeBatchSize(int32_t n) const { return ((n + m_nodeBatchSize - 1) / m_nodeBatchSize) * m_nodeBatchSize; }

    // leaf preferences
    void setLeafPreferences(int32_t minSize, int32_t maxSize)
    {
        m_minLeafSize = minSize;
        m_maxLeafSize = maxSize;
    }
    int32_t getMinLeafSize() const { return m_minLeafSize; }
    int32_t getMaxLeafSize() const { return m_maxLeafSize; }

private:
    std::string m_name;
    float       m_SAHNodeCost;
    float       m_SAHTriangleCost;
    int32_t         m_triBatchSize;
    int32_t         m_nodeBatchSize;
    int32_t         m_minLeafSize;
    int32_t         m_maxLeafSize;
};
}