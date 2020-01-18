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

#ifndef RINGBUFFERDYNAMIC_H
#define RINGBUFFERDYNAMIC_H

// include new and delete
#include <Arduino.h>

#include <stdint.h>

/** Class RingBufferDynamic implements a circular buffer of fixed size (must be power of 2)
*   Code adapted from http://en.wikipedia.org/wiki/Circular_buffer#Mirroring
*/
class RingBufferDynamicUINT16
{
    public:
        //! Constructor, buffer is dynamically allocated using new
        RingBufferDynamicUINT16(int pow2);

        //! Destructor, buffer is deallocated using delete
        ~RingBufferDynamicUINT16();
        
        // used to check if allocation was successful
        bool isAlloc();

        //! Returns 1 (true) if the buffer is full
        int isFull();

        //! Returns 1 (true) if the buffer is empty
        int isEmpty();

        //! Write a value into the buffer
        void write(uint16_t value);

        //! Read a value from the buffer
        uint16_t read();
        
        // Clear out the whole buffer
        void clear(){b_start=0;b_end=0;}

    protected:
    private:

        int increase(int p);
        int b_size = 0;
        int    b_start = 0;
        int    b_end = 0;
        uint16_t *elems;
};


#endif // RINGBUFFERDYNAMIC_H
