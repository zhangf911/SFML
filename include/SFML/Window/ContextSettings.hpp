////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2014 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef SFML_CONTEXTSETTINGS_HPP
#define SFML_CONTEXTSETTINGS_HPP


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Structure defining the settings of the OpenGL
///        context attached to a window
///
////////////////////////////////////////////////////////////
struct ContextSettings
{
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// \param depth         Depth buffer bits
    /// \param stencil       Stencil buffer bits
    /// \param antialiasing  Antialiasing level
    /// \param major         Major number of the context version
    /// \param minor         Minor number of the context version
    /// \param compatibility Compatibility context flag
    /// \param debug         Debug context flag
    ///
    ////////////////////////////////////////////////////////////
    explicit ContextSettings(unsigned int depth = 0, unsigned int stencil = 0, unsigned int antialiasing = 0, unsigned int major = 2, unsigned int minor = 1, bool compatibility = true, bool debug = false) :
    depthBits        (depth),
    stencilBits      (stencil),
    antialiasingLevel(antialiasing),
    majorVersion     (major),
    minorVersion     (minor),
    compatibilityFlag(compatibility),
    debugFlag        (debug)
    {
    }

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned int depthBits;         ///< Bits of the depth buffer
    unsigned int stencilBits;       ///< Bits of the stencil buffer
    unsigned int antialiasingLevel; ///< Level of antialiasing
    unsigned int majorVersion;      ///< Major number of the context version to create
    unsigned int minorVersion;      ///< Minor number of the context version to create
    bool         compatibilityFlag; ///< Whether this context is a compatibility context
    bool         debugFlag;         ///< Whether this context has debugging enabled
};

} // namespace sf


#endif // SFML_CONTEXTSETTINGS_HPP


////////////////////////////////////////////////////////////
/// \class sf::ContextSettings
/// \ingroup window
///
/// ContextSettings allows to define several advanced settings
/// of the OpenGL context attached to a window. All these
/// settings with the exception of the compatibility flag
/// and anti-aliasing level have no impact on the regular
/// SFML rendering (graphics module), so you may need to use
/// this structure only if you're using SFML as a windowing
/// system for custom OpenGL rendering.
///
/// The depthBits and stencilBits members define the number
/// of bits per pixel requested for the (respectively) depth
/// and stencil buffers.
///
/// antialiasingLevel represents the requested number of
/// multisampling levels for anti-aliasing.
///
/// majorVersion and minorVersion define the version of the
/// OpenGL context that you want. Only versions greater or
/// equal to 3.0 are relevant; versions lesser than 3.0 are
/// all handled the same way (i.e. you can use any version
/// < 3.0 if you don't want an OpenGL 3 context).
///
/// When requesting a context with a version greater or equal
/// to 3.2, you have the option of specifying whether the
/// context should follow the core or compatibility profile
/// of all newer (>= 3.2) OpenGL specifications. For versions
/// 3.0 and 3.1 there is only the core profile. The default
/// value of compatibility is true. You only need to set it
/// to false if you want a core profile context to use with
/// your own OpenGL rendering.
/// <b>Warning: The graphics module will not function if you
/// request a core profile context. Make sure it is set to false
/// (the default) if you want to use the graphics module.</b>
///
/// Setting the debug flag to true will request a context with
/// additional debugging features enabled. Depending on the
/// system, this might be required for advanced OpenGL debugging.
/// OpenGL debugging is disabled by default.
///
/// Please note that these values are only a hint.
/// No failure will be reported if one or more of these values
/// are not supported by the system; instead, SFML will try to
/// find the closest valid match. You can then retrieve the
/// settings that the window actually used to create its context,
/// with Window::getSettings().
///
////////////////////////////////////////////////////////////
