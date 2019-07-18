/**
 * ====================================================================
 * Author: Nikolaus Mayer, 2018 (mayern@cs.uni-freiburg.de)
 * ====================================================================
 * A simple timing class for profiling (header-only)
 * 
 * When an instance is created, it remembers the system time. When it is
 * destroyed, it calculates the elapsed time since its creation, and
 * will print information to std::cout (if specified at creation).
 * ====================================================================
 * 
 * Example usage in code:
 * 
 * ####################################################################
 * {
 *   // some scope begins                                    \     \
 *   Timer::Timer st(true, "this scope");                    |     |
 *                                                           |     |
 *   // some code takes 30ms to run                          |30ms |
 *   some30millisecondfunction();                            |     |
 *                                                           |     |
 *   // print intermediate timing information                |     |
 *   st.Mark("something in between");                        /     |
 *                                                                 |
 *   some30millisecondfunction();                            \     |120ms
 *   some30millisecondfunction();                            |     |
 *                                                           |60ms |
 *   // print intermediate timing information                |     |
 *   st.Mark("something2");                                  /     |
 *                                                                 |
 *   some30millisecondfunction();                            \     |
 *                                                           |30ms |
 *   // when st's scope ends, its destructor is called and   |     |
 *   // prints the total elapsed time since construction     /     /
 * }
 * ####################################################################
 * 
 * This code will print (without '>>>'):
 * >>> Time mark: something in between 30.0 ms.
 * >>> Time mark: something2 60.0 ms.
 * >>> Timing information: calculation within scope omega 120.0 ms.
 * ====================================================================
 */

#ifndef TIMER_H__
#define TIMER_H__


/// System/STL
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>


namespace Timer {

  /// Typedefinitions
  typedef std::chrono::steady_clock::time_point TIME_POINT_T;
  typedef std::chrono::duration<long, std::nano> TIME_DURATION_T;


  /// /////////////////////////////////////////////////////////////////
  /// Non-class functions
  /// /////////////////////////////////////////////////////////////////
  
  /** 
   * Get current time point
   *
   * TODO
   * @returns
   */
  static TIME_POINT_T Now()
  {
    return std::chrono::steady_clock::now();
  }
  
  /**
   * Compute elapsed nanoseconds between two time points
   *
   * TODO
   * @param end
   * @param start
   *
   * @returns
   */
  static TIME_DURATION_T NanosecondsBetween(const TIME_POINT_T& end,
                                            const TIME_POINT_T& start 
                                           )
  {
    return std::chrono::duration_cast<TIME_DURATION_T>(end-start);
  }


  /// /////////////////////////////////////////////////////////////////
  /// Timer class (declarations)
  /// /////////////////////////////////////////////////////////////////
  class Timer {
    
  public:
    //////////////////////
    /// Public methods ///
    //////////////////////
    
    /**
     * Constructor
     * 
     * @param print If TRUE, the instance will print the elapsed time
     * @param msg Will be included into the message printed at destruction
     */
    Timer( 
          bool print=false,
          const std::string& msg="");
    
    /// Destructor
    ~Timer();

    /// Reset start time to NOW
    void Reset();

    /**
     * Get elapsed time (in nanoseconds) since construction or last Reset() call
     */
    float ElapsedNanoseconds();

    /**
     * Get elapsed time (in microseconds) since construction or last Reset() call
     */
    float ElapsedMicroseconds();

    /**
     * Get elapsed time (in milliseconds) since construction or last Reset() call
     */
    float ElapsedMilliseconds();

    /**
     * Get elapsed time in seconds since construction or the previous .Reset() call
     */
    float ElapsedSeconds();

    /**
     * Print a time mark (in milliseconds)
     *
     * @param msg A message text to prepend to the printed time
     */
    float Mark(
          const std::string& msg="");

    /// Get elapsed time (in ms) since construction or the previous .Mark() call
    float ElapsedMillisecondsSinceMark();

    
    
    
  private:
    ///////////////////////
    /// Private methods ///
    ///////////////////////
    
    /// Compute elapsed nanoseconds; m_diff has to be set
    float _ComputeElapsedNanoseconds();
    
    /// (Re)set m_start to NOW
    void _START();
    
    /// (Re)set m_mark_A to m_mark_B and (re)set m_mark_B to NOW
    void _MARK();

    /// (Re)set m_end to NOW
    void _END();
    
    /// Compute elapsed time ---FROM A TO B--- and write result into m_diff
    void _DIFF(
          const TIME_POINT_T& a, 
          const TIME_POINT_T& b);


    //////////////////////
    /// Private fields ///
    //////////////////////
    bool m_print;
    std::string m_msg;
    TIME_POINT_T m_start; 
    TIME_POINT_T m_mark_A; 
    TIME_POINT_T m_mark_B; 
    TIME_POINT_T m_end; 
    TIME_DURATION_T m_diff;
  };


  
    
  ////////////////////////////////////
  /// Timer class (implementation) ///
  ////////////////////////////////////
    
  /**
   * Constructor
   * 
   * @param print If TRUE, the instance will print the elapsed time
   * @param msg Will be included into the message printed at destruction
   */
  inline Timer::Timer(bool print, 
                      const std::string& msg) 
  : m_print(print),
    m_msg(msg)
  {
    Reset();
  };
  
  /// Destructor
  inline Timer::~Timer()
  {
    if (m_print) {
      _END();
      _DIFF(m_start, m_end);
      std::cout << "Timing information: "
                << m_msg << (m_msg.compare("") == 0 ? "" : " ")
                << _ComputeElapsedNanoseconds()/1e6
                << " ms.\n";
    }
  }

  /// Reset start time to NOW
  inline void Timer::Reset()
  {
    _START();
    _MARK();
  }


  /**
   * Get elapsed time (in milliseconds) since construction or last Reset() call
   */
  inline float Timer::ElapsedNanoseconds()
  {
    _END();
    _DIFF(m_start, m_end);
    return _ComputeElapsedNanoseconds();
  }

  inline float Timer::ElapsedMicroseconds()
  {
    return ElapsedNanoseconds() / 1e3;
  }

  inline float Timer::ElapsedMilliseconds()
  {
    return ElapsedNanoseconds() / 1e6;
  }

  /**
   * Get elapsed time in seconds since construction or the previous .Reset() call
   */
  inline float Timer::ElapsedSeconds()
  {
    return ElapsedNanoseconds() / 1e9;
  }


  /**
   * Print a time mark (in milliseconds)
   *
   * @param msg A message text to prepend to the printed time
   */
  inline float Timer::Mark(const std::string& msg)
  {
    float elapsed = ElapsedMillisecondsSinceMark();
    if (m_print) {
      std::cout << "Time mark: "
                << msg << (msg.compare("") == 0 ? "" : " ")
                << elapsed
                << " ms.\n";
    }
    return elapsed;
  }

  /// Get elapsed time (in ms) since construction or the previous .Mark() call
  inline float Timer::ElapsedMillisecondsSinceMark()
  {
    _MARK();
    _DIFF(m_mark_A, m_mark_B);
    return _ComputeElapsedNanoseconds() / 1e6;
  }

  
  /// Yield elapsed nanoseconds; m_diff has to be set
  inline float Timer::_ComputeElapsedNanoseconds()
  {
    return (float)m_diff.count();
  }
  

  /// (Re)set m_start to NOW
  inline void Timer::_START()
  {
    m_start = Now();
  }
  
  /// (Re)set m_mark_A to m_mark_B and (re)set m_mark_B to NOW
  inline void Timer::_MARK()
  {
    m_mark_A = m_mark_B;
    m_mark_B = Now();
  }
  
  /// (Re)set m_end to NOW
  inline void Timer::_END()
  {
    m_end = Now();
  }
  
  /// Compute elapsed time ---FROM A TO B--- and write result into m_diff
  inline void Timer::_DIFF(const TIME_POINT_T& a, 
                           const TIME_POINT_T& b)
  {
    m_diff = NanosecondsBetween(b, a);
  }




}  // namespace Timer


#endif  // TIMER_H__

