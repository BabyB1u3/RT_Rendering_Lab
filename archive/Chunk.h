#ifndef CHUNK_H
#define CHUNK_H
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "GameObject.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Noise.h"


// Chunk holds all Tiles as part of a world map and 
// hosts functionality to Load/render chunks from the harddisk.
class Chunk
{
public:
    // chunk state
    std::vector<GameObject> Cubes;
    const GLuint X = 16;
    const GLuint Z = 16;
    // Constructor
    Chunk() { }
    // Loads chunk from file
    void      Load(const GLchar* file);
    // Creates chunk with noise
    void      Create(float persistence, int octave);
    // Render chunk
    void      Draw(Renderer& renderer);
private:
    // Initialize Chunks from tile data
    void      Generate(std::vector<std::vector<GLuint>> heightData);
};

#endif