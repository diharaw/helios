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

#include "sort.h"
#include <assert.h>
#include <stdint.h>

#define QSORT_STACK_SIZE 32

static inline void InsertionSort(int start, int size, void* data, SortCompareFunc compareFunc, SortSwapFunc swapFunc);
static inline int  Median3(int low, int high, void* data, SortCompareFunc compareFunc);
static void        Qsort(int low, int high, void* data, SortCompareFunc compareFunc, SortSwapFunc swapFunc);

void InsertionSort(int start, int size, void* data, SortCompareFunc compareFunc, SortSwapFunc swapFunc)
{
    assert(compareFunc && swapFunc);
    assert(size >= 0);

    for (int i = 1; i < size; i++)
    {
        int j = start + i - 1;
        while (j >= start && compareFunc(data, j, j + 1) > 0)
        {
            swapFunc(data, j, j + 1);
            j--;
        }
    }
}

//------------------------------------------------------------------------

int Median3(int low, int high, void* data, SortCompareFunc compareFunc)
{
    assert(compareFunc);
    assert(low >= 0 && high >= 2);

    int l = low;
    int c = (low + high) >> 1;
    int h = high - 2;

    if (compareFunc(data, l, h) > 0) swap(l, h);
    if (compareFunc(data, l, c) > 0) c = l;
    return (compareFunc(data, c, h) > 0) ? h : c;
}

//------------------------------------------------------------------------

void Qsort(int low, int high, void* data, SortCompareFunc compareFunc, SortSwapFunc swapFunc)
{
    assert(compareFunc && swapFunc);
    assert(low <= high);

    int stack[QSORT_STACK_SIZE];
    int sp      = 0;
    stack[sp++] = high;

    while (sp)
    {
        high = stack[--sp];
        assert(low <= high);

        // Use insertion sort for small values or if stack gets full.

        if (high - low <= 15 || sp + 2 > QSORT_STACK_SIZE)
        {
            InsertionSort(low, high - low, data, compareFunc, swapFunc);
            low = high + 1;
            continue;
        }

        // Select pivot using median-3, and hide it in the highest entry.

        swapFunc(data, Median3(low, high, data, compareFunc), high - 1);

        // Partition data.

        int i = low - 1;
        int j = high - 1;
        for (;;)
        {
            do
                i++;
            while (compareFunc(data, i, high - 1) < 0);
            do
                j--;
            while (compareFunc(data, j, high - 1) > 0);

            assert(i >= low && j >= low && i < high && j < high);
            if (i >= j)
                break;

            swapFunc(data, i, j);
        }

        // Restore pivot.

        swapFunc(data, i, high - 1);

        // Sort sub-partitions.

        assert(sp + 2 <= QSORT_STACK_SIZE);
        if (high - i > 2)
            stack[sp++] = high;
        if (i - low > 1)
            stack[sp++] = i;
        else
            low = i + 1;
    }
}

//------------------------------------------------------------------------

void Sort(int start, int end, void* data, SortCompareFunc compareFunc, SortSwapFunc swapFunc)
{
    assert(start <= end);
    assert(compareFunc && swapFunc);

    if (start + 2 <= end)
        Qsort(start, end, data, compareFunc, swapFunc);
}

//------------------------------------------------------------------------

int CompareS32(void* data, int idxA, int idxB)
{
    int32_t a = ((int32_t*)data)[idxA];
    int32_t b = ((int32_t*)data)[idxB];
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

//------------------------------------------------------------------------

void SwapS32(void* data, int idxA, int idxB)
{
    swap(((int32_t*)data)[idxA], ((int32_t*)data)[idxB]);
}

//------------------------------------------------------------------------

int CompareF32(void* data, int idxA, int idxB)
{
    float a = ((float*)data)[idxA];
    float b = ((float*)data)[idxB];
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

//------------------------------------------------------------------------

void SwapF32(void* data, int idxA, int idxB)
{
    swap(((float*)data)[idxA], ((float*)data)[idxB]);
}

//------------------------------------------------------------------------
