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


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/System/InputStream.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <fstream>
#include <vector>


#ifndef SFML_OPENGL_ES

#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)

    #define castToGlHandle(x) reinterpret_cast<GLEXT_GLhandle>(static_cast<ptrdiff_t>(x))
    #define castFromGlHandle(x) static_cast<unsigned int>(reinterpret_cast<ptrdiff_t>(x))

#else

    #define castToGlHandle(x) (x)
    #define castFromGlHandle(x) (x)

#endif

namespace
{
    sf::Mutex mutex;

    bool vertexShaderAvailable = false;
    bool fragmentShaderAvailable = false;
    bool geometryShaderAvailable = false;

    GLint checkMaxTextureUnits()
    {
        GLint maxUnits = 0;
        glCheck(glGetIntegerv(GLEXT_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits));

        return maxUnits;
    }

    // Retrieve the maximum number of texture units available
    GLint getMaxTextureUnits()
    {
        // TODO: Remove this lock when it becomes unnecessary in C++11
        sf::Lock lock(mutex);

        static GLint maxUnits = checkMaxTextureUnits();

        return maxUnits;
    }

    // Read the contents of a file into an array of char
    bool getFileContents(const std::string& filename, std::vector<char>& buffer)
    {
        std::ifstream file(filename.c_str(), std::ios_base::binary);
        if (file)
        {
            file.seekg(0, std::ios_base::end);
            std::streamsize size = file.tellg();
            if (size > 0)
            {
                file.seekg(0, std::ios_base::beg);
                buffer.resize(static_cast<std::size_t>(size));
                file.read(&buffer[0], size);
            }
            buffer.push_back('\0');
            return true;
        }
        else
        {
            return false;
        }
    }

    // Read the contents of a stream into an array of char
    bool getStreamContents(sf::InputStream& stream, std::vector<char>& buffer)
    {
        bool success = true;
        sf::Int64 size = stream.getSize();
        if (size > 0)
        {
            buffer.resize(static_cast<std::size_t>(size));
            stream.seek(0);
            sf::Int64 read = stream.read(&buffer[0], size);
            success = (read == size);
        }
        buffer.push_back('\0');
        return success;
    }

    bool checkShadersAvailable()
    {
        // Create a temporary context in case the user checks
        // before a GlResource is created, thus initializing
        // the shared context
        sf::Context context;

        // Make sure that extensions are initialized
        sf::priv::ensureExtensionsInit();

        bool available = GLEXT_multitexture         &&
                         GLEXT_shading_language_100 &&
                         GLEXT_shader_objects       &&
                         GLEXT_vertex_shader        &&
                         GLEXT_fragment_shader;

        vertexShaderAvailable = available;
        fragmentShaderAvailable = available;
        geometryShaderAvailable = GLEXT_geometry_shader4;

        return available;
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
Shader::CurrentTextureType Shader::CurrentTexture;


////////////////////////////////////////////////////////////
Shader::Shader() :
m_shaderProgram (0),
m_currentTexture(-1),
m_textures      (),
m_params        ()
{
    // Make sure that extensions are initialized
    priv::ensureExtensionsInit();
}


////////////////////////////////////////////////////////////
Shader::~Shader()
{
    ensureGlContext();

    // Destroy effect program
    if (m_shaderProgram)
        glCheck(GLEXT_glDeleteObject(castToGlHandle(m_shaderProgram)));
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& filename, Type type)
{
    // Read the file
    std::vector<char> shader;
    if (!getFileContents(filename, shader))
    {
        err() << "Failed to open shader file \"" << filename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    bool result = false;
    switch(type)
    {
    case Vertex:
        result = compile(&shader[0], NULL, NULL);
        break;
        
    case Geometry:
        result = compile(NULL, &shader[0], NULL);
        break;
        
    case Fragment:
        result = compile(NULL, NULL, &shader[0]);
        break;
    }
    
    return result;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& vertexShaderFilename, const std::string& geometryShaderFilename, const std::string& fragmentShaderFilename)
{
    // Read the vertex shader file if given
    std::vector<char> vertexShader;
    bool useVertexShader = !vertexShaderFilename.empty();
    if (useVertexShader)
    {
        if (!getFileContents(vertexShaderFilename, vertexShader))
        {
            err() << "Failed to open vertex shader file \"" << vertexShaderFilename << "\"" << std::endl;
            return false;
        }
    }
    
    // Read the geometry shader file if given
    std::vector<char> geometryShader;
    bool useGeometryShader = !geometryShaderFilename.empty();
    if (useGeometryShader)
    {
        if (!getFileContents(geometryShaderFilename, geometryShader))
        {
            err() << "Failed to open geometry shader file \"" << geometryShaderFilename << "\"" << std::endl;
            return false;
        }
    }

    // Read the fragment shader file if given
    std::vector<char> fragmentShader;
    bool useFragmentShader = !fragmentShaderFilename.empty();
    if (useFragmentShader)
    {
        if (!getFileContents(fragmentShaderFilename, fragmentShader))
        {
            err() << "Failed to open fragment shader file \"" << fragmentShaderFilename << "\"" << std::endl;
            return false;
        }
    }

    // Compile the shader program
    return compile(useVertexShader ? &vertexShader[0] : NULL,
                   useGeometryShader ? &geometryShader[0] : NULL,
                   useFragmentShader ? &fragmentShader[0] : NULL);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& shader, Type type)
{
    // Compile the shader program
    bool result = false;
    switch(type)
    {
    case Vertex:
        result = compile(shader.c_str(), NULL, NULL);
        break;
        
    case Geometry:
        result = compile(NULL, shader.c_str(), NULL);
        break;
        
    case Fragment:
        result = compile(NULL, NULL, shader.c_str());
        break;
    }
    
    return result;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& vertexShader, const std::string& geometryShader, const std::string& fragmentShader)
{
    // Check which shaders are given
    bool useVertexShader = !vertexShader.empty();
    bool useGeometryShader = !geometryShader.empty();
    bool useFragmentShader = !fragmentShader.empty();

    // Compile the shader program
    return compile(useVertexShader ? vertexShader.c_str() : NULL,
                   useGeometryShader ? geometryShader.c_str() : NULL,
                   useFragmentShader ? fragmentShader.c_str() : NULL);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& stream, Type type)
{
    // Read the shader code from the stream
    std::vector<char> shader;
    if (!getStreamContents(stream, shader))
    {
        err() << "Failed to read shader from stream" << std::endl;
        return false;
    }

    // Compile the shader program
    bool result = false;
    switch(type)
    {
    case Vertex:
        result = compile(&shader[0], NULL, NULL);
        break;
        
    case Geometry:
        result = compile(NULL, &shader[0], NULL);
        break;
        
    case Fragment:
        result = compile(NULL, NULL, &shader[0]);
        break;
    }
    
    return result;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& geometryShaderStream, InputStream& fragmentShaderStream)
{    
    // Read the vertex shader code from the stream if given
    std::vector<char> vertexShader;
    bool useVertexShader = vertexShaderStream.getSize() > 0;
    if (useVertexShader)
    {
        if (!getStreamContents(vertexShaderStream, vertexShader))
        {
            err() << "Failed to read vertex shader from stream" << std::endl;
            return false;
        }
    }
    
    // Read the geometry shader code from the stream if given
    std::vector<char> geometryShader;
    bool useGeometryShader = geometryShaderStream.getSize() > 0;
    if (useGeometryShader)
    {
        if (!getStreamContents(geometryShaderStream, geometryShader))
        {
            err() << "Failed to read geometry shader from stream" << std::endl;
            return false;
        }
    }

    // Read the fragment shader code from the stream if given
    std::vector<char> fragmentShader;
    bool useFragmentShader = fragmentShaderStream.getSize() > 0;
    if (useFragmentShader)
    {
        if (!getStreamContents(fragmentShaderStream, fragmentShader))
        {
            err() << "Failed to read fragment shader from stream" << std::endl;
            return false;
        }
    }

    // Compile the shader program
    return compile(useVertexShader ? &vertexShader[0] : NULL,
                   useGeometryShader ? &geometryShader[0] : NULL,
                   useFragmentShader ? &fragmentShader[0] : NULL);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Enable program
        GLEXT_GLhandle program = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

        // Get parameter location and assign it new values
        GLint location = getParamLocation(name);
        if (location != -1)
        {
            glCheck(GLEXT_glUniform1f(location, x));
        }

        // Disable program
        glCheck(GLEXT_glUseProgramObject(program));
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Enable program
        GLEXT_GLhandle program = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

        // Get parameter location and assign it new values
        GLint location = getParamLocation(name);
        if (location != -1)
        {
            glCheck(GLEXT_glUniform2f(location, x, y));
        }

        // Disable program
        glCheck(GLEXT_glUseProgramObject(program));
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Enable program
        GLEXT_GLhandle program = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

        // Get parameter location and assign it new values
        GLint location = getParamLocation(name);
        if (location != -1)
        {
            glCheck(GLEXT_glUniform3f(location, x, y, z));
        }

        // Disable program
        glCheck(GLEXT_glUseProgramObject(program));
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z, float w)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Enable program
        GLEXT_GLhandle program = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

        // Get parameter location and assign it new values
        GLint location = getParamLocation(name);
        if (location != -1)
        {
            glCheck(GLEXT_glUniform4f(location, x, y, z, w));
        }

        // Disable program
        glCheck(GLEXT_glUseProgramObject(program));
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector2f& v)
{
    setParameter(name, v.x, v.y);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector3f& v)
{
    setParameter(name, v.x, v.y, v.z);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Color& color)
{
    setParameter(name, color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const sf::Transform& transform)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Enable program
        GLEXT_GLhandle program = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

        // Get parameter location and assign it new values
        GLint location = getParamLocation(name);
        if (location != -1)
        {
            glCheck(GLEXT_glUniformMatrix4fv(location, 1, GL_FALSE, transform.getMatrix()));
        }

        // Disable program
        glCheck(GLEXT_glUseProgramObject(program));
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Texture& texture)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Find the location of the variable in the shader
        int location = getParamLocation(name);
        if (location != -1)
        {
            // Store the location -> texture mapping
            TextureTable::iterator it = m_textures.find(location);
            if (it == m_textures.end())
            {
                // New entry, make sure there are enough texture units
                GLint maxUnits = getMaxTextureUnits();
                if (m_textures.size() + 1 >= static_cast<std::size_t>(maxUnits))
                {
                    err() << "Impossible to use texture \"" << name << "\" for shader: all available texture units are used" << std::endl;
                    return;
                }

                m_textures[location] = &texture;
            }
            else
            {
                // Location already used, just replace the texture
                it->second = &texture;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, CurrentTextureType)
{
    if (m_shaderProgram)
    {
        ensureGlContext();

        // Find the location of the variable in the shader
        m_currentTexture = getParamLocation(name);
    }
}


////////////////////////////////////////////////////////////
unsigned int Shader::getNativeHandle() const
{
    return m_shaderProgram;
}


////////////////////////////////////////////////////////////
void Shader::bind(const Shader* shader)
{
    ensureGlContext();

    // Make sure that we can use shaders
    if (!isAvailable())
    {
        err() << "Failed to bind or unbind shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable() before trying to use the Shader class)" << std::endl;
        return;
    }

    if (shader && shader->m_shaderProgram)
    {
        // Enable the program
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(shader->m_shaderProgram)));

        // Bind the textures
        shader->bindTextures();

        // Bind the current texture
        if (shader->m_currentTexture != -1)
            glCheck(GLEXT_glUniform1i(shader->m_currentTexture, 0));
    }
    else
    {
        // Bind no shader
        glCheck(GLEXT_glUseProgramObject(0));
    }
}


////////////////////////////////////////////////////////////
bool Shader::isAvailable(int shaderTypes)
{
    // TODO: Remove this lock when it becomes unnecessary in C++11
    Lock lock(mutex);

    static bool available = checkShadersAvailable();


    if ((shaderTypes & Vertex) && !vertexShaderAvailable)
        return false;

    if ((shaderTypes & Fragment) && !fragmentShaderAvailable)
        return false;

    if ((shaderTypes & Geometry) && !geometryShaderAvailable)
        return false;

    return true;
}


////////////////////////////////////////////////////////////
bool Shader::compile(const char* vertexShaderCode, const char* geometryShaderCode, const char* fragmentShaderCode)
{
    ensureGlContext();

    // First make sure that we can use shaders
    int shadersToUse =     (vertexShaderCode ? Vertex : 0)      |
                        (geometryShaderCode ? Geometry : 0)  |
                        (fragmentShaderCode ? Fragment : 0);
    if (!isAvailable(shadersToUse))
    {
        err() << "Failed to create a shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable(shaderTypes) before trying to use the Shader class)" << std::endl;
        return false;
    }

    // Destroy the shader if it was already created
    if (m_shaderProgram)
    {
        glCheck(GLEXT_glDeleteObject(castToGlHandle(m_shaderProgram)));
        m_shaderProgram = 0;
    }

    // Reset the internal state
    m_currentTexture = -1;
    m_textures.clear();
    m_params.clear();

    // Create the program
    GLEXT_GLhandle shaderProgram = glCheck(GLEXT_glCreateProgramObject());

    // Create the vertex shader if needed
    if (vertexShaderCode == NULL && geometryShaderCode)
        vertexShaderCode = getDefaultVertexShaderCode().c_str();
    if (vertexShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle vertexShader = glCheck(GLEXT_glCreateShaderObject(GLEXT_GL_VERTEX_SHADER));
        glCheck(GLEXT_glShaderSource(vertexShader, 1, &vertexShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(vertexShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetObjectParameteriv(vertexShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetInfoLog(vertexShader, sizeof(log), 0, log));
            err() << "Failed to compile vertex shader:" << std::endl
                  << log << std::endl;
            glCheck(GLEXT_glDeleteObject(vertexShader));
            glCheck(GLEXT_glDeleteObject(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachObject(shaderProgram, vertexShader));
        glCheck(GLEXT_glDeleteObject(vertexShader));
    }
    
    // Create the geometry shader if needed
    if (geometryShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle geometryShader = GLEXT_glCreateShaderObject(GLEXT_GL_GEOMETRY_SHADER);
        glCheck(GLEXT_glShaderSource(geometryShader, 1, &geometryShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(geometryShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetObjectParameteriv(geometryShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetInfoLog(geometryShader, sizeof(log), 0, log));
            err() << "Failed to compile geometry shader:" << std::endl
                  << log << std::endl;
            glCheck(GLEXT_glDeleteObject(geometryShader));
            glCheck(GLEXT_glDeleteObject(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachObject(shaderProgram, geometryShader));
        glCheck(GLEXT_glDeleteObject(geometryShader));
    }

    // Create the fragment shader if needed
    if (fragmentShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle fragmentShader = glCheck(GLEXT_glCreateShaderObject(GLEXT_GL_FRAGMENT_SHADER));
        glCheck(GLEXT_glShaderSource(fragmentShader, 1, &fragmentShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(fragmentShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetObjectParameteriv(fragmentShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetInfoLog(fragmentShader, sizeof(log), 0, log));
            err() << "Failed to compile fragment shader:" << std::endl
                  << log << std::endl;
            glCheck(GLEXT_glDeleteObject(fragmentShader));
            glCheck(GLEXT_glDeleteObject(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachObject(shaderProgram, fragmentShader));
        glCheck(GLEXT_glDeleteObject(fragmentShader));
    }

    // Link the program
    glCheck(GLEXT_glLinkProgram(shaderProgram));

    // Check the link log
    GLint success;
    glCheck(GLEXT_glGetObjectParameteriv(shaderProgram, GLEXT_GL_OBJECT_LINK_STATUS, &success));
    if (success == GL_FALSE)
    {
        char log[1024];
        glCheck(GLEXT_glGetInfoLog(shaderProgram, sizeof(log), 0, log));
        err() << "Failed to link shader:" << std::endl
              << log << std::endl;
        glCheck(GLEXT_glDeleteObject(shaderProgram));
        return false;
    }

    m_shaderProgram = castFromGlHandle(shaderProgram);

    // Force an OpenGL flush, so that the shader will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    return true;
}


////////////////////////////////////////////////////////////
void Shader::bindTextures() const
{
    TextureTable::const_iterator it = m_textures.begin();
    for (std::size_t i = 0; i < m_textures.size(); ++i)
    {
        GLint index = static_cast<GLsizei>(i + 1);
        glCheck(GLEXT_glUniform1i(it->first, index));
        glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0 + index));
        Texture::bind(it->second);
        ++it;
    }

    // Make sure that the texture unit which is left active is the number 0
    glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
}


////////////////////////////////////////////////////////////
int Shader::getParamLocation(const std::string& name)
{
    // Check the cache
    ParamTable::const_iterator it = m_params.find(name);
    if (it != m_params.end())
    {
        // Already in cache, return it
        return it->second;
    }
    else
    {
        // Not in cache, request the location from OpenGL
        int location = GLEXT_glGetUniformLocation(castToGlHandle(m_shaderProgram), name.c_str());
        m_params.insert(std::make_pair(name, location));

        if (location == -1)
            err() << "Parameter \"" << name << "\" not found in shader" << std::endl;

        return location;
    }
}

} // namespace sf

#else // SFML_OPENGL_ES

// OpenGL ES 1 doesn't support GLSL shaders at all, we have to provide an empty implementation

namespace sf
{
////////////////////////////////////////////////////////////
Shader::CurrentTextureType Shader::CurrentTexture;


////////////////////////////////////////////////////////////
Shader::Shader() :
m_shaderProgram (0),
m_currentTexture(-1)
{
}


////////////////////////////////////////////////////////////
Shader::~Shader()
{
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& filename, Type type)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& vertexShaderFilename, const std::string& fragmentShaderFilename)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& shader, Type type)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& vertexShader, const std::string& fragmentShader)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& stream, Type type)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& fragmentShaderStream)
{
    return false;
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z, float w)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector2f& v)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector3f& v)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Color& color)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const sf::Transform& transform)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Texture& texture)
{
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, CurrentTextureType)
{
}


////////////////////////////////////////////////////////////
void Shader::bind(const Shader* shader)
{
}


////////////////////////////////////////////////////////////
bool Shader::isAvailable()
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::compile(const char* vertexShaderCode, const char* fragmentShaderCode)
{
    return false;
}


////////////////////////////////////////////////////////////
void Shader::bindTextures() const
{
}

} // namespace sf

#endif // SFML_OPENGL_ES
