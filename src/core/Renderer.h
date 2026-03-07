#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Texture.h"
#include "Shader.h"

enum Type
{
    CUBE, SCREEN
};

class Renderer
{
public:
    // Constructor (inits shaders/shapes)
    Renderer(const Shader& shader, Type t);
    // Destructor
    ~Renderer();
    // Renders a defined quad textured with given sprite
    void DrawUI();
    void DrawScreen(Texture2D texture1);
    void DrawObject(Texture2D texture1, Texture2D texture2, glm::vec3 position);

private:
    // Render state
    Shader shader;
    GLuint VAO;
    Type type;
    // Initializes and configures the quad's buffer and vertex attributes
    void initRenderData();
};

#endif