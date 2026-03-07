
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

// The third part libs
#include <glad/glad.h>

class FrameBuffer
{
public:
	// Holds the ID of an FBO, used for all FBO operations to reference to this particlar FBO
	GLuint ID;

	GLuint Width, Height;
	// Constructor (sets default FBO modes)
	FrameBuffer();
	~FrameBuffer();
	void Configure(GLuint texture, GLuint type, GLuint rbo);
	void BeginRender();
	void EngRender();

private:

};
#endif
