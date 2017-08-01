/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Non-blocking event logger intended for safe communication between
// processes via shared memory

#ifndef ANDROID_MEDIA_PERFORMANCEANALYSIS_H
#define ANDROID_MEDIA_PERFORMANCEANALYSIS_H

#include <deque>
#include <map>
#include <vector>

#include <media/nbaio/ReportPerformance.h>

namespace android {

namespace ReportPerformance {

class PerformanceAnalysis;

// a map of PerformanceAnalysis instances
// The outer key is for the thread, the inner key for the source file location.
using PerformanceAnalysisMap = std::map<int, std::map<log_hash_t, PerformanceAnalysis>>;

class PerformanceAnalysis {
    // This class stores and analyzes audio processing wakeup timestamps from NBLog
    // FIXME: currently, all performance data is stored in deques. Need to add a mutex.
    // FIXME: continue this way until analysis is done in a separate thread. Then, use
    // the fifo writer utilities.
public:

    PerformanceAnalysis() {};

    friend void dump(int fd, int indent,
                     PerformanceAnalysisMap &threadPerformanceAnalysis);

    // Given a series of audio processing wakeup timestamps,
    // compresses and and analyzes the data, and flushes
    // the timestamp series from memory.
    void processAndFlushTimeStampSeries();

    // Called when an audio on/off event is read from the buffer,
    // e.g. EVENT_AUDIO_STATE.
    // calls flushTimeStampSeries on the data up to the event,
    // effectively discarding the idle audio time interval
    void handleStateChange();

    // Writes wakeup timestamp entry to log and runs analysis
    // TODO: make this thread safe. Each thread should have its own instance
    // of PerformanceAnalysis.
    void logTsEntry(timestamp ts);

    // FIXME: make peakdetector and storeOutlierData a single function
    // Input: mOutlierData. Looks at time elapsed between outliers
    // finds significant changes in the distribution
    // writes timestamps of significant changes to mPeakTimestamps
    bool detectAndStorePeak(msInterval delta, timestamp ts);

    // runs analysis on timestamp series before it is converted to a histogram
    // finds outliers
    // writes to mOutlierData <time elapsed since previous outlier, outlier timestamp>
    bool detectAndStoreOutlier(const msInterval diffMs);

    // Generates a string of analysis of the buffer periods and prints to console
    // TODO: WIP write more detailed analysis
    // FIXME: move this data visualization to a separate class. Model/view/controller
    void reportPerformance(String8 *body, int maxHeight = 10);

    // This function detects glitches in a time series.
    // TODO: learn what timestamp sequences correlate with glitches instead of
    // manually designing a heuristic. Ultimately, detect glitches directly from audio.
    // void alertIfGlitch(const std::vector<timestamp_raw> &samples);

private:

    // TODO use a circular buffer for the deques and vectors below

    // stores outlier analysis:
    // <elapsed time between outliers in ms, outlier beginning timestamp>
    std::deque<std::pair<msInterval, timestamp>> mOutlierData;

    // stores each timestamp at which a peak was detected
    // a peak is a moment at which the average outlier interval changed significantly
    std::deque<timestamp> mPeakTimestamps;

    // stores buffer period histograms with timestamp of first sample
    std::deque<std::pair<timestamp, Histogram>> mHists;

    // Parameters used when detecting outliers
    struct BufferPeriod {
        double    mMean = -1;          // average time between audio processing wakeups
        double    mOutlierFactor = -1; // values > mMean * mOutlierFactor are outliers
        double    mOutlier = -1;       // this is set to mMean * mOutlierFactor
        timestamp mPrevTs = -1;        // previous timestamp
    } mBufferPeriod;

    // TODO: delete values below if unused or add them to the BufferPeriod struct
    // FIXME: decide whether to make kPeriodMs static.
    // static const int kNumBuff = 3; // number of buffers considered in local history
    // int kPeriodMs; // current period length is ideally 4 ms
    // static const int kOutlierMs = 7; // values greater or equal to this cause glitches
    // DAC processing time for 4 ms buffer
    // static constexpr double kRatio = 0.75; // estimate CPU time as ratio of period
    // int kPeriodMsCPU; // compute based on kPeriodLen and kRatio

    // capacity allocated to data structures
    // TODO: make these values longer when testing is finished
    struct MaxLength {
        size_t Hists; // number of histograms stored in memory
        size_t TimeStamps; // histogram size
        size_t Outliers; // number of values stored in outlier array
        size_t Peaks; // number of values stored in peak array
        int HistTimespanMs; // maximum histogram timespan
    };
    static constexpr MaxLength kMaxLength = {.Hists = 20, .TimeStamps = 1000,
            .Outliers = 100, .Peaks = 100, .HistTimespanMs = 5 * kMsPerSec };

    // these variables ensure continuity while analyzing the timestamp
    // series one sample at a time.
    // TODO: change this to a running variance/mean class
    struct OutlierDistribution {
        msInterval mMean = 0;         // sample mean since previous peak
        msInterval mSd = 0;           // sample sd since previous peak
        msInterval mElapsed = 0;      // time since previous detected outlier
        const int  kMaxDeviation = 5; // standard deviations from the mean threshold
        msInterval mTypicalDiff = 0;  // global mean of outliers
        double     mN = 0;            // length of sequence since the last peak
        double     mM2 = 0;           // used to calculate sd
    } mOutlierDistribution;
};

void dump(int fd, int indent, PerformanceAnalysisMap &threadPerformanceAnalysis);
void dumpLine(int fd, int indent, const String8 &body);

} // namespace ReportPerformance

}   // namespace android

#endif  // ANDROID_MEDIA_PERFORMANCEANALYSIS_H
