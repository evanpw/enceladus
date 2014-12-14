#ifndef PLATFORM_HPP
#define PLATFORM_HPP

// We have to add an underscore to the name of C functions on OSX in order to
// link correctly
#ifdef __APPLE__
    #define FOREIGN_NAME(s) std::string("_") + std::string(s)
#else
    #define FOREIGN_NAME(s) std::string(s)
#endif

#endif
