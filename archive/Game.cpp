#include "Game.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "FrameBuffer.h"
#include "Text.h"
#include "Ray.h"
#include "Player.h"

// Game-related State data
Renderer* depth;
FrameBuffer* framebuffer;
FrameBuffer* depthframebuffer;
Renderer* cube;
Renderer* screen;
Player* player;
TextRenderer* text;
Chunk* one;

glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

Noise pln(0.5f, 3);

glm::vec3 SpawnPoint(0.0f,8.0f,0.0f);


Game::Game(GLuint width, GLuint height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{

}

Game::~Game()
{
    delete cube;
    delete screen;
    delete depth;
    delete player;
    delete framebuffer;
    delete depthframebuffer;
    delete text;
}

void Game::Init()
{
    // Load shaders
    ResourceManager::LoadShader("Resource/Shaders/cube.vertex", "Resource/Shaders/cube.fragment", nullptr, "cube");
    ResourceManager::LoadShader("Resource/Shaders/screen.vertex", "Resource/Shaders/screen.fragment", nullptr, "screen");
    ResourceManager::LoadShader("Resource/Shaders/depth.vertex", "Resource/Shaders/depth.fragment", nullptr, "depth");
    // Set render-specific controls
    cube = new Renderer(ResourceManager::GetShader("cube"), CUBE);
    screen = new Renderer(ResourceManager::GetShader("screen"), SCREEN);
    depth = new Renderer(ResourceManager::GetShader("depth"), CUBE);
    text = new TextRenderer(this->Width, this->Height);
    // Load fonts
    text->Load("Resource/Fonts/AGENCYR.TTF", 48);
    // Load textures
    ResourceManager::LoadTexture("Resource/Textures/block/deepslate_diamond_ore.png", "box");
    ResourceManager::LoadTexture("Resource/Textures/block/box2.png", "box2");
    ResourceManager::LoadTexture(this->Width, this->Height, GL_DEPTH_COMPONENT, "depthTexture");
    ResourceManager::LoadTexture(this->Width, this->Height, GL_RGB, "screenTexture");

    ResourceManager::GetShader("cube").Use().SetInteger("diffuseTexture", 0);
    ResourceManager::GetShader("cube").Use().SetInteger("specularTexture", 1);
    ResourceManager::GetShader("cube").Use().SetInteger("shadowMap", 2);
    ResourceManager::GetShader("screen").Use().SetInteger("screen", 0);
    // Configure shaders
    player = new Player(SpawnPoint);
    
    // frame buffer for depthmap
    depthframebuffer = new FrameBuffer();
    depthframebuffer->Configure(ResourceManager::GetTexture("depthTexture").ID, GL_DEPTH_ATTACHMENT, NULL);

    // framebuffer for screen
    framebuffer = new FrameBuffer();
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, this->Width, this->Height);
    framebuffer->Configure(ResourceManager::GetTexture("screenTexture").ID, GL_COLOR_ATTACHMENT0, rbo);

    // Load chunks
    one = new Chunk();
    one->Create(0.001f, 10);
    //one->Load("Resource/Levels/one.lvl");
    this->Chunks.push_back(*one);
    this->chunk = 0;
}

void Game::Update(GLfloat dt)
{
    //player->Position -= glm::vec3(0.0f, 0.0075f, 0.0f);

    glm::mat4 projection = glm::perspective(glm::radians(player->FirstPlayerCamera->Zoom), (float)800 / (float)600, 0.1f, 100.0f);
    ResourceManager::GetShader("cube").Use().SetMatrix4("projection", projection);
    glm::mat4 view = player->FirstPlayerCamera->GetViewMatrix(player->Position);
    ResourceManager::GetShader("lightShader").Use().SetVector3f("dirLight.direction", -0.2f, -1.0f, -0.3f);
    ResourceManager::GetShader("cube").Use().SetMatrix4("view", view);
    ResourceManager::GetShader("cube").Use().SetVector3f("viewPos", player->Position);
    ResourceManager::GetShader("cube").Use().SetVector3f("lightPos", lightPos);

    glm::mat4 lightProjection, lightView;
    glm::mat4 lightSpaceMatrix;
    float near_plane = 1.0f, far_plane = 7.5f;
    lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
    ResourceManager::GetShader("cube").Use().SetMatrix4("lightSpaceMatrix", lightSpaceMatrix);
    ResourceManager::GetShader("depth").Use().SetMatrix4("lightSpaceMatrix", lightSpaceMatrix);

    player->UpDate(projection, view);

    for (GameObject& cube : Chunks[this->chunk].Cubes)
    {
        if (player->Direction->Intersect(player->Position, cube))
        {
            cube.Selected = true;
        }
        else
        {
            cube.Selected = false;
        }
    }
}

void Game::ProcessInput(GLfloat dt)
{
    if (this->State == GAME_ACTIVE)
    {
        if (this->Keys[GLFW_KEY_W])
            player->FirstPlayerCamera->ProcessKeyboard(FORWARD, dt, player->Position);
        if (this->Keys[GLFW_KEY_S])
            player->FirstPlayerCamera->ProcessKeyboard(BACKWARD, dt, player->Position);
        if (this->Keys[GLFW_KEY_A])
            player->FirstPlayerCamera->ProcessKeyboard(LEFT, dt, player->Position);
        if (this->Keys[GLFW_KEY_D])
            player->FirstPlayerCamera->ProcessKeyboard(RIGHT, dt, player->Position);
    }
}

void Game::Render()
{
    glViewport(0, 0, this->Width, this->Height);
    glBindFramebuffer(GL_FRAMEBUFFER, depthframebuffer->ID);
    glClear(GL_DEPTH_BUFFER_BIT);

    this->Chunks[this->chunk].Draw(*cube);
    //cube->DrawObject(ResourceManager::GetTexture("box"), ResourceManager::GetTexture("box2"), glm::vec3(0.0f, 0.0f, 0.0f));

    framebuffer->BeginRender();

    glActiveTexture(GL_TEXTURE2);
    ResourceManager::GetTexture("depthTexture").Bind();

    this->Chunks[this->chunk].Draw(*cube);
    //cube->DrawObject(ResourceManager::GetTexture("box"), ResourceManager::GetTexture("box2"), glm::vec3(0.0f, 0.0f, 0.0f));

    std::stringstream x;
    std::stringstream y;
    std::stringstream z;
    x << player->Direction->ray.x;
    y << player->Direction->ray.y;
    z << player->Direction->ray.z;
    text->RenderText("Ray:(" + x.str() + ", " + y.str() + ", " + z.str() + ")", 5.0f, 5.0f);

    framebuffer->EngRender();

    screen->DrawScreen(ResourceManager::GetTexture("screenTexture"));
}

Player* Game::GetPlayer()
{
    return player;
}

Chunk* Game::GetChunk()
{
    return &Chunks[this->chunk];
}