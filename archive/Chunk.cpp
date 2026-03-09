#include "Chunk.h"

#include <fstream>
#include <sstream>


void Chunk::Load(const GLchar* file)
{
    // Clear old data
    this->Cubes.clear();
    // Load from file
    GLuint height;
    std::string line;
    std::ifstream fstream(file);
    std::vector<std::vector<GLuint>> heightData;
    if (fstream)
    {
        while (std::getline(fstream, line)) // Read each line from chunk file
        {
            std::istringstream sstream(line);
            std::vector<GLuint> row;
            while (sstream >> height) // Read each word seperated by spaces
                row.push_back(height);
            heightData.push_back(row);


        }
        if (heightData.size() > 0)
            this->Generate(heightData);
    }
}

void Chunk::Create(float persistence, int octave)
{
    this->Cubes.clear();
    Noise perlin(persistence, octave);
    for (unsigned int x = 0; x < X; x++)
    {
        for (unsigned int z = 0; z < Z; z++)
        {
            for (unsigned int y = 0; y < perlin.PerlinNoise(x, z) * 5 + 5; y++)
            {
                glm::vec3 pos(1.0f * x, 1.0f * y, 1.0f * z);
                GameObject obj(pos, glm::vec3(1.0f), ResourceManager::GetTexture("box"), ResourceManager::GetTexture("box2"));
                obj.IsSolid = GL_TRUE;
                this->Cubes.push_back(obj);
            }
        }
    }
}

void Chunk::Draw(Renderer& renderer)
{
    for (GameObject& tile : this->Cubes)
        if (!tile.Destroyed)
        {
            if (!tile.Selected)
                tile.Draw(renderer);
        }
}

void Chunk::Generate(std::vector<std::vector<GLuint>> heightData)
{
    // Calculate dimensions
    GLuint ZAxis = heightData.size();
    GLuint XAxis = heightData[0].size();
    // Initialize level tiles based on tileData
    for (GLuint x = 0; x < XAxis; ++x)
    {
        for (GLuint z = 0; z < ZAxis; ++z)
        {
            for (GLuint y = 0; y < heightData[z][x]; ++y)
            {
                glm::vec3 pos(1.0f * x, 1.0f * y, 1.0f * z);
                GameObject obj(pos, glm::vec3(1.0f), ResourceManager::GetTexture("box"), ResourceManager::GetTexture("box2"));
                obj.IsSolid = GL_TRUE;
                this->Cubes.push_back(obj);
            }
        }
    }
}