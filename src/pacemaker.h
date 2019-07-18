/**
 * ====================================================================
 * Author: Nikolaus Mayer, 2015 (mayern@cs.uni-freiburg.de)
 * ====================================================================
 * A query-able clock that ticks X times per second (header-only)
 * ====================================================================
 *
 * Usage Example:
 * 
 * >
 * > #include <stdio>
 * > #include "pacemaker.h"
 * >
 * > int main( int argc, char** argv ) {
 * >   
 * >   /// 30 FPS clock
 * >   /// If a NEGATIVE number is given, the pacemaker will ALWAYS
 * >   /// return TRUE from ::IsDue() (unless it is paused)!
 * >   Pacemaker::Pacemaker my_clock(30.f);
 * >
 * >   for (;;) {
 * >     if ( my_clock.IsDue() ) {
 * >       /// This will be executed at most 30 times a second
 * >     }
 * >   }
 * >   
 * >   return 0;
 * > }
 * >
 *
 * ====================================================================
 */


#ifndef PACEMAKER_H__
#define PACEMAKER_H__


/// Print debugging information
//#define DEBUG_MODE

/// Make thread-safe
#define THREAD_SAFE

/// System/STL
#ifdef DEBUG_MODE
  #include <iostream>
  #include <sstream>
#endif
#ifdef THREAD_SAFE
  #include <mutex>
#endif
#include <chrono>


namespace Pacemaker {
  
  /// Typedefinitions
  typedef std::chrono::steady_clock::time_point TIME_POINT_T;
  //typedef std::chrono::duration<std::chrono::nanoseconds> TIME_DURATION_T;
  typedef std::chrono::duration<long, std::nano> TIME_RESOLUTION_T;


  /// /////////////////////////////////////////////////////////////////
  /// Non-class functions
  /// /////////////////////////////////////////////////////////////////
  
  /// Get current time point
  static TIME_POINT_T Now()
  {
    return std::chrono::steady_clock::now();
  }
  
  /// Compute elapsed nanoseconds between two time points
  static TIME_RESOLUTION_T NanosecondsBetween( const TIME_POINT_T& end,
                                               const TIME_POINT_T& start 
                                             )
  {
    return std::chrono::duration_cast<TIME_RESOLUTION_T>(end-start);
  }



  
  /// /////////////////////////////////////////////////////////////////
  /// Pacemaker class
  /// /////////////////////////////////////////////////////////////////
  class Pacemaker {
    
  public:
    
    /// Possible states
    enum Stage {
      Running=0,
      Paused
    };
    
    /**
     * Constructor
     * 
     * @param target_fps FPS target for the pacemaker beat
     * @param accumulate_unfetched_ticks The pacemaker will always generate X ticks per second. If this parameter is TRUE, then unfetched ticks are backed up (Example: On a 10fps-pacemaker, if no tick is fetched for 1s, then the next 10 calls to IsDue() will return true, no matter how fast the calls come). If this parameter is FALSE (default), then these unfetched ticks expire and are ignored (In the example: the next calls after the 1s-pause still only return true every 100ms).
     */
    Pacemaker( 
          float target_fps=30.f,
          bool accumulate_unfetched_ticks=false);
    
    /// Destructor
    ~Pacemaker() { }
    
    /** 
     * Check if the next beat is due. This method will try to return TRUE exactly "target_fps" times per second.
     * 
     * @returns TRUE IFF at least 1/target_fps seconds have passed since the last IsDue() call, ELSE FALSE
     */
    bool IsDue();
    /**
     * Check if the next beat is due. This method will try to return TRUE exactly "target_fps" times per second.
     * NOTE: Calling this method is equivalent to calling .IsDue()
     * 
     * @returns TRUE IFF at least 1/target_fps seconds have passed since the last IsDue() call, ELSE FALSE
     */
    bool operator()();

    /// Pause the instance
    void Pause();

    /// Unpause the instance
    void Resume();
    
    /// Reset ("time of last beat" is set to "now")
    void Reset();
    
    /**
     * Reassign a target FPS count
     * 
     * @param new_target_fps The new target FPS count
     */
    void SetTargetFPS( 
          float new_target_fps=30.f);
    
  private:
    
    Stage m_current_stage;
    
    TIME_POINT_T m_time_of_last_beat;
    float m_target_fps;
    TIME_RESOLUTION_T m_ns_per_beat;

    bool m_accumulated_unfetched_ticks;

    #ifdef THREAD_SAFE
      std::mutex m_IsDue__LOCK;
    #endif
    
  };



  /// /////////////////////////////////////////////////////////////////
  /// Pacemaker class implementation
  /// /////////////////////////////////////////////////////////////////

  /**
   * Constructor
   * 
   * @param target_fps FPS target for the pacemaker beat
   * @param accumulate_unfetched_ticks The pacemaker will always generate X ticks per second. If this parameter is TRUE, then unfetched ticks are backed up (Example: On a 10fps-pacemaker, if no tick is fetched for 1s, then the next 10 calls to IsDue() will return true, no matter how fast the calls come). If this parameter is FALSE (default), then these unfetched ticks expire and are ignored (In the example: the next calls after the 1s-pause still only return true every 100ms).
   */
  Pacemaker::Pacemaker(float target_fps,
                       bool accumulate_unfetched_ticks)
    : m_current_stage(Running),
      m_accumulated_unfetched_ticks(accumulate_unfetched_ticks)
  {
    m_time_of_last_beat = Now();
    SetTargetFPS(target_fps);
  }
  
  /** 
   * Check if the next beat is due. This method will try to return TRUE exactly "target_fps" times per second.
   * 
   * @returns TRUE IFF at least 1/target_fps seconds have passed since the last IsDue() call, ELSE FALSE
   */
  bool Pacemaker::IsDue()
  {
    if (m_current_stage == Paused)
      return false;

    /// FPS target is zero? -> Always-OFF mode
    if (m_target_fps == 0.f)
      return false;
    
    /// FPS target is negative? -> Always-ON mode
    if (m_target_fps < 0.f)
      return true;

    #ifdef THREAD_SAFE
      std::lock_guard<std::mutex> lock(m_IsDue__LOCK);
    #endif
    
    const TIME_POINT_T now = Now();
    
    const TIME_RESOLUTION_T difference = NanosecondsBetween(now, 
                                                    m_time_of_last_beat);

    /// Check if a tick is due
    if (difference < m_ns_per_beat) {
      return false;
    } else {
      if (m_accumulated_unfetched_ticks) {
        m_time_of_last_beat += m_ns_per_beat;
      } else {
        #ifdef DEBUG_MODE
          if ((difference / m_ns_per_beat) > 1)
            std::cout << "Pacemaker: Time since last fetched beat=" 
                      << difference.count() << "ns, skipping "
                      << (difference / m_ns_per_beat) << " beats of "
                      << m_ns_per_beat.count() << "ns each.\n";
        #endif
        m_time_of_last_beat += (difference / m_ns_per_beat)*m_ns_per_beat;
      }

      #ifdef DEBUG_MODE
        std::ostringstream oss;
        oss << "Pacemaker: Last beat now " 
            << NanosecondsBetween(now, m_time_of_last_beat).count()
            << "us in the past.\n";
        std::cout << oss.str();
      #endif

      return true;
    }
  }


  /** 
   * Check if the next beat is due. This method will try to return TRUE exactly "target_fps" times per second.
   * NOTE: Calling this method is equivalent to calling .IsDue()
   * 
   * @returns TRUE IFF at least 1/target_fps seconds have passed since the last IsDue() call, ELSE FALSE
   */
  bool Pacemaker::operator()()
  {
    return IsDue();
  }

  /// Pause the instance
  void Pacemaker::Pause()
  {
    m_current_stage = Paused;
  }

  /// Unpause the instance
  void Pacemaker::Resume()
  {
    m_current_stage = Running;
  }
  
  /// Reset ("time of last beat" is set to "now")
  void Pacemaker::Reset()
  {
    m_time_of_last_beat = Now();
  }
  
  /**
   * Reassign a target FPS count
   * 
   * @param new_target_fps The new target FPS count
   */
  void Pacemaker::SetTargetFPS(float new_target_fps)
  {
    m_target_fps = new_target_fps;
    if (m_target_fps > 0.f)
      m_ns_per_beat = TIME_RESOLUTION_T((long)1e12/(long)(1e3*m_target_fps));

    #ifdef DEBUG_MODE
      std::ostringstream oss;
      if (m_target_fps == 0.f)
        oss << "Pacemaker: New settings:"
            << " 0 fps specified, no ticks will be issued.\n";
      else if (m_target_fps < 0.f)
        oss << "Pacemaker: New settings:"
            << " Negative fps specified, every request will generate a tick.\n";
      else
        oss << "Pacemaker: New settings:"
            << " At " << m_target_fps << "fps, one tick every "
            << m_ns_per_beat.count() << "ns.\n";
      std::cout << oss.str();
    #endif
  }
  



  
  
}  // namespace Pacemaker



#ifdef DEBUG_MODE
#undef DEBUG_MODE
#endif

#ifdef THREAD_SAFE
#undef THREAD_SAFE
#endif


#endif  // PACEMAKER_H__

