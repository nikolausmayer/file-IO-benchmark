/**
 * ====================================================================
 * Author: Nikolaus Mayer, 2015 (mayern@cs.uni-freiburg.de)
 * ====================================================================
 * Estimate frames-per-second from manual samples (header-only class)
 * ====================================================================
 *
 * Usage Example:
 * 
 * >
 * > #include <stdio>
 * > #include "fps.h"
 * >
 * > int main( int argc, char** argv ) {
 * >
 * >   FramesPerSecond::FPSEstimator fps;
 * >
 * >   for (;;) {
 * >     if ( something_has_happened ) {
 * >       fps.AddSample();
 * >     }
 * >     
 * >     /// Estimate the occurence rate over the past 2 seconds
 * >     std::cout << "Current FPS=" << fps.FPS(2.f) << '\n';
 * >   }
 * >
 * >   return 0;
 * > }
 * >
 *
 * ====================================================================
 */


#ifndef FRAMESPERSECOND_H__
#define FRAMESPERSECOND_H__


/// Print debugging information
//#define DEBUG_MODE

/// Enable thread-safe behaviour
#define THREAD_SAFE


/// System/STL
#ifdef DEBUG_MODE
  #include <iostream>
  #include <sstream>
#endif
#include <chrono>
#include <vector>
#ifdef THREAD_SAFE
  #include <mutex>
#endif


namespace FramesPerSecond {
  
  typedef std::chrono::steady_clock::time_point TIME_POINT_T;
  typedef std::chrono::duration<TIME_POINT_T> TIME_DURATION_T;
  typedef std::chrono::nanoseconds TIME_RESOLUTION_T;


  /// /////////////////////////////////////////////////////////////////
  /// Non-class functions
  /// /////////////////////////////////////////////////////////////////
    
  /// Get current time point
  static TIME_POINT_T Now()
  {
    return std::chrono::steady_clock::now();
  }
  
  /// Compute elapsed milliseconds between two time points
  static float NanosecondsBetween(const TIME_POINT_T& end,
                                  const TIME_POINT_T& start)
  {
    return std::chrono::duration_cast<TIME_RESOLUTION_T>(end-start).count() /
           1e3f;
  }
  



  /// /////////////////////////////////////////////////////////////////
  /// FPSEstimator class declaration
  /// /////////////////////////////////////////////////////////////////
  class FPSEstimator {
    
  public:

    enum EstimationMethod {
      CountSamples = 0,
      AverageIntervals
    };
    
    /// Constructor
    FPSEstimator();
        
    /// Destructor
    ~FPSEstimator() { }
    
    /// Set decay factor
    void SetDecayFactor(
          float new_decay_factor = 0.f);

    /// Add a sample
    void AddSample(float amount=1.f);    
    
    /** 
     * Estimate FPS over a given window. Larger choices of the argument 
     * "window_seconds" will lead to more stable estimates, but may smooth 
     * out (and thus lose) high frequency changes in the FPS rate.
     * 
     * @param window_seconds Number of past seconds over which to measure
     * @param soft_estimate IFF TRUE, the return value slowly changes (rolling weighted average)
     * 
     * @returns an estimate of the rate with which new samples are currently 
     *          arriving, computed over a past time window of "window_seconds" 
     *          seconds (starting now). If there is not enough data available, 
     *          a negative value is returned.
     */
    float FPS(
          float window_seconds = 1.f,
          bool soft_estimate = false,
          EstimationMethod method = CountSamples);
        
    /// Reset the instance
    void Reset();
    
  private:
    
    
    std::vector<TIME_POINT_T> m_sample_times;
    std::vector<float> m_samples;

    float m_rolling;
    float m_decay_factor;
    
    #ifdef DEBUG_MODE
      TIME_POINT_T m_debug_start_time;
    #endif

    #ifdef THREAD_SAFE
      std::mutex m_data_access_mutex;
    #endif
  };



  /// /////////////////////////////////////////////////////////////////
  /// FPSEstimator class implementation
  /// /////////////////////////////////////////////////////////////////

  /// Constructor
  FPSEstimator::FPSEstimator()
  : m_rolling(0.f),
    m_decay_factor(0.f)
  { 
    #ifdef DEBUG_MODE
      m_debug_start_time = Now();
    #endif
  }

  /**
   * Set decay factor
   *
   * @param new_decay_factor The new decay factor
   */
  void FPSEstimator::SetDecayFactor(float new_decay_factor)
  {
    m_decay_factor = new_decay_factor;
  }
  
  /// Add a sample
  void FPSEstimator::AddSample(float amount)
  {
    const TIME_POINT_T now = Now();
    {
      #ifdef THREAD_SAFE
        std::lock_guard<std::mutex> lock(m_data_access_mutex);
      #endif
      m_sample_times.push_back(now);
      m_samples.push_back(amount);
    }
    
    #ifdef DEBUG_MODE
      float elapsed = NanosecondsBetween(now, m_debug_start_time);
      std::cout << "FPSEstimator: New sample stored ("
                << elapsed << "ns)\n";
    #endif
  }
  
  /** 
   * Estimate FPS over a given window. Larger choices of the argument 
   * "window_seconds" will lead to more stable estimates, but may smooth 
   * out (and thus lose) high frequency changes in the FPS rate.
   * 
   * @param window_seconds Number of past seconds over which to measure
   * @param soft_estimate IFF TRUE, the return value slowly changes (rolling weighted average)
   * 
   * @returns an estimate of the rate with which new samples are currently 
   *          arriving, computed over a past time window of "window_seconds" 
   *          seconds (starting now). If there is not enough data available, 
   *          a negative value is returned.
   */
  float FPSEstimator::FPS(float window_seconds,
                          bool soft_estimate,
                          FPSEstimator::EstimationMethod method)
  {
    if (m_sample_times.size() <= 0)
      return -1.f;

    switch (method) {
    
      case CountSamples: {
        #ifdef DEBUG_MODE
          std::ostringstream oss;
          oss << "FPSEstimator: Sampling.. ( ";
        #endif

        double samples = 0.f;
        int i = m_sample_times.size()-1;
        const TIME_POINT_T now = Now();
        {
          #ifdef THREAD_SAFE
            std::lock_guard<std::mutex> lock(m_data_access_mutex);
          #endif
          for (; i >= 0; --i) {
            if (NanosecondsBetween(now, m_sample_times[i]) >= (window_seconds*1e6f))
              break;
            #ifdef DEBUG_MODE
              oss << NanosecondsBetween(now, m_sample_times[i]) << "ns ";
            #endif
            samples += m_samples[i];
          }
        }
        
        #ifdef DEBUG_MODE
          oss << ")";
        #endif

        /// If "index" is negative, there were not enough samples to fill the time window
        if (i <= 0)
          return -1.f;
        
        /** 
         * We have an integer estimate, but we want to be more informative. 
         * Let's compute an estimate for the mission fraction.
         *
         *    window_seconds*1e6-NsB(now,i+1)
         *              ╭──┴─╮
         *              │╭NsB(now,i)-NsB(now,i-1)           
         *           ╭───┴───╮                              
         *  o    o   o  [    o      o  o      o       o     ]  ◀◀ Samples
         *  0   i-1  i  │   i+1                   size()-1  │
         *           │  ╰────────────────┬──────────────────╯
         *           │       │    window_seconds*1e6)       │
         *           │       ╰────────────────┬─────────────╯
         *           │                  NsB(now,i+1)        │
         *           ╰────────────────────────┬─────────────╯
         *                              NsB(now,i)
         *
         *  >>>>>>>>>>>>│>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>│  ◀◀ Timeline
         *    "window_seconds" ago                         Now
         *
         *  Old samples (indices 0 to i-1) are periodically discarded.
         */
        
        #ifdef DEBUG_MODE
          oss << "\n";
          std::cout << oss.str();
        #endif

        /// Discard old samples
        {
          static int cleanup = 0;
          if (++cleanup >= 1000) {
            #ifdef DEBUG_MODE
              oss << "\nFPSEstimator: Discarding " << (i-1) << " old samples.";
            #endif
            cleanup = 0;
            {
              #ifdef THREAD_SAFE
                std::lock_guard<std::mutex> lock(m_data_access_mutex);
              #endif
              m_sample_times.erase(m_sample_times.begin(),
                                   m_sample_times.begin()+i-1);
              m_samples.erase(m_samples.begin(),
                              m_samples.begin()+i-1);
            }
          }
        }

        /// Adjust the rolling weighted average estimate
        m_rolling =      m_decay_factor  * m_rolling + 
                    (1.f-m_decay_factor) * samples/window_seconds;
          
        if (soft_estimate)
          return m_rolling;
        else
          return samples/window_seconds;
      }

      case AverageIntervals: {
        int samples = 0;
        int i = m_sample_times.size()-1;
        const TIME_POINT_T now = Now();
        TIME_POINT_T youngest_sample;
        
        #ifdef DEBUG_MODE
          std::ostringstream oss;
          oss << "FPSEstimator: Passing samples: (";
        #endif
          
        {
          #ifdef THREAD_SAFE
            std::lock_guard<std::mutex> lock(m_data_access_mutex);
          #endif
          youngest_sample = m_sample_times[i];
          for (; i >= 0; --i) {
            if (NanosecondsBetween(now, m_sample_times[i]) >= (window_seconds*1e6f))
              break;
            #ifdef DEBUG_MODE
              oss << NanosecondsBetween(now, m_sample_times[i]) << "ns ";
            #endif
            ++samples;
          }
        }
        
        #ifdef DEBUG_MODE
          oss << ") = " << samples << " samples, youngest sample="
              << NanosecondsBetween(now, youngest_sample)
              << "ns\n";
        #endif

        /// If "index" is negative, there were not enough samples to fill the time window
        if (i < 0)
          return -1.f;

        float average_interval = NanosecondsBetween(youngest_sample, 
                                                     m_sample_times[i]) /
                                 samples;
        float fps_estimate = 1e6f / average_interval;

        #ifdef DEBUG_MODE
          oss << "Interval=" << NanosecondsBetween(now, m_sample_times[i])
              << "ns => " << NanosecondsBetween(now, youngest_sample)
              << "ns is " << NanosecondsBetween(youngest_sample, 
                                                m_sample_times[i])
              << "ns => average over " << samples << " intervals is "
              << average_interval << "ns\n";
        #endif
        
        /// Discard old samples
        {
          static int cleanup = 0;
          if (++cleanup >= 1000) {
            #ifdef DEBUG_MODE
              oss << "FPSEstimator: Discarding " << (i-1) << " old samples.\n";
            #endif
            cleanup = 0;
            {
              #ifdef THREAD_SAFE
                std::lock_guard<std::mutex> lock(m_data_access_mutex);
              #endif
              m_sample_times.erase(m_sample_times.begin(),
                                   m_sample_times.begin()+i-1);
            }
          }
        }
        
        #ifdef DEBUG_MODE
          std::cout << oss.str();
        #endif

        /// Adjust the rolling weighted average estimate
        m_rolling =      m_decay_factor  * m_rolling + 
                    (1.f-m_decay_factor) * fps_estimate;
          
        if (soft_estimate)
          return m_rolling;
        else
          return fps_estimate;
      }

      default: {
        throw std::runtime_error("FPSEstimator: Unknown value for 'method'");
      }
    }
  }
  
  /// Reset the instance
  void FPSEstimator::Reset()
  {
    m_sample_times.clear();
    
    #ifdef DEBUG_MODE
      std::cout << "FPSEstimator: Resetting..\n";
      m_debug_start_time = Now();
    #endif
  }

  
}  // namespace FramesPerSecond



#ifdef DEBUG_MODE
#undef DEBUG_MODE
#endif

#ifdef THREAD_SAFE
#undef THREAD_SAFE
#endif


#endif  // FRAMESPERSECOND_H__

