/** 
 * ===================================================================
 *
 * Author: Nikolaus Mayer, 2015 (mayern@cs.uni-freiburg.de)
 *
 * TextDecorator
 *
 * Print colorized and styled text to terminals (if compatible)
 *
 * ===================================================================
 *
 * Usage example:
 *
 * >
 * > #include <iostream>
 * > #include <string>
 * > #include "TextDecorator.h"
 * >
 * > int main(int argc, char** argv)
 * > {
 * >   TextDecorator::TextDecorator TD(true);  // We want output, but no debug
 * >
 * >   std::cout << TD.red("Here's some red text, ")
 * >             << TD.bold("this text is bold, ")
 * >             << TD.decorate("and this is HYPER", TextDecorator::Blue|
 * >                                                 TextDecorator::Bold|
 * >                                                 TextDecorator::Inverse)
 * >             << '\n';
 * >
 * >   return 0;
 * > }
 * >
 * 
 *
 * ===================================================================
 */


#ifndef TEXTDECORATOR_H__
#define TEXTDECORATOR_H__


#include <iostream>
#include <map>       // std::map
#include <string>    // std::string
#include <sstream>   // std::ostringstream


namespace TextDecorator {


  /// User-exposed formatting codes
  enum Value
  {
    Default    = 0,
    Red        = 1<<0, 
    Green      = 1<<1, 
    Blue       = 1<<2, 
    Black      = 1<<3, 
    Bold       = 1<<4,
    Underline  = 1<<5,
    Inverse    = 1<<6
  };


  class TextDecorator
  {
    
  public:
    
    /// Constructor
    TextDecorator( bool action=true,
                   bool debug=false
                 )
      : m_action(action),
        m_debug(debug),
        m_SGR_map({{Default,    0},
                   {Red,       31},
                   {Green,     32},
                   {Blue,      34},
                   {Black,     30},
                   {Bold,       1},
                   {Underline,  4},
                   {Inverse,    7}}),
        m_SGR_names_map({{Default,   "(reset)"  },
                         {Red,       "red"      },
                         {Green,     "green"    },
                         {Blue,      "blue"     },
                         {Black,     "black"    },
                         {Bold,      "bold"     },
                         {Underline, "underline"},
                         {Inverse,   "inverse"  }})

    { 
      if ( m_debug )
        std::cout << "TextDecorator: DEBUG INFORMATION:\n"
                  << "  " << Default   << " = Default"   << "\n"
                  << "  " << Red       << " = Red"       << "\n"
                  << "  " << Green     << " = Green"     << "\n"
                  << "  " << Blue      << " = Blue"      << "\n"
                  << "  " << Black     << " = Black"     << "\n"
                  << "  " << Bold      << " = Bold"      << "\n"
                  << "  " << Underline << " = Underline" << "\n"
                  << "  " << Inverse   << " = Inverse"   << "\n";
    }
    
    
    /**
     * DEPRECATED DEPRECATED DEPRECATED DEPRECATED DEPRECATED DEPRECATED 
     * 
     * Legacy method: Decorate a string with color and face
     *
     * @param input The input. Can be any datatype that can be string-ified
     * @param color Red, Green, Blue, Black
     * @param face Bold, Default
     * @param override_action Iff TRUE, will do something even is m_action is FALSE
     *
     * @returns The "input" string, decorated with leading and trailing formatting code
     *
     * DEPRECATED DEPRECATED DEPRECATED DEPRECATED DEPRECATED DEPRECATED 
     */
    template <typename T>
    std::string decorate( const T& input,
                          Value color=Default,
                          Value face=Default,
                          bool override_action=false
                        ) const
    {
      return decorate(input, color+face, override_action);
    }


    /**
     * Decorate a string
     *
     * @param input The input. Can be any datatype that can be string-ified
     * @param format The formatting code
     * @param override_action Iff TRUE, will do something even is m_action is FALSE
     *
     * @returns The "input" string, decorated with leading and trailing formatting code
     */
    template <typename T>
    std::string decorate( const T& input,
                          unsigned int format=0,
                          bool override_action=false
                        ) const
    {

      if ( m_debug )
        std::cout << "TextDecorator: DEBUG INFORMATION: Codepoint <<"
                  << format
                  << ">>\n";

      std::ostringstream oss;
      
      if ( format == 0 or
           (!m_action and !override_action) ) {
        oss << input;
        return oss.str();
      }

      /**
       * Short tutorial on the ANSI escape codes used here:
       *
       * -----------------------------------------------------------------------
       *
       * Escape sequences begin with the ESC character. This is an unprintable
       * ASCII character, so it has to be directly encoded by its number. The
       * numeric code for ESC is 27 in decimal, or 0x1b in hexadecimal, and so
       * the first step is to output
       *    \x1b
       * into the stream. Following the ESC character, a "[" (opening square
       * bracket) is expected, called the CSI or "Control Sequence Introducer".
       * Luckily, this is a printable character:
       *    [
       * The stream is now ready to receive the actual formatting command(s).
       *
       * These commands are numbers. There may be zero or more of these SGR or
       * "Select Graphic Rendition" parameters. The ones used here include:
       *    1           (bold)
       *    4           (underline)
       *    7           (inverse display; swaps background and foreground colors)
       *    30-37       (text color; there are "standard" colors, but a terminal
       *                 emulator is free to redefine them so your TextDecorator
       *                 might not actually print "red" text using the "Red"
       *                 color option.)
       *    0 or empty  (reset; all attributes off)
       * If more than one is used, the individual options are separated by ";"
       * (semicolon). For example, the code for "red normal" text is:
       *    31
       * Combining multiple commands for "bold underlined green" text yields:
       *    1;4;32
       *
       * The code sequence is finalized by a single "m" which specifies that all
       * preceding codes should be interpreted as SGR parameters:
       *    m
       *
       *
       * After choosing a formatting using these commands, all following text
       * entered into the stream will be formatted accordingly. In the end,
       * the formatting should be reset to "nothing" by sending the special "all
       * attributes off" parameter (0 or empty).
       *
       * -----------------------------------------------------------------------
       *
       * EXAMPLE: Printing "Hello World" in bold red text:
       *  
       *       Start formatting╮       Text       ╭Reset formatting
       *                       │         │        │
       *                  ╭────┴───╮╭────┴────╮╭──┴─╮
       *    std::cout << "\x1b[1;31mHello World\x1b[m";
       *                  ╰┬─╯│││╰┤│           ╰┬─╯││
       *                   │  │││ ││            │  ││
       *                ESC╯  │││ ││           ESC │╰SGR terminator
       *                   CSI╯││ ││              CSI
       *                       ││ │╰SGR terminator
       *               SGR:bold╯│ ╰SGR:red
       *                        │
       *                    separator
       *
       * -----------------------------------------------------------------------
       *
       * See e.g. "ANSI escape code" on Wikipedia for much more information.
       */


      /// Count the number of set bits in "format". This is the number of active
      /// formatting options. (Algorithm by Brian Kernighan.)
      unsigned int active = 0;
      {
        unsigned int n = format;
        while ( n != 0 ) {
          n &= n-1;
          ++active;
        }
      }

      /// Begin formatting code (ESC + Command Sequence Introducer)
      oss << "\x1b[";
      
      /// Colors
      if (active>0) _ProcessSingleSGR(oss, format, Red,       active);
      if (active>0) _ProcessSingleSGR(oss, format, Green,     active);
      if (active>0) _ProcessSingleSGR(oss, format, Blue,      active);
      if (active>0) _ProcessSingleSGR(oss, format, Black,     active);
      /// Font faces
      if (active>0) _ProcessSingleSGR(oss, format, Bold,      active);
      if (active>0) _ProcessSingleSGR(oss, format, Underline, active);
      if (active>0) _ProcessSingleSGR(oss, format, Inverse,   active);

      /// Now feed the input and reset the formatting
      oss << "m" << input << "\x1b[m";

      return oss.str();
    }



    
    /** 
     * Predefined styles for warnings / errors 
     */
    std::string warning() const { return decorate("WARNING: ", Red|Bold); }
    std::string error()   const { return decorate("!!!ERROR!!!: ", Red|Bold|Inverse); }
    
    template <typename T>
    std::string warning( const T& input ) const { return decorate(input, Red|Bold); }
    template <typename T>
    std::string error(   const T& input ) const { return decorate(input, Red|Bold|Inverse); }
    
    /** 
     * Predefined styles for colors 
     */  
    template <typename T>
    std::string red(   const T& input ) const { return decorate(input, Red); }
    template <typename T>
    std::string green( const T& input ) const { return decorate(input, Green); }
    template <typename T>
    std::string blue(  const T& input ) const { return decorate(input, Blue); }
    template <typename T>
    std::string black( const T& input ) const { return decorate(input, Black); }

    /** 
     * Predefined styles for font faces 
     */
    template <typename T>
    std::string bold(      const T& input ) const { return decorate(input, Bold); }
    template <typename T>
    std::string underline( const T& input ) const { return decorate(input, Underline); }
    template <typename T>
    std::string inverse(   const T& input ) const { return decorate(input, Inverse); }


    /**
     * Set private attributes
     */
    void setAction(bool action) { m_action = action; }
    void setDebug (bool debug ) { m_debug  = debug; }

    
  private:

    bool m_action;
    bool m_debug;
    
    const std::map<unsigned int, unsigned int> m_SGR_map;
    const std::map<unsigned int, std::string> m_SGR_names_map;


    /**
     * Check if a given SGR is active in a formatting code, and if yes,
     * feed the corresponding commands into the output stream. Decrease
     * the "active" SGRs counter.
     *
     * @param oss Output stringstream
     * @param codepoint Number encoding all active SGRs
     * @param SGR The SGR to check
     * @param active The number of activated SGR options
     */
    void _ProcessSingleSGR( std::ostringstream& oss,
                            unsigned int& codepoint,
                            unsigned int SGR,
                            unsigned int& active
                          ) const
    {
      /// Each SGR is encoded by a specific bit -> binary AND reveals matches
      if ( codepoint & SGR ) {
        oss << m_SGR_map.at(SGR);
        if ( active > 1 ) oss << ";";

        /// SGR parameter has been processed; delete if from the formatting code
        codepoint ^= SGR;
        --active; 

        if ( m_debug )
          std::cout << "TextDecorator: DEBUG INFORMATION: SGR <<"
                    << SGR << ">> (" << m_SGR_names_map.at(SGR)
                    << ") is active.\n";
      }
    }
    
    
  };


}  // namespace TextDecorator




#endif  // TEXTDECORATOR_H__

