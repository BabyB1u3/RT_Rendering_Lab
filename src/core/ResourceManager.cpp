
#include "ResourceManager.h"
// The standard C++ libs
#include <iostream>
#include <sstream>
#include <fstream>
// The third part libs
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Instantiate static variables
std::map<std::string, Texture2D>    ResourceManager::Textures;
std::map<std::string, Shader>       ResourceManager::Shaders;
//std::map<std::string, RenderBuffer> ResourceManager::Renderbuffers;


Shader ResourceManager::LoadShader(const GLchar* vShaderFile, const GLchar* fShaderFile, const GLchar* gShaderFile, std::string name)
{
    Shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
    return Shaders[name];
}

Shader ResourceManager::GetShader(std::string name)
{
    return Shaders[name];
}

Texture2D ResourceManager::LoadTexture(const GLchar* file, std::string name)
{
    Textures[name] = loadTextureFromFile(file);
    return Textures[name];
}

Texture2D ResourceManager::LoadTexture(GLuint width, GLuint height, GLenum format, std::string name)
{
    Textures[name] = CreateTextureWithoutFile(width, height, format);
    return Textures[name];
}

Texture2D ResourceManager::GetTexture(std::string name)
{
    return Textures[name];
}

//RenderBuffer ResourceManager::LoadRenderBuffer(GLuint width, GLuint height, GLboolean multi, std::string name)
//{
//    Renderbuffers[name] = loadRenderBuffer(width, height, multi);
//    return Renderbuffers[name];
//}

//RenderBuffer ResourceManager::GetRenderBuffer(std::string name)
//{
//    return Renderbuffers[name];
//}

void ResourceManager::Clear()
{
    // (Properly) delete all shaders	
    for (auto iter : Shaders)
        glDeleteProgram(iter.second.ID);
    // (Properly) delete all textures
    for (auto iter : Textures)
        glDeleteTextures(1, &iter.second.ID);
//    for (auto iter : Renderbuffers)
//        glDeleteRenderbuffers(1, &iter.second.ID);
}

Shader ResourceManager::loadShaderFromFile(const GLchar* vShaderFile, const GLchar* fShaderFile, const GLchar* gShaderFile)
{
    // 1. Retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::string geometryCode;
    try
    {
        // Open files
        std::ifstream vertexShaderFile(vShaderFile);
        std::ifstream fragmentShaderFile(fShaderFile);
        std::stringstream vShaderStream, fShaderStream;
        // Read file's buffer contents into streams
        vShaderStream << vertexShaderFile.rdbuf();
        fShaderStream << fragmentShaderFile.rdbuf();
        // close file handlers
        vertexShaderFile.close();
        fragmentShaderFile.close();
        // Convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
        // If geometry shader path is present, also load a geometry shader
        if (gShaderFile != nullptr)
        {
            std::ifstream geometryShaderFile(gShaderFile);
            std::stringstream gShaderStream;
            gShaderStream << geometryShaderFile.rdbuf();
            geometryShaderFile.close();
            geometryCode = gShaderStream.str();
        }
    }
    catch (std::exception e)
    {
        std::cout << "ERROR::SHADER: Failed to read shader files" << std::endl;
    }
    const GLchar* vShaderCode = vertexCode.c_str();
    const GLchar* fShaderCode = fragmentCode.c_str();
    const GLchar* gShaderCode = geometryCode.c_str();
    // 2. Now create shader object from source code
    Shader shader;
    shader.Compile(vShaderCode, fShaderCode, gShaderFile != nullptr ? gShaderCode : nullptr);
    return shader;
}

Texture2D ResourceManager::loadTextureFromFile(const GLchar* file)
{
    // Create Texture object
    Texture2D texture;
    // Load image
    int width, height, nrComponents;

    unsigned char* image = stbi_load(file, &width, &height, &nrComponents, 0);

    // Now generate texture
    if (image)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        texture.Generate(width, height, format, image);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << file << std::endl;
    }
    // And finally free image data
    stbi_image_free(image);
    return texture;
}

Texture2D ResourceManager::CreateTextureWithoutFile(GLuint width, GLuint height, GLenum format)
{
    // Create Texture object
    Texture2D texture;
    texture.Create(width, height, format);
    return texture;
}

//RenderBuffer ResourceManager::loadRenderBuffer(GLuint width, GLuint height, GLboolean multi)
//{
//    RenderBuffer RBO;
//    RBO.Generate(width, height, multi);
//    return RBO;
//}