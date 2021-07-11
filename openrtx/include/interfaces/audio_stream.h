/***************************************************************************
 *   Copyright (C) 2021 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdint.h>
#include <sys/types.h>
#include <interfaces/audio_path.h>

typedef uint16_t stream_sample_t;

#ifdef __cplusplus
#include <array>

extern "C" {
#endif

enum BufMode
{
    BUF_LINEAR,        ///< Linear buffer mode, conversion stops when full.
    BUF_CIRC,          ///< Circular buffer mode, conversion never stops, thread woken up when full.
    BUF_CIRC_DOUBLE    ///< Circular double buffer mode, conversion never stops, thread woken up whenever half of the buffer is full.
};

typedef struct
{
    stream_sample_t *data;
    size_t len;
}
dataBlock_t;

typedef int8_t streamId;

/**
 * Try to activate the sampler to acquire an input audio stream.
 * This will open the required audio path and start the data streaming.
 * If the path is not available or busy with another process,
 * this function will return an error.
 * If a stream is opened with a higher priority than a current stream using the same
 * source, the new stream takes over the previous stream.
 *
 * @param source: input source specifier.
 * @param prio: priority of the requester
 * @param buf: pointer to a buffer used for management of sampled data.
 * @param bufLength: length of the buffer, in elements.
 * @param mode: buffer management mode
 * @param sampleRate: sample rate, in Hz.
 * @return a unique identifier for the stream or -1 if the stream could not be opened.
 */
streamId inputStream_start(const enum AudioSource source,
                           const enum AudioPriority prio,
                           const stream_sample_t* buf,
                           const size_t bufLength,
                           const enum BufMode mode,
                           const uint32_t sampleRate);

/**
 * Get a chunk of data, blocking function.
 * If buffer management is configured to BUF_LINEAR this function also starts a
 * new data acquisition.
 *
 * @return dataBlock_t containing a pointer to the chunk head and its length. If
 * another thread is pending on this function, it returns immediately a dataBlock_t
 * cointaining < NULL, 0 >.
 * @param id: identifier of the stream to get data from
 */
dataBlock_t inputStream_getData(streamId id);

/**
 * Release the current input stream, allowing for a new call of startInputStream.
 * If this function is called when sampler is running, acquisition is stopped and
 * any thread waiting on getData() is woken up and given a partial result.
 * @param id: identifier of the stream to be stopped
 */
void inputStream_stop(streamId id);

/**
 * Send and audio stream to a given output.
 * This function blocks until all data has been sent.
 * If a stream is opened with a higher priority than a current stream using the
 * same source, the new stream takes over the previous stream.
 *
 * @param destination: destination of the output stream.
 * @param prio: priority of the requester
 * @param buf: buffer containing the audio samples.
 * @param length: length of the buffer, in elements.
 * @param sampleRate: sample rate in Hz.
 * @return a unique identifier for the stream or -1 if the stream could not be opened.
 */
streamId outputStream_start(const enum AudioSink destination,
                            const enum AudioPriority prio,
                            const stream_sample_t* const buf,
                            const size_t length,
                            const uint32_t sampleRate);

/**
 * Interrupt a currently ongoing output stream before its natural ending.
 * @param id: identifier of the stream to be stopped
 */
void outputStream_stop(streamId id);

#ifdef __cplusplus
}

/**
 * Get a chunk of data, blocking function.
 * This specialized API works when DMA is configured in BUF_CIRC_DOUBLE mode,
 * returning a std::array containing the current samples. Two arrays are used
 * to perform a double buffering, the two are returned in an interleaved way.
 *
 * @return std::array pointer containing the acquired samples, nullptr if
 * another thread is pending on this function.
 * @param id: identifier of the stream to get data from
 */
template <size_t N>
std::array<stream_sample_t, N> *inputStream_getDataDoubleBuffer(streamId id);

#endif

#endif /* AUDIO_STREAM_H */
