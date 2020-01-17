/* Teensy 4, 3.x, LC ADC library
 * https://github.com/pedvide/ADC
 * Copyright (c) 2019 Pedro Villanueva
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "RingBufferDynamic.h"


RingBufferDynamicUINT16::RingBufferDynamicUINT16(int pow2)
{
    size_t size = 1 << pow2;
    elems = new uint16_t[size]; //allocate, return nullptr if fails
    if (elems != nullptr){
        b_size = size;
    } //otherwise b_size remains zero
}

RingBufferDynamicUINT16::~RingBufferDynamicUINT16()
{
    delete[] elems;
}

bool RingBufferDynamicUINT16::isAlloc() { //used to check if allocation was successful
    return (elems != nullptr);
}

int RingBufferDynamicUINT16::isFull() {
    return (b_end == (b_start ^ b_size));
}

int RingBufferDynamicUINT16::isEmpty() {
    return (b_end == b_start);
}

void RingBufferDynamicUINT16::write(uint16_t value) {
    elems[b_end&(b_size-1)] = value;
    if (isFull()) { /* full, overwrite moves start pointer */
        b_start = increase(b_start);
    }
    b_end = increase(b_end);
}

uint16_t RingBufferDynamicUINT16::read() {
    uint16_t result = elems[b_start&(b_size-1)];
    b_start = increase(b_start);
    return result;
}

// increases the pointer modulo 2*size-1
int RingBufferDynamicUINT16::increase(int p) {
    return (p + 1)&(2*b_size-1);
}
